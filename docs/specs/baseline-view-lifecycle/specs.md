# Baseline View Lifecycle

## Goal

Add the first persistent view DDL lifecycle for MyLite-owned descriptors:

```sql
CREATE VIEW view_name AS SELECT ...
DROP VIEW [IF EXISTS] view_name [, view_name] ...
SHOW CREATE VIEW view_name
```

The slice is deliberately metadata-first. It adds durable view descriptors,
view column descriptors, dependency metadata for one source base table, `SHOW`
rows, and `INFORMATION_SCHEMA` rows. It does not execute `SELECT` from views
or allow DML through views yet. That keeps descriptor authority clear while
view expansion, invalidation, and updatability are still unimplemented.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, `CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-view.html>
- Official MySQL 8.4 Reference Manual, `DROP VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/drop-view.html>
- Official MySQL 8.4 Reference Manual, `SHOW CREATE VIEW`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-view.html>
- Official MySQL 8.4 Reference Manual, `SHOW TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-tables.html>
- Official MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- Official MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.VIEWS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-views-table.html>
- Official MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.VIEW_TABLE_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-view-table-usage-table.html>
- Existing MyLite view metadata shims:
  - `docs/specs/baseline-information-schema-views/specs.md`
  - `docs/specs/baseline-information-schema-view-table-usage/specs.md`
  - `docs/specs/baseline-show-full-tables/specs.md`
- Existing catalog and DDL lifecycle specs:
  - `docs/specs/baseline-catalog-foundation/specs.md`
  - `docs/specs/baseline-basic-table-lifecycle/specs.md`
  - `docs/specs/baseline-table-rename-lifecycle/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_view_lifecycle_expectations.sh`.

This specification is independently authored from official MySQL
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were run against the local `mysql:8.4.9` Docker runtime named
`mylite-mysql-849`.

Observed behavior shaping this slice:

- `CREATE VIEW v AS SELECT id, name AS label FROM t` succeeds when the target
  schema and source table exist.
- An unqualified target without a selected database fails with
  `1046 / 3D000 No database selected`.
- A schema-qualified target using an unknown schema fails with
  `1049 / 42000 Unknown database 'schema'`.
- A source table that does not exist fails with `1146 / 42S02
  Table 'schema.table' doesn't exist`.
- An unknown source column fails with `1054 / 42S22 Unknown column 'col' in
  'field list'`.
- Duplicate output column names fail with `1060 / 42S21 Duplicate column name
  'name'`.
- `SHOW FULL TABLES` lists persistent views with `Table_type = 'VIEW'`.
- `SHOW CREATE VIEW v` returns columns `View`, `Create View`,
  `character_set_client`, and `collation_connection`.
- `SHOW CREATE TABLE v` for a view returns the same view-shaped result columns
  and row as `SHOW CREATE VIEW v`.
- `SHOW CREATE VIEW t` where `t` is a base table fails with
  `1347 / HY000 'schema.t' is not VIEW`.
- `SHOW COLUMNS FROM v` returns the view's projected column metadata.
- `INFORMATION_SCHEMA.VIEWS` reports a row with `CHECK_OPTION = 'NONE'`,
  `SECURITY_TYPE = 'DEFINER'`, creation-time session character set and
  collation, and a canonical select definition.
- MySQL marks a simple direct-column view as `IS_UPDATABLE = 'YES'`. MyLite
  stores `NO` in this metadata until view DML is implemented, so clients are
  not told that unsupported view mutation is available.
- `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` reports one dependency row for the
  source table used by the admitted direct-column view.
- `INFORMATION_SCHEMA.TABLES` reports view rows with `TABLE_TYPE = 'VIEW'`,
  most storage fields as SQL `NULL`, and `TABLE_COMMENT = 'VIEW'`.
- `SHOW TABLE STATUS LIKE 'v'` emits one view row with most storage fields
  SQL `NULL`, `Create_time` populated, and `Comment = 'VIEW'`.
- `DROP VIEW v` drops an existing view.
- `DROP VIEW missing_v` fails with `1051 / 42S02 Unknown table
  'schema.missing_v'` and makes no changes.
- `DROP VIEW IF EXISTS missing_v` succeeds and appends a `1051 / 42S02` note.
- `DROP VIEW t` where `t` is a base table fails with
  `1347 / HY000 'schema.t' is not VIEW`.
