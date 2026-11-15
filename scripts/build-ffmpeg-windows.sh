#!/usr/bin/env bash
set -euo pipefail
out="$(realpath -m "$1")"
mkdir -p "$out/bin" "$out/sources"
cp "${BASH_SOURCE[0]}" "$out/sources/"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
cd "$work"
version=9.0.2
revision=31e19f92f00c7003fa115047ce50978bc98c3a0d
curl -fsSL --retry 3 "https://ffmpeg.org/releases/ffmpeg-$version.tar.xz" -o ffmpeg.tar.xz
curl -fsSL --retry 3 "https://codeload.github.com/mirror/x264/tar.gz/$revision" -o x264.tar.gz
sha256sum -c <<'SUMS'
8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e  ffmpeg.tar.xz
d053c9d86988d6bc78237ca5205865c5ddf99c98ef4cd9927eec8f6d388f6dd9  x264.tar.gz
SUMS
cp ffmpeg.tar.xz "$out/sources/"
cp x264.tar.gz "$out/sources/"
tar -xf x264.tar.gz
cd "x264-$revision"
./configure --host=x86_64-w64-mingw32 --cross-prefix=x86_64-w64-mingw32- --prefix="$work/prefix" --enable-static --disable-cli --disable-opencl
make -j"$(nproc)"
make install
cp COPYING "$out/sources/x264-COPYING.txt"
cd "$work"
tar -xf ffmpeg.tar.xz
cd "ffmpeg-$version"
PKG_CONFIG_LIBDIR="$work/prefix/lib/pkgconfig" ./configure --arch=x86_64 --target-os=mingw32 --cross-prefix=x86_64-w64-mingw32- --enable-cross-compile --disable-autodetect --disable-shared --enable-static --enable-gpl --enable-libx264 --pkg-config-flags=--static --extra-ldflags=-static --disable-ffplay --disable-doc --disable-debug
make -j"$(nproc)" ffmpeg.exe ffprobe.exe
x86_64-w64-mingw32-strip ffmpeg.exe ffprobe.exe
cp ffmpeg.exe ffprobe.exe "$out/bin/"
cp COPYING.GPLv2 LICENSE.md "$out/sources/"
