# Baseline Schema IF EXISTS Lifecycle

## Status

This feature specifies the next narrow schema-lifecycle slice for file-backed
`.mylite` handles. It adds optional existence clauses to the already supported
persistent schema lifecycle:

- `CREATE DATABASE IF NOT EXISTS schema_name`
- `CREATE SCHEMA IF NOT EXISTS schema_name`
- `DROP DATABASE IF EXISTS schema_name`
- `DROP SCHEMA IF EXISTS schema_name`

The feature is intentionally not full MySQL schema administration. It keeps the
current persistent MyLite catalog-schema model, optionless schema descriptors,
limited warning/diagnostics model, and descriptor-owned base-table cleanup. It
does not implement schema options, privileges, system schemas, filesystem
directories, implicit commit semantics, temporary tables, views, triggers,
foreign keys, or general data dictionary behavior.

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
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline table IF EXISTS lifecycle:
  `docs/specs/baseline-table-if-exists-lifecycle/specs.md`
- Baseline show warnings diagnostics:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline diagnostics count variables:
  `docs/specs/baseline-diagnostics-count-variables/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- MySQL 8.4 Reference Manual, `DROP DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/drop-database.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for `IF NOT EXISTS` after `CREATE DATABASE` and
  `CREATE SCHEMA`;
- parser and AST support for `IF EXISTS` after `DROP DATABASE` and
  `DROP SCHEMA`;
- existing descriptor-driven schema creation for non-existing targets;
- existing descriptor-driven schema drop and physical base-table cleanup for
  existing targets;
- successful no-op behavior with a note-level condition when
  `CREATE DATABASE IF NOT EXISTS` or `CREATE SCHEMA IF NOT EXISTS` targets an
  existing schema;
- successful no-op behavior when `DROP DATABASE IF EXISTS` or
  `DROP SCHEMA IF EXISTS` targets a missing schema;
- MySQL-compatible result warning counts, `SHOW WARNINGS`, and
  `@@warning_count` behavior for admitted existence no-op cases;
- no catalog generation, descriptor cache, SQLite schema generation, physical
  schema, row data, or `.mylite` preamble changes for no-op existing-create and
  missing-drop statements;
- existing selected-schema clearing behavior when an existing selected schema
  is dropped;
- result behavior for successful schema DDL through the existing public result
  API conventions.

## Non-Goals

This feature must not implement:

- schema `DEFAULT CHARACTER SET`, `COLLATE`, `ENCRYPTION`, or any other schema
  option;
- `ALTER DATABASE`, `SHOW CREATE DATABASE IF NOT EXISTS`, `SHOW DATABASES LIKE`
  changes, privilege filtering, partial revokes, or mutable schema defaults;
- physical database directories, symlinks, external files, or filesystem-level
  cleanup;
- MySQL system schemas such as `information_schema`, `mysql`,
  `performance_schema`, or `sys`;
- temporary tables, views, triggers, cascades, foreign keys, generated columns,
  defaults, indexes, constraints, auto-increment behavior, arbitrary SQLite SQL
  pass-through, or SQLite fork patches;
- table-level `IF EXISTS` behavior beyond the already implemented table slice;
- privilege semantics or implicit commit behavior.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result-handle ownership, public misuse behavior, diagnostics snapshots, and
  failure cleanup.
- Statement context owns diagnostics reset, note/warning collection, warning
  count, affected rows, row-count state, and the top-level statement boundary.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code copies schema identifiers from AST spans, rejects
  reserved `_mylite_*` schema names, resolves catalog descriptors, distinguishes
  true not-found from catalog read failure, and chooses no-op versus real
  mutation before generated SQLite SQL.
- The catalog module remains authoritative for schema, table, and column
  descriptors. Existing schema create/drop catalog mutations are reused only
  when a real create/drop occurs.
- SQLite owns durable b-tree row storage and physical user table objects. For a
  real schema drop, runtime drops only descriptor-owned physical table names.
- The result builder owns empty DDL results and warning counts copied from live
  diagnostics.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Existence no-ops must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits these extensions to the already supported schema lifecycle:

```sql
CREATE DATABASE IF NOT EXISTS schema_name
CREATE SCHEMA IF NOT EXISTS schema_name
DROP DATABASE IF EXISTS schema_name
DROP SCHEMA IF EXISTS schema_name
```

`schema_name` remains one identifier. Backtick-quoted identifiers are supported
by the existing identifier machinery. Qualified names such as `a.b` remain
outside this slice.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
create_schema_statement ::=
    CREATE DATABASE create_schema_if_not_exists_opt identifier.
create_schema_statement ::=
    CREATE SCHEMA create_schema_if_not_exists_opt identifier.

create_schema_if_not_exists_opt ::= .
create_schema_if_not_exists_opt ::= IF NOT EXISTS.

drop_schema_statement ::=
    DROP DATABASE drop_schema_if_exists_opt identifier.
drop_schema_statement ::=
    DROP SCHEMA drop_schema_if_exists_opt identifier.

