# Conditions

```chopcap
if score > 90:
    say "excellent"
else if score > 70:
    say "good"
else:
    say "keep going"
```

`else if` can repeat as many times as you like. `else` is optional.

## Comparing

```chopcap
say 1 == 1        # true
say 1 != 2        # true
say 3 < 5         # true
say 3 <= 3        # true
say "a" < "b"     # true
```

Numbers compare with numbers, text compares with text. Comparing a number
with text is an error rather than a surprise:

```text
cannot compare number with text using `<`.
Compare numbers with numbers, or text with text.
```

`==` is the exception: it works on any two values and simply answers `false`
when they are different kinds. Whole numbers and decimals compare by value,
so `3 == 3.0` is `true`.

## Combining

```chopcap
if age >= 18 and has_ticket:
    say "come in"

if day == "saturday" or day == "sunday":
    say "weekend"

if not ready:
    say "not yet"
```

`and` and `or` stop as soon as the answer is known, and they give back the
value that decided it — so `name or "stranger"` is a neat default:

```chopcap
name = ask "Name? "
say "Hello, " + (name or "stranger")
```

## Checking membership

```chopcap
say 2 in [1, 2, 3]          # true
say "cap" in "chopcap"      # true
say "z" in "chopcap"        # false
```
