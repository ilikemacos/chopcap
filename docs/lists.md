# Lists

A list holds values in order. It can hold any mix of them.

```chopcap
things = [1, 2, 3]
mixed = [1, "two", true, nothing, [5]]
empty = []
```

## Reading and changing

Positions start at `0`. Negative positions count back from the end.

```chopcap
say things[0]        # 1
say things[-1]       # 3
things[1] = 20
say things           # [1, 20, 3]
say len(things)      # 3
```

Reaching past the end is an error with a clear message:

```text
position 7 is outside this list of length 3.
Valid positions run from 0 to the length minus one.
```

## Growing and shrinking

`push` adds to the end, `pop` takes the last item off and gives it to you.

```chopcap
push(things, 4)      # [1, 20, 3, 4]
last = pop(things)   # last is 4
```

## Joining and searching

```chopcap
say [1, 2] + [3]         # [1, 2, 3]
say 2 in [1, 2, 3]       # true
```

## Going through a list

```chopcap
for thing in things:
    say thing
```

## The `lists` module

Everything else lives in the `lists` module:

```chopcap
use lists

nums = [3, 1, 2]
say lists.sort(nums)             # [1, 2, 3]   (a new list; nums is unchanged)
say lists.reverse(nums)          # [2, 1, 3]
say lists.sum(nums)              # 6
say lists.min(nums), lists.max(nums)
say lists.contains(nums, 2)      # true
say lists.find(nums, 2)          # 2   (or -1 if it is not there)
say lists.slice(nums, 1, 3)      # [1, 2]
say lists.join(["a", "b"], "-")  # a-b
```

`lists.sort`, `lists.reverse`, `lists.slice` and `lists.copy` make new lists.
`lists.add`, `lists.insert` and `lists.remove` change the list you give them.

See [Standard Library](standard-library.md#lists) for the full list.
