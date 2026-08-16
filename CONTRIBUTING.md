# Contributing to Chopcap

Thanks for taking a look. Chopcap is deliberately small, and the best way to
keep it useful is to keep it small — so the bar for adding to the language is
high, and the bar for fixing and clarifying is low.

## Getting set up

You need a C compiler and `make`. There are no other dependencies.

```bash
git clone https://github.com/ilikemacos/chopcap.git
cd chopcap
make          # builds build/chopcap
make test     # runs everything
```

## The test suite

`make test` runs four things:

1. **Unit tests** (`tests/unit_tests.c`) — the lexer, parser, values and
   environments, at the C level.
2. **Program tests** (`tests/programs/`) — each `NAME.chop` is run with
   stdout and stderr merged and compared against `NAME.expected`. If the file
   `NAME.in` exists it is fed on standard input.
3. **CLI and REPL tests** — flags, exit codes, and the interactive prompt.
4. **Example programs** — every file in `examples/` must run successfully.

To add a program test, drop in `tests/programs/NN_thing.chop`, run
`make bless` to record the output, then **read the generated `.expected` file
and check it is what you meant**. Blessing without reading defeats the point.

## What a good change looks like

- **Bug fixes** are always welcome. A failing test in `tests/programs/` that
  demonstrates the bug is the ideal first commit.
- **Better error messages** are welcome. Chopcap's errors should name the
  thing that went wrong in ordinary words, and suggest what to do about it.
  No jargon, no stack traces.
- **Documentation fixes** are welcome. If the docs and the implementation
  disagree, that is a bug in one of them — say which you think it is.
- **New library functions** need a reason a beginner would recognise, plus
  tests and a row in `docs/standard-library.md`.
- **New syntax** is the hardest sell. Open an issue first and describe the
  program that is awkward to write without it.

## House style

- C11, no dependencies, no compiler warnings (`-Wall -Wextra` is on).
- Comments explain *why*, not *what*.
- Every user-facing message is a lowercase sentence ending in a full stop,
  with an optional hint on the following line. Read it out loud; if it sounds
  like a compiler, rewrite it.
- Keep the language's own vocabulary plain: `say`, `ask`, `fun`, `skip`.

## Before opening a pull request

```bash
make clean && make && make test
```

Everything must pass. Mention what you changed in `CHANGELOG.md` under an
`## [Unreleased]` heading.

## Reporting a bug

Include the smallest `.chop` program that shows the problem, what you
expected, what happened, and the output of `chopcap --version`.
