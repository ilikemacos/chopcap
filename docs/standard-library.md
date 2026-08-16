# Standard Library

Chopcap 0.1.0 has seven built-in functions and six modules. That is the whole
library — small enough to keep in your head.

## Built-in functions

Always available, no `use` needed.

| Function | What it does |
| --- | --- |
| `len(x)` | how long some text or a list is |
| `text(x)` | turn any value into text |
| `number(x)` | turn text or a boolean into a number |
| `whole(x)` | turn a value into a whole number, cutting towards zero |
| `type(x)` | the name of a value's kind, as text |
| `push(list, value)` | add a value to the end of a list |
| `pop(list)` | take the last value off a list and give it back |

```chopcap
say len("chopcap"), len([1, 2])     # 7 2
say text(42) + "!"                  # 42!
say number("3.5") + 1               # 4.5
say whole(3.9)                      # 3
say type([1])                       # list
```

## math

```chopcap
use math
```

| Function | What it does |
| --- | --- |
| `math.pi`, `math.e` | the constants |
| `math.sqrt(n)` | square root |
| `math.abs(n)` | distance from zero |
| `math.floor(n)` | round down to a whole number |
| `math.ceil(n)` | round up to a whole number |
| `math.round(n)`, `math.round(n, places)` | round to the nearest |
| `math.pow(a, b)` | `a` to the power of `b` |
| `math.min(a, b, ...)`, `math.max(a, b, ...)` | smallest / largest |
| `math.sin(n)`, `math.cos(n)`, `math.tan(n)` | trigonometry, in radians |
| `math.log(n)`, `math.log(n, base)` | natural log, or log in any base |

```chopcap
say math.round(math.pi, 4)      # 3.1416
say math.log(8, 2)              # 3
```

## text

```chopcap
use text
```

| Function | What it does |
| --- | --- |
| `text.upper(s)` / `text.lower(s)` | change the case |
| `text.trim(s)` | remove spaces from both ends |
| `text.split(s)` | split on runs of whitespace |
| `text.split(s, sep)` | split on a separator |
| `text.join(list)` / `text.join(list, sep)` | glue a list into text |
| `text.replace(s, from, to)` | replace every occurrence |
| `text.contains(s, part)` | is it in there? |
| `text.starts(s, part)` / `text.ends(s, part)` | does it start / end with it? |
| `text.find(s, part)` | where it starts, or `-1` |
| `text.slice(s, from)` / `text.slice(s, from, to)` | a piece of it |
| `text.repeat(s, times)` | repeat it |
| `text.reverse(s)` | back to front |
| `text.chars(s)` | a list of single characters |
| `text.code(s)` | the character code of the first character |
| `text.char(n)` | the character with that code |
| `text.length(s)` | same as `len(s)` |

```chopcap
say text.split("a,b,c", ",")        # ["a", "b", "c"]
say text.join(["a", "b"], "-")      # a-b
say text.slice("chopcap", 0, 4)     # chop
```

`text.slice` clamps its range, so it never fails on a short string.

## lists

```chopcap
use lists
```

Makes a new list: `sort`, `reverse`, `slice`, `copy`.
Changes the list you pass in: `add`, `insert`, `remove`.

| Function | What it does |
| --- | --- |
| `lists.length(l)` | how many items |
| `lists.add(l, value)` | add to the end |
| `lists.insert(l, at, value)` | insert at a position |
| `lists.remove(l, at)` | remove a position and give the value back |
| `lists.contains(l, value)` | is it in there? |
| `lists.find(l, value)` | its position, or `-1` |
| `lists.copy(l)` | a separate copy |
| `lists.reverse(l)` | a new list, back to front |
| `lists.slice(l, from)` / `(l, from, to)` | a new list from a range |
| `lists.sort(l)` | a new sorted list |
| `lists.sum(l)` | add the numbers up |
| `lists.min(l)` / `lists.max(l)` | smallest / largest item |
| `lists.join(l)` / `lists.join(l, sep)` | glue into text |

`lists.sort` needs every item to be the same kind — all numbers or all text.

## random

```chopcap
use random
```

| Function | What it does |
| --- | --- |
| `random.number(low, high)` | a whole number, both ends included |
| `random.decimal()` | a decimal from 0 up to (not including) 1 |
| `random.pick(list)` | one item from a list |
| `random.shuffle(list)` | a new list in a random order |
| `random.seed(n)` | fix the sequence, so runs repeat |

```chopcap
use random
say random.number(1, 6)
say random.pick(["rock", "paper", "scissors"])
```

## files

```chopcap
use files
```

| Function | What it does |
| --- | --- |
| `files.read(path)` | the whole file as text |
| `files.lines(path)` | the file as a list of lines |
| `files.write(path, text)` | write, replacing anything there |
| `files.append(path, text)` | add to the end |
| `files.exists(path)` | is there a file there? |
| `files.remove(path)` | delete it |

Paths are used exactly as given, relative to wherever you ran `chopcap`.

## time

```chopcap
use time
```

| Function | What it does |
| --- | --- |
| `time.now()` | seconds since 1 January 1970 |
| `time.clock()` | a steadily rising number, for measuring how long something took |
| `time.sleep(seconds)` | wait |
| `time.date()` | today as `"2026-08-16"` |
| `time.stamp()` | now as `"2026-08-16 14:05:09"` |
| `time.year()`, `time.month()`, `time.day()` | parts of today |
| `time.hour()`, `time.minute()`, `time.second()` | parts of the time |

```chopcap
use time
start = time.clock()
time.sleep(0.25)
say "that took", time.clock() - start, "seconds"
```
