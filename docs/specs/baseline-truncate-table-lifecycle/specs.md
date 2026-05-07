# Baseline Truncate Table Lifecycle

## Status

This feature specifies the next narrow table lifecycle slice for file-backed
`.mylite` handles. It adds descriptor-driven `TRUNCATE [TABLE] table_name`
execution on top of `mylite_execute()`, statement context, the MyLite parser
scaffold, shifted `.mylite` storage, durable catalog descriptors,
create/drop/rename table lifecycle, row-value storage, descriptor-driven
`SELECT`, and descriptor-driven DML.

The feature is intentionally not full MySQL `TRUNCATE TABLE` support. It
supports only persistent base tables described by the MyLite catalog. It does
not implement implicit commit behavior, table locks, foreign keys, triggers,
partitions, auto-increment reset semantics, corruption repair, or privilege
checks.

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
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline delete lifecycle:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `TRUNCATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/truncate-table.html
- MySQL 8.4 Reference Manual, statements that cause implicit commits:
  https://dev.mysql.com/doc/refman/8.4/en/implicit-commit.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited `TRUNCATE [TABLE] table_name` statement;
- unqualified and schema-qualified target table resolution using MyLite catalog
  descriptors;
- one persistent base-table target only;
- reserved `_mylite_*` schema and table name rejection before any generated
  SQLite SQL;
- descriptor-driven physical table lookup and stable physical table names;
- generated SQLite physical row removal that does not depend on SQLite schema
  text or SQLite metadata;
- result-handle behavior matching MySQL's visible baseline status for
  truncation: no row result set, `affected_rows == 0`, and `warning_count == 0`
  for supported successful truncates;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- `TRUNCATE` for temporary tables, views, partitions, Performance Schema summary
  tables, system schemas, or any non-base-table descriptor;
- `IF EXISTS`, multiple tables, aliases, `WHERE`, `ORDER BY`, `LIMIT`, CTEs,
  subqueries, query modifiers, or arbitrary SQLite SQL pass-through;
- implicit commit boundaries, user transactions, table locks, binary logging,
  replication, handler closing, privilege checks, or corruption repair;
- triggers, cascades, foreign keys, generated columns, defaults, indexes,
  constraints, or auto-increment reset semantics;
- descriptor version churn, catalog generation changes, descriptor cache
  invalidation, or `sqlite_schema_generation` changes for successful truncates;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, backend status, and transaction completion. Successful
  truncates produce an empty non-row result with `affected_rows == 0`.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target schema and table against MyLite
  catalog descriptors, rejects unsupported shapes, and builds a descriptor
  physical truncate plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. This slice does not
  mutate catalog rows or descriptor identity.
- The result builder owns the empty result returned for supported truncates.
- SQLite owns physical b-tree row storage for generated internal SQL. SQLite
  schema text and `PRAGMA` output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Truncates occur only inside the shifted SQLite payload and must not touch byte
  range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
TRUNCATE table_name
TRUNCATE TABLE table_name
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= truncate_table_statement.

truncate_table_statement ::= TRUNCATE table_name.
truncate_table_statement ::= TRUNCATE TABLE table_name.
```

The parser must reject unsupported `TRUNCATE` forms as syntax errors whenever
the unsupported form cannot be represented by this grammar. That includes `IF
EXISTS`, multiple table names, `WHERE`, `ORDER BY`, `LIMIT`, aliases,
`PARTITION`, and `TEMPORARY`.

## Schema And Table Resolution

Unqualified target names use the selected schema. If no schema is selected,
MyLite returns `No database selected`, matching MySQL 8.4.9.

Schema-qualified target names use the explicit schema name and do not require a
selected schema. MySQL 8.4.9 reports `Table 'schema.table' doesn't exist` for
`TRUNCATE schema.table` when the schema is absent. This slice must match that
visible diagnostic instead of reusing `CREATE TABLE`'s unknown-schema
diagnostic.

The target table descriptor is resolved by logical schema id and logical table
name. The current catalog comparison policy remains case-insensitive for
duplicate detection and name lookup, consistent with the earlier table
lifecycle slices. If the descriptor is missing, MyLite reports `Table
'schema.table' doesn't exist`.

Reserved `_mylite_*` schema or table names are MyLite-owned internals. They are
rejected before SQLite SQL generation with a deterministic MyLite diagnostic.

Only `MYLITE_CATALOG_TABLE_KIND_BASE` is supported. When later catalog versions
add views, temporary tables, or other table-like descriptors, this statement
must reject those kinds before generated SQLite SQL.

## Runtime Semantics

For this limited baseline, `TRUNCATE` removes all physical rows from the target
base table while preserving the MyLite logical descriptor and physical table
object. The generated work is equivalent to an internal full-table physical row
emptying, not user-visible `DELETE` execution:

