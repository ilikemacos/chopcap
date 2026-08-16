#!/bin/sh
# Chopcap installer.
#
#   curl -fsSL https://raw.githubusercontent.com/ilikemacos/chopcap/main/install.sh | sh
#
# What it does:
#   * checks that you are on macOS
#   * works out whether you have Apple Silicon or an Intel Mac
#   * downloads the matching chopcap binary from GitHub Releases
#   * checks the download against its published checksum
#   * copies it into a bin directory (nothing else is touched)
#   * runs `chopcap --version` to prove it worked
#
# Settings you can pass in:
#   CHOPCAP_VERSION=0.1.0        install a specific version
#   CHOPCAP_INSTALL_DIR=~/bin    choose where the binary goes

set -eu

REPO="ilikemacos/chopcap"
VERSION="${CHOPCAP_VERSION:-0.1.0}"

# ---------------------------------------------------------------- output ---
if [ -t 1 ]; then
    BOLD=$(printf '\033[1m'); DIM=$(printf '\033[2m')
    GREEN=$(printf '\033[32m'); RED=$(printf '\033[31m'); OFF=$(printf '\033[0m')
else
    BOLD=''; DIM=''; GREEN=''; RED=''; OFF=''
fi

step() { printf '%s==>%s %s\n' "$BOLD" "$OFF" "$1"; }
info() { printf '    %s%s%s\n' "$DIM" "$1" "$OFF"; }
die()  { printf '\n%serror:%s %s\n\n' "$RED" "$OFF" "$1" >&2; exit 1; }

printf '\n%sChopcap installer%s  (version %s)\n\n' "$BOLD" "$OFF" "$VERSION"

# ------------------------------------------------------------- platform ---
step "Checking your system"

OS="$(uname -s)"
if [ "$OS" != "Darwin" ]; then
    die "Chopcap 0.1.0 ships macOS binaries only, and this looks like $OS.
       You can still build it from source:
         git clone https://github.com/$REPO.git && cd chopcap && make && sudo make install"
fi

MACOS_VERSION="$(sw_vers -productVersion 2>/dev/null || echo unknown)"

case "$(uname -m)" in
    arm64)  ARCH="arm64";  CHIP="Apple Silicon" ;;
    x86_64) ARCH="x86_64"; CHIP="Intel" ;;
    *)      die "unknown processor type '$(uname -m)'." ;;
esac

info "macOS $MACOS_VERSION on $CHIP ($ARCH)"

command -v curl >/dev/null 2>&1 || die "curl is required but was not found."

# ------------------------------------------------------------ where to ---
step "Choosing where to install"

if [ -n "${CHOPCAP_INSTALL_DIR:-}" ]; then
    BIN_DIR="$CHOPCAP_INSTALL_DIR"
elif [ -w /usr/local/bin ] 2>/dev/null; then
    BIN_DIR="/usr/local/bin"
else
    BIN_DIR="$HOME/.local/bin"
fi

mkdir -p "$BIN_DIR" || die "cannot create $BIN_DIR"
[ -w "$BIN_DIR" ] || die "cannot write to $BIN_DIR — set CHOPCAP_INSTALL_DIR to somewhere you own."

info "$BIN_DIR"

EXISTING="$(command -v chopcap 2>/dev/null || true)"
if [ -n "$EXISTING" ] && [ "$EXISTING" != "$BIN_DIR/chopcap" ]; then
    info "note: another chopcap is already on your PATH at $EXISTING"
fi

# -------------------------------------------------------------- download ---
TARBALL="chopcap-$VERSION-macos-$ARCH.tar.gz"
BASE_URL="https://github.com/$REPO/releases/download/v$VERSION"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chopcap-install.XXXXXX")"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT INT TERM

step "Downloading Chopcap $VERSION"
info "$BASE_URL/$TARBALL"

if ! curl -fsSL "$BASE_URL/$TARBALL" -o "$TMP_DIR/$TARBALL"; then
    die "download failed.
       Check https://github.com/$REPO/releases for available versions."
fi

# ------------------------------------------------------------- checksum ---
step "Checking the download"
if curl -fsSL "$BASE_URL/SHASUMS256.txt" -o "$TMP_DIR/SHASUMS256.txt" 2>/dev/null; then
    EXPECTED="$(grep " $TARBALL\$" "$TMP_DIR/SHASUMS256.txt" | awk '{print $1}' || true)"
    ACTUAL="$(shasum -a 256 "$TMP_DIR/$TARBALL" | awk '{print $1}')"
    if [ -z "$EXPECTED" ]; then
        info "no checksum published for $TARBALL — skipping"
    elif [ "$EXPECTED" != "$ACTUAL" ]; then
        die "checksum mismatch — the download does not match what was published.
       expected $EXPECTED
       got      $ACTUAL"
    else
        info "sha256 matches"
    fi
else
    info "checksum file unavailable — skipping"
fi

# -------------------------------------------------------------- install ---
step "Installing"
tar -xzf "$TMP_DIR/$TARBALL" -C "$TMP_DIR" || die "could not unpack $TARBALL"
[ -f "$TMP_DIR/chopcap" ] || die "the archive did not contain a chopcap binary."

chmod +x "$TMP_DIR/chopcap"
xattr -d com.apple.quarantine "$TMP_DIR/chopcap" 2>/dev/null || true

# Replace only the chopcap binary itself; nothing else in the directory.
mv -f "$TMP_DIR/chopcap" "$BIN_DIR/chopcap" || die "could not write $BIN_DIR/chopcap"
info "$BIN_DIR/chopcap"

# ------------------------------------------------------------------ PATH ---
case ":$PATH:" in
    *":$BIN_DIR:"*) ON_PATH=1 ;;
    *)              ON_PATH=0 ;;
esac

if [ "$ON_PATH" -eq 0 ]; then
    step "Adding $BIN_DIR to your PATH"
    case "${SHELL:-/bin/zsh}" in
        */zsh)  PROFILE="$HOME/.zshrc" ;;
        */bash) PROFILE="$HOME/.bash_profile" ;;
        *)      PROFILE="$HOME/.profile" ;;
    esac
    LINE="export PATH=\"$BIN_DIR:\$PATH\""
    if [ -f "$PROFILE" ] && grep -qF "$LINE" "$PROFILE" 2>/dev/null; then
        info "already present in $PROFILE"
    else
        printf '\n# added by the Chopcap installer\n%s\n' "$LINE" >> "$PROFILE"
        info "appended to $PROFILE"
    fi
    info "run this once, or open a new terminal:  $LINE"
fi

# ------------------------------------------------------------------ done ---
step "Checking the installation"
if ! "$BIN_DIR/chopcap" --version >/dev/null 2>&1; then
    die "chopcap was installed but would not run. Please open an issue at
       https://github.com/$REPO/issues"
fi

printf '\n    %s\n' "$("$BIN_DIR/chopcap" --version)"
printf '\n%sChopcap is ready.%s\n\n' "$GREEN$BOLD" "$OFF"
printf '    chopcap                 start the interactive prompt\n'
printf '    chopcap hello.chop      run a program\n'
printf '    chopcap --help          see every command\n\n'
printf '  Try this:\n'
printf '    %secho '"'"'say "Hello from Chopcap!"'"'"' > hello.chop && chopcap hello.chop%s\n\n' "$DIM" "$OFF"
