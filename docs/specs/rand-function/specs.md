# `RAND()`

## Scope

MyLite implements `RAND()` and `RAND(seed)` for scalar and table-backed
expression contexts.

## Behavior

- The result is a non-NULL `DOUBLE` in `[0, 1)`.
- `RAND()` uses statement-local pseudo-random state initialized from current
  time and connection-specific data.
- `RAND(seed)` uses deterministic per-expression state. The seed initializes
  that expression's generator the first time the expression is evaluated, then
  the generator advances on later rows.
- Row-dependent seed expressions reinitialize the generator for each evaluated
  row, so equal seed values produce equal random values even when separated by
  other rows.
- Two separate `RAND(seed)` expressions in the same select list produce the same
  first value when their seeds are equal.
- `RAND(NULL)` behaves like `RAND(0)`.
- Numeric seed conversion follows MySQL integer conversion closely enough for
  signed integer, unsigned integer, approximate, and text inputs.

## Verified Expectations

Verified against MySQL 8.4.9:

| Expression | Result |
| --- | --- |
| `RAND(1)` | `0.40540353712197724` |
| `RAND(2)` | `0.6555866465490187` |
| `RAND(7)` | `0.9065021936842261` |
| `RAND(8)` | `0.15668530311126755` |
| `RAND(NULL)` | same first value as `RAND(0)` |

For a three-row table, `SELECT id, RAND(7) FROM t ORDER BY id` returns the first
three values from the seeded sequence:

| id | RAND(7) |
| --- | --- |
| 1 | `0.9065021936842261` |
| 2 | `0.37600881361962185` |
| 3 | `0.16053751777907602` |

For row-dependent seeds:

| id | seed | RAND(seed) |
| --- | --- | --- |
| 1 | `1` | `0.40540353712197724` |
| 2 | `2` | `0.6555866465490187` |
| 3 | `1` | `0.40540353712197724` |
| 4 | `NULL` | `0.15522042769493574` |
