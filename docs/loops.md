# Loops

## `while` — repeat while something is true

```chopcap
count = 0
while count < 3:
    say count
    count = count + 1
```

## `for ... in` — go through a list or some text

```chopcap
for name in ["ada", "grace", "alan"]:
    say "hello " + name

for letter in "cap":
    say letter
```

## `for ... in a to b` — count

```chopcap
for i in 1 to 5:
    say i               # 1 2 3 4 5
```

Both ends are included. Counting always goes **up**: if the start is past the
end, the loop simply does not run.

```chopcap
for i in 5 to 1:
    say "never runs"
```

To count down, or in bigger steps, add `by`:

```chopcap
for i in 5 to 1 by -1:
    say i               # 5 4 3 2 1

for i in 0 to 10 by 5:
    say i               # 0 5 10
```

## `break` and `skip`

`break` leaves the loop. `skip` jumps to the next turn.

```chopcap
for i in 1 to 10:
    if i == 4:
        break
    if i % 2 == 0:
        skip
    say i               # 1 3
```

## Looping over positions

`len` gives the length, and positions start at `0`:

```chopcap
basket = ["bread", "milk"]
for i in 0 to len(basket) - 1:
    say i, basket[i]
```
