# CLI Reference

```text
chopcap                     start the interactive prompt
chopcap <file.chop>         run a Chopcap program
chopcap run <file.chop>     the same thing, spelled out
chopcap -e "say 1 + 1"      run a line of Chopcap directly
chopcap repl                start the interactive prompt

  -h, --help                show help
  -v, --version             show the version
```

## Running a program

```bash
chopcap hello.chop
chopcap run hello.chop      # identical
chopcap ./examples/primes.chop
```

Files conventionally end in `.chop`, but any file works.

## Running a snippet

```bash
chopcap -e 'say "Hello from Chopcap!"'
chopcap -e 'for i in 1 to 3:
    say i'
```

## The interactive prompt

```text
$ chopcap
Chopcap 0.1.0
Type some Chopcap, or `exit` to quit. `help` shows a reminder.

>>> say "hi"
hi
>>> 6 * 7
42
```

- An expression on its own prints its value; text is shown with quotes so you
  can tell `42` from `"42"`.
- A line ending in `:` starts a block. Keep typing, and press Enter on a
  blank line to run it.
- Variables and functions stay defined for the whole session.
- `help` prints a reminder, `exit` (or `quit`, or Ctrl-D) leaves.
- Errors are reported and the session carries on.

## Reading from a pipe

The prompt also works without a terminal, which is handy in scripts:

```bash
echo 'say 2 + 2' | chopcap
printf 'x = 5\nx * 2\n' | chopcap
```

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | the program ran to the end |
| `64` | the command line was wrong |
| `65` | the program could not be read as Chopcap |
| `66` | the file could not be opened |
| `70` | the program stopped with an error while running |
