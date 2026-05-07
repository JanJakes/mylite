# Baseline EXPLAIN Table Introspection

## Status

This feature specifies a narrow table-column introspection slice for
`EXPLAIN table_name`. It builds on `mylite_execute()`, statement context, the
MyLite parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, schema/table lifecycle, descriptor-driven row storage, and the
baseline `SHOW COLUMNS`/`DESCRIBE` introspection path.

This is intentionally not execution-plan support. The supported `EXPLAIN`
form is only the table-structure synonym that MySQL exposes alongside
`DESCRIBE`. Statement-plan `EXPLAIN`, `EXPLAIN ANALYZE`, `FORMAT`, `FOR
CONNECTION`, column filters, wildcard filters, and optimizer metadata remain
outside this slice.

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
- File-backed MyLite opening and VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline schema, table, row, query, write, count, and introspection slices:
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-select-where-lifecycle/specs.md`,
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-count-aggregate/specs.md`, and
  `docs/specs/baseline-show-columns-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `EXPLAIN`:
  https://dev.mysql.com/doc/refman/8.4/en/explain.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `EXPLAIN db.t` and `EXPLAIN t` with a selected default schema return the
  same table-column result shape as `DESCRIBE t` for this slice's table
  shapes.
- The standard table-structure result columns are exactly `Field`, `Type`,
  `Null`, `Key`, `Default`, and `Extra`.
- For `INT`, `INTEGER`, `INT UNSIGNED`, `BIGINT`, and `BIGINT UNSIGNED`, MySQL
  reports type strings `int`, `int unsigned`, `bigint`, and
  `bigint unsigned`.
- `Null` is `YES` for nullable columns and `NO` for `NOT NULL` columns.
- With no indexes or explicit defaults, `Key` and `Extra` are empty strings and
  `Default` is SQL `NULL`, including for `NOT NULL` integer columns that have
  no explicit default.
- Successful table-form `EXPLAIN` returns a result set, leaves warning count
  `0`, and makes the following `ROW_COUNT()` return `-1`.
- Unqualified `EXPLAIN t` fails with error `1046`, SQLSTATE `3D000`, when no
  default schema is selected.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown tables in a known schema fail with error `1146`, SQLSTATE `42S02`,
  and the message includes the resolved `schema.table` name.
- Unlike `SHOW COLUMNS`, `EXPLAIN db.t FROM other_db` and
  `EXPLAIN db.t IN other_db` are syntax errors (`1064`, SQLSTATE `42000`).
- MySQL accepts broader forms including `EXPLAIN t col_name`,
  `EXPLAIN t wild_pattern`, `EXPLAIN SELECT ...`,
  `EXPLAIN FORMAT=JSON SELECT ...`, and `EXPLAIN ANALYZE SELECT ...`; those
  forms remain outside this slice.

## Scope

The implementation must add:

- parser support for `EXPLAIN table_name`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- descriptor-driven result construction from the MyLite catalog column
  descriptors for one persistent base table;
- the same six-column table-structure result shape already used by the
  baseline `SHOW COLUMNS` path;
- deterministic diagnostics for unsupported `EXPLAIN` syntax and unresolved
  names;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `EXPLAIN table column_name`, wildcard/pattern filters, or table-form output
  narrowing;
- `EXPLAIN SELECT`, `EXPLAIN DELETE`, `EXPLAIN INSERT`, `EXPLAIN REPLACE`,
  `EXPLAIN UPDATE`, `EXPLAIN TABLE`, statement execution plans, optimizer
  metadata, `FORMAT`, `INTO`, `FOR SCHEMA`, `FOR DATABASE`, `FOR CONNECTION`,
  `EXPLAIN ANALYZE`, `EXTENDED`, or `PARTITIONS`;
- `SHOW COLUMNS` functionality beyond the already supported baseline subset;
- temporary tables, views, indexes, primary keys, unique keys, foreign keys,
  triggers, generated columns, explicit defaults, auto-increment metadata,
  privileges, SQL modes, arbitrary SQLite metadata reads, arbitrary SQLite SQL
  pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful table-form `EXPLAIN` is a result-set statement and
  therefore stores `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target schema/table and rejects
  unsupported object kinds using MyLite descriptors.
- The catalog module remains authoritative for schema/table/column descriptors.
  This slice reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite catalog descriptors.
  It does not query SQLite table metadata, `sqlite_schema`, or pragma output.
- The result builder owns the six text columns and SQL `NULL` default values.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Introspection reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
EXPLAIN table_name
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

MyLite Lemon-syntax grammar snippets:

```lemon
statement ::= explain_table_statement.

explain_table_statement ::= EXPLAIN table_name.
```

The parser may represent the supported table-form `EXPLAIN` as the same
internal table-column introspection AST node used by `SHOW COLUMNS`,
`SHOW FIELDS`, `DESCRIBE`, and `DESC`, because the supported runtime behavior
and result shape are identical. This is an internal parser/runtime choice and
does not add public ABI.

