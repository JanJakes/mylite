# ANALYZE TABLE

This feature accepts the base MySQL 8.4 `ANALYZE TABLE` maintenance statement
and returns MySQL-shaped result rows for embedded MyLite tables. The first
implemented slice covers key-distribution analysis syntax and result metadata;
histogram management syntax and persistent optimizer statistics are specified
as follow-up work.

## Sources

- MySQL 8.4 Reference Manual, `ANALYZE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/analyze-table.html>
- MySQL 8.4.9 runtime probes run against the local `mylite-mysql-849`
  container, covering base tables, temporary tables, missing tables, unknown
  schemas, `information_schema`, views, histogram clauses, and invalid options.

## Syntax

Base key-distribution analysis:

```lemon
table_maintenance_statement ::= ANALYZE opt_write_to_binlog TABLE
    maintenance_table_name_list.
```

The optional logging modifier accepts `LOCAL` and `NO_WRITE_TO_BINLOG`. MyLite
stores no binary log, so both modifiers are accepted and ignored.

`FAST`, `QUICK`, `MEDIUM`, `EXTENDED`, `CHANGED`, `FOR UPGRADE`, and
`USE_FRM` are not valid `ANALYZE TABLE` options and remain syntax errors.

Histogram forms are valid MySQL syntax but are not part of the first runtime
slice:

```lemon
table_maintenance_statement ::= ANALYZE opt_write_to_binlog TABLE table_name
    UPDATE HISTOGRAM ON identifier_list analyze_histogram_update_tail.
table_maintenance_statement ::= ANALYZE opt_write_to_binlog TABLE table_name
    DROP HISTOGRAM ON identifier_list.
```

The histogram slice must preserve MySQL's single-table rule, per-column
diagnostic rows, temporary-table rejection, unsupported data-type checks,
`WITH N BUCKETS`, `USING DATA`, and `{MANUAL|AUTO} UPDATE` behavior.

## Result Metadata

Base `ANALYZE TABLE` returns four columns:

| Column | Type | Charset | Length | Decimals | Nullability |
| --- | --- | --- | --- | --- | --- |
| `Table` | `VAR_STRING` | latin1 / id 8 | 128 | 31 | nullable |
| `Op` | `VAR_STRING` | latin1 / id 8 | 10 | 31 | nullable |
| `Msg_type` | `VAR_STRING` | latin1 / id 8 | 10 | 31 | nullable |
| `Msg_text` | `MEDIUM_BLOB` | latin1 / id 8 | 393216 | 31 | nullable |

These descriptors match the shared table-maintenance metadata already used by
`CHECK TABLE`, `OPTIMIZE TABLE`, and `REPAIR TABLE`.

## Runtime Semantics

For supported MyLite base and temporary tables, base `ANALYZE TABLE` returns one
row:

| `Table` | `Op` | `Msg_type` | `Msg_text` |
| --- | --- | --- | --- |
| `<schema>.<table>` | `analyze` | `status` | `OK` |

MyLite does not currently store optimizer key-distribution statistics, so this
is an embedded no-op surface. The statement does not change data, row counts,
affected rows, warnings, or schema metadata.

For a missing table in an existing schema, MySQL returns two result rows and no
warnings:

| `Table` | `Op` | `Msg_type` | `Msg_text` |
| --- | --- | --- | --- |
| `<schema>.<table>` | `analyze` | `Error` | `Table '<schema>.<table>' doesn't exist` |
| `<schema>.<table>` | `analyze` | `status` | `Operation failed` |

For an unknown schema, MySQL returns two result rows and no warnings:

| `Table` | `Op` | `Msg_type` | `Msg_text` |
| --- | --- | --- | --- |
| `<schema>.<table>` | `analyze` | `Error` | `Unknown database '<schema>'` |
| `<schema>.<table>` | `analyze` | `error` | `Corrupt` |

For `information_schema` targets, MySQL rejects `ANALYZE TABLE` before returning
rows with error 1044:

```text
Access denied for user 'root'@'localhost' to database 'information_schema'
```

For views, MySQL returns non-base-table result rows. User views are deferred
until MyLite supports persistent view metadata; once views exist, `ANALYZE
TABLE` must report:

| `Table` | `Op` | `Msg_type` | `Msg_text` |
| --- | --- | --- | --- |
| `<schema>.<view>` | `analyze` | `Error` | `'<schema>.<view>' is not BASE TABLE` |
| `<schema>.<view>` | `analyze` | `status` | `Operation failed` |

## Errors, Warnings, And Diagnostics

- An unqualified table name without a selected schema fails with MySQL's
  no-selected-database diagnostic.
- `information_schema` targets fail with error/warning code 1044.
- Missing user tables and unknown user schemas are represented as result rows,
  not diagnostics-area warnings.
- Syntax that MySQL rejects for base `ANALYZE TABLE`, such as trailing `FAST`,
  remains a parser error.

## Tests

Tests cover:

- parser acceptance for base, `LOCAL`, and `NO_WRITE_TO_BINLOG` forms
- parser rejection for missing table names and invalid `FAST`/missing `TABLE`
  forms
- result metadata descriptors
- existing persistent and temporary table rows
- missing-table and unknown-schema rows
- selected-schema and qualified-table resolution
- `information_schema` access-denied diagnostics and warning code 1044

Histogram tests must be added with the later histogram implementation and must
compare against MySQL 8.4.9 for successful create/drop rows, multi-column
partial diagnostics, temporary-table rejection, invalid JSON, bucket bounds, and
single-table enforcement.

## Compatibility Status

`ANALYZE TABLE` is partially supported. Base table key-distribution syntax and
the no-op result-set surface are implemented. Histogram statistics,
`information_schema.COLUMN_STATISTICS`, persistent cardinality maintenance,
views, privilege filtering, table locks, and partition-specific analysis are
deferred.
