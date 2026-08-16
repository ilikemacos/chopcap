# Examples

Every program below lives in the [`examples/`](../examples) folder and is run
as part of the test suite.

Run one with:

```bash
chopcap examples/fizzbuzz.chop
```

| File | What it shows |
| --- | --- |
| [`hello.chop`](../examples/hello.chop) | the smallest program there is |
| [`greeting.chop`](../examples/greeting.chop) | `ask`, defaults, text length |
| [`fizzbuzz.chop`](../examples/fizzbuzz.chop) | functions, `%`, early returns |
| [`temperature.chop`](../examples/temperature.chop) | arithmetic and a small table |
| [`shopping.chop`](../examples/shopping.chop) | lists, `push`, the `lists` module |
| [`words.chop`](../examples/words.chop) | splitting text and counting things |
| [`primes.chop`](../examples/primes.chop) | nested loops and `math` |
| [`notes.chop`](../examples/notes.chop) | reading and writing files |
| [`guess.chop`](../examples/guess.chop) | a game: input, `random`, `try`/`catch` |

## Hello, world

```chopcap
say "Hello, world!"
```

## FizzBuzz

```chopcap
fun label(n):
    if n % 15 == 0:
        return "FizzBuzz"
    if n % 3 == 0:
        return "Fizz"
    if n % 5 == 0:
        return "Buzz"
    return text(n)

for i in 1 to 20:
    say label(i)
```

## Counting words

```chopcap
use text
use lists

passage = "the quick brown fox jumps over the lazy dog the end"
words = text.split(passage)

seen = []
counts = []
for w in words:
    at = lists.find(seen, w)
    if at == -1:
        push(seen, w)
        push(counts, 1)
    else:
        counts[at] = counts[at] + 1

for i in 0 to len(seen) - 1:
    if counts[i] > 1:
        say seen[i], "appears", counts[i], "times"
```

## A guessing game

```chopcap
use random

secret = random.number(1, 100)
tries = 0

say "I am thinking of a number between 1 and 100."

while tries < 7:
    reply = ask "Your guess? "
    if reply == nothing:
        break

    try:
        guess = whole(reply)
    catch:
        say "That was not a number. Try again."
        skip

    tries = tries + 1
    if guess == secret:
        say "Correct! It took you", tries, "tries."
        break
    else if guess < secret:
        say "Higher."
    else:
        say "Lower."
```
