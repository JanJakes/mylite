# Baseline COERCIBILITY Function

## Summary

This phase adds a narrow MySQL-compatible `COERCIBILITY(expr)` scalar surface
for the same contexts and argument envelope already supported by the baseline
`CHARSET()` and `COLLATION()` functions. It exposes MySQL's collation
coercibility metadata for currently admitted scalar values and descriptor-backed
columns without adding full collation aggregation, charset conversion, or
collation-aware expression evaluation.

The implementation remains MyLite-owned. Parser and runtime code classify the
argument from the MyLite AST and catalog descriptors, return the verified
coercibility number as a scalar result, and keep SQLite responsible only for
ordinary row filtering/order/limit and physical row access in row-backed
queries.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing related MyLite slices:
  - `docs/specs/baseline-charset-collation-functions/specs.md`
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
  - `docs/specs/baseline-result-column-metadata/specs.md`
  - `docs/specs/baseline-concat-function/specs.md`
  - `docs/specs/baseline-cast-binary/specs.md`
  - `docs/specs/baseline-convert-using-binary/specs.md`
- Official MySQL 8.4 Reference Manual:
  - information functions, including `COERCIBILITY()`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - collation coercibility in expressions:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-collation-coercibility.html>
  - character set and collation of function results:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions-charset.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_coercibility_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `COERCIBILITY(expr)` returns a numeric scalar. Supported calls observed here
  produce no warnings.
- Literal strings and scalar `CONCAT()` results built from ordinary literals
  have coercibility `4`.
- `NULL` and `CONCAT(NULL, NULL)` have coercibility `6`.
- Integer literals, boolean literals, and `RAND()` have coercibility `5`.
- `DATABASE()`, `SCHEMA()`, and `VERSION()` have coercibility `3`.
- `CAST(... AS BINARY)`, `CONVERT(..., BINARY)`,
  `CONVERT(... USING BINARY)`, and `CONVERT(... USING utf8mb4)` have
  coercibility `2`.
- Unknown column references inside the admitted binary cast/convert wrappers
  still fail with MySQL's ordinary unknown-column diagnostic instead of
  returning wrapper metadata.
- Scalar `CONCAT()` uses the lowest precedence number from its supported
  arguments after MySQL's scalar-result classification. In the supported slice,
  this means binary/converted arguments can make the result `2`, system
  constants can make it `3`, ordinary literal/numeric mixed results are `4`,
  and all-`NULL` concatenation is `6`.
- Descriptor-backed nonbinary string, binary string, `BIT`, `ENUM`, `SET`,
  `JSON`, and spatial columns have coercibility `2`, even when the row value
  is `NULL`.
- Descriptor-backed numeric and temporal columns have coercibility `5`, even
  when the row value is `NULL`.
- Zero-argument and multi-argument native calls fail in MySQL with
  `1582 / 42000`; MyLite will continue to reject these through the current
  parser/runtime unsupported path used by one-argument metadata functions until
  a broader native-function argument-count diagnostic slice is added.

MySQL also supports explicit `COLLATE`, character-set introducers, stored
program values, user variables, parameters, generated columns, complete
expression collation aggregation, and coercibility-driven comparison
resolution. Those are outside this baseline.

## Ownership Boundaries

- Public API: unchanged. `COERCIBILITY()` returns through existing scalar result
  conventions as text containing a decimal integer.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Successful supported calls add no warnings.
- Lexer/parser/AST: admits exact one-argument `COERCIBILITY()` expressions and
  preserves source spans for labels and diagnostics.
- Analyzer/planner: resolves descriptor-backed row-scalar arguments from
  MyLite catalog descriptors. Unsupported expression shapes are rejected before
  generated SQLite SQL is built.
- Catalog: read-only for table and column descriptor metadata. The feature does
  not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- SQLite execution: table-backed projections lower descriptor-derived
  coercibility constants to bound SQLite parameters over stable physical table
  names. MyLite does not inspect SQLite schema text or rely on SQLite collation
  metadata.
- Result builder: uses existing row result conventions and source-span labels
  or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT coercibility_item[, coercibility_item ...]
SELECT coercibility_item[, coercibility_item ...] FROM DUAL
```

`DO` form:

```sql
DO coercibility_expr[, coercibility_expr ...]
```

Single-table row-backed forms, with at least one selected expression containing
`COERCIBILITY()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
coercibility_expr:
    COERCIBILITY ( coercibility_value )

