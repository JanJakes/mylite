# Baseline Insert Select Dual Source

## Status

This feature expands the current descriptor-driven `INSERT ... SELECT` baseline
with a narrow row-scalar source path: `INSERT [INTO] target [(columns)] SELECT
...` with no `FROM`, or with `FROM DUAL`, and optional `WHERE [NOT] EXISTS
(subquery)` when `FROM DUAL` is present. The source produces either zero or one
row. The target write then reuses the existing MyLite `INSERT` row path so
primary keys, unique indexes, foreign keys, auto-increment, defaults, and
conversion stay descriptor-owned.

This remains a limited compatibility slice. It does not implement full
`INSERT ... SELECT`, general expression sources, joins, `TABLE`, `IGNORE`,
`ON DUPLICATE KEY UPDATE`, or `REPLACE ... SELECT` scalar sources.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline insert select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline row-scalar expression and scalar subquery coverage in current
  runtime/parser tests
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_select_dual_source_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT INTO dst(cols) SELECT scalar_items FROM DUAL` and
  `INSERT INTO dst(cols) SELECT scalar_items` both succeed when the selected
  values are valid for the target descriptors.
- Successful one-row inserts return no result set, set `ROW_COUNT()` to `1`,
  and leave `@@warning_count == 0`.
- `FROM DUAL WHERE EXISTS (...)` inserts one row when the predicate is true and
  zero rows when it is false.
- `FROM DUAL WHERE NOT EXISTS (...)` inverts the row decision.
- A zero-row source succeeds with `ROW_COUNT() == 0` and does not check omitted
  target columns with no explicit default.
- If the source produces a row, omitted `NOT NULL` no-default target columns
  fail with `1364 / HY000`.
- Selected `NULL` into a `NOT NULL` target column fails with `1048 / 23000`.
- Target/selected column count mismatch fails with `1136 / 21S01`.
- `SELECT * FROM DUAL` used as the source fails with `1096 / HY000`.
- Key-bearing targets use normal insert behavior. Supported auto-increment
  targets generate ids for omitted, explicit `NULL`, and explicit `0` values
  and set `LAST_INSERT_ID()` for generated rows. Duplicate unique-key values
  fail with `1062 / 23000`.
- `DEFAULT` is not a valid `SELECT` projection item in this shape and fails
  with `1064 / 42000`.
- Target resolution precedes source-subquery resolution. Unknown target schemas
  fail before unknown source references.
- Schema-qualified targets work without a selected default schema.

## Scope

The implementation must add:

- parser support for `SELECT ... FROM DUAL WHERE predicate` in the existing
  `select_statement` AST shape;
- `INSERT [INTO] target [(columns)] SELECT select_item_list` with no source, or
  `SELECT select_item_list FROM DUAL [WHERE exists_predicate]`, as a
  row-scalar `INSERT ... SELECT` source;
- optional row filtering for `FROM DUAL` only when the predicate is exactly
  `EXISTS (select_statement)`, `NOT EXISTS (select_statement)`, or parenthesized
  versions of those forms;
- uncorrelated inner `EXISTS` subqueries using the existing MyLite `EXISTS`
  subquery subset: no-source/`DUAL` literal selects, or one descriptor-backed
  table source with optional supported inner `WHERE` and `LIMIT`;
- source projection using the existing row-scalar select-item planner, including
  the currently supported scalar literals, session scalar values, functions,
  and scalar expressions;
- target resolution, target column-list mapping, invisible-column handling,
  omitted target defaults, target conversion, duplicate-key checks,
  auto-increment generation, foreign-key checks, and final physical insertion
  through the existing descriptor-owned insert path;
- affected-row behavior matching MySQL for one-row and zero-row sources;
- zero warnings for supported in-range statements;
- MySQL-runtime expectation coverage and focused C runtime/parser tests.

## Non-Goals

This feature must not implement:

- `INSERT IGNORE ... SELECT`, `ON DUPLICATE KEY UPDATE`, `PARTITION`,
  `RETURNING`, `TABLE`, row constructors, CTEs, joins, grouping, `HAVING`,
  unions, parenthesized query expressions, or arbitrary SQLite pass-through;
- `REPLACE ... SELECT` scalar/no-source/`DUAL` sources;
- source `SELECT *` from `DUAL`;
- `FROM DUAL WHERE` predicates other than the limited `[NOT] EXISTS` subset;
- no-source `WHERE` syntax;
- source `ORDER BY`, `LIMIT`, or locking clauses for tableless/`DUAL`
  row-scalar insert sources;
- correlated inner `EXISTS` predicates for this tableless/`DUAL` insert source;
- general expression conversion beyond values the existing row-scalar planner
  can materialize and the target descriptor validators already admit;
- warning demotion, non-strict conversion, triggers, cascades beyond current
  supported foreign-key checks/actions, generated columns, privileges, or
  protocol metadata changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` still owns public call
  validation, dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()` state, and successful non-row result finalization.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code decides whether an `INSERT ... SELECT` source is the
  existing descriptor-backed table source or this new row-scalar source. It
  resolves the target first to preserve observed MySQL diagnostic ordering.
- The row-scalar select planner owns supported projection expression planning
  and the optional tableless `EXISTS` row filter.
- The existing insert planner/runtime owns target descriptor conversion,
  defaults, auto-increment, primary/unique key checks, foreign-key checks,
  SQLite physical row insertion, affected rows, and last-insert-id state.
- The catalog module remains the metadata authority. This feature must not
  mutate catalog rows except for normal auto-increment counter advancement when
  a generated row is inserted.
- SQLite owns execution of the generated row-scalar source statement and the
  final physical row insert. MyLite builds SQL from descriptors, quoted
  physical names, and bound parameters; SQLite schema text is not metadata
  authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only through ordinary physical row storage.

## Supported SQL Grammar

Supported subset:

```sql
INSERT [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item[, select_item ...]

INSERT [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item[, select_item ...]
FROM DUAL
[WHERE exists_predicate]
```

`exists_predicate` is limited to:

```sql
EXISTS (select_statement)
NOT EXISTS (select_statement)
(exists_predicate)
```

The inner `select_statement` is the existing MyLite `EXISTS` subquery subset.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
    FROM(F) DUAL(D) where_clause_opt(W) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D),
        W, NULL, NULL, NULL, NULL, K);
}
```

Runtime planning rejects row-scalar insert sources with unsupported optional
clauses, wildcard projection, unsupported predicates, or unsupported row-scalar
expressions using deterministic MyLite diagnostics unless MySQL-compatible
diagnostics are already implemented for that path.

## Semantics

Target resolution, target column resolution, reserved `_mylite_*` rejection,
unknown schema/table diagnostics, and `information_schema` write access
diagnostics follow the existing insert target path.

For tableless or `FROM DUAL` insert sources:

- the source can produce at most one row;
- a missing row means the insert succeeds without checking omitted no-default
  target columns and reports `affected_rows == 0`;
- a present row is validated and inserted exactly as a one-row `INSERT`;
- omitted target columns are materialized only when a source row exists;
- explicit target column lists may name invisible columns, while omitted target
  lists use the visible descriptor columns;
- `SELECT * FROM DUAL` is rejected with MySQL-compatible `1096 / HY000` where
  this feature observes that MySQL reports `No tables used`;
- successful supported statements return no row result set and report
  `warning_count == 0`;
- generated auto-increment values advance the descriptor counter and update
  `LAST_INSERT_ID()` through the existing insert path.

`FROM DUAL WHERE [NOT] EXISTS (...)` evaluates the `EXISTS` predicate before
materializing an insert row. Inner subqueries are planned through the existing
descriptor-owned `EXISTS` machinery. This feature admits only uncorrelated
inner predicates because there is no outer source row to correlate against.

## SQLite Handling

For a row-scalar source, MyLite generates a standard SQLite `SELECT` with bound
parameters for every planned source value and predicate value. With a true
`EXISTS` filter the shape is:

```sql
SELECT ?, ...
WHERE EXISTS (SELECT 1 FROM "physical_inner" AS "_mylite_s1" WHERE ...)
```

For `NOT EXISTS`, the predicate is wrapped in `NOT`. For a no-source or
unfiltered `DUAL` source, no `FROM` clause is emitted. `DUAL` itself is not
materialized as a SQLite table.

When the source returns a row, MyLite materializes just that one row into a
single `planned_insert_row` and calls the existing insert executor. The existing
descriptor-backed `INSERT ... SELECT FROM source_table` path keeps its
SQLite-side temporary table streaming plan unchanged.

No SQLite fork patch is required for this feature.

## Diagnostics

Diagnostics must cover:

- syntax errors and unsupported optional clauses;
- unsupported wildcard `DUAL` sources;
- missing default schema, unknown schema, unknown target table, and reserved
  target names through existing target resolution;
- `information_schema` write targets through existing access checks;
- unknown inner `EXISTS` schemas, tables, or columns through existing
  descriptor subquery planning;
- unsupported row-scalar source expressions through existing row-scalar
  diagnostics;
- target/source column-count mismatch with `1136 / 21S01`;
- duplicate target columns with existing insert diagnostics;
- omitted `NOT NULL` no-default target columns when a row exists with
  `1364 / HY000`;
- selected `NULL` into `NOT NULL` with `1048 / 23000`;
- target conversion, range, length, temporal, binary, `BIT`, `ENUM`, `SET`,
  JSON, unique-key, foreign-key, and auto-increment diagnostics through the
  existing insert path;
- physical SQLite failures, allocation failures, and public API misuse through
  existing runtime policies.

## Test Plan

- MySQL expectation script for one-row, zero-row, no-source, `FROM DUAL`,
  `EXISTS`, `NOT EXISTS`, omitted columns, `NULL` into `NOT NULL`, count
  mismatch, wildcard `DUAL`, auto-increment, duplicate keys, schema-qualified
  targets, target-before-source diagnostic order, unknown inner tables, and
  unknown inner columns.
- Parser tests for `FROM DUAL WHERE EXISTS (...)` and `WHERE NOT EXISTS (...)`
  inside `INSERT ... SELECT`.
- Runtime tests for:
  - successful row-scalar insert into integer and string targets;
  - no-source insert;
  - `WHERE EXISTS` and `WHERE NOT EXISTS` one-row/zero-row behavior;
  - zero-row source skipping omitted no-default validation;
  - auto-increment and unique-key target behavior;
  - schema-qualified target without a selected schema;
  - `NULL` into `NOT NULL`, omitted no-default, count mismatch, wildcard
    `DUAL`, unknown source table, and unknown source column diagnostics;
  - close/reopen persistence and `.mylite` preamble preservation;
  - no catalog/schema-generation mutation except auto-increment descriptor
    advancement after generated rows.
