# Baseline SHOW CREATE TABLE

## Status

This feature specifies a narrow descriptor-driven `SHOW CREATE TABLE` slice
for persistent base tables. It builds on `mylite_execute()`, statement context,
the MyLite parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, schema and table lifecycle, row storage, descriptor-driven DML,
and the existing baseline `SHOW TABLES`, `SHOW COLUMNS`, `EXPLAIN table`, and
`SHOW ... LIKE` introspection paths.

The feature is intentionally not full MySQL DDL reconstruction. It renders a
MySQL-style `CREATE TABLE` statement only for the descriptor shapes MyLite can
create today: persistent base tables with `INT`, `INTEGER`, `BIGINT`, optional
`UNSIGNED`, and `NULL`/`NOT NULL` column nullability. It does not add table
options, indexes, defaults, constraints, generated columns, auto-increment,
views, temporary tables, `sql_quote_show_create`, privileges, or
`INFORMATION_SCHEMA`.

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
  `docs/specs/baseline-count-aggregate/specs.md`,
  `docs/specs/baseline-show-columns-introspection/specs.md`,
  `docs/specs/baseline-explain-table-introspection/specs.md`, and
  `docs/specs/baseline-show-like-filters/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SHOW CREATE TABLE table_name` returns a result set with columns `Table` and
  `Create Table` for base tables.
- The `Table` cell is the logical table name without a schema qualifier, even
  when the target is schema-qualified.
- `SHOW CREATE TABLE db.table` does not require a selected default schema when
  `db` exists.
- Unqualified `SHOW CREATE TABLE table_name` fails with error `1046`, SQLSTATE
  `3D000`, when no default schema is selected.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown tables in known schemas fail with error `1146`, SQLSTATE `42S02`,
  and the message includes the resolved `schema.table` name.
- With the default `@@sql_quote_show_create = 1`, base-table output quotes
  table and column identifiers with backticks and doubles embedded backticks.
- `INTEGER` displays as `int`; `INT UNSIGNED`, `BIGINT`, and
  `BIGINT UNSIGNED` display as `int unsigned`, `bigint`, and
  `bigint unsigned`.
- Nullable integer-family columns with no explicit default display
  `DEFAULT NULL`, including both implicit nullable columns and columns declared
  `NULL`.
- Non-null integer-family columns with no explicit default display
  `NOT NULL`.
- The observed table option suffix for default InnoDB tables is
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`.
- Successful base-table `SHOW CREATE TABLE` leaves warning count `0` and makes
  the following `ROW_COUNT()` return `-1`.
- MySQL also accepts `SHOW CREATE TABLE view_name` and returns the view-specific
  four-column shape `View`, `Create View`, `character_set_client`, and
  `collation_connection`. Views remain outside this slice.
- `LIKE`, `WHERE`, `FROM schema`, `TEMPORARY`, and `FULL` modifiers are syntax
  errors on `SHOW CREATE TABLE`.
- Setting `sql_quote_show_create = 0` changes identifier quoting in MySQL. This
  slice does not add the system variable or unquoted rendering.

## Scope

The implementation must add:

- parser and AST support for `SHOW CREATE TABLE table_name`;
- unqualified and schema-qualified table-name resolution through the existing
  selected/default schema policy;
- reserved `_mylite_*` schema and table-name rejection before descriptor lookup;
- persistent base-table descriptor lookup from the MyLite catalog;
- descriptor-driven DDL rendering for current integer-family column
  descriptors and nullability;
- MySQL-observed result shape with `Table` and `Create Table` columns;
- MySQL-observed warning and row-count state for result-set statements;
- deterministic diagnostics for unsupported syntax and unresolved names;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `SHOW CREATE VIEW`, `SHOW CREATE DATABASE`, or any other `SHOW CREATE`
  variant;
- the MySQL view-specific `SHOW CREATE TABLE view_name` result shape;
- temporary tables, views, aliases, partitions, table options, row formats,
  charsets, collations, comments, indexes, primary keys, unique keys, foreign
  keys, check constraints, generated columns, generated invisible primary keys,
  explicit defaults, auto-increment, triggers, privileges, SQL modes,
  `sql_quote_show_create`, arbitrary SQLite metadata reads, arbitrary SQLite
  SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW CREATE TABLE` statements are result-set
  statements and therefore store `-1` as the connection-local previous row
  count.
- Lexer/parser/AST own syntax admission and source spans. They admit only
  `SHOW CREATE TABLE table_name` and stay independent of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner code resolves the target schema/table and rejects
  unsupported object kinds using MyLite descriptors.
- The catalog module remains authoritative for schema/table/column descriptors.
  This slice reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite catalog descriptors.
  It does not query SQLite table metadata, `sqlite_schema`, or pragma output.
- The result builder owns the `Table` and `Create Table` result columns and the
  rendered DDL text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  `SHOW CREATE TABLE` reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW CREATE TABLE table_name
```

`table_name` uses the existing table-name subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

MyLite Lemon-syntax grammar snippets:

```lemon
statement ::= show_create_table_statement.

show_create_table_statement ::= SHOW CREATE TABLE table_name.
```

The statement creates a dedicated AST node with one child: the table-name node.
No optional clauses are admitted in this slice.

## Schema and Table Resolution

Unqualified table names resolve through the current selected/default schema. If
no schema is selected, MyLite reports `1046` / `3D000` (`No database
selected`).

Schema-qualified table names resolve the named schema directly and do not
require a selected default schema. Unknown schemas report `1049` / `42000`
(`Unknown database`). Unknown tables in known schemas report `1146` / `42S02`
(`Table 'schema.table' doesn't exist`).

