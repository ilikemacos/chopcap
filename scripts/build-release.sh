#!/usr/bin/env bash
# Builds the macOS release archives into dist/.
#
#   ./scripts/build-release.sh 0.1.0
#
# Produces, for arm64, x86_64 and universal:
#   dist/chopcap-<version>-macos-<arch>.tar.gz
#   dist/SHASUMS256.txt

set -euo pipefail

VERSION="${1:-}"
if [ -z "$VERSION" ]; then
    echo "usage: $0 <version>   e.g. $0 0.1.0" >&2
    exit 64
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

SOURCE_VERSION="$(sed -n 's/.*CHOPCAP_VERSION "\(.*\)".*/\1/p' src/chopcap.h)"
if [ "$SOURCE_VERSION" != "$VERSION" ]; then
    echo "error: src/chopcap.h says $SOURCE_VERSION but you asked for $VERSION" >&2
    exit 1
fi

echo "==> Building Chopcap $VERSION"
make clean >/dev/null
make >/dev/null
make universal >/dev/null

rm -rf dist
mkdir -p dist

pack() { # arch, path to binary
    local arch="$1" bin="$2"
    local name="chopcap-$VERSION-macos-$arch"
    local stage="dist/.stage/$name"
    mkdir -p "$stage"
    cp "$bin" "$stage/chopcap"
    cp README.md LICENSE CHANGELOG.md "$stage/"
    cp -R examples "$stage/examples"
    chmod +x "$stage/chopcap"
    tar -czf "dist/$name.tar.gz" -C "$stage" .
    echo "    dist/$name.tar.gz  ($(lipo -archs "$bin" 2>/dev/null || echo "$arch"))"
}

echo "==> Packing"
pack arm64     build/arm64/chopcap
pack x86_64    build/x86_64/chopcap
pack universal build/universal/chopcap
rm -rf dist/.stage

echo "==> Checksums"
( cd dist && shasum -a 256 ./*.tar.gz | sed 's| \./| |' > SHASUMS256.txt )
cat dist/SHASUMS256.txt

echo
echo "==> Done. Archives are in dist/"
