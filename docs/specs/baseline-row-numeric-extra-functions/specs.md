# Baseline Row Numeric Extra Functions

## Summary

This slice extends the scalar `CRC32()`, `FORMAT()`, `TRUNCATE()`, and `PI()`
baselines into MyLite's single-table row-scalar path. These functions already
parse and execute in no-source, `FROM DUAL`, and `DO` contexts. The new
behavior keeps the same syntax and scalar semantics, while admitting
descriptor-backed row projection, supported comparison predicates, `ORDER BY`
expression keys, and covered nesting for the narrow value domains that can be
lowered safely to SQLite scalar callbacks.

## Supported Surface

MyLite supports these row-backed forms:

- `CRC32(value)` over supported row-scalar value arguments, including descriptor
  integer, exact decimal, nonbinary string, baseline text, binary string, BLOB,
  `BIT`, `YEAR`, and temporal values, plus supported nested row-scalar value
  expressions. The input is converted to the SQLite value bytes used by the row
  callback: BLOB values use their bytes and other values use their text form.
- `FORMAT(value, decimals)` over integer or exact-decimal descriptor values,
  signed integer or exact decimal literals, boolean and `NULL` literals, and
  supported row-scalar numeric operands. `decimals` admits the existing
  signed-integer row-scalar operand subset. The locale form remains rejected.
- `TRUNCATE(value, decimals)` over the same value and decimal-place subset as
  `FORMAT()`.
- `PI()` as a zero-argument row-scalar function that returns MySQL's default
  visible scalar text `3.141593` for each source row.

The row callbacks are private SQLite functions registered by MyLite during
connection bootstrap:

```lemon
row_scalar_numeric_extra ::= CRC32 LPAREN row_scalar_value RPAREN.
row_scalar_numeric_extra ::= FORMAT LPAREN row_scalar_exact_decimal COMMA row_scalar_integer RPAREN.
row_scalar_numeric_extra ::= TRUNCATE LPAREN row_scalar_exact_decimal COMMA row_scalar_integer RPAREN.
row_scalar_numeric_extra ::= PI LPAREN RPAREN.
```

## Semantics

`CRC32()` returns an unsigned 32-bit CRC value with MySQL-visible decimal text
and returns `NULL` for `NULL`. It does not alter diagnostics for supported
inputs.

`FORMAT()` rounds half away from zero, clamps decimal places above 30 to 30,
treats negative decimal-place counts as zero places, inserts comma group
separators, returns `NULL` when either argument is `NULL`, and preserves the
existing exact-decimal scalar result formatting.

`TRUNCATE()` truncates toward zero, supports negative decimal-place counts by
zeroing integer places, returns `NULL` when either argument is `NULL`, and
preserves the existing exact-decimal scalar result formatting. When truncating a
descriptor `DECIMAL` value with a descriptor or row-expression decimal-place
argument, the result preserves the descriptor scale observed from MySQL. Literal
decimal-place arguments keep the existing visible truncated scale.

`PI()` returns the same display text as the existing scalar baseline for every
source row.

## Deliberate Gaps

This is not a full expression-engine slice. It does not add:

- three-argument `FORMAT()` locale behavior;
- warning-producing string, binary, `BIT`, temporal, or approximate numeric
  coercion for `FORMAT()` / `TRUNCATE()`;
- grouping, DML assignment, expression defaults, or generated-column use beyond
  the row-scalar projection/predicate/order/nesting envelope;
- exact protocol-grade result metadata for these expressions;
- no-source expression forms that are currently outside the scalar baseline,
  such as no-source `CRC32(1+2)`.

## Tests

Coverage must include:

- MySQL 8.4.9 expectation probes for row-backed `CRC32()`, `FORMAT()`,
  `TRUNCATE()`, and `PI()` values;
- focused MyLite runtime tests for table-backed projection, aliases, supported
  nesting inside `CONCAT()`, comparison predicates, `ORDER BY` expression keys,
  and `NULL` handling;
- preservation of existing scalar, `FROM DUAL`, `DO`, arity, and unsupported
  locale diagnostics.
