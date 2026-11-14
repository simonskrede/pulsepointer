# Upstream release package: bundle the Cargo.lock dependencies in Source0.
%global debug_package %{nil}
%{!?package_revision:%global package_revision 1}
# Allow dnf builddep to parse the spec before KWin is installed. A real build
# requires kwin-devel, which pulls in the matching native runtime library.
%global kwin_evr %(if rpm -q --whatprovides 'kwin-libs%{?_isa}' >/dev/null 2>&1; then rpm -q --whatprovides 'kwin-libs%{?_isa}' --qf '%%{EVR}'; else echo 0; fi)

Name:           pulsepointer
Version:        0.4.0
Release:        %{package_revision}%{?dist}
Summary:        Audio-reactive KDE Plasma pointer and desktop effects
License:        MIT
URL:            https://github.com/simonskrede/pulsepointer
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cargo
BuildRequires:  redhat-rpm-config
BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  ninja-build
BuildRequires:  kwin-devel >= 6.6
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  libdrm-devel
BuildRequires:  libepoxy-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  pkgconf-pkg-config
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-rpm-macros
BuildRequires:  systemd-rpm-macros
# Mesa supplies the surfaceless software renderer used by the shader tests.
BuildRequires:  mesa-dri-drivers
BuildRequires:  mesa-libEGL
BuildRequires:  mesa-libGLES

# KWin's native effect API is private; a SONAME dependency is insufficient.
Requires:       kwin-libs%{?_isa} = %{kwin_evr}
Requires:       pipewire-pulseaudio
%{?systemd_requires}

%description
PulsePointer captures the system audio output through PipeWire's PulseAudio
service and combines cursor animation, pointer visualizers, and desktop
effects through a native KWin plugin. Requires a KDE Plasma Wayland session.

%prep
%autosetup

%build
export CARGO_HOME="$PWD/.cargo-home"
cargo build --release --frozen
%cmake -S kwin-effect -G Ninja \
    -DKDE_INSTALL_QTPLUGINDIR=%{_qt6_plugindir} \
    -DBUILD_TESTING=ON \
    -DPULSEPOINTER_BUILD_KWIN_WAYLAND=ON \
    -DPULSEPOINTER_BUILD_KWIN_X11=OFF
%cmake_build

%install
install -Dm755 target/release/pulsepointer %{buildroot}%{_bindir}/pulsepointer
%cmake_install
install -Dm644 packaging/systemd/pulsepointer.service \
    %{buildroot}%{_userunitdir}/pulsepointer.service
mkdir -p %{buildroot}%{_userunitdir}/graphical-session.target.wants
ln -s ../pulsepointer.service \
    %{buildroot}%{_userunitdir}/graphical-session.target.wants/pulsepointer.service
mkdir -p %{buildroot}%{_docdir}/%{name}/docs
cp -a README.md CHANGELOG.md %{buildroot}%{_docdir}/%{name}/
cp -a docs/music-effects.md docs/ipc-protocol.md %{buildroot}%{_docdir}/%{name}/docs/

%check
export CARGO_HOME="$PWD/.cargo-home"
cargo test --frozen
%ctest --output-on-failure --no-tests=error

%post
%systemd_user_post pulsepointer.service

%preun
%systemd_user_preun pulsepointer.service

%postun
%systemd_user_postun_with_restart pulsepointer.service

%files
%license LICENSE
%{_bindir}/pulsepointer
%{_qt6_plugindir}/kwin/effects/plugins/pulsepointer.so
%{_qt6_plugindir}/kwin/effects/configs/pulsepointer_config.so
%{_userunitdir}/pulsepointer.service
%dir %{_userunitdir}/graphical-session.target.wants
%{_userunitdir}/graphical-session.target.wants/pulsepointer.service
%doc %{_docdir}/%{name}

%changelog
* Sat Nov 14 2026 Simon Skrede <5637642+simonskrede@users.noreply.github.com> - 0.4.0-1
- Add Fedora 45 Wayland packaging with an exact KWin dependency.
