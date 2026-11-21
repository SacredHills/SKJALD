#!/bin/bash
# Builds an unsigned macOS installer package.
# usage: build_pkg.sh <package dir containing SKJALD.vst3 + skjald_offline> <version> <output dir>
set -euo pipefail
PKG_DIR=$(cd "$1" && pwd); VERSION=$2; OUT=$(mkdir -p "$3" && cd "$3" && pwd)
WORK=$(mktemp -d)
mkdir -p "$WORK/vst3/Library/Audio/Plug-Ins/VST3" "$WORK/tools/usr/local/bin"
cp -R "$PKG_DIR/SKJALD.vst3" "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp "$PKG_DIR/skjald_offline" "$WORK/tools/usr/local/bin/skjald_offline"
chmod 755 "$WORK/tools/usr/local/bin/skjald_offline"
# ad-hoc sign so Apple Silicon accepts the binaries (not notarised)
codesign --force --deep --sign - "$WORK/vst3/Library/Audio/Plug-Ins/VST3/SKJALD.vst3" || true
codesign --force --sign - "$WORK/tools/usr/local/bin/skjald_offline" || true
pkgbuild --root "$WORK/vst3" --identifier com.sacredhills.skjald.vst3 --version "$VERSION" --install-location / "$WORK/vst3.pkg"
pkgbuild --root "$WORK/tools" --identifier com.sacredhills.skjald.tools --version "$VERSION" --install-location / "$WORK/tools.pkg"
cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>SKJALD $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline><line choice="vst3"/><line choice="tools"/></choices-outline>
  <choice id="vst3" title="SKJALD VST3 plug-in" description="Installs SKJALD.vst3 into /Library/Audio/Plug-Ins/VST3" start_selected="true">
    <pkg-ref id="com.sacredhills.skjald.vst3"/>
  </choice>
  <choice id="tools" title="Offline processor (command line)" description="Installs skjald_offline into /usr/local/bin" start_selected="true">
    <pkg-ref id="com.sacredhills.skjald.tools"/>
  </choice>
  <pkg-ref id="com.sacredhills.skjald.vst3" version="$VERSION">vst3.pkg</pkg-ref>
  <pkg-ref id="com.sacredhills.skjald.tools" version="$VERSION">tools.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT/SKJALD-$VERSION-macOS-universal.pkg"
rm -rf "$WORK"
