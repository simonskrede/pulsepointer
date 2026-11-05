#!/usr/bin/env bash
set -Eeuo pipefail
# Terminal colors (ANSI)
if [[ -t 1 && -z "${NO_COLOR:-}" && "${TERM:-dumb}" != "dumb" ]]; then
    RED=$'\033[0;31m'
    GREEN=$'\033[0;32m'
    YELLOW=$'\033[0;33m'
    BLUE=$'\033[0;34m'
    CYAN=$'\033[0;36m'
    GRAY=$'\033[0;90m'
    BOLD=$'\033[1m'
    RESET=$'\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    GRAY=''
    BOLD=''
    RESET=''
fi

# PulsePointer local installer for Arch Linux and Arch-based distributions.
# It installs only PulsePointer files. Build dependencies are intentionally
# never removed by the uninstaller.

readonly APP_ID="pulsepointer-arch"
readonly SERVICE_NAME="pulsepointer.service"
readonly STATE_DIR="/var/lib/${APP_ID}"
readonly OWNED_FILE="${STATE_DIR}/owned-files.txt"
readonly BACKUP_LIST="${STATE_DIR}/backed-up-files.txt"
readonly BACKUP_DIR="${STATE_DIR}/backup"

readonly PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SYSTEMD_UNIT="/usr/lib/systemd/user/${SERVICE_NAME}"
readonly SYSTEMD_WANTS="/usr/lib/systemd/user/graphical-session.target.wants/${SERVICE_NAME}"

print_header() {
    printf "${CYAN}╭──────────────────────────────────────╮${RESET}\n"
    printf "${CYAN}│${RESET}       ${BOLD}PulsePointer for Arch${RESET}          ${CYAN}│${RESET}\n"
    printf "${CYAN}╰──────────────────────────────────────╯${RESET}\n"
    printf '\n'
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_regular_user() {
    [[ "${EUID}" -ne 0 ]] || die "Run this script as a regular user, not as root."
}

require_arch() {
    [[ -r /etc/os-release ]] || die "Cannot identify the Linux distribution."
    # shellcheck disable=SC1091
    source /etc/os-release

    case " ${ID:-} ${ID_LIKE:-} " in
        *" arch "*) ;;
        *) die "This installer supports Arch Linux and Arch-based distributions." ;;
    esac

    command -v pacman >/dev/null 2>&1 || die "pacman was not found."
    command -v sudo >/dev/null 2>&1 || die "sudo is required."
}

detect_session() {
    local session_type="${XDG_SESSION_TYPE:-}"

    if [[ -z "$session_type" && -n "${XDG_SESSION_ID:-}" ]] && command -v loginctl >/dev/null 2>&1; then
        session_type="$(loginctl show-session "$XDG_SESSION_ID" -p Type --value 2>/dev/null || true)"
    fi

    case "$session_type" in
        wayland)
            printf 'wayland\n'
            ;;
        x11)
            printf 'x11\n'
            ;;
        *)
            die "Could not detect a supported graphical session (Wayland/X11). Run this from your KDE desktop session."
            ;;
    esac
}

show_menu() {
    print_header

    printf "${GRAY}Select an action:${RESET}\n"
    printf "  ${GREEN}1)${RESET} Install PulsePointer\n"
    printf "  ${RED}2)${RESET} Uninstall PulsePointer\n"
    printf "  ${GRAY}3)${RESET} Exit\n\n"

    local choice
    read -r -p "$(printf "${CYAN}Choose [1-3]: ${RESET}")" choice

    case "$choice" in
        1) install_pulsepointer ;;
        2) uninstall_pulsepointer ;;
        3) exit 0 ;;
        *) die "Invalid selection." ;;
    esac
}

