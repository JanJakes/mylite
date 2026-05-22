# Baseline TABLE Statement

## Summary

This phase adds a narrow top-level MySQL `TABLE` statement:

```sql
TABLE table_name [ORDER BY column_name [, column_name ...]] [LIMIT row_count [OFFSET offset]]
TABLE table_name [ORDER BY column_name [, column_name ...]] [LIMIT offset, row_count]
```

The statement returns the visible columns of the named table, like
`SELECT * FROM table_name`, with optional ordering and limiting. The
implementation is intentionally a parser/runtime translation to the existing
descriptor-backed `SELECT * FROM table_name` path. It does not add a second
table-read planner, does not bypass MyLite descriptors, and does not pass
arbitrary SQL through to SQLite.

This is not full MySQL query-expression support. `TABLE` is admitted only as a
top-level statement over the currently supported visible table sources. Use in
set operations, `INSERT ... TABLE`, parenthesized query expressions, subqueries,
or general expression positions is deferred.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline union select lifecycle:
  `docs/specs/baseline-union-select-lifecycle/specs.md`
- Temporary table lifecycle:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/table.html>
- MySQL 8.4 Reference Manual, `SELECT`:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, `LIMIT` optimization and tie-order behavior:
  <https://dev.mysql.com/doc/refman/8.4/en/limit-optimization.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_table_statement_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `TABLE t` returns the same visible columns and rows as `SELECT * FROM t`.
- Invisible columns are omitted from the result.
- `ROW_COUNT()` after a successful `TABLE` result-set statement is `-1`, and
  supported in-range statements leave `@@warning_count == 0`.
- `TABLE` can read a session temporary table; a temporary table shadows a
  persistent table of the same effective name.
- `TABLE t ORDER BY v` sorts ascending by default. `ASC` is accepted and
  equivalent to the default. `DESC` reverses the order.
- `NULL` order follows the same behavior as `SELECT`: `NULL` sorts before
  non-`NULL` ascending and after non-`NULL` descending.
- MySQL accepts multiple order keys, table-qualified order columns, ordinal
  order keys, and expression order keys in the probed runtime, even though the
  documented syntax shows a column-name form. MyLite admits only the existing
  descriptor-column order subset in this slice.
- Rows tied on the admitted order keys have nondeterministic relative order
  unless an additional admitted key makes the order unique.
- `TABLE t LIMIT 0` returns an empty result set. Exact row counts and row
  counts larger than the result set behave like `SELECT`.
- MySQL accepts both `LIMIT row_count OFFSET offset` and `LIMIT offset,
  row_count` for `TABLE`.
- Signed limit literals such as `LIMIT +1` and `LIMIT -1` are syntax errors.
  A literal beyond MySQL's unsigned limit range is also a syntax error.
- `@@sql_select_limit` caps `TABLE` statements without an explicit `LIMIT`.
  An explicit `LIMIT` overrides the session cap.
- `TABLE t WHERE ...` and `TABLE t AS alias` are syntax errors.
- Missing default schema, unknown schema, unknown table, and unknown order
  column diagnostics match the equivalent selected MySQL paths:
  `1046 / 3D000`, `1049 / 42000`, `1146 / 42S02`, and `1054 / 42S22`.

## Scope

Supported:

- top-level `TABLE table_name`;
- top-level `TABLE schema_name.table_name`;
- selected/default schema resolution for unqualified names;
- visible table resolution, including session temporary table shadowing of
  persistent base tables;
- visible descriptor column projection equivalent to `SELECT *`;
- optional `ORDER BY` using the existing non-distinct descriptor-backed
  `SELECT` order-key subset: one or more unqualified or table-qualified
  integer, `BIT`, `YEAR`, `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, or ASCII
  nonbinary string descriptor columns, each with optional `ASC` or `DESC`;
- optional `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and
  `LIMIT offset, row_count` using the existing unsigned integer literal limit
  subset;
- `@@sql_select_limit` capping for supported top-level `TABLE` statements
  without an explicit `LIMIT`;
- public result behavior matching existing row-result conventions:
  `ROW_COUNT() == -1`, result rows are present, `affected_rows == 0`, and
  supported in-range statements have `warning_count == 0`;
