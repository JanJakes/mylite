# Baseline ODKU VALUES cross-column references

## Status

Implemented.

## Compatibility Source

- Official MySQL 8.4 Reference Manual, "INSERT ... ON DUPLICATE KEY UPDATE
  Statement": `https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html`.
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_insert_on_duplicate_key_update_expectations.sh`
  and
  `packages/libmylite/tests/mysql_baseline_insert_select_on_duplicate_key_update_expectations.sh`.

MySQL accepts deprecated `VALUES(column_name)` in the
`ON DUPLICATE KEY UPDATE` assignment expression and returns the would-be
inserted value for that target-table column. The referenced column does not
need to be the assignment target. MySQL emits one `1287` deprecation warning per
`VALUES()` occurrence.

## Scope

This slice expands the existing duplicate-key update baseline:

```sql
insert_duplicate_assignment ::= target_column EQ VALUES LPAREN source_column RPAREN
```

`target_column` and `source_column` are unqualified target-table column
identifiers. The source column may differ from the assignment target when the
inserted source value has the same logical and physical storage descriptor as
the target descriptor. Character-string descriptors must also have the same
character set and collation.

Supported statement envelopes are the current `INSERT ... VALUES`,
`INSERT ... SET`, and `INSERT ... SELECT` duplicate-key-update subsets.

## Semantics

On a duplicate-key update, MyLite copies the planned inserted value for
`source_column` and writes the assignment target column. Values are taken from
the would-be inserted row, not from the existing physical row and not from
earlier assignments in the same duplicate-key tail.

Same-column `VALUES(target_column)` keeps its current no-op handling for
auto-increment assignment-target detection and duplicate-key bookkeeping.
Cross-column references are treated as ordinary assignments to the target
column.

## Errors

- Qualified `VALUES(table.column)` references remain rejected in this baseline.
- Unknown columns return MySQL-shaped unknown-column diagnostics.
- Source/target descriptors that require target-side implicit conversion,
  truncation, or range handling return the existing MyLite unsupported
  diagnostic rather than silently applying incomplete conversion behavior.
- General non-ODKU `VALUES(column)` scalar use remains outside this slice.

## Tests

The runtime tests cover:

- `INSERT ... VALUES ... ON DUPLICATE KEY UPDATE c = VALUES(a)`;
- multiple cross-column references in one duplicate-key tail, proving values
  come from the inserted row and are not affected by assignment order;
- the same cross-column behavior for `INSERT ... SELECT`;
- continued warnings and unknown-column diagnostics.
