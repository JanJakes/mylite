# Baseline Schema Lifecycle

## Status

This feature specifies the next narrow schema lifecycle slice for file-backed
`.mylite` handles. It adds public schema descriptors and descriptor-driven
database listing on top of `mylite_execute()`, statement context, the MyLite
parser scaffold, shifted `.mylite` storage, durable catalog descriptors,
limited table lifecycle, table rename, row-value storage, descriptor-driven
`SELECT`, baseline `DELETE`, `UPDATE`, and `TRUNCATE`.

The feature is intentionally not full MySQL schema administration. It supports
only persistent MyLite catalog schemas with no options, no privileges, no
system-schema catalog, no `IF EXISTS` or `IF NOT EXISTS`, and no `LIKE` or
`WHERE` filters for `SHOW DATABASES`.

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
- Baseline truncate table lifecycle:
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- MySQL 8.4 Reference Manual, `DROP DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-database.html
- MySQL 8.4 Reference Manual, `SHOW DATABASES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-databases.html
- MySQL 8.4 Reference Manual, `USE`:
  https://dev.mysql.com/doc/refman/8.4/en/use.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for a limited `CREATE DATABASE`, `CREATE SCHEMA`,
  `DROP DATABASE`, `DROP SCHEMA`, `SHOW DATABASES`, and `SHOW SCHEMAS`
  surface;
- public creation of persistent MyLite schema descriptors;
- public deletion of persistent MyLite schema descriptors and every persistent
  base table descriptor inside the schema;
- physical SQLite cleanup for all MyLite-owned physical base tables in a
  dropped schema;
- existing `USE schema_name` behavior over schemas created through public SQL;
- descriptor-driven unfiltered database listing;
- reserved `_mylite_*` schema name rejection before catalog or SQLite mutation;
- MySQL-compatible diagnostics for duplicate schema creation, unknown schema
  selection, and missing schema deletion;
- result-handle behavior matching MySQL-visible baseline status: successful
  `CREATE DATABASE`/`CREATE SCHEMA` has no rows, `affected_rows == 1`, and
  `warning_count == 0`; successful `DROP DATABASE`/`DROP SCHEMA` has no rows,
  `affected_rows == <number of removed table descriptors>`, and
  `warning_count == 0`; successful `USE` has no rows, `affected_rows == 0`, and
  `warning_count == 0`; successful `SHOW DATABASES`/`SHOW SCHEMAS` returns one
  text column named `Database`;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `IF NOT EXISTS`, `IF EXISTS`, `DEFAULT CHARACTER SET`, `COLLATE`,
  `ENCRYPTION`, or any other schema option;
- `ALTER DATABASE`, `SHOW CREATE DATABASE`, `SHOW DATABASES LIKE`, `SHOW
  DATABASES WHERE`, privilege filtering, partial revokes, or system variables;
- physical directories, file-name mapping, symbolic links, external files, or
  filesystem-level database removal;
- full MySQL system schemas such as `information_schema`, `mysql`,
  `performance_schema`, or `sys`;
- information-schema tables, `DATABASE()`, schema metadata functions, SQL
  modes, privileges, locks, binlog behavior, replication, or implicit commit
  boundaries;
- temporary tables, views, triggers, cascades, foreign keys, generated columns,
  defaults, indexes, constraints, auto-increment behavior, arbitrary SQLite SQL
  pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and transaction completion.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code copies schema identifiers from AST spans, rejects
  unsupported shapes and reserved names, resolves catalog descriptors, and
  computes the descriptor-owned physical table cleanup plan for schema drops.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. This slice adds
  composable schema deletion in an active catalog mutation so physical SQLite
  cleanup and descriptor cleanup commit or roll back together.
- The result builder owns empty non-row results and one-column `SHOW DATABASES`
  result sets.
- SQLite owns physical b-tree row storage and physical user table objects. The
  runtime drops physical tables only by descriptor-owned physical names.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Schema lifecycle statements operate only inside the shifted SQLite payload
  and must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
CREATE DATABASE schema_name
CREATE SCHEMA schema_name
DROP DATABASE schema_name
DROP SCHEMA schema_name
USE schema_name
SHOW DATABASES
SHOW SCHEMAS
```

`schema_name` is one identifier. Backtick-quoted identifiers are supported by
the existing identifier machinery. Unquoted qualified names such as `a.b` are
not schema names in this slice.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= create_schema_statement.
statement ::= drop_schema_statement.
statement ::= show_databases_statement.

create_schema_statement ::= CREATE DATABASE identifier.
create_schema_statement ::= CREATE SCHEMA identifier.

drop_schema_statement ::= DROP DATABASE identifier.
drop_schema_statement ::= DROP SCHEMA identifier.

show_databases_statement ::= SHOW DATABASES.
show_databases_statement ::= SHOW SCHEMAS.
```

The parser must reject unsupported schema forms as syntax errors whenever the
unsupported form cannot be represented by this grammar. That includes
`CREATE DATABASE IF NOT EXISTS`, `DROP DATABASE IF EXISTS`, schema options,
qualified unquoted names, `SHOW DATABASES LIKE`, `SHOW DATABASES WHERE`, and
`SHOW FULL DATABASES`.