- descriptor-backed read persistence and `.mylite` file-format safety inherited
  from the existing `SELECT *` execution path.

Deferred:

- `TABLE` in `UNION`, `INTERSECT`, `EXCEPT`, `WITH`, parenthesized query
  expressions, subqueries, derived tables, scalar expressions, or DML sources;
- `INSERT ... TABLE`, `REPLACE ... TABLE`, `CREATE TABLE ... TABLE`, and
  `CREATE TEMPORARY TABLE ... TABLE`;
- aliases, `WHERE`, `GROUP BY`, `HAVING`, locking clauses, query modifiers,
  `INTO OUTFILE`, `PARTITION`, index hints, optimizer hints, or table
  sampling;
- ordinal order keys, expression order keys, string-literal order keys,
  collations, and binary/decimal/approximate/JSON/enum/set/spatial ordering
  beyond the existing `SELECT` subset;
- full `INFORMATION_SCHEMA` parity for `TABLE` beyond behavior inherited from
  the existing descriptor/system `SELECT` paths;
- arbitrary SQLite SQL pass-through or SQLite fork changes.

## Ownership Boundaries

- Public API: no ABI change. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, `ROW_COUNT()`,
  found-row state, and successful result-set finalization. `TABLE` is a
  top-level result-set statement and must therefore update `ROW_COUNT()` like
  `SELECT`.
- Lexer/parser/AST: owns syntax admission. The parser constructs an ordinary
  descriptor-backed wildcard `SELECT` AST for the supported top-level `TABLE`
  statement. This keeps the runtime boundary small while preventing `TABLE`
  from appearing in compound/select-subquery positions until those are
  specified.
- Analyzer/planner: existing `SELECT` planning owns selected-schema resolution,
  visible table resolution, temporary-table shadowing, descriptor-column
  projection, descriptor-column ordering, `LIMIT`, and unsupported-shape
  diagnostics.
- Catalog: descriptors remain authoritative. `TABLE` must not read SQLite
  schema text for logical metadata and must not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: existing `SELECT` result construction owns column names,
  metadata, row values, warning count, and result cleanup.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged. `TABLE` is read-only and must not write either region.
- SQLite physical storage: existing generated `SELECT` SQL reads stable
  physical table names and quoted physical columns from descriptors. No SQLite
  extension point or fork patch is needed.

## Supported SQL Grammar

Supported top-level subset:

```sql
TABLE table_name
TABLE table_name ORDER BY column_name [ASC | DESC] [, column_name [ASC | DESC] ...]
TABLE table_name LIMIT row_count
TABLE table_name LIMIT row_count OFFSET offset
TABLE table_name LIMIT offset, row_count
TABLE table_name ORDER BY column_name [ASC | DESC] [, column_name [ASC | DESC] ...] LIMIT ...
```

`table_name` may be unqualified or schema-qualified using the existing
identifier grammar. `column_name` is an unqualified or table-qualified
descriptor column name for runtime support in this phase.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar shape and is independently
authored for this project:

```lemon
statement(A) ::=
    TABLE(T) table_name(N) select_order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_table_statement(state, T, N, O, L);
}
```

The helper intentionally returns a `MYLITE_SQL_AST_SELECT_STATEMENT` whose
children are equivalent to:

```sql
SELECT * FROM table_name [ORDER BY ...] [LIMIT ...]
```

The statement-level production keeps `TABLE` out of nested `SELECT` grammar and
compound `UNION` grammar for this slice.

## Resolution and Semantics

Schema and table resolution are the same as current descriptor-backed
single-table `SELECT`:

1. An unqualified table requires a selected schema.
2. A schema-qualified table requires an existing schema descriptor unless the
   existing system-schema `SELECT` resolver handles that schema.
3. Reserved `_mylite_*` schema or table names are rejected before generated
   SQLite SQL is built.
4. Session temporary tables shadow persistent base tables of the same
   effective schema/table name.
5. Existing visible-table resolution rejects missing tables and unsupported
   object kinds deterministically.

Wildcard projection expands visible descriptor columns in descriptor ordinal
order. Invisible columns are omitted, matching MySQL `TABLE` behavior and
current MyLite `SELECT *` behavior.