install_pulsepointer() {
    require_regular_user
    require_arch

    local session_type
    session_type="$(detect_session)"

    if [[ "$session_type" == "x11" ]] &&
       ! pacman -Q kwin-x11 >/dev/null 2>&1 &&
       ! pacman -Si kwin-x11 >/dev/null 2>&1; then
        die "KWin X11 (kwin-x11) is not installed or available in your repositories. Run this installer from Plasma Wayland, or install a compatible KWin X11 package first."
    fi

    local build_dir="${PROJECT_DIR}/target/arch-kwin-${session_type}"

    # The previous installer did not keep a system-wide ownership manifest.
    # Capture its CMake manifest before the old build directory is cleaned.
    local legacy_manifest="${PROJECT_DIR}/target/arch-kwin-wayland/install_manifest.txt"
    local -a legacy_cmake_files=()
    if [[ -f "$legacy_manifest" ]]; then
        mapfile -t legacy_cmake_files < "$legacy_manifest"
    fi

    local stage_dir
    stage_dir="$(mktemp -d "${TMPDIR:-/tmp}/pulsepointer-stage.XXXXXXXX")"
    trap "rm -rf -- $(printf '%q' "$stage_dir")" RETURN

    [[ -f "${PROJECT_DIR}/Cargo.toml" ]] || die "Cargo.toml was not found."
    [[ -f "${PROJECT_DIR}/Cargo.lock" ]] || die "Cargo.lock was not found; --locked builds require it."
    [[ -f "${PROJECT_DIR}/kwin-effect/CMakeLists.txt" ]] || die "KWin effect CMakeLists.txt was not found."
    [[ -f "${PROJECT_DIR}/packaging/systemd/pulsepointer.service" ]] || die "PulsePointer systemd service file was not found."
    [[ -f "${PROJECT_DIR}/README.md" ]] || die "README.md was not found."
    [[ -f "${PROJECT_DIR}/LICENSE" ]] || die "LICENSE was not found."

    print_header
    printf 'Distribution: %s\n' "${PRETTY_NAME:-Arch-based Linux}"
    printf 'Session:      %s\n\n' "$session_type"

    local -a packages=(
        base-devel
        cmake
        extra-cmake-modules
        kcmutils
        qt6-base
        qt6-declarative
        libpulse
        pkgconf
        ninja
        rust
        vulkan-headers
    )

    local -a cmake_options=(
        -DPULSEPOINTER_BUILD_KWIN_WAYLAND=OFF
        -DPULSEPOINTER_BUILD_KWIN_X11=OFF
    )

    if [[ "$session_type" == "wayland" ]]; then
        packages+=(kwin)
        cmake_options[0]="-DPULSEPOINTER_BUILD_KWIN_WAYLAND=ON"
    else
        # A compatible distribution may retain an installed KWin X11 package
        # even after it is no longer available in its package repositories.
        if ! pacman -Q kwin-x11 >/dev/null 2>&1; then
            packages+=(kwin-x11)
        fi
        cmake_options[1]="-DPULSEPOINTER_BUILD_KWIN_X11=ON"
    fi

    printf "\n${CYAN}[1/6]${RESET} Installing build dependencies...\n"
    sudo -v
    sudo pacman -S --needed "${packages[@]}"

    printf "\n${CYAN}[2/6]${RESET} Checking build environment...\n"
    local command_name
    for command_name in cargo cmake ninja pkg-config; do
        command -v "$command_name" >/dev/null 2>&1 || die "Required command is missing: $command_name"
    done

    pkg-config --exists libpulse || die "PulseAudio development files (libpulse) were not found."

    if [[ "$session_type" == "wayland" ]]; then
        [[ -f /usr/lib/cmake/KWin/KWinConfig.cmake ]] ||
            die "KWin CMake files were not found. Check the installed kwin package."
    else
        [[ -f /usr/lib/cmake/KWinX11/KWinX11Config.cmake ]] ||
            die "KWinX11 CMake files were not found. Check the installed kwin-x11 package."
    fi

    printf "\n${CYAN}[3/6]${RESET} Building Rust daemon...\n"
    (
        cd "$PROJECT_DIR"
        cargo build --release --locked
    )
    [[ -x "${PROJECT_DIR}/target/release/pulsepointer" ]] ||
        die "Rust build succeeded but target/release/pulsepointer was not produced."

    printf "\n${CYAN}[4/6]${RESET} Building KWin %s effect...\n" "$session_type"
    rm -rf -- "$build_dir"

    cmake -S "${PROJECT_DIR}/kwin-effect" \
        -B "$build_dir" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        "${cmake_options[@]}"

    cmake --build "$build_dir" --parallel

    printf "\n${CYAN}[5/6]${RESET} Preparing staged installation...\n"
    install -Dm755 \
        "${PROJECT_DIR}/target/release/pulsepointer" \
        "${stage_dir}/usr/bin/pulsepointer"

    DESTDIR="$stage_dir" cmake --install "$build_dir"

    install -Dm644 \
        "${PROJECT_DIR}/packaging/systemd/pulsepointer.service" \
        "${stage_dir}/usr/lib/systemd/user/${SERVICE_NAME}"

    install -d "${stage_dir}/usr/lib/systemd/user/graphical-session.target.wants"
    ln -s "../${SERVICE_NAME}" \
        "${stage_dir}/usr/lib/systemd/user/graphical-session.target.wants/${SERVICE_NAME}"

    [[ -d "${stage_dir}/usr" ]] || die "The staged installation is empty."

    # The stage is deliberately restricted to /usr. The manifest contains
    # only regular files and symlinks created by this installation.
    local -a new_files=()
    mapfile -t new_files < <(
        cd "$stage_dir"
        find usr \( -type f -o -type l \) -print | LC_ALL=C sort
    )
    ((${#new_files[@]} > 0)) || die "No files were produced by the staged installation."

    local -A new_set=()
    local -A old_owned_set=()
    local -A old_backup_set=()
    local -a old_owned=()
    local -a old_backups=()
    local -a next_backups=()

    sudo install -d -m700 "$STATE_DIR" "$BACKUP_DIR"

    if sudo test -f "$OWNED_FILE"; then
        mapfile -t old_owned < <(sudo cat "$OWNED_FILE")
    fi
    if sudo test -f "$BACKUP_LIST"; then
        mapfile -t old_backups < <(sudo cat "$BACKUP_LIST")
    fi

    # Adopt files installed by the earlier version of this repository's
    # installer. This avoids treating the old PulsePointer install as an
    # unrelated file and restoring it after a later uninstall.
    if ((${#old_owned[@]} == 0)); then
        local legacy_path legacy_rel
        for legacy_path in "${legacy_cmake_files[@]}"; do
            case "$legacy_path" in
                /usr/*)
                    legacy_rel="${legacy_path#/}"
                    if [[ -e "$legacy_path" || -L "$legacy_path" ]]; then
                        old_owned+=("$legacy_rel")
                    fi
                    ;;
            esac
        done

        if [[ -f /usr/lib/systemd/user/pulsepointer.service ]] &&
           cmp -s "$PROJECT_DIR/packaging/systemd/pulsepointer.service" /usr/lib/systemd/user/pulsepointer.service; then
            local legacy_fixed
            for legacy_fixed in \
                usr/bin/pulsepointer \
                usr/lib/systemd/user/pulsepointer.service \
                usr/lib/systemd/user/graphical-session.target.wants/pulsepointer.service \
                usr/share/doc/pulsepointer/README.md \
                usr/share/licenses/pulsepointer/LICENSE; do
                if [[ -e "/$legacy_fixed" || -L "/$legacy_fixed" ]]; then
                    old_owned+=("$legacy_fixed")
                fi
            done
        fi
    fi

    local rel
    for rel in "${old_owned[@]}"; do
        [[ "$rel" == usr/* && "$rel" != *"/../"* ]] || die "Invalid path in the previous installation manifest."
        old_owned_set["$rel"]=1
    done
    for rel in "${old_backups[@]}"; do
        [[ "$rel" == usr/* && "$rel" != *"/../"* ]] || die "Invalid path in the backup manifest."
        old_backup_set["$rel"]=1
    done
    for rel in "${new_files[@]}"; do
        [[ "$rel" == usr/* && "$rel" != *"/../"* ]] || die "Unexpected path in staged installation."
        new_set["$rel"]=1
    done

    printf "\n${CYAN}[6/6]${RESET} Installing PulsePointer files...\n"

    # Remove files from a previous backend/build that are no longer part of
    # this install. Restore any pre-existing file that this installer backed up.
    for rel in "${old_owned[@]}"; do
        if [[ -z "${new_set[$rel]+present}" ]]; then
            sudo rm -f -- "/$rel"
            if [[ -n "${old_backup_set[$rel]+present}" ]]; then
                if sudo test -e "${BACKUP_DIR}/${rel}" || sudo test -L "${BACKUP_DIR}/${rel}"; then
                    sudo install -d "/$(dirname -- "$rel")"
                    sudo cp -a -- "${BACKUP_DIR}/${rel}" "/$rel"
                    sudo rm -f -- "${BACKUP_DIR}/${rel}"
                fi
            fi
        fi
    done

    # Back up pre-existing files not owned by this installer before replacing
    # them. This makes uninstall restore the exact previous file rather than
    # deleting a file that may have belonged to another package/application.
    for rel in "${new_files[@]}"; do
        local destination="/${rel}"

        if [[ -d "$destination" && ! -L "$destination" ]]; then
            die "A directory already exists where PulsePointer needs to install a file: $destination"
        fi

        if [[ -e "$destination" || -L "$destination" ]]; then
            if [[ -z "${old_owned_set[$rel]+present}" && -z "${old_backup_set[$rel]+present}" ]]; then
                sudo install -d "${BACKUP_DIR}/$(dirname -- "$rel")"
                sudo cp -a -- "$destination" "${BACKUP_DIR}/${rel}"
                old_backup_set["$rel"]=1
            fi
        fi

        sudo install -d "$(dirname -- "$destination")"
        sudo rm -f -- "$destination"
        sudo cp -a -- "${stage_dir}/${rel}" "$destination"
        sudo chown -h root:root -- "$destination"
    done

    # Keep only backups for files still owned by the new installation.
    for rel in "${new_files[@]}"; do
        if [[ -n "${old_backup_set[$rel]+present}" ]]; then
            next_backups+=("$rel")
        fi
    done

    local owned_tmp backup_tmp
    owned_tmp="$(mktemp)"
    backup_tmp="$(mktemp)"
    printf '%s\n' "${new_files[@]}" > "$owned_tmp"
    if ((${#next_backups[@]} > 0)); then
        printf '%s\n' "${next_backups[@]}" > "$backup_tmp"
    fi

    sudo install -Dm644 "$owned_tmp" "$OWNED_FILE"
    sudo install -Dm644 "$backup_tmp" "$BACKUP_LIST"
    rm -f -- "$owned_tmp" "$backup_tmp"

    # Remove backup copies no longer needed after a backend switch.
    for rel in "${old_backups[@]}"; do
        if [[ -z "${new_set[$rel]+present}" ]]; then
            sudo rm -f -- "${BACKUP_DIR}/${rel}"
        fi
    done

    systemctl --user daemon-reload 2>/dev/null || true

    printf '\n'
    printf "${GREEN}╭──────────────────────────────────────╮${RESET}\n"
    printf "${GREEN}│${RESET}       ${BOLD}Installation successful!${RESET}       ${GREEN}│${RESET}\n"
    printf "${GREEN}╰──────────────────────────────────────╯${RESET}\n\n"
    printf "${GRAY}PulsePointer was built for:${RESET} ${CYAN}%s${RESET}\n" "$session_type"
    printf "${YELLOW}Log out and back in once so KWin discovers the effect.${RESET}\n"
    printf "${GRAY}The pulsepointer.service user service is enabled for graphical sessions.${RESET}\n"
    printf "${GRAY}Open System Settings → Desktop Effects to configure PulsePointer.${RESET}\n\n"
}

uninstall_pulsepointer() {
    require_regular_user
    command -v sudo >/dev/null 2>&1 || die "sudo is required to remove system-installed files."

    print_header

    if ! sudo test -f "$OWNED_FILE"; then
        printf 'No PulsePointer installation managed by this installer was found.\n'
        printf 'No system files were changed.\n'
        return 0
    fi

    printf 'This removes PulsePointer files only.\n'
    printf 'Rust, Cargo, CMake, Qt, KWin, system packages, project sources,\n'
    printf 'and user settings will NOT be removed.\n\n'

    local confirmation
    read -r -p "Uninstall PulsePointer? [y/N]: " confirmation
    [[ "$confirmation" =~ ^[Yy]$ ]] || {
        printf 'Uninstallation cancelled.\n'
        return 0
    }

    # Stop the service for the current logged-in user, if their user manager
    # is available. The global Wants symlink is removed with the owned files.
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user stop "$SERVICE_NAME" 2>/dev/null || true
    fi

    local -a owned_files=()
    local -a backup_files=()
    mapfile -t owned_files < <(sudo cat "$OWNED_FILE")
    if sudo test -f "$BACKUP_LIST"; then
        mapfile -t backup_files < <(sudo cat "$BACKUP_LIST")
    fi

    local -A backup_set=()
    local rel
    for rel in "${backup_files[@]}"; do
        [[ "$rel" == usr/* && "$rel" != *"/../"* ]] || die "Invalid path in backup manifest; refusing unsafe removal."
        backup_set["$rel"]=1
    done

    printf '\nRemoving PulsePointer files...\n'
    for rel in "${owned_files[@]}"; do
        [[ "$rel" == usr/* && "$rel" != *"/../"* ]] || die "Invalid path in installation manifest; refusing unsafe removal."

        sudo rm -f -- "/$rel"

        if [[ -n "${backup_set[$rel]+present}" ]]; then
            if sudo test -e "${BACKUP_DIR}/${rel}" || sudo test -L "${BACKUP_DIR}/${rel}"; then
                sudo install -d "/$(dirname -- "$rel")"
                sudo cp -a -- "${BACKUP_DIR}/${rel}" "/$rel"
            fi
        fi
    done

    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user daemon-reload 2>/dev/null || true
    fi

    # This directory is created exclusively by this installer and contains
    # only its manifests and backups of files it replaced.
    sudo rm -rf -- "$STATE_DIR"

    printf '\n'
    printf "${GREEN}╭──────────────────────────────────────╮${RESET}\n"
    printf "${GREEN}│${RESET}       ${BOLD}Uninstallation complete!${RESET}       ${GREEN}│${RESET}\n"
    printf "${GREEN}╰──────────────────────────────────────╯${RESET}\n\n"
    printf "${GREEN}✓${RESET} PulsePointer system files have been removed.\n"
    printf "${GREEN}✓${RESET} Previously existing files have been restored.\n"
    printf "${GREEN}✓${RESET} Build tools and system dependencies were left untouched.\n"
    printf "${GREEN}✓${RESET} Source repository and user configuration were left untouched.\n\n"
}

show_help() {
    cat <<EOF
Usage: ./install.sh [option]

Options:
  --install     Install PulsePointer for the current Wayland/X11 session
  --uninstall   Remove PulsePointer files installed by this script
  --help        Show this help

Without an option, an interactive menu is displayed.
EOF
}

main() {
    require_regular_user

    case "${1:-}" in
        --install)
            install_pulsepointer
            ;;
        --uninstall)
            uninstall_pulsepointer
            ;;
        --help|-h)
            show_help
            ;;
        "")
            show_menu
            ;;
        *)
            show_help
            die "Unknown option: $1"
            ;;
    esac
}

main "$@"
