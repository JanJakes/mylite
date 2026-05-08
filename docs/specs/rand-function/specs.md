# `RAND()`

## Scope

MyLite implements `RAND()` and `RAND(seed)` for scalar and table-backed
expression contexts.

## Behavior

- The result is a non-NULL `DOUBLE` in `[0, 1)`.
- Result metadata uses MySQL's `DOUBLE` shape: length 23, decimals 31, binary
  numeric collation, and `NOT_NULL` only when no seed is present or the seed
  expression is statically non-null. `RAND(NULL)` and nullable seed expressions
  report nullable metadata even though the runtime value is non-NULL.
- `RAND()` uses session pseudo-random state initialized from current time and
  connection-specific data when no explicit RAND seed variables have been set.
- `SET rand_seed1 = n` and `SET rand_seed2 = n` seed the current session's next
  unseeded `RAND()` sequence. MySQL exposes both variables as session-only
  unsigned system variables that read and show as `0` even after assignment.
- `RAND(seed)` uses deterministic per-expression state. The seed initializes
  that expression's generator the first time the expression is evaluated, then
  the generator advances on later rows.
- Row-dependent seed expressions reinitialize the generator for each evaluated
  row, so equal seed values produce equal random values even when separated by
  other rows.
- Two separate `RAND(seed)` expressions in the same select list produce the same
  first value when their seeds are equal.
- `RAND(NULL)` behaves like `RAND(0)`.
- Approximate numeric seeds round to the nearest integer with half values away
  from zero.
- Seed initialization uses MySQL's signed 32-bit seed normalization after SQL
  value coercion, so values such as `4294967295`, `4294967296`, and
  `4294967297` behave like `-1`, `0`, and `1`.
- Text seeds parse an integer prefix after optional whitespace and sign.
  Fractional, nonnumeric, trailing-garbage, and overflow text seeds emit warning
  1292 and use the parsed or clamped seed value.

## Verified Expectations

Verified against MySQL 8.4.9:

| Expression | Result |
| --- | --- |
| `RAND(1)` | `0.40540353712197724` |
| `RAND(2)` | `0.6555866465490187` |
| `RAND(7)` | `0.9065021936842261` |
| `RAND(8)` | `0.15668530311126755` |
| `RAND(NULL)` | same first value as `RAND(0)` |
| `RAND(4294967295)` | same first value as `RAND(-1)` |
| `RAND(4294967296)` | same first value as `RAND(0)` |
| `RAND(1.5)` | same first value as `RAND(2)` |
| `RAND(-1.5)` | same first value as `RAND(-2)` |
| `RAND('7.9')` | same first value as `RAND(7)`, warning 1292 |
| `RAND('bad')` | same first value as `RAND(0)`, warning 1292 |
| `SHOW VARIABLES LIKE 'rand\_seed%'` | returns `rand_seed1 = 0` and `rand_seed2 = 0` |
| `SHOW GLOBAL VARIABLES LIKE 'rand\_seed%'` | returns no rows |
| `SELECT @@rand_seed1, @@rand_seed2` | returns unsigned `0`, `0` |
| `SELECT @@GLOBAL.rand_seed1` | error 1238: `Variable 'rand_seed1' is a SESSION variable` |
| `SET rand_seed1 = 1; SET rand_seed2 = 2; SELECT RAND(), RAND()` | `0.000000004656612877414201`, then `0.000000051222741651556214` |
| `SET @@SESSION.rand_seed1 = 3; SET @@SESSION.rand_seed2 = 4; SELECT RAND(), RAND()` | `0.000000012107193481276923`, then `0.00000008288770921797278` |
| `SET rand_seed1 = DEFAULT` | error 1230: `Variable 'rand_seed1' doesn't have a default value` |
| `SET rand_seed1 = '7'` | error 1232: `Incorrect argument type to variable 'rand_seed1'` |
| `SET GLOBAL rand_seed1 = 4` | error 1228: `Variable 'rand_seed1' is a SESSION variable and can't be used with SET GLOBAL` |
| `SET rand_seed1 = -1` | succeeds, stores seed `0`, and emits warning 1292: `Truncated incorrect rand_seed1 value: '-1'` |
| `SET rand_seed1 = 4294967296; SET rand_seed2 = 1; SELECT RAND()` | same as using `rand_seed1 = 4`, because assigned seeds are reduced modulo `0x3fffffff` |

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
