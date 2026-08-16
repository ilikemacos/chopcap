# Types

Chopcap has six kinds of value. `type(x)` tells you which one you have.

| Name | Example | `type()` gives |
| --- | --- | --- |
| number | `42`, `-7`, `1_000_000` | `"number"` |
| decimal | `3.14`, `1.5e3` | `"decimal"` |
| text | `"hello"` | `"text"` |
| boolean | `true`, `false` | `"boolean"` |
| list | `[1, 2, 3]` | `"list"` |
| nothing | `nothing` | `"nothing"` |

Functions and modules are values too; both report as `"function"` and
`"module"`.

## Numbers and decimals

Whole numbers are exact 64-bit integers. Decimals are IEEE-754 doubles.
Underscores make long numbers readable: `1_000_000`.

Arithmetic keeps whole numbers whole where it can:

```chopcap
say 2 + 3        # 5
say 10 / 5       # 2      — divides exactly, so you get a whole number
say 10 / 4       # 2.5    — does not divide exactly
say 2 ^ 10       # 1024
say 2 ^ 0.5      # 1.414213562
say 7 % 3        # 1
say 1.5 + 1.5    # 3
```

Dividing by zero is an error rather than a silent `infinity`.

## Text

Text is written with double or single quotes and understands the escapes
`\n`, `\t`, `\r`, `\\`, `\"` and `\'`.

```chopcap
greeting = "Hello"
say greeting + ", world!"
say "count: " + 7          # count: 7   — the number becomes text
say len(greeting)          # 5
say greeting[0]            # H
say greeting[-1]           # o          — counting back from the end
```

Text is compared and ordered by its characters:

```chopcap
say "apple" < "banana"     # true
say "a" == "A"             # false
```

Positions count bytes, so text outside plain ASCII should be handled with
whole strings rather than by position. See [Limitations](specification.md#limitations).

## Booleans and truth

`true` and `false` are the booleans. Anywhere a condition is expected,
these count as false:

```text
false     nothing     0     ""     []
```

Everything else counts as true.

```chopcap
if "":
    say "never"
else:
    say "empty text is false"
```

## nothing

`nothing` is the absence of a value. Functions that do not `return` anything
give back `nothing`, and `ask` gives `nothing` when input has run out.

## Converting between types

```chopcap
say text(42)          # "42"
say text([1, 2])      # [1, 2]
say number("3.5")     # 3.5
say number("42")      # 42
say whole(3.9)        # 3     — cuts towards zero
say whole(-3.9)       # -3
say number("abc")     # error: cannot turn "abc" into a number
```

`number` refuses text that is not a number, so you can catch the mistake:

```chopcap
try:
    age = number(ask "Age? ")
catch problem:
    say "That was not a number."
```