Any schema or table name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices. Once non-base-table descriptors exist, this slice must reject
them with a deterministic unsupported-object diagnostic.

Descriptor lookup uses the current catalog name comparison behavior. This slice
does not add MySQL filesystem-dependent table-name case folding or collation
semantics.

## DDL Rendering Semantics

For current descriptors, MyLite renders exactly one DDL cell shaped like the
observed MySQL default output:

```sql
CREATE TABLE `table_name` (
  `column_name` int DEFAULT NULL,
  `required_id` bigint unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
```

Rendering rules:

- the table name is the logical descriptor table name, not the physical SQLite
  table name and not schema-qualified;
- table and column identifiers are quoted with MySQL backticks;
- embedded backticks are doubled;
- columns are emitted in catalog `ordinal_position` order;
- `INT` and `INTEGER` logical descriptors render as `int`;
- `INT UNSIGNED` and `INTEGER UNSIGNED` render as `int unsigned`;
- `BIGINT` renders as `bigint`;
- `BIGINT UNSIGNED` renders as `bigint unsigned`;
- nullable columns render `DEFAULT NULL`;
- non-null columns render `NOT NULL`;
- a comma follows every column line except the last one;
- the table suffix is the fixed baseline default
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`.

Unknown future descriptor types must produce a deterministic unsupported
diagnostic until their `SHOW CREATE TABLE` representation is specified. MyLite
does not inspect SQLite column types because MyLite descriptors are the
authoritative logical schema.

## Result and Statement State

On success:

- the result has exactly two text columns: `Table` and `Create Table`;
- one row is returned for the resolved descriptor table;
- the first cell is the logical table name;
- the second cell is the rendered DDL text;
- `warning_count` is `0`;
- `affected_rows` remains `0` by existing MyLite result conventions for
  result-set statements;
- the connection-local previous row-count state is result-set state, so a
  following `SELECT ROW_COUNT()` returns `-1`;
- no catalog or physical row state is changed.

## Diagnostics

This slice must provide deterministic diagnostics for:

- syntax errors and unsupported `SHOW CREATE` variants;
- missing default schema for unqualified targets;
- unknown explicit schema;
- unknown table in a known schema;
- reserved `_mylite_*` schema and table names;
- unsupported object kinds once such descriptors exist;
- unsupported future descriptor column types;
- invalid or corrupt table descriptors with zero columns;
- result allocation failure;
- physical catalog read failures;
- public API misuse through the existing public API diagnostics.

Where existing MyLite diagnostics already match the previous lifecycle slices,
this feature reuses them. Unsupported syntax that MySQL rejects at parse time
may use MyLite's existing parse error wording.

## Physical Storage and File Format

`SHOW CREATE TABLE` does not generate SQLite SQL and does not bind SQLite
parameters. It reads MyLite-owned schema, table, and column descriptors through
catalog APIs and builds a MyLite-owned result set in memory.

The implementation must not mutate catalog rows, descriptor versions,
descriptor caches, catalog generation, `sqlite_schema_generation`, user rows,
physical SQLite tables, or the MyLite file preamble. It must not add indexes,
triggers, virtual tables, SQLite functions, or SQLite fork patches.

## Tests

Fast C tests must cover:

- successful unqualified and schema-qualified `SHOW CREATE TABLE`;
- all current integer-family logical descriptor renderings, including
  `INT`, `INTEGER`, `INT UNSIGNED`, `INTEGER UNSIGNED`, `BIGINT`, and
  `BIGINT UNSIGNED`;
- nullable, explicit `NULL`, and `NOT NULL` rendering;
- table and column identifier quoting, including embedded backticks;
- warning count, affected rows, result column names, row count, and following
  `ROW_COUNT()`;
- no catalog or user-row mutation;
- reopen persistence;
- result after table rename and after drop;
- missing default schema, unknown schema, unknown table, reserved names, and
  unsupported syntax;
- independent file-backed handles with independent catalog state;
- preamble preservation;
- zero-initialized cleanup for any new planner/result objects.

The MySQL expectation artifact must verify the observed MySQL 8.4.9 behavior
for successful rendering, diagnostics, deferred view behavior, unsupported
modifiers, warnings, and row-count state. A missing MySQL 8.4.9 runtime is a
blocker for changing the expectations.

## Compatibility Documentation

After implementation:

- update `COMPATIBILITY.md` to mark `SHOW CREATE TABLE` as partial/limited;
- update `docs/compatibility/sql-show-statements.md` for the exact persistent
  base-table subset;
- update `docs/compatibility/sql-table-ddl.md` only if the current limited
  table option exposure needs explicit cross-reference wording;
- do not overclaim full `SHOW CREATE TABLE`, views, temporary tables, table
  options, indexes, constraints, defaults, generated columns, auto-increment,
  privileges, or `sql_quote_show_create`.

## Verification

Before committing the implementation:

1. `cmake --build --preset dev`
2. Run the new CTest entry and the existing parser/schema/table lifecycle,
   table rename, row values, `SHOW TABLES`, `SHOW COLUMNS`, `EXPLAIN table`,
   and `SHOW ... LIKE` entries.
3. Run the MySQL 8.4.9 expectation script added for this feature.
4. `cmake --workflow --preset check`
5. Review the diff for parser independence, descriptor authority, result
   fidelity, row-count semantics, file-format safety, cleanup on failure,
   compatibility docs, and scope control.
