# Baseline Insert Select Lifecycle

## Status

This feature specifies a narrow descriptor-driven `INSERT ... SELECT` DML slice
for persistent base tables. It builds on `mylite_execute()`, statement context,
the MyLite SQL parser, file-backed `.mylite` opening, durable catalog
descriptors, baseline table lifecycle, integer and `NULL` row storage,
descriptor-backed `SELECT ... WHERE ... ORDER BY ... LIMIT`, and the current
`INSERT ... VALUES` / `INSERT ... SET` row-write paths.

This is not full MySQL `INSERT ... SELECT`. The slice admits one target table,
one descriptor-backed source `SELECT`, existing integer-family and `NULL`
storage, optional target column lists, and the existing source `SELECT`
predicate/order/limit subset.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `INSERT INTO dst(cols) SELECT ...` and `INSERT dst(cols) SELECT ...` both
  succeed.
- Successful statements return no result rows, set `ROW_COUNT()` to the number
  of inserted rows, and leave `@@warning_count == 0`.
- A source `SELECT` returning zero rows inserts zero rows and succeeds with
  `ROW_COUNT() == 0`.
- If the source `SELECT` returns zero rows, omitted target columns with no
  explicit default do not raise an error.
- If the source `SELECT` returns at least one row, omitted `NOT NULL`
  no-default target columns fail with error `1364`, SQLSTATE `HY000`.
- Assigning selected `NULL` into a `NOT NULL` target column fails with error
  `1048`, SQLSTATE `23000`.
- Selected integer values outside the target column's range fail in strict mode
  with error `1264`, SQLSTATE `22003`; the row number in the message is the
  selected row position after filtering, ordering, and limiting.
- Target column count and selected column count mismatches fail with error
  `1136`, SQLSTATE `21S01`, message `Column count doesn't match value count at
  row 1`.
- Duplicate target columns fail with error `1110`, SQLSTATE `42000`.
- Unknown target and source columns fail with error `1054`, SQLSTATE `42S22`,
  and `field list` context for this slice's supported projection forms.
- Target resolution happens before source resolution for observed missing
  schema/table combinations. If both explicit schemas are unknown, MySQL
  reports the target schema. If both target and source tables are unknown in an
  existing schema, MySQL reports the target table.
- Unqualified targets without a selected default schema fail with error `1046`,
  SQLSTATE `3D000`, before source resolution.
- Schema-qualified targets and sources work without a selected default schema.
- `SELECT *` omits invisible source columns. An omitted target column list maps
  to target visible columns, matching the existing MyLite implicit insert
  policy. Explicit target and source column references may name invisible
  columns.
- MySQL supports much wider forms such as modifiers, partitions, `IGNORE`,
  `ON DUPLICATE KEY UPDATE`, `TABLE`, row constructors, expressions, joins, and
  literal projections. They are out of scope for this baseline.

## Scope

The implementation must add:

- parser and AST support for limited `INSERT [INTO] table_name
  insert_column_list_opt SELECT ...`;
- optional `INTO`;
- one persistent base-table target and one persistent base-table source;
- target resolution for unqualified and schema-qualified names using the
  existing selected/default schema policy;
- source `SELECT` limited to the existing descriptor-backed table subset,
  including optional table alias, `WHERE`, one-column `ORDER BY`, and `LIMIT`;
- target column list handling using unqualified descriptor column names;
- omitted target column handling with descriptor integer defaults or effective
  nullable `NULL` defaults, applied only to rows actually inserted;
- no-column-list target mapping to visible descriptor target columns only;
- source `SELECT *` expansion to visible descriptor source columns only;
- explicit source column references, including currently supported qualified
  source-column forms and invisible columns;
- target/source column-count validation before mutation;
- MyLite-owned validation for selected `NULL` into `NOT NULL` target columns;
- MyLite-owned integer range validation for each selected source value against
  the target descriptor before physical insertion;
- generated SQLite physical statements built only from descriptors and stable
  physical table names;
- prepared-statement parameter binding for source predicates, limits, and
  inserted descriptor default integer values;
- SQLite-side scan/filter/sort/limit execution into an internal temporary
  table, followed by MyLite streaming validation and SQLite-side insertion from
  that same temporary table, without buffering the selected row set in C memory;
- affected-row and warning-count behavior matching MySQL for supported
  in-range statements;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `INSERT ... TABLE`, `INSERT ... VALUES ROW(...)`, `INSERT ... VALUES` row
  aliases, `INSERT ... SET` changes, or `CREATE TABLE ... SELECT`;
- `LOW_PRIORITY`, `HIGH_PRIORITY`, `DELAYED`, `IGNORE`, `PARTITION`,
  `ON DUPLICATE KEY UPDATE`, `RETURNING`, CTEs, query expression parentheses,
  unions, intersections, excepts, joins, grouping, `HAVING`, windows, locking
  clauses, or arbitrary SQLite pass-through;
