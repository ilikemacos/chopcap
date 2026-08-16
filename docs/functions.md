# Functions

```chopcap
fun add(a, b):
    return a + b

say add(5, 7)       # 12
```

`fun` names it, the parentheses list the parameters, and the indented block
is the body.

## Returning

`return` gives a value back and ends the function immediately. A function
that never returns gives back `nothing`.

```chopcap
fun greet(name):
    say "Hello, " + name + "!"

greet("Ada")            # Hello, Ada!
say greet("Bob")        # Hello, Bob!  then  nothing
```

`return` on its own is fine as an early exit:

```chopcap
fun describe(n):
    if n < 0:
        say "negative"
        return
    say "zero or more"
```

## The right number of values

Chopcap checks that a call matches the definition:

```chopcap
fun add(a, b):
    return a + b
say add(1)
```

```text
`add` needs 2 values but got 1.
```

## Functions calling themselves

```chopcap
fun fact(n):
    if n <= 1:
        return 1
    return n * fact(n - 1)

say fact(6)     # 720
```

A function that never stops calling itself is caught rather than crashing:

```text
`forever` went too deep.
A function is probably calling itself without ever stopping.
```

## Functions are values

You can pass a function to another function, return one, and store one in a
list.

```chopcap
fun double(n):
    return n * 2

fun apply(f, v):
    return f(v)

say apply(double, 21)       # 42
```

Functions defined inside other functions remember the variables around them —
see [Variables](variables.md#scope).
