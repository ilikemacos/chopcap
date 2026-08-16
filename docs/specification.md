# Chopcap Language Specification

Version 0.1.0. This describes the language as the reference implementation in
[`src/`](../src) actually behaves; where the two disagree, the implementation
is the bug.

## 1. Source text

A program is UTF-8 text. Only ASCII is given meaning by the language itself;
other bytes pass through inside text values untouched.

Line endings may be `\n` or `\r\n`. A `#` begins a comment that runs to the
end of the line. `#` inside a text value is an ordinary character.

## 2. Lines and blocks

A logical line ends at a newline, unless a bracket (`(` or `[`) is still open,
in which case the line continues.

Blocks are written by indentation. A statement that opens a block ends with
`:`; the statements underneath it are indented further. Every line in a block
must start at the same column. A tab advances to the next multiple of four
columns. Blank lines and comment-only lines have no effect on indentation.

Formally, the lexer emits `INDENT` when a line's indentation is greater than
the enclosing block's, and one `DEDENT` for each block it closes when it is
smaller. Landing between two open levels is an error.

## 3. Keywords

```text
and    as     ask    break  catch  else   error  false  for    fun
if     in     not    nothing       or     return say    skip   true
try    use    while
```

Keywords may not be used as names.

`to` and `by` are contextual: they are recognised only between the parts of a
counting `for` header, and are ordinary names anywhere else.

## 4. Names

```text
name := (letter | "_") (letter | digit | "_")*
```

Names are case sensitive.

## 5. Literals

| Kind | Syntax | Notes |
| --- | --- | --- |
| number | `42`, `-7`, `1_000_000` | 64-bit signed integer; `_` is ignored |
| decimal | `3.14`, `1.5e3`, `2e-2` | IEEE-754 double |
| text | `"hi"`, `'hi'` | escapes `\n \t \r \\ \" \'`; must close on the same line |
| boolean | `true`, `false` | |
| nothing | `nothing` | |
| list | `[]`, `[1, 2, 3]` | may span lines |

An unknown escape is an error, so typos are caught rather than silently kept.

## 6. Types

Six value types: `number`, `decimal`, `text`, `boolean`, `list`, `nothing`.
Functions and modules are also values, reported by `type()` as `"function"`
and `"module"`.

Lists are references: assigning one to a second name gives two names for the
same list. Everything else behaves as a value.

## 7. Expressions

### 7.1 Precedence

Loosest first; every level is left-associative except `^`.

| Level | Operators |
| --- | --- |
| 1 | `or` |
| 2 | `and` |
| 3 | `==` `!=` `<` `<=` `>` `>=` `in` |
| 4 | `+` `-` |
| 5 | `*` `/` `%` |
| 6 | unary `-`, `not` |
| 7 | `^` (right-associative, binds tighter than unary minus) |
| 8 | call `f(...)`, index `x[...]`, member `m.name` |

So `1 + 2 * 3` is `7`, `-2 ^ 2` is `-4`, and `2 ^ 3 ^ 2` is `512`.

### 7.2 Arithmetic

`+ - *` on two numbers give a number; if either side is a decimal the result
is a decimal.

`/` gives a number when both sides are numbers and the division is exact, and
a decimal otherwise. Dividing by zero is an error.

`%` is the remainder, following C semantics for negative operands.

`^` gives a number when both sides are numbers, the exponent is not negative,
and the result fits exactly; otherwise a decimal.

### 7.3 `+` on other types

- If either side is text, both are converted to text and joined.
- Two lists are joined into a new list.
- Anything else is an error.

### 7.4 Comparison

`==` and `!=` accept any two values. Numbers and decimals compare by value.
Lists compare item by item. Different kinds are never equal.

`< <= > >=` accept two numbers or two pieces of text (compared byte by byte).
Anything else is an error.

`in` answers whether a value is in a list, or whether one piece of text
occurs inside another.