- target aliases, table-qualified target column-list names, duplicate target
  tables, user-visible temporary tables, views, or unsupported object kinds;
- source literal projection, `FROM DUAL` source projection, expression
  projection, arithmetic, functions, variables, parameters, subqueries,
  string/decimal/float/hex/bit/date/time/json selected values, or general
  expression evaluation;
- DML `DEFAULT` keyword values in the source `SELECT` or target list;
- primary/unique/foreign keys, duplicate-key handling, auto-increment,
  `LAST_INSERT_ID()` changes, generated columns, check constraints, triggers,
  cascades, privileges, warning demotion, non-strict SQL modes, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()` state, and successful non-row result finalization.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table, target columns, source
  table, source projection, predicate, order key, and limit against MyLite
  catalog descriptors; rejects unsupported shapes; and builds a
  descriptor-driven physical plan.
- MyLite runtime owns conversion and validation semantics for target
  nullability and integer range. Validation is streaming over an internal
  SQLite temporary table that materializes the selected source values once, so
  validation and insertion consume the same selected row set even when `LIMIT`
  is unordered or `ORDER BY` has ties. MyLite must not copy the full selected
  row set into a C-side buffer.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. `INSERT ... SELECT`
  must not mutate catalog rows, descriptor versions, catalog generation, or
  `sqlite_schema_generation`.
- SQLite owns physical b-tree row storage, source scans, filtering, sorting,
  limiting, internal temporary storage, and the final physical insert. SQLite
  schema text and PRAGMA output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Row writes occur only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
INSERT [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item_list
FROM table_name [AS] alias
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]
```

The source `SELECT` subset is exactly the descriptor-backed single-table
subset currently implemented for ordinary `SELECT`, except that no-source
literal projection and `FROM DUAL` projection are intentionally deferred.

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= insert_select_statement(B). {
    A = B;
}

insert_select_statement(A) ::=
    INSERT(I) INTO table_name(T) insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S);
}
insert_select_statement(A) ::=
    INSERT(I) table_name(T) insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S);
}
```

The `select_statement` nonterminal is the existing MyLite descriptor-backed
`SELECT` grammar. Runtime planning rejects source `SELECT` shapes outside the
supported table-backed subset with deterministic unsupported diagnostics.

## Schema, Table, And Object Resolution

Target resolution follows existing insert behavior and MySQL precedence:

- unqualified targets require a selected default schema;
- schema-qualified targets resolve without a selected schema;
- unknown target schemas fail with error `1049`, SQLSTATE `42000`;
- unknown target tables fail with error `1146`, SQLSTATE `42S02`;
- reserved `_mylite_*` target schemas or table names are rejected before any
  generated SQLite SQL;
- only persistent base-table target descriptors are supported.

Source resolution happens after the target descriptor and target column list
are valid. Source resolution uses the existing descriptor-backed `SELECT`
policy:

- unqualified sources require a selected default schema;
- schema-qualified sources resolve without a selected schema;
- unknown source schemas fail with error `1049`, SQLSTATE `42000`;
- unknown source tables fail with error `1146`, SQLSTATE `42S02`;
- reserved `_mylite_*` source schemas or table names are rejected before any
  generated SQLite SQL;
- only persistent base-table source descriptors are supported.

If both target and source names are invalid in the supported subset, MyLite
reports the target error first to match observed MySQL 8.4.9 behavior.

## Column Mapping

If a target column list is present, each name resolves case-insensitively
against target descriptors. Only unqualified target column names are supported.
Unknown target columns fail with `1054 (42S22)` in `field list`; duplicates
fail with `1110 (42000)`.

If no target column list is present, the target list is the target table's
visible descriptor columns in ordinal order. Invisible target columns are
omitted unless explicitly named.

The source `SELECT` result column count must equal the target list count.
Mismatch fails before mutation with `1136 (21S01)` and row `1` in the message.
`SELECT *` expands to visible source descriptor columns. Explicit source
projection may name invisible columns through existing descriptor column
resolution.

For physical insertion, MyLite builds a complete target physical column list in
descriptor ordinal order:

- target-list columns receive the corresponding selected source expression;
- omitted columns with descriptor integer defaults receive bound default
  integer values;
- omitted nullable columns with no explicit default receive `NULL`;
- omitted `NOT NULL` no-default columns are validated only if the source
  `SELECT` matches at least one row. If it matches zero rows, the statement is
  a successful zero-row insert.

## Value Conversion And Validation

Selected source values in this slice are SQLite `INTEGER` or `NULL` values from
MyLite-owned descriptor columns. MyLite validates them against target
descriptors before the physical insert:

- `NULL` into a nullable target is allowed;
- `NULL` into a `NOT NULL` target fails with `1048 (23000)`;
- signed integer target ranges follow the existing row-values conversion
  boundaries for `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, and `BIGINT`;