- `DROP TABLE v` where `v` is a view fails as an unknown table
  (`1051 / 42S02`).
- `DROP VIEW IF EXISTS v, missing1, missing2` drops the existing view and
  appends one note for each missing view.
- MySQL parses and ignores `DROP VIEW ... RESTRICT` and `CASCADE`. MyLite
  defers these optional keywords for this slice.

## Supported SQL

The accepted `CREATE VIEW` subset is:

```sql
CREATE VIEW view_name AS
SELECT direct_projection [, direct_projection ...]
FROM source_table [AS alias]
```

`view_name` and `source_table` may be unqualified or schema-qualified. The
selected/default schema policy is the existing MyLite policy:

- one-part target names use the selected schema and fail with `1046 / 3D000`
  when none is selected;
- two-part target names resolve the explicit schema and fail with
  `1049 / 42000` when it does not exist;
- one-part source names use the selected schema, matching MySQL's source-name
  resolution;
- two-part source names resolve the explicit schema;
- `_mylite_*` schema and view names are reserved and rejected before any
  generated SQL or catalog mutation.

`direct_projection` is one of:

```sql
*
source_alias_or_table.*
column_name [AS alias]
source_alias_or_table.column_name [AS alias]
```

The source must be a persistent base table descriptor. View-on-view,
temporary-table sources, joins, derived tables, CTEs, tableless views,
subqueries, expressions, aggregate functions, scalar functions, predicates,
grouping, ordering, limits, locking clauses, index hints, `DISTINCT`,
`SQL_CALC_FOUND_ROWS`, `OR REPLACE`, column-name lists, `ALGORITHM`, `DEFINER`,
`SQL SECURITY`, `WITH CHECK OPTION`, and `TEMPORARY` views are out of scope.

The accepted `DROP VIEW` subset is:

```sql
DROP VIEW [IF EXISTS] view_name [, view_name] ...
```

The accepted `SHOW` subset is:

```sql
SHOW CREATE VIEW view_name
SHOW CREATE TABLE view_name
SHOW [FULL] TABLES [{FROM | IN} schema_name] [LIKE 'pattern' | WHERE predicate]
SHOW TABLE STATUS [{FROM | IN} schema_name] [LIKE 'pattern' | WHERE predicate]
SHOW [FULL] COLUMNS FROM view_name [{FROM | IN} schema_name] [LIKE 'pattern' | WHERE predicate]
```

Existing `SHOW TABLES`, `SHOW TABLE STATUS`, and `SHOW COLUMNS` filter limits
remain in force.

MyLite Lemon-syntax snippets:

```lemon
statement ::= create_view_statement.
statement ::= drop_view_statement.
statement ::= show_create_view_statement.

create_view_statement ::=
    CREATE VIEW table_name AS select_statement.

drop_view_statement ::=
    DROP VIEW drop_if_exists_opt table_name_list.

show_create_view_statement ::=
    SHOW CREATE VIEW table_name.
```

These snippets describe MyLite's intended grammar and are independently
authored from observed behavior and project parser conventions.

## Catalog And Storage Design

The catalog gains a persistent `MYLITE_CATALOG_TABLE_KIND_VIEW` table kind and
a `_mylite_catalog_views` descriptor table keyed by `table_id`.

The view descriptor stores:

- `table_id` for the matching `_mylite_catalog_tables` row;
- `view_definition` for `INFORMATION_SCHEMA.VIEWS.VIEW_DEFINITION`;
- `show_create_sql` for the `Create View` / `Create Table` output;
- `check_option`, always `NONE` for this slice;
- `is_updatable`, always `NO` for this slice;
- `definer`, currently MyLite's embedded `root@%` identity;
- `security_type`, always `DEFINER`;
- `character_set_client` and `collation_connection` captured from the session
  at create time;
- `source_schema_id` and `source_table_id` for the single admitted source
  table.

The corresponding `_mylite_catalog_tables` row has kind `VIEW`, a stable
internal physical name such as `_mylite_user_view_<table_id>`, creation time,
and ordinary descriptor version/generation fields. The physical name is
reserved for future view execution. This slice does not create a physical
SQLite view or table and does not mutate `sqlite_schema_generation`.

