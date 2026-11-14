#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$project_dir"

if (($#)); then
    echo "Usage: $0 (set PACKAGE_REVISION to a positive rebuild number)" >&2
    exit 2
fi

if [[ ! -r /etc/os-release ]]; then
    echo "Cannot identify the build distribution." >&2
    exit 1
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ ${ID:-} != fedora || ${VERSION_ID:-} != 45 ]]; then
    echo "Build RPMs on Fedora 45, using its fully updated development packages." >&2
    exit 1
fi

package_revision=${PACKAGE_REVISION:-1}
if [[ ! $package_revision =~ ^[1-9][0-9]*$ ]]; then
    echo "PACKAGE_REVISION must be a positive integer." >&2
    exit 2
fi
for command_name in cargo rpmbuild rpm tar; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Missing build command: $command_name" >&2
        exit 1
    fi
done

app_version=$(awk -F '"' '/^version = / { print $2; exit }' Cargo.toml)
effect_version=$(sed -n \
    's/^project(pulsepointer-kwin-effect VERSION \([^ ]*\) LANGUAGES CXX)$/\1/p' \
    kwin-effect/CMakeLists.txt)
spec_version=$(awk '/^Version:/ { print $2; exit }' packaging/pulsepointer.spec)
if [[ -z $app_version || $app_version != "$effect_version" || $app_version != "$spec_version" ]]; then
    echo "Cargo, KWin effect, and RPM spec versions must match." >&2
    exit 1
fi

build_dir="$project_dir/target/rpm-build"
mkdir -p "$build_dir" "$project_dir/dist"
source_root=$(mktemp -d "$build_dir/sources.XXXXXXXX")
trap 'rm -rf -- "$source_root"' EXIT
source_dir="$source_root/pulsepointer-$app_version"
mkdir -p "$source_dir/.cargo"
# Copy the working source, including local changes, without build outputs or Git data.
cp -a Cargo.toml Cargo.lock LICENSE README.md CHANGELOG.md \
    src kwin-effect tests docs packaging "$source_dir/"
(
    cd "$source_dir"
    # Bundle the locked dependencies so rpmbuild and SRPM rebuilds work offline.
    cargo vendor --locked --versioned-dirs vendor >.cargo/config.toml
)
tar -C "$source_root" -czf "$source_root/pulsepointer-$app_version.tar.gz" \
    "pulsepointer-$app_version"

rpmbuild -ba packaging/pulsepointer.spec \
    --define "_topdir $build_dir" \
    --define "_sourcedir $source_root" \
    --define "_rpmdir $project_dir/dist" \
    --define "_srcrpmdir $project_dir/dist" \
    --define '_rpmfilename %{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm' \
    --define "package_revision $package_revision"

echo "Built Fedora RPMs in $project_dir/dist"