drop_schema_if_exists_opt ::= .
drop_schema_if_exists_opt ::= IF EXISTS.
```

The AST should represent existence policy explicitly as a compact marker child
or an equivalent typed flag. Runtime must not infer the policy by string
searching statement source.

## Schema Resolution And Identifier Handling

`CREATE DATABASE IF NOT EXISTS schema_name` and
`CREATE SCHEMA IF NOT EXISTS schema_name` create a schema descriptor when the
name is not present. They do not select the schema.

If the schema already exists, the `IF NOT EXISTS` forms succeed as no-ops. They
must not compare schema options, because this slice does not support schema
options and MySQL does not replace the existing schema definition in this case.

`DROP DATABASE IF EXISTS schema_name` and `DROP SCHEMA IF EXISTS schema_name`
drop existing schema descriptors and descriptor-owned physical base tables by
reusing the existing schema-drop plan. If the schema is missing, the `IF EXISTS`
forms succeed as no-ops.

User-authored schema names beginning with `_mylite_`, using ASCII
case-insensitive comparison, are reserved for MyLite internals and are rejected
before catalog or SQLite mutation, including when an existence clause is
present.

Catalog lookups must distinguish three states: found, not found, and read
failure. Only true not-found may become an existence-clause no-op. Catalog or
SQLite read failures must remain failures and must not be converted into
MySQL-style notes.

This slice preserves the current catalog name comparison policy. Broader
platform and collation-specific MySQL schema-name case behavior remains a future
identifier-policy feature.

## Create Semantics

If the target schema does not exist, `CREATE DATABASE IF NOT EXISTS` and
`CREATE SCHEMA IF NOT EXISTS` follow the existing limited schema-create path:

- validate and copy the schema identifier;
- reject reserved names;
- insert one `_mylite_catalog_schemas` row;
- advance catalog generation exactly like ordinary `CREATE DATABASE`;
- leave SQLite schema generation unchanged;
- leave selected schema unchanged;
- return an empty result with `affected_rows == 1` and warning count `0`.

If the target schema already exists, MyLite succeeds as a no-op. The statement
must not mutate catalog rows, descriptor versions, catalog generation,
descriptor caches, SQLite schema generation, physical SQLite schema, row data,
selected-schema state, or the `.mylite` preamble.

The no-op records a MySQL-compatible note-level condition:

- level: `Note`
- code: `1007`
- SQLSTATE: `HY000`
- message: `Can't create database '<schema>'; database exists`

For this no-op, MySQL 8.4.9 reports `ROW_COUNT() == 1`, `@@warning_count == 1`,
and `SHOW WARNINGS` displays the note. MyLite follows those public semantics.
The public result object's warning count is `1`; its affected rows are `1`,
matching the existing schema-create public result convention for created
schemas and MySQL's observed row-count function.

Without `IF NOT EXISTS`, duplicate schema creation remains an error with code
`1007`, SQLSTATE `HY000`, and the same message text, as implemented by the
baseline schema lifecycle.

## Drop Semantics

If the target schema exists, `DROP DATABASE IF EXISTS` and
`DROP SCHEMA IF EXISTS` follow the existing limited schema-drop path:

- resolve the schema descriptor;
- collect persistent base-table descriptors in the schema;
- delete schema-owned catalog rows inside one catalog mutation;
- drop descriptor-owned physical SQLite tables;
- commit the catalog mutation;
- increment SQLite schema generation only when at least one physical table was
  dropped;
- clear the selected schema if this connection had selected the dropped schema;
- return an empty result whose public API affected rows equal the number of
  removed table descriptors and whose warning count is `0`.

If the target schema is missing, MyLite succeeds as a no-op. The statement must
not mutate catalog rows, descriptor versions, catalog generation, descriptor
caches, SQLite schema generation, physical SQLite schema, row data,
selected-schema state, or the `.mylite` preamble.