View columns are stored in `_mylite_catalog_columns` with copied source type,
nullability, charset/collation, visibility, and comments. Output aliases
replace the descriptor column names. View columns have no defaults,
auto-increment, generated expressions, indexes, foreign keys, or checks.

`DROP VIEW` deletes the view descriptor row, view column rows, and table row in
one catalog mutation. It does not execute SQLite DDL because no physical view
is created in this slice.

Dropping a schema containing views removes those view descriptors along with
base-table descriptors. Future physical view execution must define dependency
ordering before adding SQLite view drops.

## Ownership Boundary

- Public API: unchanged. Applications use `mylite_execute()` and existing
  result accessors.
- Statement context: successful DDL uses the existing non-row result
  convention, affected rows `0`, warning count `0`, and no result rows.
- Parser/AST: admits the new DDL/SHOW statements and stores target/source
  syntax without resolving descriptors.
- Runtime analyzer/planner: resolves target schema, source schema, source
  table, projection columns, duplicate output names, and object-kind
  diagnostics against MyLite descriptors.
- Catalog module: owns durable view descriptors, table kind validation,
  schema migration, descriptor reads, descriptor deletion, and generation
  updates. MyLite descriptors remain authoritative; SQLite metadata is not
  consulted.
- Result builder: owns `SHOW CREATE VIEW`, `SHOW TABLES`, `SHOW TABLE STATUS`,
  `SHOW COLUMNS`, and information-schema result rows.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants must not change.
- SQLite physical storage: no SQLite fork patch or public SQLite extension hook
  is needed. The implementation is MyLite wrapper/metadata work only.

## Semantics

`CREATE VIEW`:

- resolves and validates the target before catalog mutation;
- rejects target collisions with any persistent table or view as
  `1050 / 42S01 Table 'name' already exists`;
- rejects reserved `_mylite_*` target names;
- resolves the source table from descriptors and requires a persistent base
  table;
- rejects duplicate output column names case-insensitively with
  `1060 / 42S21`;
- stores the view, projected columns, and source dependency in one catalog
  mutation;
- returns no result set and warning count `0`.

`DROP VIEW`:

- resolves all targets before mutation;
- without `IF EXISTS`, any missing view makes the statement fail with
  `1051 / 42S02` and no descriptors are changed;
- with `IF EXISTS`, missing views append notes and existing views are dropped;
- base-table targets fail with `1347 / HY000`;
- reserved targets are rejected before mutation;
- returns no result set and warning count equal to the number of missing
  targets under `IF EXISTS`.

`SHOW CREATE VIEW` and `SHOW CREATE TABLE` for views:

- resolve the target and require a persistent view descriptor;
- return `View`, `Create View`, `character_set_client`, and
  `collation_connection` columns;
- render `CREATE ALGORITHM=UNDEFINED DEFINER=\`root\`@\`%\` SQL
  SECURITY DEFINER VIEW ... AS select ...`;
- use session-independent stored text captured at creation time.

`SHOW TABLES` and `SHOW FULL TABLES`:

- include view descriptors along with base-table descriptors;
- ordinary `SHOW TABLES` lists view names in the existing table-name column;
- `FULL` reports `VIEW` for view descriptors.

`SHOW TABLE STATUS`:

- includes view descriptors;
- reports `Name`, `Create_time`, and `Comment = 'VIEW'`;
- emits SQL `NULL` for storage fields that MySQL does not populate for views;
- preserves existing filter behavior.

`SHOW COLUMNS`:

- uses the stored view column descriptors;
- reports view column type, nullability, aliases, charset/collation, and
  comments through the existing descriptor formatter;
- reports empty `Key` and `Extra`, and SQL `NULL` defaults.

`INFORMATION_SCHEMA`:

- `VIEWS` emits one row per stored view descriptor;
- `VIEW_TABLE_USAGE` emits one row per stored baseline view dependency;
- `TABLES` emits view rows with `TABLE_TYPE = 'VIEW'`;
- `COLUMNS` emits view column rows from descriptors;
- index, constraint, partition, statistics, key-usage, check, referential, and
  privilege metadata remain base-table-only until separate specs include
  views.

## Diagnostics

The slice must define deterministic diagnostics for:

- syntax outside the admitted grammar: `1064 / 42000` parse error or existing
  unsupported diagnostics;
