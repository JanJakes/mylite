# TRUNCATE() scalar function

## Scope

This feature implements MySQL-compatible scalar numeric `TRUNCATE(X,D)` for the
expression surfaces MyLite currently executes:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and ordering
- supported single-table `DELETE` predicates and ordering

This feature is only the scalar mathematical function. `TRUNCATE TABLE` remains
owned by `docs/specs/truncate-table/specs.md`.

`TRUNCATE()` remains out of scope for generated columns, default expressions,
prepared-statement parameters, stored programs, aggregate/window contexts, and
deferred DML expression paths that are not yet part of MyLite's scalar
evaluator.

## Sources

Behavior is independently specified from:

- MySQL 8.4 Reference Manual, Mathematical Functions:
  https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html
- observed MySQL 8.4.9 runtime behavior in Docker container
  `mylite-mysql-849`

Runtime probes used:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -uroot --column-type-info -vvv --force
```

This specification does not copy MySQL grammar, documentation prose, or
implementation sources.

## Syntax and parser behavior

`TRUNCATE` is accepted as both a statement keyword and, in expression position,
as a scalar function name. The scalar form is case-insensitive and uses the
shared function-call AST:

```lemon
primary_expression ::= scalar_function_call.
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.

function_argument_list ::= expression COMMA expression.
```

Binding accepts exactly two arguments. MySQL 8.4.9 reports parse error 1064 for
`TRUNCATE()`, `TRUNCATE(1)`, and `TRUNCATE(1,2,3)`, because the keyword has a
fixed scalar grammar as well as statement grammar. MyLite's first slice rejects
wrong arity through the existing scalar-function arity path until function-level
native syntax diagnostics are generalized.

## Semantics

`TRUNCATE(X,D)` truncates `X` toward zero at the decimal position selected by
`D`.

`D` is converted to a signed integer scale:

- positive `D` keeps that many digits right of the decimal point
- `D = 0` removes the fractional part
- negative `D` replaces `-D` digits left of the decimal point with zero
- values greater than `30` are treated as `30`
- values less than `-30` are treated as `-30`

If either argument is `NULL`, the result is `NULL`. MySQL still evaluates and
converts both arguments, so warnings from `1/0`, invalid text `X`, or invalid
text `D` are preserved even when the other argument is `NULL`.

Text conversion follows MyLite's existing scalar numeric conversion policy:

- `X` text is converted as a DOUBLE-style numeric value and warning 1292
  reports `Truncated incorrect DOUBLE value` when the text is invalid or has
  trailing nonnumeric bytes
- `D` text is parsed as an integer and warning 1292 reports
  `Truncated incorrect INTEGER value` when the text is invalid or has trailing
  non-integer bytes
- invalid numeric text in `X` contributes numeric value `0`
- invalid integer text in `D` contributes scale `0`

MySQL emits duplicate 1292 INTEGER warning records for some exact DECIMAL
`X` expressions with truncated text `D`, including
`TRUNCATE(123.456,'2abc')` and
`TRUNCATE(CAST(123.456 AS DECIMAL(8,3)),'2abc')`. Approximate `X`
expressions such as `TRUNCATE(123.456E0,'2abc')` emit one scale-conversion
warning, and mixed invalid value/scale text such as
`TRUNCATE('123abc','2abc')` emits one DOUBLE warning and one INTEGER warning.
This slice follows the existing MyLite expression-warning architecture and
emits one warning per actual value or scale conversion; native duplicate
DECIMAL scale-warning counts are deferred.

Exact integer and exact decimal literal inputs use MyLite-owned decimal-text
truncation, not SQLite behavior. Approximate and nonnumeric inputs use the
current DOUBLE fallback, with truncation toward zero implemented in MyLite.

## Result metadata

Metadata follows MySQL's return-type rules for `TRUNCATE()`:

- integer and unsigned integer `X` return `LONGLONG`, decimal scale `0`, binary
  numeric collation, and the unsigned flag when `X` is unsigned
- floating-point and nonnumeric `X` return `DOUBLE`, length `23`, decimals
  `31`, binary numeric collation
- `NULL` literal `X` returns nullable `DOUBLE`
- DECIMAL `X` returns `NEWDECIMAL`; when `D` is a constant integer, positive
  scale reduces the returned scale to `min(D, input_scale)`, negative scale
  returns scale `0`, and metadata scale is clamped to the `[-30,30]` range
- unlike `ROUND()`, `TRUNCATE()` does not add an extra precision digit for
  carry propagation

Representative MySQL 8.4.9 metadata probes:

| Expression | Type | Length | Decimals | Flags |
| --- | --- | ---: | ---: | --- |
| `TRUNCATE(123,2)` | `LONGLONG` | `21` | `0` | `NOT_NULL BINARY NUM` |
| `TRUNCATE(123.456,2)` | `NEWDECIMAL` | `7` | `2` | `NOT_NULL BINARY NUM` |
| `TRUNCATE(123.456E0,2)` | `DOUBLE` | `23` | `31` | `NOT_NULL BINARY NUM` |
| `TRUNCATE('123.456',2)` | `DOUBLE` | `23` | `31` | `NOT_NULL BINARY NUM` |
| `TRUNCATE(NULL,2)` | `DOUBLE` | `23` | `31` | nullable `BINARY NUM` |
| `TRUNCATE(d,2)` for nullable `d DECIMAL(8,3)` | `NEWDECIMAL` | `9` | `2` | `BINARY NUM` |
| `TRUNCATE(d,2)` for `d DECIMAL(8,3) NOT NULL` | `NEWDECIMAL` | `9` | `2` | `NOT_NULL BINARY NUM` |

## SQL mode notes

Default MyLite strict-mode behavior promotes expression warnings in supported
`UPDATE` and `DELETE` predicates/assignments to statement errors, matching the
current scalar-expression policy. Plain `SELECT` keeps conversion diagnostics as
warnings.

`IGNORE_SPACE` and other SQL-mode-sensitive parser variants are not part of this
feature. The existing ordinary function-call parser accepts whitespace between a
function name and `(` under the current default mode.

## Interactions

`TRUNCATE()` uses the shared scalar evaluator and warning collection. It must be
usable anywhere other supported scalar functions are usable today.

`TRUNCATE()` results participate in boolean predicates, comparisons, ordering,
and assignment conversion through the existing expression value conversion
rules. Metadata inference must not evaluate expressions and therefore must not
create conversion warnings for `LIMIT 0` metadata-only queries.

## Storage and runtime implications

No file-format changes are required. `TRUNCATE()` is deterministic, has no
session state, and does not require new dependencies. Exact decimal literal
truncation is implemented inside MyLite so negative scale and fixed-scale text
results stay MySQL-compatible.

Current MyLite stores and evaluates many runtime numeric values through the
existing integer, unsigned, real, and text expression value classes. Full
fixed-point arithmetic for stored DECIMAL values remains a broader numeric
runtime task; this feature still provides MySQL-compatible metadata and covered
runtime behavior for the current value classes.

## MySQL-runtime-verified expectations

```sql
SELECT TRUNCATE(1.223,1), TRUNCATE(1.999,1), TRUNCATE(1.999,0),
       TRUNCATE(-1.999,1), TRUNCATE(122,-2), TRUNCATE(10.28*100,0);
-- 1.2, 1.9, 1, -1.9, 100, 1028

SELECT TRUNCATE(123.456,2), TRUNCATE(123.456,5),
       TRUNCATE(123.456,-1), TRUNCATE(123.456,-5),
       TRUNCATE(-123.456,-1);
-- 123.45, 123.456, 120, 0, -120

SELECT TRUNCATE(0.00123,2), TRUNCATE(-0.00123,2),
       TRUNCATE(-999,-1), TRUNCATE(-999,-2),
       TRUNCATE(-999,-3), TRUNCATE(-999,-4);
-- 0.00, 0.00, -990, -900, 0, 0

SELECT TRUNCATE(NULL,1), TRUNCATE(1.23,NULL);
-- NULL, NULL, no warnings

SELECT TRUNCATE(NULL,1/0);
-- NULL, warning 1365 Division by 0

SELECT TRUNCATE(1/0,NULL);
-- NULL, warning 1365 Division by 0

SELECT TRUNCATE(NULL,'2abc');
-- NULL, warning 1292 Truncated incorrect INTEGER value: '2abc'

SELECT TRUNCATE('abc',NULL);
-- NULL, warning 1292 Truncated incorrect DOUBLE value: 'abc'

SELECT TRUNCATE('123.456',2), TRUNCATE('123abc',2),
       TRUNCATE('abc',2), TRUNCATE(123.456,'2abc'),
       TRUNCATE(123.456,'abc');
-- 123.45, 123, 0, 123.45, 123
-- MyLite warning 1292 for each truncated DOUBLE or INTEGER conversion.
-- MySQL emits two INTEGER warnings for the exact DECIMAL scale conversions.

SELECT TRUNCATE(123.456E0,'2abc'), TRUNCATE('123abc','2abc');
-- 123.45, 123
-- warning 1292 for approximate scale conversion, then one DOUBLE and one
-- INTEGER warning for the mixed text case
```

Table-surface expectations use:

```sql
CREATE TABLE t (
  id INT PRIMARY KEY AUTO_INCREMENT,
  i INT,
  d DECIMAL(8,3),
  nd DECIMAL(8,3) NOT NULL,
  f DOUBLE,
  s VARCHAR(16),
  nn DECIMAL(8,3) NOT NULL
);

INSERT INTO t (i,d,nd,f,s,nn) VALUES
  (123,123.456,123.456,123.456,'123.456',0.001),
  (-123,-123.456,-123.456,-123.456,'abc',1.001),
  (0,NULL,0.001,NULL,NULL,2.001);
```

Expected covered behavior:

- `ORDER BY TRUNCATE(d,2)` sorts `NULL`, `-123.45`, `123.45`
- `WHERE TRUNCATE(d,0) = -123` matches the negative row
- `UPDATE t SET i = TRUNCATE(d,0)` preserves existing `123` and `-123`
  values and reports affected rows `0`
- `UPDATE t SET marker = 9 ORDER BY TRUNCATE(v,0), id LIMIT 1` can use
  `TRUNCATE()` as an update order key
- `DELETE FROM t WHERE TRUNCATE(nn,0) = 0` removes only the `nn = 0.001`
  row
- `DELETE FROM t ORDER BY TRUNCATE(v,0) DESC, id LIMIT 1` can use
  `TRUNCATE()` as a delete order key

## Known gaps

- Native fixed-point runtime storage for all DECIMAL column values is still
  broader than this feature. The implementation covers current MyLite value
  classes and keeps decimal metadata compatible for supported descriptors.
- MyLite's first slice rejects wrong `TRUNCATE()` arity through the existing
  unsupported scalar function path. MySQL's native behavior is parse error
  1064 for this keyword-function arity.
- Native duplicate warning counts for exact DECIMAL `X` with truncated text
  `D` are deferred. MyLite records the conversion warning, but currently does
  not duplicate it to match MySQL's exact count for those expression shapes.
- Full SQL mode and protocol diagnostic fidelity, including exact packet-level
  warning state and metadata outside the current C API, remains deferred with
  the broader scalar-function surface.
