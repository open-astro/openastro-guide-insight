#!/usr/bin/env bash
#
# Make the Linux AppImage from an installed tree (DESTDIR with usr/ in it),
# bundling the GTK libraries with linuxdeploy so it runs on most
# distributions. Build on an older distribution (CI uses Ubuntu 22.04) so
# the AppImage's glibc requirement stays low.
#
# usage: scripts/make-appimage.sh STAGE ARCH VERSION OUTPUT.AppImage
#   ARCH is x86_64 or aarch64
#
set -euo pipefail

stage=$(realpath "${1:?usage: $0 STAGE ARCH VERSION OUTPUT}")
arch=${2:?usage: $0 STAGE ARCH VERSION OUTPUT}
version=${3:?usage: $0 STAGE ARCH VERSION OUTPUT}
out=$(realpath -m "${4:?usage: $0 STAGE ARCH VERSION OUTPUT}")
root=$(cd "$(dirname "$0")/.." && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"

base=https://github.com/linuxdeploy
curl -fsSL -o linuxdeploy "$base/linuxdeploy/releases/download/continuous/linuxdeploy-$arch.AppImage"
curl -fsSL -o linuxdeploy-plugin-gtk.sh "$base/linuxdeploy-plugin-gtk/raw/master/linuxdeploy-plugin-gtk.sh"
chmod +x linuxdeploy linuxdeploy-plugin-gtk.sh

mkdir -p AppDir
cp -a "$stage/usr" AppDir/

# CI machines have no FUSE, so AppImages there unpack themselves to run
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=$arch
export DEPLOY_GTK_VERSION=3
export LINUXDEPLOY_OUTPUT_VERSION=$version
./linuxdeploy --appdir AppDir \
  --executable AppDir/usr/bin/openastro-guide-insight \
  --desktop-file AppDir/usr/share/applications/openastro-guide-insight.desktop \
  --icon-file "$root/openastro-guide-insight.png" \
  --plugin gtk --output appimage

mv ./*.AppImage "$out"
