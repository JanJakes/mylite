# ROUND() scalar function

## Scope

This feature implements MySQL-compatible `ROUND()` for the scalar expression
surfaces that MyLite currently executes:

- no-table scalar `SELECT`
- one-table `SELECT` projection, `WHERE`, and `ORDER BY`
- supported single-table `UPDATE` assignments, predicates, and ordering
- supported single-table `DELETE` predicates and ordering

The feature covers ordinary scalar function syntax only:

```sql
ROUND(X)
ROUND(X, D)
```

`ROUND()` remains out of scope for generated columns, default expressions,
prepared-statement parameters, stored programs, aggregate/window contexts, and
deferred DML expression paths that are not yet part of MyLite's scalar
evaluator.

## Sources

Behavior is independently specified from:

- MySQL 8.4 Reference Manual, Mathematical Functions
- MySQL 8.4 Reference Manual, Precision Math Rounding Behavior
- observed MySQL 8.4.9 runtime behavior in Docker container
  `mylite-mysql-849`

Runtime probes used:

```sh
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --batch --raw --show-warnings --force
docker exec -i mylite-mysql-849 mysql -h127.0.0.1 -uroot --column-type-info -vvv --force
```

This specification does not copy MySQL grammar, documentation prose, or
implementation sources.

## Syntax and parser behavior

`ROUND` is accepted as a normal scalar function name. It is case-insensitive and
uses the shared function-call AST:

```lemon
primary_expression ::= scalar_function_call.
scalar_function_call ::= function_name LPAREN function_argument_list RPAREN.
function_name ::= identifier.

function_argument_list ::= expression.
function_argument_list ::= function_argument_list COMMA expression.
```

Binding accepts exactly one or two arguments. Other arities are rejected through
the existing unsupported-function-arity path.

## Semantics

`ROUND(X)` behaves as `ROUND(X, 0)`. `ROUND(X, D)` rounds `X` to `D` decimal
places.

If `X` or `D` is `NULL`, the result is `NULL`.

`D` is converted to a signed integer before rounding:

- integer and unsigned inputs are used directly, with values outside the
  supported range clamped after conversion
- approximate and exact numeric expression values are rounded to the nearest
  integer, with half values away from zero for MyLite's current numeric
  conversion model
- text scale arguments use integer parsing, so trailing non-integer text such
  as `'1.9'` produces warning 1292 and scale `1`
- values greater than `30` are treated as `30`
- values less than `-30` are treated as `-30`

Positive `D` rounds to places right of the decimal point. `D = 0` rounds to an
integer-valued result. Negative `D` rounds digits left of the decimal point,
replacing less significant digits with zero.

The rounding rule depends on the effective type of `X`:

- exact integer, unsigned integer, and exact decimal literal inputs round half
  away from zero
- approximate inputs use the host C library's nearest-integer behavior for
  halfway cases; on the verified runtime this matches nearest-even behavior for
  `ROUND(25E-1)`
- text inputs are converted as DOUBLE values, so decimal-looking text such as
  `'2.5'` follows approximate behavior rather than exact decimal-literal
  behavior

Text conversion warnings use MySQL warning 1292. Invalid or trailing text in
`X` reports `Truncated incorrect DOUBLE value`. Invalid or trailing text in
`D` reports `Truncated incorrect INTEGER value`.

Integer overflow after negative-scale rounding is an error. MySQL reports error
1690 / SQLSTATE `22003` for cases such as
`ROUND(9223372036854775807, -1)` and
`ROUND(18446744073709551615, -1)`. MyLite should surface the same error code
through its current diagnostics model.

## Result metadata

Metadata follows MySQL's result-type family for the first argument:

- integer and unsigned integer `X` return `LONGLONG`, decimal scale `0`, binary
  numeric collation, and the unsigned flag when `X` is unsigned
- approximate or nonnumeric `X` return `DOUBLE`, length `23`, decimals `31`,
  binary numeric collation
- decimal `X` returns `NEWDECIMAL`; when `D` is a constant, positive scale
  reduces the result scale to `min(D, input_scale)`, zero and negative scales
  return scale `0` and remove the input fractional digits from declared
  length, and extreme positive/negative scale is clamped to `30`
- nullability is nullable if either argument can be `NULL`

Representative MySQL 8.4.9 metadata probes:

