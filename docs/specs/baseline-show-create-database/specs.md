# Baseline SHOW CREATE DATABASE

## Status

This feature specifies a narrow descriptor-driven `SHOW CREATE DATABASE` slice
for persistent MyLite catalog schemas. It builds on `mylite_execute()`,
statement context, the MyLite parser scaffold, file-backed `.mylite` opening,
durable catalog descriptors, schema lifecycle, descriptor-driven introspection,
and existing `SHOW CREATE TABLE` rendering.

The feature is intentionally not full MySQL schema-option support. It renders
a MySQL-style `CREATE DATABASE` statement for current MyLite schemas using the
fixed default character set, collation, and encryption text observed from
MySQL 8.4.9. MyLite schema descriptors still do not store per-schema options.

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
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline show-create-table:
  `docs/specs/baseline-show-create-table/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-database.html
- MySQL 8.4 Reference Manual, `CREATE DATABASE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW CREATE DATABASE db_name` and `SHOW CREATE SCHEMA db_name` are synonyms.
- Successful output has columns `Database` and `Create Database`.
- The `Database` cell is the schema name without quoting.
- The DDL cell uses `CREATE DATABASE`, even for `SHOW CREATE SCHEMA`.
- Default-created schemas render as:

  ```sql
  CREATE DATABASE `db_name` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
  ```

- Embedded backticks in the schema name are doubled inside the DDL cell.
- Successful `SHOW CREATE DATABASE` leaves `@@warning_count == 0` and makes
  `ROW_COUNT()` return `-1`.
- Unknown schemas fail with error `1049`, SQLSTATE `42000`.
- MySQL accepts `SHOW CREATE DATABASE IF NOT EXISTS db_name` and includes the
  `/*!32312 IF NOT EXISTS*/` version comment in the DDL cell. This first MyLite
  slice defers that optional form.
- With `@@sql_quote_show_create = 0`, MySQL omits backticks around simple
  schema names in the rendered DDL. MyLite does not expose system variable
  assignment yet, so this slice always renders quoted identifiers and
  documents `sql_quote_show_create` as a compatibility gap.
- `SHOW CREATE DATABASE IF EXISTS db_name`, missing schema names, `LIKE`,
  `WHERE`, and schema options are syntax errors.

## Scope

The implementation must add:

- parser and AST support for `SHOW CREATE DATABASE identifier` and
  `SHOW CREATE SCHEMA identifier`;
- descriptor-driven schema lookup from the MyLite catalog;
- reserved `_mylite_*` schema-name rejection before descriptor lookup;
- MySQL-observed result shape with `Database` and `Create Database` columns;
- MySQL-observed fixed DDL rendering for current optionless MyLite schema
  descriptors;
- MySQL-style backtick quoting and doubled-backtick escaping in the DDL cell;
- warning and row-count behavior for a result-set statement;
- deterministic diagnostics for unsupported syntax and unresolved names;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `SHOW CREATE DATABASE IF NOT EXISTS`;
- schema option storage or rendering beyond the fixed default text;
- `sql_quote_show_create`;
- `ALTER DATABASE`, schema default charset/collation mutation, encryption
  mutation, privilege filtering, system schemas, partial revokes,
  `INFORMATION_SCHEMA.SCHEMATA`, or `SHOW DATABASES WHERE`;
- arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or SQLite
  fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW CREATE DATABASE` is a result-set statement and
  stores `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They admit only the
  two statement forms in this slice and stay independent of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner code copies the schema identifier, rejects reserved names,
  and resolves the schema descriptor from the MyLite catalog.
- The catalog module remains authoritative for schema descriptors. This slice
  reads descriptors but does not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite catalog descriptors.
  It does not query SQLite schema metadata.
- The result builder owns the two columns and one row.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  `SHOW CREATE DATABASE` reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW CREATE DATABASE schema_name
SHOW CREATE SCHEMA schema_name
```

`schema_name` is one identifier. Backtick-quoted identifiers are supported by
the existing identifier machinery. Qualified names are not admitted for schema
names in this slice.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_create_database_statement.

show_create_database_statement ::= SHOW CREATE DATABASE identifier.
show_create_database_statement ::= SHOW CREATE SCHEMA identifier.
```

The statement creates a dedicated AST node with one child: the schema-name node.
No optional clauses are admitted in this slice.

## Schema Resolution

`SHOW CREATE DATABASE schema_name` and `SHOW CREATE SCHEMA schema_name` resolve
the named schema directly and do not require a selected default schema. Unknown
schemas report `1049` / `42000` (`Unknown database`).

Schema names beginning with MyLite's reserved `_mylite_` prefix are rejected
before descriptor lookup with the existing reserved database-name diagnostic.

Descriptor lookup uses the current catalog name comparison behavior. This slice
does not add MySQL filesystem-dependent schema-name case folding or collation
semantics.

## Rendering Semantics

For current MyLite schema descriptors, the result has one row:

| Column | Value |
| --- | --- |
| `Database` | schema descriptor name |
| `Create Database` | rendered fixed-default DDL |

The DDL cell shape is:

```sql
CREATE DATABASE `schema_name` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
```

The schema name is always backtick quoted in the DDL cell. Embedded backticks
are escaped by doubling them. The first `Database` cell is the raw descriptor
name. MySQL's `sql_quote_show_create` session variable can suppress quoting in
server output; MyLite does not implement that variable yet, so the
descriptor-rendered DDL remains quoted for deterministic baseline output.

The fixed option suffix matches observed MySQL 8.4.9 output for schemas
created with default options. MyLite does not store schema character-set,
collation, or encryption options yet, so this output is a compatibility
placeholder for the current descriptor model.

## Result And Row Count

Successful `SHOW CREATE DATABASE` returns a row result set. It has:

- `column_count == 2`;
- `row_count == 1`;
- `affected_rows == 0`;
- `warning_count == 0`;
- connection-local previous row count `-1`, visible through `ROW_COUNT()`.

## Diagnostics

Diagnostics must be deterministic:

- syntax errors and unsupported grammar: `1064` / `42000`;
- unknown schema: `1049` / `42000`;
- reserved schema names: existing MyLite reserved database-name diagnostic;
- allocation failure: `MYLITE_NOMEM` with the existing out-of-memory
  diagnostic;
- public API misuse: unchanged existing public API misuse behavior.

## Physical SQLite And File Format

This feature generates no SQLite SQL and requires no SQLite fork patches. The
implementation reads MyLite catalog descriptors through existing catalog APIs,
builds public result text in memory, and leaves physical SQLite user objects,
catalog generations, SQLite schema generations, and the `.mylite` preamble
unchanged.

## Tests

Tests must cover:

- `SHOW CREATE DATABASE` and `SHOW CREATE SCHEMA`;
- exact result column names and rendered fixed-default DDL;
- quoted schema names with embedded backticks;
- no selected-schema requirement;
- warning count, affected rows, row count, and absence of extra rows;
- unknown schema, reserved schema, and unsupported syntax diagnostics;
- reopen persistence after schema creation;
- schema drop behavior;
- independent file-backed handles;
- catalog generation, SQLite schema generation, and preamble preservation;
- the MySQL 8.4.9 expectation artifact for supported behavior and deferred
  `IF NOT EXISTS` and `sql_quote_show_create` behavior.

Existing lexer, parser, runtime handle, diagnostics, statement context, result
metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
foundation, schema lifecycle, show-create-table, show-like, and related runtime
tests must continue to pass.