- missing default schema: `1046 / 3D000`;
- unknown target schema on `CREATE VIEW`: `1049 / 42000`;
- unknown target in `DROP VIEW`: `1051 / 42S02`;
- unknown source schema/table: MySQL-compatible unknown schema/table errors;
- existing target object: `1050 / 42S01`;
- base table supplied to view-only statements: `1347 / HY000`;
- view supplied to base-table-only statements such as `DROP TABLE`: MySQL-like
  unknown-table behavior where verified;
- unknown projection columns: `1054 / 42S22`;
- duplicate output columns: `1060 / 42S21`;
- source object kinds outside persistent base tables: deterministic MyLite
  unsupported diagnostics;
- unsupported view options or SELECT shapes: deterministic unsupported
  diagnostics;
- allocation failures: existing public API allocation diagnostics;
- catalog corruption or SQLite failures: existing internal/physical error
  conventions.

## Performance

The implementation does not materialize view data and does not create physical
SQLite views. `CREATE VIEW` resolves descriptors and copies column metadata
once. Metadata reads iterate catalog descriptors in the same in-memory result
builder paths used by existing `SHOW` and `INFORMATION_SCHEMA` features.

Future view execution should prefer descriptor-backed query rewriting or
generated SQLite views only after dependency invalidation, updatability, and
type metadata behavior are specified.

## Tests

Add a focused C runtime test and update existing metadata tests. Coverage must
include:

- parser acceptance for `CREATE VIEW`, `DROP VIEW`, and `SHOW CREATE VIEW`;
- parser rejection for `CREATE OR REPLACE VIEW`, `CREATE VIEW ... (columns)`,
  `ALGORITHM`, `DEFINER`, `SQL SECURITY`, `WITH CHECK OPTION`,
  `DROP VIEW ... CASCADE`, and `DROP VIEW ... RESTRICT` for this slice;
- successful `CREATE VIEW` over a persistent base table using `*`, direct
  columns, qualified columns, qualified wildcard, source aliases, and output
  aliases;
- stored view columns, duplicate output column diagnostics, unknown source
  column diagnostics, unknown source table diagnostics, and source view/temporary
  table rejection;
- schema-qualified and unqualified target/source resolution, missing default
  schema, unknown explicit schema, and reserved `_mylite_*` names;
- target collisions with base tables and existing views;
- `SHOW CREATE VIEW` and `SHOW CREATE TABLE` for views;
- `SHOW CREATE VIEW` on base tables and unknown targets;
- `SHOW TABLES`, `SHOW FULL TABLES`, and filters including `WHERE Table_type =
  'VIEW'`;
- `SHOW TABLE STATUS` view rows and filters;
- `SHOW COLUMNS` / `SHOW FULL COLUMNS` for views;
- `INFORMATION_SCHEMA.VIEWS`, `VIEW_TABLE_USAGE`, `TABLES`, and `COLUMNS`
  rows for stored views;
- `DROP VIEW`, multi-view drop, missing target atomicity without `IF EXISTS`,
  missing-target notes with `IF EXISTS`, and `DROP TABLE` against a view;
- persistence across close/reopen, independent file-backed handles, and drop
  database cleanup;
- `.mylite` preamble preservation and unchanged shifted SQLite payload
  invariants;
- zero-initialized cleanup for new statement/planner/catalog structs;
- existing parser, catalog, basic table lifecycle, rename lifecycle,
  information-schema views, view-table-usage, show-full-tables, show-columns,
  show-create-table, show-table-status, file-format, VFS, and runtime lifecycle
  tests.

Verification before commit:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.(parser|runtime)\.(view_lifecycle|information_schema_views|information_schema_view_table_usage|show_full_tables|show_columns|show_create_table|show_table_status)$' \
  --output-on-failure
packages/libmylite/tests/mysql_baseline_view_lifecycle_expectations.sh
cmake --workflow --preset check
```

## Compatibility

`CREATE VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`, view rows in
`INFORMATION_SCHEMA.VIEWS`, and view dependency rows in
`INFORMATION_SCHEMA.VIEW_TABLE_USAGE` move to partial support. Full view
execution, view-on-view dependencies, updatable views, check options,
definer/security semantics, privileges, `ALTER VIEW`, `CREATE OR REPLACE VIEW`,
column-name lists, physical SQLite view execution, triggers on views, and full
view invalidation remain unsupported.
