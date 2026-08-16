<div align="center">

<img src="website/assets/logo.svg" alt="Chopcap" width="88" height="88">

# Chopcap

**Programming without the clutter.**

A very small language for people who want to write a program, not a
ceremony. One binary, no dependencies, friendly errors, and a manual you can
read in an afternoon.

[Install](#install) · [Documentation](docs/) · [Examples](examples/) · [Specification](docs/specification.md)

![version](https://img.shields.io/badge/version-0.1.0-1f6feb) ![license](https://img.shields.io/badge/license-MIT-informational) ![platform](https://img.shields.io/badge/macOS-arm64%20%7C%20x86__64-lightgrey)

</div>

---

```chopcap
say "Hello, world!"

name = ask "What is your name? "
say "Hello, " + name + "!"

fun add(a, b):
    return a + b

say "5 + 7 is", add(5, 7)

for i in 1 to 3:
    say "line", i
```

```bash
$ chopcap hello.chop
Hello, world!
What is your name? Ada
Hello, Ada!
5 + 7 is 12
line 1
line 2
line 3
```

> **Chopcap 0.1.0 is an early release.** The language works, it is covered by
> an automated test suite, and the documentation matches the implementation.
> It is not a mature production language, and the syntax may still change
> before 1.0.

## Install

macOS, Apple Silicon or Intel:

```bash
curl -fsSL https://raw.githubusercontent.com/ilikemacos/chopcap/main/install.sh | sh
```

The installer detects your chip, downloads the matching binary from GitHub
Releases, verifies its SHA-256 checksum, and installs a single file into
`/usr/local/bin` (or `~/.local/bin` if that is not writable). Nothing else is
touched. See [Installation](docs/installation.md) for manual downloads,
choosing the directory, and building from source.

Prefer to build it yourself? You need nothing but a C compiler:

```bash
git clone https://github.com/ilikemacos/chopcap.git
cd chopcap && make && make test && sudo make install
```

## Use it

```bash
chopcap                     # interactive prompt
chopcap hello.chop          # run a program
chopcap run hello.chop      # the same thing, spelled out
chopcap -e 'say 1 + 1'      # run a line directly
chopcap --version
chopcap --help
```

```text
$ chopcap
Chopcap 0.1.0
Type some Chopcap, or `exit` to quit. `help` shows a reminder.

>>> 6 * 7
42
>>> fun square(n):
...     return n * n
...
>>> square(9)
81
```

## Why Chopcap

**Errors that read like a person wrote them.** Never a stack trace:

```text
Chopcap Error

  hello.chop, line 4:
    say name

  `name` does not exist.
  Did you mean `username`?
```

**A language you can hold in your head.** Twenty-two keywords, seven
built-in functions, six library modules. That is all of it.

**It starts instantly.** A 110 KB binary with no runtime to load — fast
enough that a `.chop` script feels like a shell command.

**No setup.** No virtual environments, no package manager, no project file.
A `.chop` file and the `chopcap` command.

## The language in one screen

```chopcap
# Comments start with a hash.

count = 42                     # number
price = 9.99                   # decimal
name = "Chopcap"               # text
ready = true                   # boolean
things = [1, "two", [3]]       # list
missing = nothing              # nothing

say "Hello", name              # Hello Chopcap
say things[0], things[-1]      # 1 [3]
say len(name), "cap" in name   # 7 true

if count > 40 and ready:
    say "big enough"
else if count > 20:
    say "getting there"
else:
    say "small"

while count > 40:
    count = count - 1

for thing in things:
    say thing

for i in 1 to 5 by 2:
    say i                      # 1 3 5

fun greet(who):
    if who == "":
        return "Hello, stranger!"
    return "Hello, " + who + "!"

say greet(ask "Your name? ")

try:
    age = number("not a number")
catch problem:
    say "could not read that:", problem

use math
use text
say math.round(math.pi, 2)     # 3.14
say text.upper("chopcap")      # CHOPCAP
```

Full details in [the documentation](docs/):
[Getting Started](docs/getting-started.md) ·
[Language Basics](docs/language-basics.md) ·
[Types](docs/types.md) ·
[Functions](docs/functions.md) ·
[Standard Library](docs/standard-library.md) ·
[CLI Reference](docs/cli.md) ·
[Specification](docs/specification.md)

## Examples

The [`examples/`](examples/) folder holds nine complete programs, all of them
run by the test suite:

```bash
chopcap examples/fizzbuzz.chop
chopcap examples/primes.chop
chopcap examples/guess.chop      # a game, so run it in a terminal
```

## How it is built

Chopcap is about 3,500 lines of C11 with no third-party dependencies:

```text
src/lexer.c      source text  ->  tokens (including INDENT / DEDENT)
src/parser.c     tokens       ->  a small AST
src/interp.c     AST          ->  results, by walking the tree
src/builtins.c   the seven built-ins and six library modules
src/value.c      values, reference counting, environments
src/error.c      friendly error reports and "did you mean ...?"
src/main.c       the chopcap command and the interactive prompt
```

```bash
make            # build/chopcap
make test       # unit tests + golden-output tests + CLI tests + examples
make universal  # one binary for both Apple Silicon and Intel
make install    # into PREFIX/bin, default /usr/local
```

## Contributing

Bug reports and small, focused changes are welcome — see
[CONTRIBUTING.md](CONTRIBUTING.md). The whole test suite runs with `make test`
and must pass before anything is merged.

## License

[MIT](LICENSE) © 2026 The Chopcap Authors.