- unsigned integer target ranges follow the existing row-values boundaries for
  unsigned integer descriptors, within MyLite's current signed-64 physical
  storage limit for `BIGINT UNSIGNED`;
- the first invalid selected row determines the row number reported in range
  diagnostics, after source filtering, ordering, and limiting.

No string, decimal, float, hex, bit, temporal, JSON, expression, parameter, or
function selected values are admitted in this feature.

## Generated SQLite Handling

The implementation uses public SQLite prepared statements only. No SQLite fork
patch is required.

Planning creates:

- a `CREATE TEMP TABLE <internal_temp> AS SELECT ...` statement over the source
  physical table, using the same `WHERE`, `ORDER BY`, and `LIMIT` as the
  source and descriptor-generated aliases for selected values;
- a validation `SELECT` over the internal temporary table for selected target
  values;
- a final `INSERT INTO <target_physical>(all descriptor columns) SELECT ...`
  statement over the same internal temporary table, whose selected expressions
  are temporary value columns, bound integer default values, or `NULL`;
- a `DROP TABLE IF EXISTS temp.<internal_temp>` cleanup statement.

The internal temporary table name is generated from MyLite's reserved internal
namespace and is never exposed through catalog descriptors. Every SQLite
identifier is quoted. Predicate values, limits, and descriptor integer defaults
are bound parameters. User SQL literals are never interpolated into generated
SQLite SQL.

The statement executes inside a transaction. If validation or physical
insertion fails, the transaction rolls back and no target rows remain from the
failed statement. The `.mylite` preamble and shifted SQLite payload invariants
are preserved by the existing VFS and SQLite transaction machinery.

## Result Semantics

Successful statements return the existing non-row statement result shape:

- zero result columns;
- zero result rows;
- `affected_rows == inserted row count`;
- `warning_count == 0` for supported in-range statements.

The following `ROW_COUNT()` returns the inserted row count. A source `SELECT`
that matches zero rows returns `affected_rows == 0` and produces no warnings.

## Diagnostics

The implementation must preserve or add deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default target or source schema;
- unknown target or source schema;
- unknown target or source table;
- reserved target or source schema/table names;
- unsupported target or source object kind once non-base descriptors exist;
- unknown, duplicate, or qualified target columns;
- unknown source projection, predicate, or ordering columns;
- source `SELECT` shapes outside the supported descriptor-backed table subset;
- target/source column-count mismatch;
- omitted `NOT NULL` no-default target columns when the source matches rows;
- `NULL` into `NOT NULL`;
- integer out-of-range selected values;
- unsupported modifiers, partitions, `IGNORE`, `ON DUPLICATE KEY UPDATE`,
  `TABLE`, row constructors, aliases, joins, grouping, literal projections,
  expressions, parameters, strings, decimals, floats, hex, bit, and functions;
- physical SQLite failures;
- allocation failures;
- public API misuse if the public surface changes.

## Tests

Add MySQL-runtime-verified expectations and C tests covering:

- `INSERT INTO dst(cols) SELECT ...` and `INSERT dst(cols) SELECT ...`;
- schema-qualified and unqualified target/source resolution, including missing
  default schema, unknown schema, unknown table, and target-before-source
  diagnostic precedence;
- successful full-row and column-list inserts over `INT`, `INTEGER`, `BIGINT`,
  and unsigned integer descriptors within supported physical range;
- source `WHERE`, `ORDER BY`, and `LIMIT` reuse, including `LIMIT 0`;
- source `SELECT *` visible-column expansion and explicit invisible source and
  target columns;
- omitted nullable and integer-default target columns;
- omitted `NOT NULL` no-default target columns with both zero-row and
  matching-row sources;
- selected `NULL` into nullable and `NOT NULL` targets;
- signed and unsigned target range boundaries and out-of-range diagnostics;
- target/source column-count mismatch;
- duplicate and unknown target columns; unknown source projection, predicate,
  and ordering columns;
- affected rows, warning count, `ROW_COUNT()`, absence of result rows, and
  remaining rows after success and failure;
- self-insert from the same persistent base table for the admitted subset;
- reopen persistence, target/source after table rename, source/target after
  drop, independent file-backed handles, and preamble preservation;
- unsupported modifiers and query shapes listed in non-goals;
- zero-initialized cleanup for new statement/planner objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, storage/VFS, catalog, row values, SELECT, DELETE, UPDATE,
  REPLACE, CREATE LIKE, and ALTER lifecycle tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` to describe
only this limited descriptor-backed `INSERT ... SELECT` subset. Update
`docs/compatibility/sql-query-expressions.md` only if source `SELECT` support
changes beyond reusing the already documented subset. Do not claim joins,
literal projection inserts, CTAS, keys, duplicate handling, ignored inserts,
warning demotion, auto-increment, generated columns, triggers, privileges, or
general expression evaluation.