### 7.5 Truth

`false`, `nothing`, `0`, `0.0`, `""` and `[]` are false. Everything else is
true. `and` and `or` evaluate their right side only when needed and give back
the value that decided the result. `not` always gives a boolean.

### 7.6 Indexing

`x[i]` reads position `i` of a list (giving the item) or of text (giving a
one-character piece of text). Positions start at `0`; negative positions count
back from the end. Out of range is an error.

`x[i] = v` assigns into a list. Text cannot be changed in place.

### 7.7 Calls and members

`f(a, b)` calls a function with exactly the arguments it declares; a mismatch
is an error. `m.name` reads a member of a module.

### 7.8 `ask`

`ask` is an expression. `ask "prompt"` prints the prompt with no newline and
reads one line from standard input, giving it back as text with the newline
removed. `ask` on its own reads without printing anything. At end of input it
gives `nothing`.

## 8. Statements

```text
say expr ("," expr)*                     print, space separated, newline at the end
name = expr                              assign
target[index] = expr                     assign into a list
expr                                     evaluate and discard

if expr ":" block
("else" "if" expr ":" block)*
("else" ":" block)?

while expr ":" block

for name "in" expr ":" block             over a list or text
for name "in" expr "to" expr ("by" expr)? ":" block

fun name "(" params ")" ":" block
return expr?
break
skip

try ":" block catch name? ":" block
error expr

use modulename ("as" name)?
use "path.chop" "as" name
```

`for ... to ...` includes both ends and counts up in steps of `1` unless `by`
gives another step. A step of `0` is an error. If the range is empty the body
does not run.

`break` and `skip` apply to the innermost enclosing loop.

## 9. Scope

There is one global scope, plus one scope per function call and one per
`catch` block.

Assignment updates the nearest existing binding of that name in any enclosing
scope; if there is none, it creates the name in the current scope. Parameters
are always local. Functions capture the scope they were defined in, so nested
functions can read and update the variables around them.

## 10. Modules

`use math` evaluates to a module value bound to the name `math` (or the name
after `as`). The built-in modules are `math`, `text`, `lists`, `random`,
`files` and `time`.

`use "file.chop" as name` reads that file relative to the file doing the
loading, runs it once from top to bottom in a scope whose parent is the
globals, and binds a module holding everything that scope defined. Circular
loads are detected and reported.

## 11. Errors

Every error carries a line number, the source line, a plain-English message
and often a hint. Errors are reported on standard error in this shape:

```text
Chopcap Error

  file.chop, line 4:
    say name

  `name` does not exist.
  Did you mean `username`?
```

`error expr` raises an error whose message is `expr` converted to text.
`try: ... catch name: ...` runs the guarded block, and on an error binds the
message to `name` as text and runs the catch block. Errors from the lexer and
parser happen before the program runs and cannot be caught.

Exit codes: `0` success, `64` bad command line, `65` source could not be
parsed, `66` file could not be opened, `70` error while running.

## 12. Evaluation order

Left to right, throughout: `say` arguments, operands, list literals, call
arguments, and the parts of an indexed assignment. The first error stops the
program (or the enclosing `try`).

## Limitations

Known and deliberate for 0.1.0:

- **Text is bytes.** `len`, indexing and `text.chars` work on bytes, so
  non-ASCII characters count as more than one. Whole-string operations
  (joining, comparing, `text.replace`) are safe with any UTF-8.
- **No maps or objects.** Lists are the only container.
- **No `elif` keyword** — `else if` does the job.
- **Reference cycles are not collected.** Values are reference counted; a
  function that captures the scope that holds it keeps that memory until the
  program exits. Ordinary programs are unaffected.
- **Integers wrap** on overflow rather than growing.
- **macOS binaries only** in this release. The source is portable C11 using
  only POSIX interfaces, so it is expected to build on Linux with `make`, but
  0.1.0 neither ships nor tests non-macOS builds.
