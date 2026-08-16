# Language Basics

Everything about the shape of a Chopcap program, on one page.

## One statement per line

Lines are statements. There are no semicolons and no braces.

```chopcap
x = 1
y = 2
say x + y
```

## Comments start with `#`

```chopcap
# a whole line
x = 1  # or the end of a line
```

## Blocks are indented under a `:`

Anything that opens a block ends its line with `:`, and the body is indented.
Four spaces is the convention; any consistent amount works, as long as every
line in the same block lines up.

```chopcap
if ready:
    say "go"
    say "still inside the block"
say "outside again"
```

## Printing: `say`

`say` prints its values with a space between them and a newline at the end.

```chopcap
say "Hello"
say "x is", 42, "and that is that"
say                     # an empty line
```

## Input: `ask`

`ask` prints a prompt and gives back what was typed, as text.

```chopcap
name = ask "What is your name? "
age = number(ask "How old are you? ")
```

At the end of input, `ask` gives back `nothing`.

## Values

```chopcap
count   = 42            # number
price   = 9.99          # decimal
name    = "Chopcap"     # text
ready   = true          # boolean
missing = nothing       # nothing
things  = [1, 2, 3]     # list
```

## Operators

| Kind | Operators |
| --- | --- |
| Arithmetic | `+` `-` `*` `/` `%` `^` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logic | `and` `or` `not` |
| Membership | `in` |

`+` also joins text (`"a" + "b"`) and lists (`[1] + [2]`). If either side of
`+` is text, the other side is turned into text for you.

## The whole keyword list

```text
and    as     ask    break  catch  else   error  false  for    fun
if     in     not    nothing       or     return say    skip   true
try    use    while
```

Twenty-two words. `to` and `by` are special only inside a counting `for`
line, so you are still free to use them as names elsewhere.

That is the entire language. Everything else lives in
[the standard library](standard-library.md).
