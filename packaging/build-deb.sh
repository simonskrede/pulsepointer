#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$project_dir"

build_x11=true
if (($# > 1)); then
    echo "Usage: $0 [--wayland-only]" >&2
    exit 2
fi

sdk_root=${PULSEPOINTER_SDK_ROOT:-}
if [[ -n $sdk_root && ! -d $sdk_root/usr ]]; then
    echo "PULSEPOINTER_SDK_ROOT must contain an extracted usr directory" >&2
    exit 2
fi
if (($# == 1)); then
    if [[ $1 != --wayland-only ]]; then
        echo "Usage: $0 [--wayland-only]" >&2
        exit 2
    fi
    build_x11=false
fi

for command_name in cargo cmake dpkg-deb dpkg-query dpkg-shlibdeps ninja strip; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Missing build command: $command_name" >&2
        exit 1
    fi
done

if [[ -n $sdk_root ]]; then
    multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
    export CMAKE_PREFIX_PATH="$sdk_root/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    export PKG_CONFIG_PATH="$sdk_root/usr/lib/$multiarch/pkgconfig:$sdk_root/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export LIBRARY_PATH="$sdk_root/usr/lib/$multiarch${LIBRARY_PATH:+:$LIBRARY_PATH}"
else
    missing_packages=()
    build_packages=(
        build-essential
        ca-certificates
        extra-cmake-modules
        kwin-dev
        libdrm-dev
        libkf6kcmutils-dev
        libpulse-dev
        pkg-config
    )
    if $build_x11; then
        build_packages+=(kwin-x11-dev)
    fi
    for package_name in "${build_packages[@]}"; do
        if ! dpkg-query -W -f='${db:Status-Status}' "$package_name" 2>/dev/null | grep -qx installed; then
            missing_packages+=("$package_name")
        fi
    done

    if ((${#missing_packages[@]})); then
        echo "Missing build packages: ${missing_packages[*]}" >&2
        echo "Install them with: sudo apt install ${missing_packages[*]}" >&2
        exit 1
    fi
fi

app_version=$(awk -F '"' '/^version = / { print $2; exit }' Cargo.toml)
effect_version=$(sed -n \
    's/^project(pulsepointer-kwin-effect VERSION \([^ ]*\) LANGUAGES CXX)$/\1/p' \
    kwin-effect/CMakeLists.txt)
if [[ -z $app_version || $app_version != "$effect_version" ]]; then
    echo "Cargo and KWin effect versions do not match ($app_version != $effect_version)" >&2
    exit 1
fi
default_package_revision=1
if [[ -r /etc/os-release ]]; then
    distribution_id=$(sed -n 's/^ID=//p' /etc/os-release | tr -d '"' | head -n1)
    distribution_version=$(sed -n 's/^VERSION_ID=//p' /etc/os-release | tr -d '"' | head -n1)
    distribution_id=${distribution_id//[^a-zA-Z0-9.+~]/}
    distribution_version=${distribution_version//[^a-zA-Z0-9.+~]/}
    if [[ -n $distribution_id && -n $distribution_version ]]; then
        default_package_revision+="+${distribution_id}${distribution_version}"
    fi
fi
package_revision=${PACKAGE_REVISION:-$default_package_revision}
package_version="${app_version}-${package_revision}"
package_maintainer=${PACKAGE_MAINTAINER:-Simon Skrede <5637642+simonskrede@users.noreply.github.com>}
dpkg --validate-version "$package_version"
package_architecture=$(dpkg --print-architecture)
wayland_version=$(dpkg-query -W -f='${Version}' libkwin6)
if $build_x11; then
    x11_version=$(dpkg-query -W -f='${Version}' libkwin-x11-6)
fi

build_dir="$project_dir/target/package-build"
wayland_build_dir="$build_dir/kwin-wayland"
x11_build_dir="$build_dir/kwin-x11"
mkdir -p "$wayland_build_dir" "$project_dir/dist"

cargo build --release --locked
cmake --fresh -S kwin-effect -B "$wayland_build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DPULSEPOINTER_BUILD_KWIN_WAYLAND=ON \
    -DPULSEPOINTER_BUILD_KWIN_X11=OFF
cmake --build "$wayland_build_dir" --parallel

if $build_x11; then
    mkdir -p "$x11_build_dir"
    cmake --fresh -S kwin-effect -B "$x11_build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DPULSEPOINTER_BUILD_KWIN_WAYLAND=OFF \
        -DPULSEPOINTER_BUILD_KWIN_X11=ON
    cmake --build "$x11_build_dir" --parallel
fi

stage_root=$(mktemp -d /tmp/pulsepointer-package.XXXXXXXX)
cleanup() {
    case "$stage_root" in
        /tmp/pulsepointer-package.*) rm -rf -- "$stage_root" ;;
    esac
}
trap cleanup EXIT

main_stage="$stage_root/pulsepointer"
install -Dm755 target/release/pulsepointer "$main_stage/usr/bin/pulsepointer"
DESTDIR="$main_stage" cmake --install "$wayland_build_dir"
install -Dm644 packaging/systemd/pulsepointer.service \
    "$main_stage/usr/lib/systemd/user/pulsepointer.service"
mkdir -p "$main_stage/usr/lib/systemd/user/graphical-session.target.wants"
ln -s ../pulsepointer.service \
    "$main_stage/usr/lib/systemd/user/graphical-session.target.wants/pulsepointer.service"
install -Dm644 README.md "$main_stage/usr/share/doc/pulsepointer/README.md"
install -Dm644 docs/ipc-protocol.md "$main_stage/usr/share/doc/pulsepointer/ipc-protocol.md"
install -Dm644 LICENSE "$main_stage/usr/share/doc/pulsepointer/copyright"

find "$main_stage/usr/lib" -type f -name 'pulsepointer*.so' \
    -exec strip --strip-unneeded {} +
find "$main_stage" -type d -exec chmod 0755 {} +

dependency_workspace="$stage_root/dependency-metadata"
mkdir -p "$dependency_workspace/debian"
cat >"$dependency_workspace/debian/control" <<'EOF'
Source: pulsepointer
Section: utils
Priority: optional
Maintainer: Simon Skrede <5637642+simonskrede@users.noreply.github.com>

Package: pulsepointer
Architecture: any
Description: dependency scanner placeholder
EOF

calculate_dependencies() {
    local arguments=()
    local binary
    for binary in "$@"; do
        arguments+=("-e$binary")
    done
    (
        cd "$dependency_workspace"
        dpkg-shlibdeps -O "${arguments[@]}"
    ) | sed -n 's/^shlibs:Depends=//p'
}

without_dependency() {
    local excluded_package=$1
    local dependencies=$2
    local result=
    local dependency
    local trimmed
    while IFS= read -r dependency; do
        trimmed=${dependency#"${dependency%%[![:space:]]*}"}
        if [[ $trimmed == "$excluded_package" || $trimmed == "$excluded_package "* ]]; then
            continue
        fi
        if [[ -n $result ]]; then
            result+=", "
        fi
        result+="$trimmed"
    done < <(tr ',' '\n' <<<"$dependencies")
    printf '%s' "$result"
}

main_plugin=$(find "$main_stage/usr/lib" -type f -path '*/kwin/effects/plugins/pulsepointer.so' -print -quit)
main_config_plugin=$(find "$main_stage/usr/lib" -type f -name pulsepointer_config.so -print -quit)
main_dependencies=$(calculate_dependencies \
    "$main_stage/usr/bin/pulsepointer" "$main_plugin" "$main_config_plugin")
main_dependencies=$(without_dependency libkwin6 "$main_dependencies")
main_installed_size=$(du -sk "$main_stage" | cut -f1)
mkdir -p "$main_stage/DEBIAN"
cat >"$main_stage/DEBIAN/control" <<EOF
Package: pulsepointer
Version: $package_version
Architecture: $package_architecture
Maintainer: $package_maintainer
Depends: $main_dependencies, libkwin6 (= $wayland_version)
Section: utils
Priority: optional
Installed-Size: $main_installed_size
Homepage: https://github.com/simonskrede/pulsepointer
Description: Audio-reactive KDE Plasma mouse pointer effect
 PulsePointer captures the current PulseAudio or PipeWire-Pulse output monitor
 and animates the pointer or displays a visualization around it through a
 native KWin effect. This package supports Plasma Wayland sessions.
EOF

main_package="$project_dir/dist/pulsepointer_${package_version}_${package_architecture}.deb"
dpkg-deb --root-owner-group --build "$main_stage" "$main_package"
echo "Built $main_package"

if $build_x11; then
    x11_stage="$stage_root/pulsepointer-kwin-x11"
    DESTDIR="$x11_stage" cmake --install "$x11_build_dir"
    find "$x11_stage/usr/lib" -type f -name 'pulsepointer*.so' \
        -exec strip --strip-unneeded {} +
    find "$x11_stage" -type d -exec chmod 0755 {} +
    x11_plugin=$(find "$x11_stage/usr/lib" -type f -path '*/kwin-x11/effects/plugins/pulsepointer.so' -print -quit)
    x11_config_plugin=$(find "$x11_stage/usr/lib" -type f -name pulsepointer_config.so -print -quit)
    x11_dependencies=$(calculate_dependencies "$x11_plugin" "$x11_config_plugin")
    x11_dependencies=$(without_dependency libkwin-x11-6 "$x11_dependencies")
    x11_installed_size=$(du -sk "$x11_stage" | cut -f1)
    mkdir -p "$x11_stage/DEBIAN"
    cat >"$x11_stage/DEBIAN/control" <<EOF
Package: pulsepointer-kwin-x11
Version: $package_version
Architecture: $package_architecture
Maintainer: $package_maintainer
Depends: pulsepointer (= $package_version), $x11_dependencies, libkwin-x11-6 (= $x11_version)
Section: utils
Priority: optional
Installed-Size: $x11_installed_size
Homepage: https://github.com/simonskrede/pulsepointer
Description: PulsePointer KWin effect for Plasma X11
 This optional companion package lets the PulsePointer daemon display its
 visualization in a Plasma X11 session using the same native KWin effect
 architecture as the Wayland package.
EOF

    x11_package="$project_dir/dist/pulsepointer-kwin-x11_${package_version}_${package_architecture}.deb"
    dpkg-deb --root-owner-group --build "$x11_stage" "$x11_package"
    echo "Built $x11_package"
fi
