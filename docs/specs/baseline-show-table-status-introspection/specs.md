# Baseline SHOW TABLE STATUS Introspection

## Status

This feature specifies a narrow descriptor-driven `SHOW TABLE STATUS` slice for
current persistent base tables. It builds on `mylite_execute()`, statement
context, the parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, table lifecycle, row values, descriptor-driven DML, `SHOW TABLES`,
`SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW LIKE` filters, and empty
`SHOW INDEX` introspection.

The feature is intentionally not full table statistics support. It exposes the
MySQL 8.4.9 result column shape and one row per MyLite persistent base table,
with deterministic values for metadata MyLite currently owns and explicit
placeholder values for storage statistics, timestamps, auto-increment, table
options, comments, views, and privileges that are not yet represented in
MyLite descriptors.

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
- Baseline catalog, schema, table, row, DML, and introspection slices:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-rename-to/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-truncate-table-lifecycle/specs.md`,
  `docs/specs/baseline-show-columns-introspection/specs.md`,
  `docs/specs/baseline-show-create-table/specs.md`,
  `docs/specs/baseline-show-like-filters/specs.md`, and
  `docs/specs/baseline-show-index-empty-introspection/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- The supported grammar shape is `SHOW TABLE STATUS [{FROM | IN} db_name]
  [LIKE 'pattern' | WHERE expr]`.
- `FROM` and `IN` are synonyms for the schema clause.
- With no selected schema and no explicit schema clause, MySQL reports
  `1046` / `3D000` (`No database selected`).
- Unknown explicit schemas report `1049` / `42000` (`Unknown database`).
- There is no table-target name in this statement. A `LIKE` pattern that
  matches no table returns an empty result set rather than an unknown-table
  diagnostic.
- Successful statements return a result set, leave `@@warning_count == 0`, and
  make the following `ROW_COUNT()` return `-1`.
- The result columns are:

```text
Name
Engine
Version
Row_format
Rows
Avg_row_length
Data_length
Max_data_length
Index_length
Data_free
Auto_increment
Create_time
Update_time
Check_time
Collation
Checksum
Create_options
Comment
```

- Simple InnoDB base tables report `Engine = InnoDB`, `Version = 10`,
  `Row_format = Dynamic`, `Max_data_length = 0`, `Index_length = 0`,
  `Data_free = 0`, `Auto_increment = NULL`,
  `Collation = utf8mb4_0900_ai_ci`, `Checksum = NULL`, and empty
  `Create_options`/`Comment` cells.
- In the observed runtime, small freshly-created InnoDB tables reported
  `Data_length = 16384`; `Rows` matched the inserted row count for the tested
  small tables; `Avg_row_length` was `0` for empty tables and
  `floor(16384 / Rows)` for one or more rows.
- MySQL emits creation and update timestamps for ordinary base tables. MyLite
  has no descriptor fields for those timestamps yet; this slice returns SQL
  `NULL` for `Create_time`, `Update_time`, and `Check_time` and documents that
  as a current compatibility gap.
- MySQL also displays views, accepts `WHERE`, and omits temporary tables from
  `SHOW TABLE STATUS`. Views, temporary-table descriptors, and `WHERE` filters
  are outside this slice.
- `SHOW FULL TABLE STATUS`, non-string `LIKE` patterns, national string
  patterns, and `LIKE` placed before `FROM` are syntax errors in the observed
  runtime.

## Scope

The implementation must add:

- parser and AST support for `SHOW TABLE STATUS`;
- optional `FROM` and `IN` schema clauses;
- optional existing `LIKE 'pattern'` filter reuse over table names;
- selected/default schema resolution for unqualified statements;
- explicit schema resolution for `SHOW TABLE STATUS FROM schema` and
  `SHOW TABLE STATUS IN schema`;
- reserved `_mylite_*` schema rejection before descriptor lookup;
- descriptor-driven iteration over persistent base tables in the resolved
  schema;
- MySQL 8.4.9 18-column result shape;
- deterministic row construction for current descriptor-owned table metadata;
- exact physical row counts from generated SQLite `COUNT(*)` over stable
  MyLite physical table names;
- deterministic diagnostics for unresolved schemas and unsupported syntax;
- result-set warning and row-count behavior matching observed MySQL 8.4.9 and
  existing MyLite result conventions;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `SHOW TABLE STATUS ... WHERE`;
- views, temporary tables, privilege filtering, system schemas, or
  `INFORMATION_SCHEMA.TABLES`;
- mutable table options, comments, tablespaces, partition status, checksums,
  auto-increment metadata, generated columns, indexes, constraints, triggers,
  row-format descriptors, table-level timestamps, or full storage statistics;
- arbitrary SQLite metadata reads, `sqlite_schema` inspection, PRAGMA-based
  introspection, arbitrary SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW TABLE STATUS` is a result-set statement and
  therefore stores `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Runtime resolves the schema, decodes the optional `LIKE` pattern, iterates
  descriptors, reads per-table physical row counts, and builds result rows.
- The catalog module remains authoritative for schema and table descriptors.
  This slice reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Physical SQLite row storage is used only for `COUNT(*)` against descriptor
  resolved stable physical names such as `_mylite_user_table_<table_id>`.
  Generated SQLite identifiers must be quoted, and no user SQL or user
  literals are interpolated.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection must not touch byte range `[0, 4096)` or change the file
  format.

## Supported SQL Grammar

Supported subset:

```sql
SHOW TABLE STATUS
SHOW TABLE STATUS {FROM | IN} schema_name
SHOW TABLE STATUS [ {FROM | IN} schema_name ] LIKE 'pattern'
```