| Expression | Type | Length | Decimals | Flags |
| --- | --- | ---: | ---: | --- |
| `ROUND(123.456,2)` | `NEWDECIMAL` | `8` | `2` | `NOT_NULL BINARY NUM` |
| `ROUND(12.34)` | `NEWDECIMAL` | `4` | `0` | `NOT_NULL BINARY NUM` |
| `ROUND(CAST(12.34 AS DECIMAL(4,2)))` | `NEWDECIMAL` | `4` | `0` | `NOT_NULL BINARY NUM` |
| `ROUND(25E-1)` | `DOUBLE` | `23` | `31` | `NOT_NULL BINARY NUM` |
| `ROUND(150,2)` | `LONGLONG` | `21` | `0` | `NOT_NULL BINARY NUM` |
| `ROUND('123.455',2)` | `DOUBLE` | `23` | `31` | `NOT_NULL BINARY NUM` |
| `ROUND(NULL,2)` | `DOUBLE` | `23` | `31` | `BINARY NUM` |
| `ROUND(d,2)` for `d DECIMAL(8,3)` | `NEWDECIMAL` | `10` | `2` | `BINARY NUM` |
| `ROUND(d,-1)` for `d DECIMAL(8,3)` | `NEWDECIMAL` | `7` | `0` | `BINARY NUM` |

## SQL mode notes

Default MyLite strict-mode behavior promotes expression warnings in supported
`UPDATE` and `DELETE` predicates/assignments to statement errors, matching the
current scalar-expression policy. Plain `SELECT` keeps conversion diagnostics as
warnings.

`IGNORE_SPACE` and other SQL-mode-sensitive parser variants are not part of
this feature. The existing ordinary function-call parser accepts whitespace
between a function name and `(` under the current default mode.

## Interactions

`ROUND()` uses the shared scalar evaluator and warning collection. It must be
usable anywhere other supported scalar functions are usable today.

`ROUND()` results participate in boolean predicates, comparisons, ordering, and
assignment conversion through the existing expression value conversion rules.
Metadata inference must not evaluate expressions and therefore must not create
conversion warnings for `LIMIT 0` metadata-only queries.

## Storage and runtime implications

No file-format changes are required. `ROUND()` is deterministic, has no session
state, and does not require new dependencies. Exact decimal literal rounding is
implemented inside MyLite instead of delegating to SQLite so half-away-from-zero
behavior and negative-scale handling stay MySQL-compatible.

Current MyLite stores and evaluates many runtime numeric values through the
existing integer, unsigned, real, and text expression value classes. Full
fixed-point arithmetic for stored DECIMAL values remains a broader numeric
runtime task; this feature still provides MySQL-compatible metadata and covered
runtime behavior for the current value classes.

## MySQL-runtime-verified expectations

```sql
SELECT ROUND(2.5), ROUND(-2.5), ROUND(25E-1), ROUND(-25E-1);
-- 3, -3, 2, -2

SELECT ROUND(123.456,2), ROUND(123.455,2), ROUND(-123.455,2);
-- 123.46, 123.46, -123.46

SELECT ROUND(23.298,-1), ROUND(98765.4321,-3);
-- 20, 99000

SELECT ROUND(25E0,-1), ROUND(CAST(12.34 AS DECIMAL(4,2)),-1), ROUND(20E0);
-- 20, 10, 20

SELECT ROUND(.12345678901234567890123456789012345,35),
       ROUND(12345,-35), ROUND(12345,35);
-- 0.123456789012345678901234567890, 0, 12345

SELECT ROUND(NULL), ROUND(1.2,NULL);
-- NULL, NULL

SELECT ROUND('123.455',2), ROUND('123.455x',2), ROUND('abc',2), ROUND('2.5');
-- 123.46, 123.46, 0, 2
-- warnings 1292 for '123.455x' and 'abc'

SELECT ROUND(12.555,1.1), ROUND(12.555,1.5), ROUND(12.555,1.9),
       ROUND(12.555,'1.9');
-- 12.6, 12.56, 12.56, 12.6
-- warning 1292 for text scale '1.9'
```

Table-surface expectations use:

```sql
CREATE TABLE r (
  id INT PRIMARY KEY AUTO_INCREMENT,
  i INT,
  u INT UNSIGNED,
  d DECIMAL(8,3),
  f DOUBLE,
  s VARCHAR(16)
);

INSERT INTO r (i,u,d,f,s) VALUES
  (-25,25,123.455,2.5,'123.455'),
  (15,15,-123.455,-2.5,'abc'),
  (NULL,NULL,NULL,NULL,NULL);
```

Expected covered behavior:

- projection, filtering, and ordering can use `ROUND()`
- invalid numeric text in `SELECT` reports warning 1292
- invalid numeric text in supported strict `UPDATE` and `DELETE` predicates is
  promoted to statement error without mutating rows
- `ROUND()` assignment results convert through the existing target-column
  assignment rules

## Known gaps

- Native fixed-point runtime storage for all DECIMAL column values is still
  broader than this feature. The implementation covers current MyLite value
  classes and keeps decimal metadata compatible for supported descriptors.
- Exact MySQL diagnostic text for every overflow expression shape can be
  refined as MyLite's diagnostic model grows.
