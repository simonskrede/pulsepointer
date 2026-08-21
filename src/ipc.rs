use std::{env, io, os::unix::net::UnixDatagram, path::PathBuf};

use crate::config::{AudioData, SPECTRUM_BUCKETS, WAVEFORM_SAMPLES};

pub const MAGIC: [u8; 4] = *b"PPAU";
pub const PROTOCOL_VERSION: u16 = 2;
pub const HEADER_LEN: usize = 16;
pub const SUMMARY_VALUES: usize = 5;
pub const PACKET_LEN: usize =
    HEADER_LEN + (SUMMARY_VALUES + SPECTRUM_BUCKETS + WAVEFORM_SAMPLES) * size_of::<f32>();
pub const SOCKET_SUBDIRECTORY: &str = "pulsepointer";
pub const SOCKET_FILENAME: &str = "kwin-effect.sock";

pub struct TelemetrySender {
    socket: UnixDatagram,
    destination: PathBuf,
    packet: Vec<u8>,
}

impl TelemetrySender {
    pub fn connect_default() -> io::Result<Self> {
        Self::new(socket_path()?)
    }

    pub fn new(destination: PathBuf) -> io::Result<Self> {
        let socket = UnixDatagram::unbound()?;
        socket.set_nonblocking(true)?;
        Ok(Self {
            socket,
            destination,
            packet: Vec::with_capacity(PACKET_LEN),
        })
    }

    pub fn send(&mut self, sequence: u64, audio: &AudioData) -> io::Result<usize> {
        encode_telemetry(&mut self.packet, sequence, audio);
        self.socket.send_to(&self.packet, &self.destination)
    }
}

pub fn socket_path() -> io::Result<PathBuf> {
    let runtime_dir = env::var_os("XDG_RUNTIME_DIR").ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::NotFound,
            "XDG_RUNTIME_DIR is not set; PulsePointer must run in a graphical user session",
        )
    })?;
    Ok(PathBuf::from(runtime_dir)
        .join(SOCKET_SUBDIRECTORY)
        .join(SOCKET_FILENAME))
}

fn encode_telemetry(packet: &mut Vec<u8>, sequence: u64, audio: &AudioData) {
    packet.clear();
    packet.extend_from_slice(&MAGIC);
    packet.extend_from_slice(&PROTOCOL_VERSION.to_le_bytes());
    packet.extend_from_slice(&0_u16.to_le_bytes());
    packet.extend_from_slice(&sequence.to_le_bytes());
    append_values(
        packet,
        &[
            audio.level,
            audio.bass,
            audio.midrange,
            audio.treble,
            audio.onset,
        ],
        false,
    );
    append_values(packet, &audio.spectrum, false);
    append_values(packet, &audio.waveform, true);
    debug_assert_eq!(packet.len(), PACKET_LEN);
}

fn append_values(packet: &mut Vec<u8>, values: &[f32], signed: bool) {
    for value in values {
        let sanitized = sanitize(*value, signed);
        packet.extend_from_slice(&sanitized.to_le_bytes());
    }
}

fn sanitize(value: f32, signed: bool) -> f32 {
    if !value.is_finite() {
        return 0.0;
    }
    if signed {
        value.clamp(-1.0, 1.0)
    } else {
        value.clamp(0.0, 1.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn audio_data() -> AudioData {
        AudioData {
            level: 0.25,
            bass: 0.5,
            midrange: 0.75,
            treble: f32::NAN,
            onset: 2.0,
            spectrum: [0.125; SPECTRUM_BUCKETS],
            waveform: [-0.5; WAVEFORM_SAMPLES],
        }
    }

    #[test]
    fn telemetry_encoding_has_fixed_layout_and_sanitizes_values() {
        let mut packet = Vec::new();

        encode_telemetry(&mut packet, 7, &audio_data());

        let read_float =
            |offset| f32::from_le_bytes(packet[offset..offset + 4].try_into().unwrap());
        assert_eq!(packet.len(), PACKET_LEN);
        assert_eq!(&packet[0..4], b"PPAU");
        assert_eq!(u16::from_le_bytes(packet[4..6].try_into().unwrap()), 2);
        assert_eq!(u16::from_le_bytes(packet[6..8].try_into().unwrap()), 0);
        assert_eq!(u64::from_le_bytes(packet[8..16].try_into().unwrap()), 7);
        assert_eq!(read_float(16), 0.25);
        assert_eq!(read_float(20), 0.5);
        assert_eq!(read_float(24), 0.75);
        assert_eq!(read_float(28), 0.0);
        assert_eq!(read_float(32), 1.0);
        assert_eq!(read_float(36), 0.125);
        assert_eq!(read_float(36 + SPECTRUM_BUCKETS * 4), -0.5);
    }

    #[test]
    fn telemetry_encoding_reuses_packet_allocation() {
        let mut packet = Vec::with_capacity(PACKET_LEN);
        encode_telemetry(&mut packet, 1, &audio_data());
        let pointer = packet.as_ptr();

        encode_telemetry(&mut packet, 2, &audio_data());

        assert_eq!(packet.as_ptr(), pointer);
    }
}