`schema_name` uses the current identifier subset. The `LIKE` pattern must be a
single regular string literal token accepted by the existing `SHOW LIKE`
filter decoder. National string literals, charset introducers, numeric
literals, `NULL`, concatenated strings, expressions, functions, parameters,
and `WHERE` filters are not admitted.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_table_status_statement.

show_table_status_statement ::=
    SHOW TABLE STATUS show_schema_clause_opt show_like_clause_opt.

show_schema_clause_opt ::= .
show_schema_clause_opt ::= FROM identifier.
show_schema_clause_opt ::= IN identifier.

show_like_clause_opt ::= .
show_like_clause_opt ::= LIKE STRING.
```

The AST stores the optional schema identifier and optional `LIKE` literal as
children of a dedicated `SHOW TABLE STATUS` node. Runtime distinguishes the
optional schema child from the optional pattern child by node kind, following
the current `SHOW TABLES` shape.

## Schema and Table Handling

Without an explicit schema, the statement resolves the selected schema. If no
schema is selected, MyLite reports `1046` / `3D000` (`No database selected`).

With an explicit schema, MyLite copies the identifier, rejects reserved
`_mylite_*` schema names, then resolves the schema descriptor. Unknown schemas
report `1049` / `42000`.

The statement iterates all current persistent base-table descriptors in the
schema after filtering. There is no per-table target name and therefore no
unknown-table diagnostic. A `LIKE` pattern that matches no tables returns the
18-column result with zero rows. Current table matching uses the existing
case-sensitive `SHOW TABLES` pattern policy for catalog table names.

Only `MYLITE_CATALOG_TABLE_KIND_BASE` rows are emitted. Future view and
temporary-table descriptors must revisit this spec: MySQL displays views but
does not list temporary tables in `SHOW TABLE STATUS`.

## Result Rows

For each supported table, MyLite emits:

| Column | MyLite value in this slice |
| --- | --- |
| `Name` | Logical descriptor table name |
| `Engine` | `InnoDB` |
| `Version` | `10` |
| `Row_format` | `Dynamic` |
| `Rows` | Exact current physical row count as decimal text |
| `Avg_row_length` | `0` when `Rows == 0`; otherwise `floor(16384 / Rows)` |
| `Data_length` | `16384` |
| `Max_data_length` | `0` |
| `Index_length` | `0` |
| `Data_free` | `0` |
| `Auto_increment` | SQL `NULL` |
| `Create_time` | SQL `NULL` |
| `Update_time` | SQL `NULL` |
| `Check_time` | SQL `NULL` |
| `Collation` | `utf8mb4_0900_ai_ci` |
| `Checksum` | SQL `NULL` |
| `Create_options` | empty string |
| `Comment` | empty string |

The fixed size fields deliberately mirror the simple InnoDB baseline observed
for current no-index tables, but they are not full storage statistics. They
must be revisited when MyLite tracks row formats, pages, indexes, tablespaces,
comments, timestamps, or auto-increment descriptors.

Successful statements return through the existing result API conventions for
row-producing statements. `affected_rows` remains `0`, `warning_count == 0`,
and the connection-local `ROW_COUNT()` value is `-1`.

## Physical SQLite Handling

For each emitted base table, runtime builds:

```sql
SELECT COUNT(*) FROM "physical_table_name"
```

The physical table name comes from the catalog descriptor, is a MyLite-owned
stable generated name, and is quoted as a SQLite identifier. The statement has
no parameters because no user value is part of the count query. SQLite failures
map through existing runtime diagnostics. Allocation failures report the
existing out-of-memory diagnostic.

The count query is a read-only physical-storage observation. It must not read
SQLite metadata, mutate catalog state, mutate physical rows, or update
`catalog_generation` / `sqlite_schema_generation`.

## Diagnostics

This slice uses existing MyLite diagnostic helpers where possible:

- syntax errors and unsupported grammar: MySQL-style parse error;
- missing default schema: `1046` / `3D000`;
- unknown schema: `1049` / `42000`;
- reserved schema name: existing reserved-name diagnostic;
- unsupported `LIKE` pattern node or NUL-producing decoded pattern: existing
  `SHOW LIKE` unsupported/runtime diagnostics;
- physical SQLite count failures: runtime error;
- allocation failures: out-of-memory diagnostic;
- public API misuse: unchanged public execution/result misuse behavior.

Unknown tables, unsupported object kinds, and view-specific diagnostics are not
reachable in this slice because `SHOW TABLE STATUS` iterates schema tables and
the current catalog has only persistent base-table descriptors.

## Tests

Add a fast C test binary under `packages/libmylite/tests/`, registered with a
dotted CTest name such as `libmylite.runtime.show_table_status_introspection`.

Coverage must include:

- result columns and rows for empty and populated persistent base tables;
- schema-qualified forms, `FROM`/`IN` schema forms, selected-schema behavior,
  and missing default schema;
- `LIKE` filters, no-match filters, escaped wildcard reuse, and unsupported
  non-string pattern forms;
- warning count, affected rows, no result misuse, and `ROW_COUNT() == -1`;
- row counts after insert, update, delete, truncate, rename, and drop where
  applicable;
- reopen persistence and independent file-backed handles;
- unchanged catalog generation, `sqlite_schema_generation`, and `.mylite`
  preamble for introspection;
- unsupported syntax such as `SHOW FULL TABLE STATUS`, `WHERE`, invalid
  pattern expressions, and misplaced `LIKE`;
- existing parser, schema, table, row, DML, show-like, show-columns,
  show-create-table, show-index, and file-format tests remain passing.

The MySQL expectation artifact must verify the MySQL 8.4.9 result columns,
name resolution, filtering, status values, diagnostics, view observation, and
unsupported syntax that define this limited design.
