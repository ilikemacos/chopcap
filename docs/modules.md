# Modules

## Built-in modules

`use` loads one of the modules that ship with Chopcap:

```chopcap
use math
say math.sqrt(16)       # 4
```

The name after `use` becomes a variable holding the module. Reach inside it
with `.`.

The built-in modules are `math`, `text`, `lists`, `random`, `files` and
`time`. They are described in [Standard Library](standard-library.md).

Asking for one that does not exist tells you what does:

```text
there is no module called `sockets`.
Chopcap comes with: math, text, lists, random, files, time.
```

## Renaming a module

```chopcap
use random as dice
say dice.number(1, 6)
```

## Your own files

A `.chop` file can be loaded as a module. Its top-level variables and
functions become the module's contents.

`helpers.chop`:

```chopcap
use text

greeting = "hello from helpers"

fun double(n):
    return n * 2

fun shout(s):
    return text.upper(s) + "!"
```

`main.chop`:

```chopcap
use "helpers.chop" as helpers

say helpers.greeting        # hello from helpers
say helpers.double(21)      # 42
say helpers.shout("hey")    # HEY!
```

Notes:

- Loading a file needs a name, so `as` is required.
- The path is relative to the file doing the loading, not to where you ran
  `chopcap` from.
- The file runs once, from top to bottom, when it is loaded.
- Files that load each other in a circle are caught and reported.
