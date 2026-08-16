# Changelog

All notable changes to Chopcap are recorded here.
This project follows [Semantic Versioning](https://semver.org/). While the
major version is `0`, the syntax may still change between minor releases.

## [0.1.0] — 2026-08-16

The first release of Chopcap. Everything below is new.

### Language

- Values: `number` (64-bit integer), `decimal`, `text`, `boolean`, `list`,
  `nothing`, plus functions and modules.
- Variables with plain `=` assignment; no declarations.
- Arithmetic `+ - * / % ^`, with `/` staying whole when it divides exactly
  and refusing to divide by zero.
- Comparison `== != < <= > >=`, membership `in`, logic `and` `or` `not` with
  short-circuit evaluation.
- Text with `"` or `'`, the escapes `\n \t \r \\ \" \'`, joining with `+`,
  indexing including negative positions.
- Lists with literals, indexing, indexed assignment, `+` to join, `push` and
  `pop`.
- `if` / `else if` / `else`.
- `while`, `for x in list-or-text`, `for i in a to b` with an optional
  `by step`, plus `break` and `skip`.
- Functions with `fun`, parameters, `return`, recursion, nested definitions
  that capture their surrounding scope, and functions as values.
- `try` / `catch` with an optional name, and `error` to raise your own.
- `use` for the built-in modules, `use "file.chop" as name` for your own.
- `say` for output and `ask` for input.
- `#` comments and indentation-based blocks; `to` and `by` are contextual, so
  they remain usable as ordinary names.

### Standard library

- Built-ins: `len`, `text`, `number`, `whole`, `type`, `push`, `pop`.
- Modules: `math`, `text`, `lists`, `random`, `files`, `time`.

### Tooling

- The `chopcap` command: run a file, `run` a file, `-e` a snippet,
  `--version`, `--help`, and an interactive prompt with multi-line blocks and
  value echoing.
- Friendly errors throughout: file and line, the offending source line, a
  plain-English message, a hint, and "did you mean ...?" suggestions for
  misspelled names. No stack traces are ever shown.
- Exit codes `0`, `64`, `65`, `66` and `70`.

### Project

- Reference implementation in ~3,500 lines of dependency-free C11.
- `make`, `make test`, `make universal`, `make install`.
- Test suite: 71 unit checks over the lexer, parser, values and environments;
  31 golden-output program tests; CLI and REPL tests; and every example
  program.
- `install.sh` for macOS on Apple Silicon and Intel, with checksum
  verification.
- GitHub Actions for continuous integration and for building release
  archives.
- Documentation covering installation, the language, the standard library,
  the CLI and the full specification.
- A website with the manual, runnable examples and a playground.

### Known limitations

- Text is handled as bytes, so `len` and indexing count bytes rather than
  characters outside ASCII.
- No maps or objects; lists are the only container.
- Reference cycles are not collected.
- Binaries are published for macOS only.

[0.1.0]: https://github.com/ilikemacos/chopcap/releases/tag/v0.1.0
