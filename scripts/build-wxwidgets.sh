#!/usr/bin/env bash
#
# Build and install the pinned wxWidgets release from source.
#
# Used on Linux, where distributions package older 3.2.x releases. The
# version and the source archive's SHA-512 are read from the vcpkg overlay
# port, so Windows, macOS and Linux always use the same wxWidgets.
#
# usage: scripts/build-wxwidgets.sh PREFIX
#
set -euo pipefail

prefix=${1:?usage: $0 PREFIX}
root=$(cd "$(dirname "$0")/.." && pwd)
port="$root/vcpkg-ports/wxwidgets"

version=$(sed -n 's/^ *"version": *"\([^"]*\)".*/\1/p' "$port/vcpkg.json" | head -1)
sha512=$(sed -n 's/^ *SHA512 \([0-9a-f]\{128\}\).*/\1/p' "$port/portfile.cmake" | head -1)
[[ -n $version && -n $sha512 ]] || { echo "could not read version/hash from $port" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

echo "Building wxWidgets $version into $prefix"
curl -fsSL -o "$work/wx.tar.gz" "https://github.com/wxWidgets/wxWidgets/archive/v$version.tar.gz"
echo "$sha512  $work/wx.tar.gz" | sha512sum -c -

mkdir "$work/src"
tar -xzf "$work/wx.tar.gz" -C "$work/src" --strip-components=1

# Static libraries with bundled third-party code, and only what phdlogview
# needs, so the result has few runtime dependencies beyond GTK.
cmake -S "$work/src" -B "$work/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DwxBUILD_SHARED=OFF \
  -DwxBUILD_TOOLKIT=gtk3 \
  -DwxBUILD_SAMPLES=OFF -DwxBUILD_TESTS=OFF -DwxBUILD_DEMOS=OFF \
  -DwxUSE_REGEX=builtin -DwxUSE_ZLIB=builtin -DwxUSE_EXPAT=builtin \
  -DwxUSE_LIBPNG=builtin -DwxUSE_LIBJPEG=builtin -DwxUSE_LIBTIFF=builtin \
  -DwxUSE_NANOSVG=builtin \
  -DwxUSE_OPENGL=OFF -DwxUSE_WEBVIEW=OFF -DwxUSE_MEDIACTRL=OFF \
  -DwxUSE_LIBSDL=OFF -DwxUSE_SECRETSTORE=OFF -DwxUSE_STC=OFF
cmake --build "$work/build"
cmake --install "$work/build"

echo "$version" > "$prefix/wxwidgets-version.txt"