## Schema Resolution And Identifier Handling

`CREATE DATABASE schema_name` and `CREATE SCHEMA schema_name` create a new
schema descriptor. They do not select the schema. A duplicate schema name fails
with MySQL error `1007`, SQLSTATE `HY000`, and message `Can't create database
'<schema>'; database exists`.

`DROP DATABASE schema_name` and `DROP SCHEMA schema_name` require an existing
schema descriptor. A missing schema fails with MySQL error `1008`, SQLSTATE
`HY000`, and message `Can't drop database '<schema>'; database doesn't exist`.

`USE schema_name` keeps the existing behavior from the table lifecycle phase:
it requires an existing schema descriptor and fails with error `1049`,
SQLSTATE `42000`, and message `Unknown database '<schema>'` when the schema is
missing. On success it copies the descriptor name into connection-local session
state.

If the selected schema is dropped by the same connection, the selected-schema
flag is cleared. Subsequent unqualified statements fail with `No database
selected`, matching MySQL 8.4.9's documented `DATABASE()` behavior after
dropping the default database.

Identifiers are copied from AST spans into owned, NUL-terminated internal
buffers. Backtick-quoted identifiers are unquoted and doubled backticks are
collapsed. Identifiers that do not fit the existing catalog identifier capacity
fail before catalog mutation.

User-authored schema names beginning with `_mylite_`, using ASCII
case-insensitive comparison, are reserved for MyLite and are rejected before
catalog or SQLite mutation with a deterministic MyLite diagnostic.

This slice preserves the current catalog name comparison policy. Schema names
are looked up through the existing catalog schema name key, and `SHOW
DATABASES` orders descriptor rows by catalog schema name. Broader platform and
collation-specific MySQL schema-name case behavior remains a future
identifier-policy feature.

## Runtime Semantics

`CREATE DATABASE` and `CREATE SCHEMA` insert one `_mylite_catalog_schemas` row,
advance the catalog generation once, invalidate descriptor caches through the
catalog mutation path, and leave `sqlite_schema_generation` unchanged. They do
not create physical SQLite tables and do not modify the selected schema.

`DROP DATABASE` and `DROP SCHEMA` remove the target schema descriptor, every
table descriptor in the schema, every column descriptor owned by those tables,
and every descriptor-owned physical SQLite table for those table descriptors.
The whole operation uses one catalog mutation transaction:

1. resolve the schema descriptor;
2. collect table descriptors for the schema;
3. begin a catalog mutation;
4. delete schema-owned catalog rows inside that mutation;
5. execute `DROP TABLE` for each descriptor-owned physical table inside the
   same SQLite transaction;
6. commit the catalog mutation and generation update.

If any descriptor deletion, physical drop, allocation, prepare, step, or commit
operation fails, MyLite rolls back the catalog mutation. The catalog and
physical SQLite schema must not diverge on failure.

The public affected-row count for `DROP DATABASE`/`DROP SCHEMA` is the number
of removed persistent table descriptors. MySQL 8.4.9's client status reports
that count; SQL `ROW_COUNT()` for DDL is not the public result API authority in
this slice.

Dropping a schema that is selected by this connection clears the selected
schema after commit. Dropping another schema leaves the selected schema
unchanged. Other MyLite handles are independent and keep their own selected
schema state, but subsequent unqualified statements on a stale selected schema
will fail when resolution sees that the descriptor no longer exists.

`SHOW DATABASES` and `SHOW SCHEMAS` build a planned result from MyLite schema
descriptors. The result has one text column named `Database` and one row per
catalog schema, ordered by catalog schema name. This baseline does not
synthesize MySQL system schemas and does not apply privilege filtering.

## Physical SQLite Handling

`CREATE DATABASE`, `USE`, and `SHOW DATABASES` do not generate physical SQLite
schema SQL.

`DROP DATABASE` builds physical `DROP TABLE` SQL only from table descriptors:

```sql
DROP TABLE "<physical_table_name>"
```

The physical table name is the stable descriptor value such as
`_mylite_user_table_<table_id>`, quoted as a SQLite identifier. No user literal
or identifier is interpolated into generated SQLite SQL.

The implementation must not scan `sqlite_schema` to discover user objects.
MyLite catalog descriptors are authoritative. If later catalog versions add
non-base-table descriptors, this slice must reject unsupported object kinds or
teach the drop planner their physical cleanup rules before dropping the schema.

Because dropping a schema with base tables removes physical SQLite schema
objects, `sqlite_schema_generation` is incremented once after a successful
drop that removed at least one physical table. Dropping an empty schema leaves
SQLite schema objects unchanged.

No SQLite fork patch is required.

## Diagnostics

Supported diagnostics:

