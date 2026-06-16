# Baseline Row-Scalar Numeric Functions

## Sources

- MySQL 8.4 Reference Manual, "Mathematical Functions",
  <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>.
- MySQL 8.4 Reference Manual, "Bit Functions and Operators",
  <https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html>.
- Existing MyLite scalar specs for the covered functions under
  `docs/specs/baseline-*-function/`.

## Goal

Make the already-supported baseline numeric scalar functions usable in ordinary
row-backed scalar expression contexts instead of only no-source, `FROM DUAL`,
`DO`, and constant DML paths.

The parser already produces dedicated AST nodes for the covered functions. This
slice adds the small row-scalar grammar bridges needed to admit those nodes in
predicate and order-key contexts, then implements row-scalar planning, SQLite
lowering, and internal UDF execution.

## Covered Functions

The baseline row-scalar surface covers:

- One-argument functions: `ABS()`, `SIGN()`, `CEIL()`, `CEILING()`, `FLOOR()`,
  `SQRT()`, `DEGREES()`, `RADIANS()`, `ACOS()`, `ASIN()`, `SIN()`, `COS()`,
  `TAN()`, `COT()`, `EXP()`, `LN()`, `LOG10()`, `LOG2()`, and `BIT_COUNT()`.
- Variable-arity functions with the MySQL-supported baseline arities:
  `ROUND(value[, places])`, `ATAN(value)`, `ATAN(y, x)`, `ATAN2(value)`,
  `ATAN2(y, x)`, `LOG(value)`, `LOG(base, value)`, `POW(value, exponent)`,
  and `POWER(value, exponent)`.

`BIN()`, `OCT()`, `CONV()`, `FORMAT()`, `TRUNCATE()`, and `CRC32()` remain in
their existing scalar-only envelopes until their string-producing or
decimal-formatting semantics are made row-safe separately.

## Row Contexts

For a descriptor-backed table source, the covered functions are admitted in:

- select-list row-scalar projections;
- supported row-scalar comparison predicates;
- supported `ORDER BY` expression keys;
- single-table DML expression positions that already use row-scalar planning.

Function arguments may be integer, `DECIMAL`, approximate numeric, or nonbinary
string-family descriptor columns; integer and string literals; `NULL`; boolean
literals; supported row-scalar integer arithmetic; and nested covered numeric
functions. SQLite continues to execute row iteration, filtering, and sorting.
MyLite lowers each function call to an internal `_mylite_*` SQLite UDF.

## Semantics

- SQL `NULL` input produces SQL `NULL` for all covered functions.
- `ABS()`, `SIGN()`, `CEIL()` / `CEILING()`, `FLOOR()`, `ROUND()`, and
  `BIT_COUNT()` preserve integer results where the result fits SQLite's signed
  integer storage. Other finite results are returned as SQLite doubles.
- `SQRT()` of negative input returns `NULL`.
- `ACOS()` and `ASIN()` return `NULL` outside `[-1, 1]`.
- `LN()`, one-argument `LOG()`, `LOG10()`, and `LOG2()` return `NULL` for
  nonpositive input.
- Two-argument `LOG(base, value)` returns `NULL` for nonpositive base,
  base `1`, or nonpositive value.
- `COT(0)` raises an out-of-range error.
- `EXP()` and power functions raise an out-of-range error for non-finite
  results.
- `ATAN()` and `ATAN2()` follow the platform `atan()` / `atan2()` semantics for
  finite double inputs, matching the MySQL baseline envelope used by the
  existing scalar tests.
- Nonbinary string inputs in row-backed numeric function calls are coerced with
  MySQL-style numeric-prefix rules. Most covered functions coerce strings as
  approximate `DOUBLE` values and append warning `1292` / SQLSTATE `22007`
  `Truncated incorrect DOUBLE value: '...'` when the string has no numeric
  prefix, has a nonspace suffix after the numeric prefix, or overflows the
  finite `DOUBLE` range. Overflowing string-to-double inputs are clamped to the
  largest finite double value before function evaluation, matching the observed
  MySQL 8.4.9 baseline.
- `ROUND(value, places)` coerces a string `places` argument through integer
  prefix parsing and appends `Truncated incorrect INTEGER value: '...'` for
  truncated or missing integer prefixes.
- `BIT_COUNT()` coerces nonbinary string inputs through integer-prefix parsing
  and then counts the set bits of the unsigned 64-bit representation. For
  example, `BIT_COUNT('7x')` returns `3` and appends warning `1292`.
- `LN()`, `LOG()`, `LOG10()`, and `LOG2()` append warning `3020` /
  SQLSTATE `HY000` `Invalid argument for logarithm` when row-backed evaluation
  returns `NULL` for nonpositive logarithm values. Two-argument `LOG(base,
  value)` evaluates non-`NULL` string arguments for truncation warnings before
  applying the `NULL` result rules.

## Diagnostics and Deferred Gaps

This slice intentionally does not implement full MySQL numeric coercion for
binary string, bit-string, JSON, temporal, or arbitrary decimal inputs in
row-backed expressions. `BIT_COUNT()` does not yet implement MySQL's
binary-string byte bit-counting path. Other row-scalar string-producing
expressions are admitted only where existing row planning already treats them
as supported numeric-function operands.

Exact-decimal result typing and protocol-grade expression metadata remain
future work. SQLite result extraction also normalizes negative zero in this
row-backed UDF path, while MySQL can display `-0` for selected rounded
floating-point results. The covered functions therefore remain yellow in the
compatibility matrix, but their notes should not describe them as lacking
row-backed string coercion or invalid-logarithm warnings.

## Implementation Notes

- Add one row-scalar planned expression kind for numeric functions and store a
  small enum describing the target internal UDF.
- Lower row-scalar numeric functions to internal `_mylite_numeric_*` SQLite
  functions, never to SQLite's optional built-in math functions.
- Reuse the existing recursive row-scalar SQL generation and parameter binding
  paths so nested numeric functions and arithmetic operands work uniformly.
- Extend integer-arithmetic planning enough for numeric function operands to
  appear inside larger row arithmetic expressions such as `ABS(i) + 1`.

## Tests

Add a focused runtime test that verifies the same MySQL-observed behavior in:

- row-backed projection;
- predicate comparison;
- `ORDER BY` expression keys;
- nested function and arithmetic expressions;
- domain `NULL` cases;
- nonbinary string column and literal coercion, including truncation warnings;
- invalid-logarithm warning rows from row-backed execution.

The test should prefer exact integer-result assertions and approximate or
domain-result checks for floating-point math.
