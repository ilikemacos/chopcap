# Variables

A variable is a name with a value. Use `=` to give it one.

```chopcap
name = "Ada"
count = 3
count = count + 1
say name, count          # Ada 4
```

There is no `var`, `let` or type annotation. A name comes into existence the
first time you assign to it.

## Names

Names start with a letter or `_`, then letters, digits or `_`.
They are case sensitive: `count` and `Count` are different variables.

```chopcap
first_name = "Ada"
total2 = 10
_hidden = true
```

## Using a name before it exists

Chopcap stops and tells you, with a guess if it can make one:

```chopcap
username = "theo"
say name
```

```text
Chopcap Error

  hello.chop, line 2:
    say name

  `name` does not exist.
  Did you mean `username`?
```

## Scope

Function parameters and anything first assigned inside a function belong to
that function. Assigning to a name that already exists further out updates
that outer variable.

```chopcap
total = 0

fun bump(by):
    total = total + by      # updates the outer `total`
    return total

say bump(2)                 # 2
say bump(3)                 # 5
say total                   # 5

fun shadow(total):          # a parameter is always local
    total = 100
    return total

say shadow(1), total        # 100 5
```

Functions defined inside functions can see the variables around them:

```chopcap
fun make_counter():
    count = 0
    fun step():
        count = count + 1
        return count
    return step

next = make_counter()
say next(), next(), next()  # 1 2 3
```