coercibility_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | RAND()
  | RAND(seed_literal)
  | DATABASE()
  | SCHEMA()
  | VERSION()
  | CONCAT(non_concat_scalar_coercibility_value[, ...])
  | CAST(binary_wrapper_value AS BINARY)
  | CONVERT(binary_wrapper_value, BINARY)
  | CONVERT(binary_wrapper_value USING BINARY)
  | CONVERT(value USING utf8mb4)
  | descriptor_column_reference        -- table-backed SELECT only
  | ( coercibility_value )

non_concat_scalar_coercibility_value:
    scalar coercibility_value except CONCAT(...) and descriptor_column_reference

binary_wrapper_value:
    scalar coercibility_value except CONCAT(...)
  | descriptor_column_reference        -- direct child in table-backed SELECT only
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- nonbinary string descriptors: `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `ENUM` and `SET`;
- binary string descriptors: `BINARY`, `VARBINARY`, and baseline `BLOB` family;
- integer family, exact `DECIMAL`, approximate numeric, `BIT`, `YEAR`, `DATE`,
  `TIME`, `DATETIME`, `TIMESTAMP`, `JSON`, and currently admitted spatial
  descriptors.

For row-backed descriptor columns:

- nonbinary string, binary string, `BIT`, `ENUM`, `SET`, `JSON`, and spatial
  descriptors return `2`;
- numeric and temporal descriptors return `5`;
- the result depends on descriptor type metadata, not the current row value.

The following remain outside this phase:

- explicit character-set introducers such as `_utf8mb4 'x'`;
- explicit `COLLATE` expressions and full collation aggregation;
- comparison, ordering, grouping, distinct, set-operation, and predicate
  behavior based on coercibility;
- nested `CONCAT()` inside the admitted scalar `CONCAT(...)` metadata envelope;
- `CONCAT(...)` as the child of binary cast/convert wrappers;
- descriptor-column references inside scalar `CONCAT(...)`;
- `COERCIBILITY()` in `WHERE`, expression `ORDER BY`, aggregate arguments, DML
  assignment values, generated columns, defaults, views, parameters, user
  variables, stored functions, and broad subquery contexts.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= COERCIBILITY(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_COERCIBILITY_FUNCTION, B, R);
}
```

No zero-argument or multi-argument productions are added for this phase.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Classify the argument expression:
   - ordinary string, hexadecimal, or bit literal: `4`;
   - integer literal, signed integer literal, boolean literal, `RAND()`, or
     `RAND(seed)`: `5`;
   - `NULL`: `6`;
   - `DATABASE()`, `SCHEMA()`, and `VERSION()`: `3`;
   - `CAST(... AS BINARY)`, `CONVERT(..., BINARY)`,
     `CONVERT(... USING BINARY)`, and `CONVERT(... USING utf8mb4)`: `2`;
   - supported `CONCAT(...)`: classify each argument and return the minimum
     argument coercibility value after the supported scalar classification.
3. Return the decimal text form of the coercibility value.

For row-backed direct descriptor columns, planning derives a constant text
value from the column descriptor. For the admitted binary cast/convert wrappers
over a direct descriptor column, planning resolves the descriptor column for
unknown-column diagnostics and derives constant value `2` from the wrapper
metadata. The generated SQLite projection binds the constant value as a
parameter for every matched row. This preserves the existing row envelope for
`WHERE`, `ORDER BY`, and `LIMIT` without materializing full result sets in
MyLite memory.

## Diagnostics

- Syntax errors: malformed parentheses and grammar not admitted by this phase
  use existing parser diagnostics.
- Zero-argument and multi-argument forms are rejected deterministically by the
  existing function-argument pathway for this baseline, while the MySQL
  expectation script records MySQL's `1582 / 42000` native diagnostic for the
  same shapes.
- Unknown descriptor columns use existing MySQL-shaped unknown-column
  diagnostics.
- Unsupported expression shapes fail deterministically with MyLite unsupported
  diagnostics naming the limited `COERCIBILITY()` surface.
- Allocation failures use existing `MYLITE_NOMEM` diagnostics.
- Physical SQLite failures are reported through existing physical row-read
  diagnostics.

Successful supported calls set `warning_count == 0`.

## Compatibility And Performance Notes

This baseline intentionally exposes only coercibility values for admitted
metadata expressions. It does not make MyLite's comparison engine use full
MySQL coercibility rules yet. Existing ASCII descriptor comparison/order
behavior remains unchanged.

Row-backed `COERCIBILITY(column)` and the admitted direct-column binary wrapper
forms are planned as descriptor-derived constant projections and bound into
SQLite. MyLite does not scan rows or materialize the result set to compute the
value, so the query path remains close to the existing descriptor-driven
row-scalar SELECT path.
