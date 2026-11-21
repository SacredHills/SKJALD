#!/bin/bash
# Builds a .deb (installs to /usr/lib/vst3 and /usr/bin) and a .tar.gz with an install script.
# usage: build_deb.sh <package dir containing SKJALD.vst3 + skjald_offline> <version> <output dir>
set -euo pipefail
PKG_DIR=$(cd "$1" && pwd); VERSION=$2; OUT=$(mkdir -p "$3" && cd "$3" && pwd)
WORK=$(mktemp -d); ROOT="$WORK/skjald_${VERSION}_amd64"
mkdir -p "$ROOT/DEBIAN" "$ROOT/usr/lib/vst3" "$ROOT/usr/bin" "$ROOT/usr/share/doc/skjald"
cp -R "$PKG_DIR/SKJALD.vst3" "$ROOT/usr/lib/vst3/"
cp "$PKG_DIR/skjald_offline" "$ROOT/usr/bin/skjald_offline"; chmod 755 "$ROOT/usr/bin/skjald_offline"
cp "$PKG_DIR/INSTALL.md" "$ROOT/usr/share/doc/skjald/"
find "$ROOT/usr" -type d -exec chmod 755 {} +
cat > "$ROOT/DEBIAN/control" <<CTL
Package: skjald
Version: $VERSION
Section: sound
Priority: optional
Architecture: amd64
Maintainer: SacredHills <noreply@sacredhills.invalid>
Depends: libasound2, libfreetype6, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1
Description: SKJALD mastering EQ and maximizer (VST3)
 Eight-band equalizer and a true-peak maximizer with five limiting modes.
 Installs SKJALD.vst3 into /usr/lib/vst3 and the skjald_offline command line tool.
CTL
dpkg-deb --build --root-owner-group "$ROOT" "$OUT/SKJALD-$VERSION-Linux-x86_64.deb"
# generic tarball
TAR="$WORK/SKJALD-$VERSION-Linux-x86_64"; mkdir -p "$TAR"
cp -R "$PKG_DIR/SKJALD.vst3" "$PKG_DIR/skjald_offline" "$PKG_DIR/INSTALL.md" "$TAR/"
cat > "$TAR/install.sh" <<'SH'
#!/bin/bash
# Installs for the current user (no root): ~/.vst3 and ~/.local/bin. Use sudo ./install.sh --system for all users.
set -e
cd "$(dirname "$0")"
if [ "${1:-}" = "--system" ]; then V=/usr/lib/vst3; B=/usr/local/bin; else V="$HOME/.vst3"; B="$HOME/.local/bin"; fi
mkdir -p "$V" "$B"; rm -rf "$V/SKJALD.vst3"; cp -R SKJALD.vst3 "$V/"; install -m 755 skjald_offline "$B/"
echo "Installed SKJALD.vst3 to $V and skjald_offline to $B"
SH
chmod 755 "$TAR/install.sh"
tar -C "$WORK" -czf "$OUT/SKJALD-$VERSION-Linux-x86_64.tar.gz" "SKJALD-$VERSION-Linux-x86_64"
rm -rf "$WORK"
