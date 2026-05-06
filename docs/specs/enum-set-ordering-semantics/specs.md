# ENUM and SET Ordering Semantics

## Status

Implemented as the first value-list read-side ordering slice after SQL-level
`ENUM` and `SET` declaration integration.

## References

- MySQL 8.4 Reference Manual, The `ENUM` Type:
  https://dev.mysql.com/doc/refman/8.4/en/enum.html
- MySQL 8.4 Reference Manual, The `SET` Type:
  https://dev.mysql.com/doc/refman/8.4/en/set.html
- MySQL-runtime fixture:
  `docs/specs/enum-set-ordering-semantics/mysql-enum-set-ordering.sql`

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 runtime behavior, and the current MyLite
codebase.

## Scope

This slice covers the first read-side expression semantics that fall out of
value-list descriptors:

- `ORDER BY enum_col` sorts by the enum's one-based declaration index.
- `ORDER BY set_col` sorts by the set's numeric bit mask.
- `GROUP_CONCAT(... ORDER BY enum_col|set_col)` uses the same ordering rule.
- Numeric contexts such as `enum_col + 0`, `set_col + 0`, and comparisons
  against numeric literals continue to use the descriptor numeric context.
- String contexts such as `enum_col > 'alpha'`, `set_col = 'a,b'`, `MIN()`,
  and `MAX()` continue to compare the displayed string value unless another
  expression rule forces numeric conversion.

Deferred scope:

- collation-aware enum/set member comparison for assignment and string-context
  comparisons
- SQLite-native `ORDER BY` over MyLite value-list columns executed entirely
  through SQLite's parser
- optimizer/index ordering over value-list descriptors
- cross-column comparisons between value-list columns with different
  descriptors

## Extension Surface Evaluation

No new SQLite fork opcode is required for this slice. The existing fork
read-time column transformation already gives MyLite materialized row values
both the displayed string and the numeric context. MyLite's SELECT row sorter
and aggregate `ORDER BY` comparator need a small semantic split:

- generic comparison stays string-first for displayed enum/set values, which
  preserves `MIN()`, `MAX()`, and string-literal comparisons
- order-key comparison uses numeric context when both compared values carry it

The direct SQLite execution path is still incomplete: if a caller bypasses the
MyLite statement layer and asks SQLite to sort by a value-list column, SQLite
currently sees the physical integer storage and will sort numerically, but it
does not yet have MyLite's expression metadata, result metadata, warning
surface, or parser/catalog reload path. That direct path belongs to a later
SQLite parser/schema integration slice.

## MySQL 8.4.9 Behavior Summary

The fixture verifies:

- enum values display as strings and expose their declaration index in numeric
  context
- set values display as comma-joined labels and expose their bit mask in
  numeric context
- direct `ORDER BY` for both types uses numeric context
- `DISTINCT` and `GROUP BY` group identical displayed values as expected
- comparisons to string literals are string comparisons, not declaration-order
  comparisons
- comparisons to numeric literals are numeric-context comparisons
- `MIN()` and `MAX()` over enum/set return lexical string extrema, while
  `MIN(col + 0)` and `MAX(col + 0)` expose numeric extrema

## Implementation

- `mylite_select_compare_order_values()` compares non-NULL values by numeric
  context when both sides carry one, then falls back to the generic SELECT
  comparator.
- SELECT result sorting and aggregate-local `GROUP_CONCAT(... ORDER BY ...)`
  use the order comparator.
- Group equality, distinct equality, scalar comparisons, `MIN()`, and `MAX()`
  remain on the generic comparator.
- The runtime test suite covers ordering, grouping, string comparison, numeric
  comparison, aggregate extrema, and aggregate-local ordering with outputs
  verified against the MySQL 8.4.9 fixture.