MySQL 8.4.9 reports `DROP DATABASE IF EXISTS missing_schema` as `Query OK, 0
rows affected, 1 warning` in client status and with `--show-warnings`, but a
following `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, or `SELECT
@@warning_count` reports no stored diagnostics for this specific statement.
MyLite's public result API does expose a statement warning count, so for this
slice it returns warning count `1` for the missing-drop result while storing no
warning record in the previous diagnostics snapshot. This is a deliberately
narrow MyLite implementation detail for matching both visible result warning
count and post-statement diagnostics behavior.

The client-status note shape observed with MySQL `--show-warnings` is:

- level: `Note`
- code: `1008`
- SQLSTATE: `HY000`
- message: `Can't drop database '<schema>'; database doesn't exist`

Because MyLite currently exposes stored diagnostics through `SHOW WARNINGS` and
`@@warning_count`, this slice must not store that note for missing schema-drop
no-ops unless the diagnostics subsystem later gains explicit counted-but-not-
stored support.

Without `IF EXISTS`, missing schema deletion remains an error with code `1008`,
SQLSTATE `HY000`, and the same message text, as implemented by the baseline
schema lifecycle.

## Diagnostics And Result Behavior

Successful new creates and existing drops preserve the existing schema lifecycle
result conventions.

Successful existing creates return a non-row result with warning count `1`,
record a stored `Note 1007`, and make `SHOW WARNINGS`, `SHOW COUNT(*)
WARNINGS`, `@@warning_count`, and `@@error_count` observe the note according to
existing diagnostics snapshot policy.

Successful missing drops return a non-row result with warning count `1`, but do
not store a diagnostic row for later `SHOW WARNINGS` or `@@warning_count`, based
on MySQL 8.4.9 runtime observations for `DROP DATABASE IF EXISTS`.

`ROW_COUNT()` behavior for the admitted subset follows MySQL 8.4.9 runtime
observations:

- new schema create: `1`;
- existing schema create with `IF NOT EXISTS`: `1`;
- existing schema drop: `-1`;
- missing schema drop with `IF EXISTS`: `-1`.

Errors, including syntax errors, reserved names, unsupported options,
allocation failures, catalog read failures, physical SQLite failures, and public
API misuse use existing diagnostics policy unless explicitly overridden above.

## Physical SQLite Handling

No SQLite SQL is generated for existing-create no-ops or missing-drop no-ops.
Real schema drops reuse the existing descriptor-built physical table cleanup
path and stable physical table names such as `_mylite_user_table_<table_id>`.
Every generated SQLite identifier remains quoted, and no user SQL is passed
through to SQLite.

The implementation must not add SQLite fork patches. Public SQLite APIs and the
existing MyLite wrapper/translation layer are sufficient.

## MySQL 8.4.9 Runtime Observations

The companion script
`packages/libmylite/tests/mysql_baseline_schema_if_exists_expectations.sh`
records these MySQL 8.4.9 observations:

- `CREATE DATABASE IF NOT EXISTS db` creates a missing schema with
  `ROW_COUNT() == 1`, `@@warning_count == 0`, and `DATABASE() == NULL`.
- `CREATE SCHEMA IF NOT EXISTS db` has the same behavior.
- Repeating `CREATE DATABASE IF NOT EXISTS db` succeeds with
  `ROW_COUNT() == 1`, `@@warning_count == 1`, and `SHOW WARNINGS` row
  `Note 1007 Can't create database '<db>'; database exists`.
- `DROP DATABASE IF EXISTS db` drops an existing schema with a client status
  affected-row count equal to the number of removed tables, `ROW_COUNT() == -1`,
  and `@@warning_count == 0`.
- Dropping the selected schema clears the selected database.
- `DROP DATABASE IF EXISTS missing_db` succeeds. The immediate client status
  reports one warning, but `SHOW WARNINGS`, `SHOW COUNT(*) WARNINGS`, and
  `SELECT @@warning_count` after the statement report no stored diagnostics.
- `DROP SCHEMA IF EXISTS missing_db` has the same missing-schema behavior.
- MySQL accepts wider forms outside this slice, including schema options.
- Qualified schema names such as `CREATE DATABASE a.b` are syntax errors.

## Compatibility Documentation

After implementation:

- `COMPATIBILITY.md` should update the `CREATE DATABASE` / `CREATE SCHEMA` and
  `DROP DATABASE` / `DROP SCHEMA` rows to mention limited existence-clause
  support.
- `docs/compatibility/sql-schemas.md` should make the same exact-scope update.
- Diagnostics docs should only change if the implementation alters the existing
  warning-count or `SHOW WARNINGS` surface beyond the notes introduced here.

Do not overclaim schema options, privileges, system schemas, filesystem
directories, implicit commit behavior, temporary tables, views, triggers,
foreign keys, or arbitrary SQLite pass-through.

## Test Plan

Add a focused runtime C test, preferably
`runtime_schema_if_exists_lifecycle_test.c`, covering:

- `CREATE DATABASE IF NOT EXISTS` and `CREATE SCHEMA IF NOT EXISTS` create
  missing schemas and preserve existing schema-create behavior;
- existing-create no-op result warning count, `SHOW WARNINGS`, `SHOW COUNT(*)
  WARNINGS`, `@@warning_count`, affected rows, `ROW_COUNT()`, and no mutation;
- existing-create no-op leaves catalog generation and SQLite schema generation
  unchanged;
- `DROP DATABASE IF EXISTS` and `DROP SCHEMA IF EXISTS` drop existing schemas,
  clear selected schema where applicable, and preserve existing schema-drop
  behavior;
- missing-drop no-op result warning count, `SHOW WARNINGS` empty behavior,
  `SHOW COUNT(*) WARNINGS == 0`, `@@warning_count == 0`, `ROW_COUNT() == -1`,
  and no mutation;
- reserved schema names;
- unsupported syntax rejected deterministically, including schema options and
  qualified schema names;
- reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles with independent schema and diagnostics state;
- catalog lookup failures are not converted into existence no-ops;
- existing parser, schema lifecycle, table lifecycle, diagnostics, DML, and
  file-format tests still pass.