| Condition | Diagnostic |
| --- | --- |
| Syntax error or unsupported grammar | Error `1064`, SQLSTATE `42000` |
| Duplicate schema on create | Error `1007`, SQLSTATE `HY000`, `Can't create database '<schema>'; database exists` |
| Missing schema on drop | Error `1008`, SQLSTATE `HY000`, `Can't drop database '<schema>'; database doesn't exist` |
| Missing schema on `USE` | Error `1049`, SQLSTATE `42000`, `Unknown database '<schema>'` |
| Reserved `_mylite_*` schema name | MyLite-specific error `1102`, SQLSTATE `42000`, `Incorrect database name '<schema>'` |
| Identifier too long | Error `1059`, SQLSTATE `42000` |
| Unsupported table kind during schema drop | Error `1064`, SQLSTATE `42000` |
| Allocation failure | `MYLITE_NOMEM`, SQLSTATE `HY001` |
| Physical SQLite schema operation failure | Error `1105`, SQLSTATE `HY000` |
| Public API misuse | `MYLITE_MISUSE`, SQLSTATE `HY000` |

Successful supported statements produce `warning_count == 0`.

## MySQL 8.4.9 Runtime Observations

The feature expectations are based on a local MySQL 8.4.9 runtime:

```sql
SELECT VERSION();
-- 8.4.9
```

Observed behavior:

| Statement | MySQL 8.4.9 behavior |
| --- | --- |
| `CREATE DATABASE db; SELECT ROW_COUNT(), @@warning_count, DATABASE();` | `1`, `0`, `NULL`; schema is not selected. |
| `CREATE SCHEMA db; SELECT ROW_COUNT(), @@warning_count;` | `1`, `0`; `SCHEMA` is a synonym for `DATABASE`. |
| Duplicate `CREATE DATABASE db` | Error `1007`, SQLSTATE `HY000`. |
| `USE db; SELECT DATABASE(), ROW_COUNT(), @@warning_count;` | Current database becomes `db`; row count `0`; warnings `0`. |
| `DROP DATABASE empty_db` | Client status reports `0 rows affected`; warnings `0`. |
| `DROP DATABASE db_with_two_tables` | Client status reports `2 rows affected`; warnings `0`. |
| Dropping the selected database | Current database becomes unset. |
| Missing `DROP DATABASE db` | Error `1008`, SQLSTATE `HY000`. |
| Missing `USE db` | Error `1049`, SQLSTATE `42000`. |
| `SHOW DATABASES LIKE 'prefix%'` | One column named `Database (prefix%)`, rows sorted by database name. |
| `SHOW DATABASES` | One column named `Database`, includes all visible schemas including MySQL system schemas. |

The expectation script for this feature records these observations in
`packages/libmylite/tests/mysql_baseline_schema_lifecycle_expectations.sh`.

## Compatibility Documentation

After implementation:

- `COMPATIBILITY.md` must mark `Dynamic current database`, schema lifecycle,
  and limited database listing as partial, not complete.
- `docs/compatibility/sql-schemas.md` must document limited
  `CREATE/DROP DATABASE|SCHEMA` and public `USE` over catalog schemas.
- `docs/compatibility/sql-show-statements.md` must document limited
  descriptor-driven `SHOW DATABASES`/`SHOW SCHEMAS` without filters,
  privileges, or system schemas.

No compatibility document should claim full schema options, system schemas,
information schema, privileges, filters, or implicit commit behavior.

## Test Plan

Add a fast C test under `packages/libmylite/tests/`, preferably
`runtime_schema_lifecycle_test.c`, registered as a dotted CTest name.

Coverage must include:

- parser acceptance for the supported schema lifecycle forms;
- parser rejection for `IF EXISTS`, `IF NOT EXISTS`, schema options, qualified
  unquoted schema names, filters, modifiers, and extra clauses;
- successful `CREATE DATABASE`, `CREATE SCHEMA`, `USE`, `SHOW DATABASES`,
  `SHOW SCHEMAS`, `DROP DATABASE`, and `DROP SCHEMA`;
- duplicate create and missing drop diagnostics;
- missing `USE` and reserved `_mylite_*` diagnostics;
- `CREATE DATABASE` does not select the schema;
- dropping the selected schema clears selected-schema behavior;
- dropping another schema preserves selected-schema behavior;
- schema drop removes table and column descriptors and descriptor-owned
  physical SQLite tables;
- schema drop affected rows equal the number of removed table descriptors;
- empty schema drop affected rows are zero;
- generated physical drops preserve the `.mylite` preamble;
- reopen persistence for created schemas, selected-schema independence, and
  dropped schemas;
- independent file-backed handles with independent session state;
- zero-initialized cleanup for new planner/result helper objects if any;
- no public ABI changes and preserved public execution/result misuse behavior.

Verification before marking done:

1. `cmake --build --preset dev`
2. Run the new CTest entry and existing parser/runtime lifecycle entries,
   including basic table lifecycle, rename, row values, select-where,
   select-order-limit, delete, update, and truncate.
3. `./packages/libmylite/tests/mysql_baseline_schema_lifecycle_expectations.sh`
4. `cmake --workflow --preset check`
