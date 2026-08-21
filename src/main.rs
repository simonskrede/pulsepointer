use clap::Parser;
use std::{
    io,
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc::{self, RecvTimeoutError},
        Arc,
    },
    thread,
    time::Duration,
};

mod audio;
mod config;
mod ipc;

use crate::config::CONTROL_POLL_INTERVAL_MS;
use crate::ipc::TelemetrySender;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// PulseAudio source to capture instead of @DEFAULT_MONITOR@.
    #[arg(long)]
    source: Option<String>,

    /// Log the normalized audio level once per second.
    #[arg(long)]
    log_levels: bool,
}

fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let args = Args::parse();
    let mut sender = TelemetrySender::connect_default()?;

    let running = Arc::new(AtomicBool::new(true));
    let running_audio = Arc::clone(&running);
    let running_signal = Arc::clone(&running);
    ctrlc::set_handler(move || {
        running_signal.store(false, Ordering::Relaxed);
    })?;

    // One pending snapshot is enough. If the main thread is delayed, the
    // audio thread preserves the strongest pending onset and drops stale data.
    let (tx, rx) = mpsc::sync_channel(1);
    let audio_handle = thread::Builder::new()
        .name("pulsepointer-audio".to_string())
        .spawn(move || audio::audio_loop(running_audio, tx, args.source, args.log_levels))?;

    let mut sequence = 0u64;
    let mut effect_connected = false;
    let mut audio_finished = false;

    eprintln!("PulsePointer audio daemon started.");

    while running.load(Ordering::Relaxed) {
        let audio_data = match rx.recv_timeout(Duration::from_millis(CONTROL_POLL_INTERVAL_MS)) {
            Ok(data) => data,
            Err(RecvTimeoutError::Timeout) => continue,
            Err(RecvTimeoutError::Disconnected) => {
                audio_finished = true;
                break;
            }
        };

        match sender.send(sequence, &audio_data) {
            Ok(_) => {
                if !effect_connected {
                    eprintln!("PulsePointer connected to the KWin effect.");
                    effect_connected = true;
                }
            }
            Err(error)
                if matches!(
                    error.kind(),
                    io::ErrorKind::NotFound | io::ErrorKind::ConnectionRefused
                ) =>
            {
                if effect_connected {
                    eprintln!("PulsePointer KWin effect disconnected; waiting for it to return.");
                    effect_connected = false;
                }
            }
            Err(error) if error.kind() == io::ErrorKind::WouldBlock => {
                // KWin is behind; dropping telemetry is preferable to blocking.
            }
            Err(error) => {
                if effect_connected {
                    eprintln!("PulsePointer IPC error: {error}; retrying.");
                    effect_connected = false;
                }
            }
        }
        sequence = sequence.wrapping_add(1);
    }

    running.store(false, Ordering::Relaxed);
    if audio_finished {
        match audio_handle.join() {
            Ok(result) => result?,
            Err(error) => {
                return Err(io::Error::other(format!(
                    "audio thread terminated unexpectedly: {error:?}"
                ))
                .into());
            }
        }
    }
    // pa_simple_read() is blocking and offers no cancellation handle. On a
    // signal, dropping the JoinHandle lets process shutdown terminate a
    // suspended read immediately instead of delaying exit indefinitely.
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn daemon_options_are_audio_only() {
        let defaults = Args::try_parse_from(["pulsepointer"]).unwrap();
        assert!(defaults.source.is_none());
        assert!(!defaults.log_levels);

        let configured =
            Args::try_parse_from(["pulsepointer", "--source", "test.monitor", "--log-levels"])
                .unwrap();
        assert_eq!(configured.source.as_deref(), Some("test.monitor"));
        assert!(configured.log_levels);
    }
}
