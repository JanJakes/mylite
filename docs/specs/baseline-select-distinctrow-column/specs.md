# Baseline SELECT DISTINCTROW Column

## Summary

This feature adds MySQL's `DISTINCTROW` spelling as an alias for MyLite's
existing limited one-column `SELECT DISTINCT` path:

```sql
SELECT DISTINCTROW column_name
FROM table_name
[WHERE predicate]
[ORDER BY column_name [ASC | DESC]]
[LIMIT ...]
```

The feature is intentionally a spelling extension, not a broader distinct-row
implementation. It reuses the descriptor-driven single-table `SELECT DISTINCT
column` planner and SQLite duplicate elimination added by
`baseline-select-distinct-column`.

## Sources And Runtime Evidence

Normative sources:

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>

The MySQL documentation lists `ALL`, `DISTINCT`, and `DISTINCTROW` after
`SELECT`; it states that `ALL` is the default, `DISTINCT` removes duplicate
rows, and `DISTINCTROW` is a synonym for `DISTINCT`.

Observed MySQL 8.4.9 runtime behavior, using the local `mysql:8.4.9` Docker
runtime named `mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_distinctrow_probe;
USE mylite_distinctrow_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL, b BOOL NULL);
INSERT INTO t VALUES
  (1, NULL, TRUE),
  (2, 20, FALSE),
  (3, 20, FALSE),
  (4, 30, NULL),
  (5, NULL, TRUE);

DO 0;
SELECT DISTINCT n FROM t ORDER BY n;
SELECT @@warning_count, ROW_COUNT();
DO 0;
SELECT DISTINCTROW n FROM t ORDER BY n;
SELECT @@warning_count, ROW_COUNT();
SELECT DISTINCTROW n FROM t WHERE n IS NOT NULL ORDER BY n DESC LIMIT 1 OFFSET 1;
SELECT DISTINCTROW b FROM t ORDER BY b;
SELECT DISTINCTROW n FROM mylite_distinctrow_probe.t ORDER BY n LIMIT 2;
SELECT DISTINCTROW n FROM t ORDER BY id;
```

Observed results:

- `SELECT DISTINCT n ...` and `SELECT DISTINCTROW n ...` both returned
  `NULL`, `20`, `30`.
- Both successful result-set statements left `@@warning_count = 0` and
  following `ROW_COUNT() = -1`.
- The filtered/ordered/limited statement returned `20`.
- The `BOOL` alias column returned `NULL`, `0`, `1`.
- The schema-qualified statement worked without relying on selected-schema
  resolution.
- Ordering by a known column outside the distinct select list failed with MySQL
  error `3065 (HY000)` and the same incompatibility wording used for
  `DISTINCT`.

## Scope

In scope:

- `SELECT DISTINCTROW column_name FROM table_name`
- one descriptor-backed persistent base table;
- unqualified and schema-qualified table names, using the existing selected
  schema policy;
- one unqualified selected descriptor column, including explicitly named
  invisible columns;
- current baseline `WHERE` predicate subset;
- optional `ORDER BY` on the same selected descriptor column only;
- optional `ASC` or `DESC`;
- existing supported `SELECT` `LIMIT` and `OFFSET` forms;
- integer-family and `BOOL` / `BOOLEAN` descriptor columns already supported by
  the row-value, type, and distinct-column slices; and
- existing result conventions for successful row-returning statements.

Out of scope:

- full multi-expression `DISTINCTROW`;
- wildcard distinct rows;
- selected literals or expressions;
- aliases;
- table-qualified selected or ordering columns;
- ordering by aliases, ordinals, expressions, or non-selected descriptor
  columns;
- joins, grouping, subqueries, CTEs, set operations, locking clauses, and query
  modifiers; and
- explicit `ALL` support.

## Architecture

`DISTINCTROW` belongs in the parser/AST compatibility layer. MyLite normalizes
the spelling to the same internal select modifier already used for `DISTINCT`.

The runtime planner must not distinguish `DISTINCT` from `DISTINCTROW` after
parsing. It continues to:

- resolve schemas, tables, selected columns, predicate columns, ordering columns,
  and limit values through MyLite-owned descriptors and conversion code;
- reject reserved `_mylite_*` schema and table names before SQLite SQL is
  generated;
- generate SQLite SQL using stable physical table and column names;
- quote every generated identifier;
- bind predicate and limit values through prepared-statement parameters; and
- rely on SQLite for duplicate elimination, filtering, ordering, and limiting.

No catalog rows, descriptor versions, descriptor caches, SQLite schema
generation counters, file-format bytes, VFS code, or SQLite fork patches are
changed by this feature.

## Syntax

MyLite grammar snippets, independently authored for this slice:

```lemon
select_statement ::=
    SELECT DISTINCTROW select_item_list FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.

select_statement ::=
    SELECT DISTINCTROW STAR FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.
```

The parser may admit `DISTINCTROW` with a general select-item list and wildcard
so runtime diagnostics can match the existing `DISTINCT` compatibility path.
Runtime support remains limited to exactly one unqualified descriptor column.

## Semantics

For every supported statement, `DISTINCTROW` is equivalent to `DISTINCT`.

- Duplicate non-`NULL` stored integer values collapse to one output row.
- Duplicate `NULL` values collapse to one `NULL` output row.
- Result column names use the resolved descriptor column name, matching the
  existing descriptor-backed `DISTINCT` path. Source-spelling label parity is
  out of scope for this alias slice.
- `ORDER BY` uses the selected column only. Default direction is ascending;
  `ASC` is explicit ascending; `DESC` is descending. `NULL` sorts first
  ascending and last descending, matching the current select-order-limit slice.
- `LIMIT` and `OFFSET` use the existing supported select literal forms and
  diagnostics.
- Successful statements return row results, `affected_rows == 0`,
  `warning_count == 0`, and set the following `ROW_COUNT()` result to `-1`.

## Diagnostics

`DISTINCTROW` reuses the existing `SELECT DISTINCT column` diagnostics for:

- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema/table names;
- unknown selected, predicate, or order columns;
- wildcard, multiple select items, selected expressions, literals, aliases,
  table-qualified selected columns, table-qualified order columns, order
  aliases, ordinals, expressions, multiple sort keys, non-selected order
  columns, unsupported predicates, unsupported limits, joins, grouping,
  subqueries, CTEs, and query modifiers; and
- physical SQLite failures and allocation failures.

Supported in-range statements produce no warnings.

## Tests

Add MySQL-runtime-verified expectations for:

- `DISTINCTROW` matching `DISTINCT` for duplicate integers and `NULL`;
- warning count `0` and following `ROW_COUNT() == -1`;
- predicate, ordering, and limit composition;
- `BOOL` alias columns;
- schema-qualified table names; and
- non-selected `ORDER BY` diagnostics.

Add fast C coverage by extending the existing parser and select-order-limit
lifecycle tests:

- parse supported `DISTINCTROW` table-backed syntax into the same distinct
  select modifier;
- keep no-source/`DUAL` `DISTINCTROW` outside scope;
- execute `DISTINCTROW` over integer/nullable/`BOOL` descriptor columns;
- verify schema-qualified, reopened, renamed, and dropped-table paths where
  distinct-row spelling could diverge; and
- preserve existing `DISTINCT`, non-distinct select, parser, runtime lifecycle,
  file-format, VFS, and catalog-generation behavior.