- no predicate is evaluated;
- no triggers, cascades, or foreign keys are involved in the baseline;
- the public affected-row count is always `0` for a successful truncate;
- `warning_count` is `0` for supported successful truncates;
- result column count and result row count are both `0`;
- table descriptors, column descriptors, descriptor versions, catalog
  generation, descriptor caches, and `sqlite_schema_generation` remain
  unchanged;
- selected schema remains unchanged.

MySQL treats `TRUNCATE TABLE` as DDL and documents implicit commit behavior.
User transactions and implicit commit boundaries are not implemented in this
baseline, so this slice does not claim rollback or implicit-commit equivalence.

Auto-increment metadata is currently unsupported. The later auto-increment
feature must expand this specification before claiming MySQL's reset-to-start
truncate behavior.

## Physical SQLite Handling

The runtime builds SQLite SQL only from descriptors:

```sql
DELETE FROM "<physical_table_name>"
```

The physical table name is the stable descriptor value such as
`_mylite_user_table_<table_id>`, quoted as a SQLite identifier. No user literal
or identifier is interpolated without descriptor resolution and quoting.

No SQLite optional `TRUNCATE`, `DELETE ... LIMIT`, trigger, or fork behavior is
required. The statement executes inside a MyLite-owned `BEGIN IMMEDIATE`
transaction and commits only after SQLite reports success. On prepare, step,
allocation, or SQLite execution failure, MyLite rolls back and reports a
diagnostic without mutating the public result.

Because successful truncates do not change SQLite schema objects, the
connection-local `sqlite_schema_generation` counter is unchanged.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- public API misuse through existing `mylite_execute()` behavior;
- missing default schema;
- qualified unknown schema, reported as table-does-not-exist for this
  statement;
- unknown table;
- reserved target schema or table names;
- unsupported object kind;
- physical SQLite failures;
- allocation failures.

Supported successful truncates produce no warnings.

## MySQL 8.4.9 Runtime Observations

The expectation script `packages/libmylite/tests/
mysql_baseline_truncate_table_lifecycle_expectations.sh` records the MySQL
8.4.9 comparison cases. Observed baseline behavior:

| Case | Observed MySQL 8.4.9 behavior |
| --- | --- |
| `TRUNCATE TABLE t` with selected schema | succeeds; `ROW_COUNT() = 0`, `@@warning_count = 0`; table remains with zero rows |
| `TRUNCATE t` | same as `TRUNCATE TABLE t` |
| schema-qualified target without selected schema | succeeds |
| truncate empty table | succeeds; `ROW_COUNT() = 0`, `@@warning_count = 0` |
| no selected schema for unqualified target | error 1046 / `3D000`, `No database selected` |
| qualified unknown schema | error 1146 / `42S02`, `Table 'schema.table' doesn't exist` |
| unknown table | error 1146 / `42S02`, `Table 'schema.table' doesn't exist` |
| `IF EXISTS`, multiple tables, `WHERE`, `ORDER BY`, `LIMIT`, `TEMPORARY`, aliases, `PARTITION` | syntax error 1064 / `42000` |

## Tests

Add a fast C runtime lifecycle test under `packages/libmylite/tests/`, with a
dotted CTest name such as `libmylite.runtime.truncate_table_lifecycle`.

The test suite must cover:

- successful `TRUNCATE TABLE t` and `TRUNCATE t`;
- empty-table truncation;
- unqualified and schema-qualified target resolution, including qualified
  truncation without a selected schema;
- missing default schema, qualified unknown schema, unknown table, and reserved
  `_mylite_*` schema/table diagnostics;
- unsupported syntax rejection for `IF EXISTS`, multiple tables, `WHERE`,
  `ORDER BY`, `LIMIT`, `TEMPORARY`, aliases, and `PARTITION`;
- affected rows, warning count, and absence of result rows;
- descriptor preservation: table and column descriptors remain present with
  unchanged descriptor version, catalog generation, and SQLite schema
  generation;
- rows removed are no longer visible to `SELECT`, and new inserts after
  truncate remain readable by descriptor-driven selects;
- persistence after close/reopen;
- behavior after table rename and after drop;
- physical SQLite payload changes without touching the MyLite preamble;
- independent file-backed handles with independent truncated row state;
- zero-initialized cleanup for new planner objects;
- preservation of existing parser, basic table lifecycle, table rename,
  row-values, select-where, select-order-limit, delete, update, file-backed
  opening, VFS, catalog, diagnostics, statement-context, result-metadata,
  bootstrap, client-data, and registration tests.

## Compatibility Documentation

After implementation, update only the exact supported subset:

- `COMPATIBILITY.md`: mark `TRUNCATE TABLE` as limited/partial.
- `docs/compatibility/sql-table-ddl.md`: document limited
  descriptor-driven `TRUNCATE [TABLE]` for persistent base tables.

Do not overclaim full MySQL truncate behavior, implicit commits, transactions,
temporary tables, partitions, foreign keys, triggers, locks, privileges,
Performance Schema behavior, auto-increment reset, or physical storage rebuilds.