`EXPLAIN` is a statement keyword in this slice. The lexer already treats it as
a reserved keyword; parser keyword mapping must route it into the Lemon grammar
only when it appears in admitted syntax. Unsupported optimizer-plan forms are
rejected at parse time for now.

## Schema and Table Resolution

Unqualified table names resolve through the current selected/default schema.
If no schema is selected, MyLite reports `1046` / `3D000` (`No database
selected`).

Schema-qualified table names resolve the named schema directly. If the schema
does not exist, MyLite reports `1049` / `42000` (`Unknown database`).

The table must exist as a MyLite catalog descriptor in the resolved schema. If
it does not, MyLite reports `1146` / `42S02` with the resolved `schema.table`
name.

Reserved MyLite-owned `_mylite_*` schema or table names are rejected before
descriptor lookup and before any SQLite SQL can be generated. Current
descriptor name matching follows the existing catalog lifecycle policy for
identifier spelling and case sensitivity; this slice does not add collations or
case-folded lookup.

If future catalog descriptors add views, temporary tables, virtual tables, or
other object kinds, this slice must reject them with a deterministic
unsupported-object diagnostic until their MySQL table-structure output is
specified and tested.

## Descriptor Result Mapping

The result set has six columns, in order:

| Column | Value |
| --- | --- |
| `Field` | Descriptor column name |
| `Type` | MySQL display type for the currently supported integer descriptor |
| `Null` | `YES` for nullable descriptors, `NO` for `NOT NULL` descriptors |
| `Key` | Empty string for this slice |
| `Default` | SQL `NULL` for this slice |
| `Extra` | Empty string for this slice |

Current type mapping:

| Descriptor family | Display type |
| --- | --- |
| `INT` | `int` |
| `INTEGER` | `int` |
| `INT UNSIGNED` | `int unsigned` |
| `BIGINT` | `bigint` |
| `BIGINT UNSIGNED` | `bigint unsigned` |

Rows are emitted in descriptor ordinal order. The implementation must not use
SQLite metadata, physical column names, physical SQLite schema SQL, or
`INFORMATION_SCHEMA` emulation to derive this output.

## Result and Statement State

On success:

- the statement returns a result set through the existing public result API;
- result column names and row values match the descriptor mapping above;
- `warning_count` is `0`;
- `affected_rows` remains `0` by existing result conventions for result-set
  statements;
- the connection-local previous row-count state is result-set state, so a
  following `SELECT ROW_COUNT()` returns `-1`;
- no catalog or physical row state is changed.

## Diagnostics

Diagnostics must be deterministic for:

- syntax errors and unsupported `EXPLAIN` grammar;
- missing default schema for unqualified table names;
- unknown explicit schema;
- reserved `_mylite_*` schema or table target names;
- unknown table in a known schema;
- unsupported object kind once non-base-table descriptors exist;
- unsupported descriptor column type once wider descriptors exist;
- result allocation failure;
- catalog read failure;
- physical SQLite failure while reading catalog state;
- public API misuse if an existing public validation path is exercised.

MyLite-specific unsupported diagnostics are acceptable for syntax that MySQL
accepts but this slice deliberately defers, provided tests lock the behavior
and compatibility docs do not overclaim support.

## Physical SQLite Handling

No user-table SQLite SQL is generated for this feature. The implementation
uses MyLite catalog APIs to locate the logical table descriptor and iterate
logical column descriptors. It must not query `sqlite_schema`, use SQLite
pragma output, inspect physical table SQL, or depend on SQLite optimizer-plan
metadata.

The `.mylite` file preamble and shifted SQLite payload invariants are
unchanged. The feature must not require SQLite fork patches or new SQLite
extension points.

## Tests

Add tests covering:

- `EXPLAIN table_name` and `EXPLAIN schema.table_name`;
- descriptor output for `INT`, `INTEGER`, `INT UNSIGNED`, `BIGINT`, and
  `BIGINT UNSIGNED`, including nullable and `NOT NULL` columns;
- warning count `0`, no affected-row result semantics, no result mutation, and
  following `ROW_COUNT() = -1`;
- unqualified target resolution with a selected default schema;
- schema-qualified target resolution without a selected default schema;
- missing default schema, unknown schema, unknown table, and reserved target
  diagnostics;
- unsupported syntax for trailing schema clauses, column filters, wildcard
  filters, statement-plan forms, `FORMAT`, `ANALYZE`, `FOR CONNECTION`, and
  other query modifiers;
- reopen persistence, table rename/drop behavior, independent handles, and
  preamble preservation;
- existing parser, runtime lifecycle, catalog, storage, VFS, statement-context,
  result, and compatibility tests.

The MySQL expectation artifact must verify both supported table-form behavior
and MySQL-accepted-but-deferred wider forms against MySQL 8.4.9. A missing
MySQL 8.4.9 runtime is a blocker for changing this user-visible surface.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/sql-utility-statements.md` to mark only table-form
`EXPLAIN table_name` as partially supported. Do not overclaim execution-plan
support, formats, analysis, filters, privileges, defaults, indexes, views, or
optimizer metadata.
