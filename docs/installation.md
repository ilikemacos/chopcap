# Installation

Chopcap 0.1.0 ships as a single macOS binary with no dependencies.

## The one-line install

```bash
curl -fsSL https://raw.githubusercontent.com/ilikemacos/chopcap/main/install.sh | sh
```

The installer:

1. checks you are on macOS,
2. detects Apple Silicon (`arm64`) or Intel (`x86_64`),
3. downloads the matching binary from GitHub Releases,
4. checks it against the published SHA-256 checksum,
5. copies it to `/usr/local/bin` if that is writable, otherwise `~/.local/bin`,
6. adds that directory to your `PATH` if it is missing,
7. runs `chopcap --version` to prove it worked.

It writes exactly one file: the `chopcap` binary. Nothing else is touched.

### Options

```bash
CHOPCAP_INSTALL_DIR=~/bin  curl -fsSL .../install.sh | sh   # choose the folder
CHOPCAP_VERSION=0.1.0      curl -fsSL .../install.sh | sh   # pin a version
```

## Manual install

Download a release archive from
[the releases page](https://github.com/ilikemacos/chopcap/releases), then:

```bash
tar -xzf chopcap-0.1.0-macos-arm64.tar.gz
chmod +x chopcap
mv chopcap /usr/local/bin/
chopcap --version
```

There is also a `universal` archive that runs natively on both chip families.

## Building from source

You need a C compiler and `make`. That is all — Chopcap has no third-party
dependencies.

```bash
git clone https://github.com/ilikemacos/chopcap.git
cd chopcap
make            # builds build/chopcap
make test       # runs the full test suite
sudo make install
```

To build a universal macOS binary:

```bash
make universal  # build/universal/chopcap
```

`make install` honours `PREFIX` (default `/usr/local`) and `DESTDIR`.

## Uninstalling

```bash
rm "$(command -v chopcap)"
```

or `sudo make uninstall` from a source checkout.