`ORDER BY` uses the existing descriptor-backed `SELECT` planner. The current
supported runtime subset resolves unqualified and table-qualified descriptor
columns and selected aliases. Because the synthetic `TABLE` projection has no
aliases, practical support is descriptor columns only. `ASC` is the default.
`NULL` ordering and duplicate-key tie behavior are inherited from the current
`SELECT` implementation and documented query-expression compatibility. Tied
rows are not promised in any relative order unless the admitted key list makes
the ordering deterministic.

`LIMIT` uses existing `SELECT` limit planning and parameter binding. The
statement admits unsigned integer literals only. `LIMIT 0` returns an empty
result set. `LIMIT row_count OFFSET offset` and `LIMIT offset, row_count` are
accepted for MySQL parity. Signed and non-integer limit expressions remain
syntax errors through the shared grammar.

If no explicit `LIMIT` is present, `@@sql_select_limit` applies to the top-level
`TABLE` statement exactly as it applies to the supported top-level `SELECT`
path. An explicit `LIMIT` disables the implicit cap.

## Physical SQLite Handling

No new physical SQL generator is added. The generated SQL is the same shape as
the descriptor-backed `SELECT *` path:

- logical table names are resolved to stable physical table names, such as
  generated MyLite user-table names;
- every generated SQLite identifier is quoted by the existing SQL builder;
- `ORDER BY` references descriptor-owned physical columns;
- `LIMIT` and `OFFSET` values are bound as parameters rather than interpolated;
- user-visible literals and names are never trusted as physical SQLite names.

Because this feature is a read-only translation to the existing `SELECT` path,
it must not add indexes, constraints, triggers, cascades, auto-increment
behavior, or SQLite fork patches.

## Diagnostics

The implementation should preserve existing parser and runtime diagnostics:

- syntax errors for `TABLE ... WHERE`, aliases, locking clauses, modifiers,
  `INTO OUTFILE`, `TABLE` in compound/nested positions, signed limits,
  non-integer limits, and unsupported trailing clauses;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved `_mylite_*` schema/table names: existing MyLite reserved-name
  diagnostic before generated SQLite;
- unsupported object kind: existing visible-table diagnostic once such
  descriptors exist;
- unknown order column: `1054 / 42S22`;
- unsupported order column kind or order expression: existing
  descriptor-`ORDER BY` unsupported diagnostics;
- limit literal out of range: existing `SELECT` literal limit diagnostics;
- physical SQLite failures, allocation failures, and public API misuse:
  existing MyLite diagnostic conventions.

## Performance

The implementation stays on the existing optimized path for descriptor-backed
reads. It does not materialize rows in C in order to apply `ORDER BY` or
`LIMIT`; those clauses remain part of generated SQLite `SELECT` SQL. The only
additional cost is parser construction of a small synthetic wildcard `SELECT`
AST.

## Test Plan

Add MySQL expectation and C runtime coverage for:

- full-table `TABLE t` result rows and visible column labels;
- invisible columns omitted from `TABLE`;
- schema-qualified and unqualified table resolution;
- missing default schema, unknown schema, unknown table, and reserved-name
  diagnostics;
- temporary-table shadowing and cleanup revealing the persistent table again;
- `ORDER BY` default, `ASC`, `DESC`, multiple admitted unqualified or
  table-qualified order keys, nullable values, duplicate keys without
  overclaiming tie order, and unknown order columns;
- `LIMIT 0`, exact row count, row count larger than result set, `OFFSET`, and
  comma-offset form;
- signed limit literals and unsupported aliases/`WHERE`/compound use rejected
  deterministically;
- `@@sql_select_limit` capping and explicit `LIMIT` overriding the cap;
- `ROW_COUNT()`, warning count, and result-set shape after successful `TABLE`;
- persistence after close/reopen, table rename/drop interactions, file preamble
  safety, and independent handles;
- parser coverage for accepted and rejected syntax;
- continued pass of existing parser, select order/limit, union, temporary
  table, table lifecycle, file-format, and runtime execution tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-query-expressions.md` to
mark `TABLE` as a limited top-level row-returning statement. Do not claim
support for query-expression use, aliases, `WHERE`, `INTO OUTFILE`, full
ordering expressions, or `INSERT ... TABLE`.
