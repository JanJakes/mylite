# Baseline ALTER TABLE ADD Composite Primary Key

## Summary

This phase expands `ALTER TABLE ... ADD PRIMARY KEY` from one existing
integer-family column to ordered composite primary keys over two or more
existing integer-family columns on persistent base tables.

The feature closes the main lifecycle gap left by create-time composite primary
keys. Existing rows are validated before mutation, every key part becomes
logically `NOT NULL`, MyLite stores one primary index descriptor plus ordered
index-column descriptors, and a generated SQLite unique index enforces future
writes. The catalog remains the compatibility authority; SQLite schema text is
not used as metadata.

This is intentionally not full MySQL key DDL. String keys, named constraints,
key options, `DROP PRIMARY KEY`, auto-increment conversion, foreign keys, and
multi-action `ALTER TABLE` remain separate features.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline ALTER TABLE ADD PRIMARY KEY:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- Baseline composite primary key lifecycle:
  `docs/specs/baseline-composite-primary-key-lifecycle/specs.md`
- Baseline information schema constraints:
  `docs/specs/baseline-information-schema-constraints/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_add_composite_primary_key_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script for this feature records the defining runtime probes.
Observed behavior:

- `ALTER TABLE t ADD PRIMARY KEY (a,b)` succeeds for an existing table when
  all key-part columns contain no `NULL` values and no duplicate tuples.
- Successful execution reports `ROW_COUNT() == 0` and `@@warning_count == 0`.
- Each key part becomes `NOT NULL`, `SHOW COLUMNS` renders `PRI` for every key
  part, `SHOW INDEX` reports one `PRIMARY` index with ordered
  `Seq_in_index` rows, and `SHOW CREATE TABLE` renders a `PRIMARY KEY` clause
  over the declared key parts.
- Limited `INFORMATION_SCHEMA.COLUMNS`, `TABLE_CONSTRAINTS`,
  `KEY_COLUMN_USAGE`, and `STATISTICS` expose the ordered composite key.
- Existing duplicate tuples fail with `1062 / 23000` and a duplicate entry
  shaped like `Duplicate entry '1-2' for key 'dup.PRIMARY'`.
- Existing `NULL` values in any key part fail with `1138 / 22004` and
  `Invalid use of NULL value`.
- Repeated key parts fail with `1060 / 42S21`. Unknown key parts fail with
  `1072 / 42000`. A table that already has a primary key fails with
  `1068 / 42000`.
- Non-`NULL` integer defaults on key parts are preserved. Explicit
  `DEFAULT NULL` and implicit nullable defaults are normalized to no explicit
  default while `SHOW COLUMNS` still displays `NULL`.
- MySQL accepts wider forms including string composite primary keys,
  `ADD CONSTRAINT name PRIMARY KEY (...)`, and `ADD PRIMARY KEY USING BTREE`.
  MyLite defers those forms in this slice.

## Scope

Supported:

- persistent base tables only;
- one single-action
  `ALTER TABLE table_name ADD PRIMARY KEY (column_name, column_name[, ...])`;
- one-part `ALTER TABLE ... ADD PRIMARY KEY (column)` remains supported by the
  same path;
- unqualified and schema-qualified table names using the existing selected
  schema policy;
- two or more unqualified key columns for the new composite behavior;
- all key parts must resolve to existing MyLite integer-family descriptors:
  `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`, `BIGINT`, the
  `INT1` / `INT2` / `INT3` / `INT4` / `INT8` aliases, and admitted signed or
  unsigned forms;
- existing rows must contain no `NULL` value in any key part;
- existing rows must contain no duplicate non-`NULL` key tuple;
- successful execution adds one primary-key index descriptor, ordered
  index-column descriptors, one generated SQLite unique index, and marks every
  key-part descriptor `NOT NULL`;
- existing supported unique and nonunique secondary indexes are preserved;
- future supported `INSERT`, `INSERT IGNORE`, `INSERT ... SET`, `UPDATE`,
  `TRUNCATE`, `CREATE TABLE ... LIKE`, `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, and supported `INFORMATION_SCHEMA` surfaces observe the new
  composite primary key through existing descriptor-driven paths;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation.

Deferred:

- `ALTER TABLE ... DROP PRIMARY KEY`;
- `ALTER TABLE ... ADD CONSTRAINT name PRIMARY KEY (...)`;
- string, decimal, temporal, `TEXT`, generated, expression, hidden, or
  unsupported descriptor key parts;
- string primary keys until collation-aware string key comparison compatible
  with the fixed `utf8mb4_0900_ai_ci` baseline is specified;
- auto-increment creation or conversion involving composite primary keys;
- key-part prefixes, `ASC` / `DESC`, `USING`, comments, visibility, storage
  attributes, parser options, algorithms, locks, online DDL controls,
  temporary tables, partitions, views, privileges, and implicit-commit
  emulation;
- multi-action `ALTER TABLE`, including combined `ADD PRIMARY KEY` plus
  `ADD COLUMN`, `DROP`, `MODIFY`, `CHANGE`, `ADD KEY`, or table options;
- standalone `CREATE INDEX` / `CREATE UNIQUE INDEX`;
- generated invisible primary keys, foreign keys, cascades, triggers, check
  constraints, optimizer/index-use guarantees, and protocol flag changes beyond
  existing result conventions.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns SQL text lifetime, statement diagnostics, warning
  count, affected rows, transaction completion, and cleanup on failure.
- Parser/AST: already admits `primary_key_part_list` and preserves source
  spans. It does not inspect descriptors or physical rows.
- Analyzer/planner/runtime: resolves the target table and ordered key parts
  from MyLite descriptors, rejects unsupported shapes, validates existing
  physical rows using descriptor-built SQLite statements, plans catalog
  mutations, and creates the physical index.
- Catalog module: owns durable table, column, index, and index-column
  descriptor rows, descriptor generations, and cache invalidation. Primary-key
  metadata is not read back from SQLite schema text.
- Result/introspection builders: render primary-key metadata from descriptors
  through existing `SHOW` and `INFORMATION_SCHEMA` paths.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only SQLite payload catalog/index data and must not touch the
  preamble.
- SQLite physical storage: stores row values and enforces future duplicate
  primary-key writes with a generated unique index. MyLite remains responsible
  for MySQL-compatible validation, diagnostics, nullability, conversion, and
  metadata.

## Supported Grammar

The feature uses the existing single-action `ALTER TABLE` grammar shape:

```sql
ALTER TABLE table_name ADD PRIMARY KEY (column_name, column_name[, ...])
```

MyLite Lemon-style snippet:

```lemon
alter_table_add_primary_key_statement ::=
    ALTER TABLE table_name ADD primary_key_definition.

primary_key_definition ::= PRIMARY KEY LPAREN primary_key_part_list RPAREN.

primary_key_part_list ::= primary_key_part.
primary_key_part_list ::= primary_key_part_list COMMA primary_key_part.

primary_key_part ::= identifier.
primary_key_part ::= qualified_identifier.
```

The parser may continue admitting qualified parts so analysis can return a
deterministic unsupported diagnostic. The supported semantic subset requires
unqualified `identifier` parts. Unsupported forms that the parser does not
admit remain syntax errors.

## Schema and Name Resolution

The target table follows existing table-name policy:

- unqualified table names require a selected/default schema;
- schema-qualified names use the explicit schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` names use existing diagnostics;
- only persistent base-table descriptors are supported.

Each key part is resolved within the target table descriptor:

- key-part names must be unqualified;
- reserved `_mylite_*` column names are rejected before generating SQLite SQL;
- unknown columns fail with `1072 / 42000`;
- duplicate key parts fail with `1060 / 42S21`;
- descriptor name matching follows current MyLite catalog identifier policy.

## Descriptor Semantics

On success:

- insert a primary index descriptor with logical name `PRIMARY`,
  `kind = PRIMARY`, `is_unique = 1`, and a generated physical name such as
  `_mylite_user_index_<index_id>`;
- insert one index-column descriptor per key part with ordinal positions
  starting at `1`;
- update every key-part column descriptor so `is_nullable = false`;
- preserve non-`NULL` descriptor defaults exactly as current default logic
  stores them;
- if a previous descriptor default was implicit nullable `NULL`, explicit
  `DEFAULT NULL`, or dropped/no explicit default, record the key-part column as
  having no explicit default so future omitted-column inserts fail like MySQL
  while `SHOW COLUMNS` still displays `NULL`;
- preserve column ids, ordinal positions, physical names, type metadata,
  visibility, existing auto-increment metadata reachable from prior features,
  table descriptor identity, secondary indexes, and existing row values.

If validation or any catalog/physical step fails, the table descriptor, column
descriptors, index descriptors, physical row table, existing physical indexes,
catalog generation, and SQLite schema generation must roll back atomically to
the pre-statement state.

## Existing Row Validation

Validation runs before catalog mutation:

- `NULL` validation fails with `1138 / 22004` if any existing row has `NULL` in
  any key part;
- duplicate validation fails with `1062 / 23000` if any complete key tuple
  appears more than once;
- duplicate diagnostics format integer key values in declared key-part order
  joined with `-`, matching observed MySQL 8.4.9 behavior for the admitted
  integer values.

Validation should stay SQLite-side:

- detect `NULL` with a `SELECT 1 ... WHERE col1 IS NULL OR col2 IS NULL ...
  LIMIT 1` shape;
- detect duplicates with a grouped query over all key parts and materialize at
  most the first conflicting tuple needed for diagnostics;
- quote every generated identifier;
- bind no SQL literals from user text.

## Physical SQLite Handling

This feature uses public SQLite APIs and does not require a SQLite fork patch.

MyLite must not rebuild the physical table just to make SQLite column
declarations `NOT NULL`. MyLite descriptors are the MySQL metadata authority,
and public DML reaches SQLite through MyLite conversion and nullability checks.
After row validation, MyLite creates an ordinary SQLite unique index from
descriptor physical names:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("<physical_column_1>", "<physical_column_2>", ...)
```

Rules:

- generate SQL only from descriptors and stable physical names;
- quote every identifier;
- use SQLite schema execution helpers and normal statement transactions;
- do not use SQLite `WITHOUT ROWID` tables;
- do not modify the bundled SQLite fork.

## Result Semantics

Successful execution returns through the existing non-row statement result
conventions:

- no row result set;
- `affected_rows == 0`;
- `warning_count == 0`;
- statement diagnostics remain clear.

## Diagnostics

The supported subset uses these diagnostics:

- syntax errors: existing parser diagnostics;
- unsupported grammar or semantic forms: deterministic MyLite unsupported
  diagnostics unless a MySQL-compatible diagnostic already exists locally;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved schema/table/column names: existing reserved-name diagnostics;
- unsupported object kind: MyLite unsupported diagnostic;
- existing primary key: `1068 / 42000`;
- unknown key part: `1072 / 42000`;
- duplicate key part: `1060 / 42S21`;
- unsupported key-part type: existing integer-primary-key unsupported
  diagnostic;
- existing `NULL` in a key part: `1138 / 22004`;
- existing duplicate tuple: `1062 / 23000`;
- physical SQLite failure: existing physical-row/schema failure diagnostics;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics.

## Performance Boundary

The implementation must not materialize full table contents in C memory.
Existing-row validation is delegated to SQLite grouping/filtering and reads
only the first conflicting tuple for diagnostics. Future DML enforcement uses
the generated SQLite unique index plus MyLite duplicate diagnostic mapping.

The generated index may scan the physical table during creation, matching the
expected cost of adding a primary key to existing data. No optimizer/index-use
claim is made for query planning in this slice.

## Test Plan

- MySQL 8.4.9 expectation script covering successful composite add, metadata,
  defaults, duplicate tuples, existing `NULL`, duplicate key parts, unknown key
  parts, existing primary key, schema-qualified targets, and deferred string,
  named-constraint, and key-option forms.
- C runtime tests extending the existing ALTER ADD PRIMARY KEY test binary for
  composite success, metadata, DML enforcement, duplicate existing rows, NULL
  validation, default normalization, schema resolution, reopen persistence,
  rename/drop interactions, independent file-backed handles, unsupported
  syntax, and preamble preservation.
- Existing primary-key, composite primary-key, insert/update/delete, row-value,
  show/information-schema, parser, storage, and file-format tests must keep
  passing.
