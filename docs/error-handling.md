# Error Handling

## What an error looks like

Chopcap never prints an internal stack trace. Every error names the file and
line, shows the line, says what went wrong, and adds a hint when it can.

```text
Chopcap Error

  hello.chop, line 4:
    say name

  `name` does not exist.
  Did you mean `username`?
```

A program that fails ends with a non-zero exit code (`65` for a problem with
the source text, `70` for a problem while running).

## Catching problems: `try` / `catch`

```chopcap
try:
    age = number(ask "How old are you? ")
    say "Next year:", age + 1
catch problem:
    say "That was not a number."
    say "The details were:", problem
```

The name after `catch` receives the error message as text. It is optional:

```chopcap
try:
    risky()
catch:
    say "that did not work"
```

Only the `try` block is guarded. Once `catch` runs, the program carries on
normally.

## Raising your own: `error`

```chopcap
fun withdraw(balance, amount):
    if amount > balance:
        error "not enough money"
    return balance - amount

try:
    say withdraw(50, 100)
catch problem:
    say "Sorry:", problem       # Sorry: not enough money
```

## Things Chopcap catches for you

| Situation | Message |
| --- | --- |
| Unknown name | ``` `name` does not exist. ``` |
| Wrong argument count | ``` `add` needs 2 values but got 1. ``` |
| Dividing by zero | `cannot divide by zero.` |
| Position past the end | `position 7 is outside this list of length 3.` |
| Adding mismatched values | `cannot add number and list.` |
| Calling something that is not a function | `number is not a function.` |
| Endless recursion | ``` `forever` went too deep. ``` |
| Bad indentation | `this line's indentation does not match any open block.` |
| Missing quote | `this text value is missing its closing quote.` |
