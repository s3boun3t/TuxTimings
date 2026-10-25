# Maintainer: Death4two <https://github.com/Death4two>
pkgbase=tuxtimings
pkgname=('tuxtimings' 'tuxtimings-dkms')
pkgver=1.0.5
pkgrel=8
pkgdesc="AMD Ryzen DRAM timings and CPU telemetry viewer (GTK4)"
arch=('x86_64')
url="https://github.com/Death4two/TuxTimings"
license=('GPL3')
makedepends=('gcc' 'pkgconf' 'gtk4' 'gmp')
source=("$pkgbase-$pkgver.tar.gz::https://github.com/Death4two/TuxTimings/archive/refs/heads/main.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/TuxTimings-main/Linux"
    # Portable profile and shared embedded identity; no module build here.
    make PROFILE=portable all
}

package_tuxtimings() {
    pkgdesc="AMD Ryzen DRAM timings and CPU telemetry viewer (GTK4)"
    depends=('gtk4' 'gmp' 'polkit' 'dmidecode' 'kmod')
    optdepends=(
        'tuxtimings-dkms: kernel modules for accurate benchmarking and memory voltages'
        'ryzen_smu-dkms-git: kernel module for reading AMD SMN/PM tables'
        'nct6775-dkms-git: fan readings on boards with Nuvoton Super I/O (NCT6775F through NCT6799D)'
    )

    cd "$srcdir/TuxTimings-main/Linux"

    # Binary
    install -Dm755 tuxtimings "$pkgdir/opt/TuxTimings/bin/tuxtimings"

    install -Dm644 build/build-info.txt "$pkgdir/usr/share/doc/tuxtimings/build-info.txt"

    # Icon
    install -Dm644 tuxtimings.png "$pkgdir/usr/share/icons/hicolor/256x256/apps/tuxtimings.png"

    # Polkit policy
    install -Dm644 /dev/stdin "$pkgdir/usr/share/polkit-1/actions/com.tuxtimings.policy" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC
 "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <action id="com.tuxtimings.run">
    <description>Run TuxTimings</description>
    <message>Authentication is required to run TuxTimings</message>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>auth_admin</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
    <annotate key="org.freedesktop.policykit.exec.path">/opt/TuxTimings/bin/tuxtimings</annotate>
    <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
  </action>
</policyconfig>
EOF

    # Desktop file
    install -Dm644 /dev/stdin "$pkgdir/usr/share/applications/tuxtimings.desktop" << 'DESKTOP'
[Desktop Entry]
Name=TuxTimings
Comment=AMD Ryzen DRAM timings viewer
Exec=tuxtimings
Icon=tuxtimings
Terminal=false
Type=Application
Categories=Utility;
DESKTOP

    # Launcher script (handles env forwarding through pkexec)
    install -Dm755 /dev/stdin "$pkgdir/usr/bin/tuxtimings" << 'LAUNCHER'
#!/bin/bash
# The executable handles --version and its own elevation with exact argv.
exec /opt/TuxTimings/bin/tuxtimings "$@"
LAUNCHER
}

package_tuxtimings-dkms() {
    pkgdesc="DKMS kernel modules for TuxTimings (aod-voltages, tuxbench)"
    depends=('dkms' 'linux-headers')
    optdepends=('clang: required if your kernel was built with Clang (CachyOS, etc)')
    install=tuxtimings-dkms.install

    cd "$srcdir/TuxTimings-main/Linux"

    # Shared staging recipe includes every local header; no module is built here.
    bash tools/stage-dkms.sh "$pkgdir/usr/src"
}
