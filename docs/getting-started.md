# Getting Started

## Say hello

Put this in a file called `hello.chop`:

```chopcap
say "Hello, world!"
```

Run it:

```bash
chopcap hello.chop
```

```text
Hello, world!
```

That is a complete Chopcap program. No imports, no `main`, no semicolons.

## Try things without a file

Run `chopcap` with no arguments to get the interactive prompt:

```text
$ chopcap
Chopcap 0.1.0
Type some Chopcap, or `exit` to quit. `help` shows a reminder.

>>> say "Hello!"
Hello!
>>> 2 + 2
4
>>> name = "Ada"
>>> "Hi, " + name
"Hi, Ada"
>>> exit
```

A line ending in `:` starts a block. Keep typing indented lines, then press
Enter on a blank line to run it:

```text
>>> fun square(n):
...     return n * n
...
>>> square(9)
81
```

## A slightly bigger program

```chopcap
name = ask "What is your name? "
say "Hello, " + name + "!"

fun add(a, b):
    return a + b

say "5 + 7 is", add(5, 7)

for i in 1 to 3:
    say "line", i
```

```text
What is your name? Ada
Hello, Ada!
5 + 7 is 12
line 1
line 2
line 3
```

## What to read next

- [Language Basics](language-basics.md) for the rules in one page
- [Examples](examples.md) for small complete programs
