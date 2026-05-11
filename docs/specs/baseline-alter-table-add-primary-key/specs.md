# Baseline ALTER TABLE ADD PRIMARY KEY

## Summary

This phase expands the primary-key baseline from `CREATE TABLE`-time primary
keys to one descriptor-driven `ALTER TABLE ... ADD PRIMARY KEY (column)` action
for persistent base tables. The supported column is one existing integer-family
descriptor column. Existing rows are validated before mutation, the column
becomes logically `NOT NULL`, MyLite adds a durable primary-key descriptor and a
generated physical SQLite unique index, and existing descriptor-owned secondary
indexes are preserved.

This is intentionally not full MySQL key DDL. It does not add string primary
keys, composite keys, named constraints, `DROP PRIMARY KEY`, key options,
multi-action `ALTER TABLE`, or `ALTER ... ADD UNIQUE`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline secondary index lifecycle:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline information schema constraints:
  `docs/specs/baseline-information-schema-constraints/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_add_primary_key_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script for this feature records runtime probes for the exact
supported and deferred surface. Observed behavior that defines this slice:

- `ALTER TABLE t ADD PRIMARY KEY (id)` succeeds when `id` contains no `NULL`
  values and no duplicates.
- Successful `ADD PRIMARY KEY` reports `ROW_COUNT() == 0` and
  `@@warning_count == 0`.
- A nullable integer column becomes `NOT NULL` in `SHOW COLUMNS` and
  `SHOW CREATE TABLE`.
- If the column had no non-`NULL` default, `SHOW COLUMNS` still displays
  `NULL` in its `Default` column, `SHOW CREATE TABLE` omits a default clause,
  and later omitted-column inserts fail with `1364 / HY000`.
- A non-`NULL` integer default is preserved. Later omitted-column inserts use
  that default and can fail with a duplicate primary-key error.
- Existing nonunique and unique secondary indexes are preserved and rendered
  after the new primary key.
- `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE` expose the new primary-key descriptor.
- Existing duplicate values fail with `1062 / 23000` and a duplicate-entry
  message for key `<table>.PRIMARY`.
- Existing `NULL` values fail with `1138 / 22004` and
  `Invalid use of NULL value`.
- A missing key column fails with `1072 / 42000`.
- A table that already has a primary key fails with `1068 / 42000`.
- MySQL accepts wider forms such as composite primary keys, string primary
  keys, named primary-key constraints, key options, and multi-action
  `ALTER TABLE`; those remain deferred here.

## Scope

Supported:

- persistent base tables only;
- one `ALTER TABLE table_name ADD PRIMARY KEY (column_name)` action;
- unqualified and schema-qualified table names using the existing selected
  schema policy;
- one unqualified key column;
- target column must be an existing MyLite integer-family descriptor column:
  `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`, `BIGINT`, the
  `INT1` / `INT2` / `INT3` / `INT4` / `INT8` aliases, and admitted signed or
  unsigned forms;
- existing rows must contain no `NULL` values in the target column;
- existing rows must contain no duplicate non-`NULL` target values;
- successful execution adds one primary-key index descriptor, one index-column
  descriptor, one generated SQLite unique index, and makes the descriptor column
  logically `NOT NULL`;
- existing supported unique and nonunique secondary-index descriptors and
  physical indexes remain intact;
- future `INSERT`, `INSERT IGNORE`, `INSERT ... SET`, `UPDATE`, `TRUNCATE`,
  `CREATE TABLE ... LIKE`, `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `SHOW TABLE STATUS`, and supported `INFORMATION_SCHEMA` surfaces observe the
  new primary key through existing descriptor-driven paths;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation.

Deferred:

- `ALTER TABLE ... DROP PRIMARY KEY`;
- `ALTER TABLE ... ADD CONSTRAINT name PRIMARY KEY (...)`;
- composite primary keys;
- primary keys on `CHAR`, `VARCHAR`, `TEXT`, `DECIMAL`, `DATE`, `DATETIME`,
  binary, JSON, generated, expression, hidden, or unsupported descriptor
  columns;
- string primary keys until MyLite has collation-aware string key comparison
  compatible with the fixed `utf8mb4_0900_ai_ci` baseline;
- key-part prefixes, `ASC` / `DESC`, `USING`, comments, visibility, storage
  attributes, parser options, algorithms, locks, online DDL controls,
  temporary tables, partitions, views, privileges, and implicit-commit
  emulation;
- multi-action `ALTER TABLE`, including combined `ADD PRIMARY KEY` plus
  `DROP`, `ADD COLUMN`, `MODIFY`, `CHANGE`, `ADD UNIQUE`, or table options;
- standalone `CREATE INDEX` / `CREATE UNIQUE INDEX`;
- auto-increment definition changes, generated invisible primary keys,
  foreign keys, cascades, triggers, and check constraints;
- optimizer/index-use guarantees and protocol flag changes beyond existing
  result conventions.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns SQL text lifetime, statement diagnostics, warning
  count, affected rows, transaction completion, and cleanup on failure.
- Parser/AST: admits only the narrow `ALTER TABLE ... ADD PRIMARY KEY (...)`
  shape and preserves source spans. It does not inspect descriptors or rows.
- Analyzer/planner/runtime: resolves the target table and key column from
  MyLite descriptors, rejects unsupported shapes, validates existing physical
  rows using descriptor-built SQLite statements, plans catalog mutations, and
  creates the physical index.
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
  for MySQL-compatible validation, diagnostics, and `NULL` rejection.

## Supported Grammar

The feature extends the existing single-action `ALTER TABLE` grammar with one
action:

```sql
ALTER TABLE table_name ADD PRIMARY KEY (column_name)
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

The parser may reuse the existing `primary_key_definition` AST node. The
semantic subset requires exactly one unqualified `identifier` part. Qualified
parts and multiple parts are rejected by analysis with deterministic
unsupported diagnostics rather than being treated as supported behavior.

Unsupported forms that are not admitted by the parser remain syntax errors,
including `DROP PRIMARY KEY` until its own feature is specified.

## Schema and Name Resolution

The target table follows existing MyLite table-name policy:

- unqualified table names require a selected/default schema;
- schema-qualified names use the explicit schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` names use existing diagnostics;
- only persistent base-table descriptors are supported.

The key column is resolved within the target table descriptor:

- the column name must be unqualified;
- reserved `_mylite_*` column names are rejected before generating SQLite SQL;
- unknown columns fail with `1072 / 42000`, matching MySQL's key-column
  diagnostic for the supported subset;
- descriptor name matching follows current MyLite catalog identifier policy.

## Descriptor Semantics

On success:

- insert a primary index descriptor with logical name `PRIMARY`,
  `kind = PRIMARY`, `is_unique = 1`, and a generated physical name such as
  `_mylite_user_index_<index_id>`;
- insert one index-column descriptor with ordinal `1` that references the
  target column descriptor;
- update the target column descriptor so `is_nullable = false`;
- preserve a non-`NULL` descriptor default exactly as current integer default
  logic stores it;
- if the previous descriptor default was implicit nullable `NULL`, explicit
  `DEFAULT NULL`, or dropped/no explicit default, record the primary-key column
  as having no explicit default so future omitted-column inserts fail like
  MySQL while `SHOW COLUMNS` still displays `NULL`;
- preserve column id, ordinal position, physical name, type metadata,
  visibility, auto-increment state if any future descriptor can reach this
  state, table descriptor identity, and existing row values.

If validation or any catalog/physical step fails, the table descriptor, column
descriptor, index descriptors, physical row table, existing physical indexes,
catalog generation, and SQLite schema generation must roll back atomically to
the pre-statement state.

## Existing Row Validation

Validation must run before catalog mutation:

- `NULL` validation: if any existing target value is `NULL`, fail with
  `1138 / 22004` and `Invalid use of NULL value`;
- duplicate validation: if any non-`NULL` value appears more than once, fail
  with `1062 / 23000` and
  `Duplicate entry '<value>' for key '<table>.PRIMARY'`;
- validation values use MyLite descriptor conversion/readback formatting, not
  SQLite ad hoc text formatting.

Validation should stay SQLite-side:

- detect `NULL` with an indexed/limited existence query shape when possible;
- detect duplicates with a grouped SQLite query over the physical column;
- bind no SQL literals from user text;
- quote all generated identifiers;
- materialize at most the first conflicting value needed for a deterministic
  diagnostic.

## Physical SQLite Handling

This feature uses public SQLite APIs and does not require a SQLite fork patch.

MyLite must not rewrite or rebuild the physical table just to make SQLite's
column declaration `NOT NULL`. MyLite descriptors are the MySQL metadata
authority, and all public DML reaches SQLite through MyLite conversion and
nullability checks. After row validation, MyLite creates an ordinary SQLite
unique index from descriptor physical names:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("<physical_column_name>")
```

Rules:

- quote every generated identifier;
- generate SQL only from descriptor-owned stable physical names;
- create the physical index inside the same statement transaction as the
  catalog mutation;
- if physical index creation fails, map expected unique constraint failures to
  MySQL diagnostics when possible and otherwise surface a deterministic
  physical-execution failure;
- preserve existing physical secondary and unique indexes because no table
  rebuild occurs.

## DML and Metadata After Success

Existing DML paths should observe the newly loaded primary-key descriptor:

- future `INSERT ... VALUES`, `INSERT ... SET`, and one-assignment `UPDATE`
  reject `NULL` values in the primary-key column and report duplicate-key
  errors through the existing primary-key path;
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` demote duplicate-key
  and bad-`NULL` rows according to existing limited primary-key/ignore behavior;
- omitted-column inserts into a primary-key column with no explicit non-`NULL`
  default fail with `1364 / HY000`;
- omitted-column inserts into a primary-key column with a preserved non-`NULL`
  default use that default and can fail with a duplicate primary-key error;
- `TRUNCATE TABLE` preserves the primary-key descriptor and resets row state as
  existing primary-key tables do;
- `CREATE TABLE ... LIKE` clones the primary-key descriptor;
- `CREATE TABLE ... SELECT` continues to omit indexes and constraints.

Metadata:

- `SHOW COLUMNS` reports `PRI` and `Null = NO` for the target column;
- `SHOW CREATE TABLE` renders the primary key after column definitions and
  before unique/nonunique secondary indexes;
- `SHOW INDEX` renders the `PRIMARY` row with `Non_unique = 0`;
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` reports `PRI`;
- `INFORMATION_SCHEMA.STATISTICS` includes the `PRIMARY` index row;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` includes one `PRIMARY KEY` row;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` includes one row for the key column;
- `SHOW TABLE STATUS` remains descriptor-driven and must not regress.

## Result Semantics

Successful `ALTER TABLE ... ADD PRIMARY KEY` returns through the existing
non-row DDL result convention:

- no result rows;
- no result columns;
- `affected_rows == 0`;
- `warning_count == 0`;
- previous diagnostics are updated like existing successful non-row statements.

## Diagnostics

The supported subset uses these diagnostics:

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted parser grammar | `1064 / 42000` syntax error |
| No selected schema for an unqualified table | existing `1046 / 3D000` |
| Unknown schema | existing `1049 / 42000` |
| Unknown table | existing `1146 / 42S02` |
| Reserved `_mylite_*` table name | existing incorrect-table-name diagnostic |
| Reserved `_mylite_*` column name | existing incorrect-column-name diagnostic |
| Unsupported object kind | deterministic MyLite unsupported-base-table diagnostic |
| Existing primary key | `1068 / 42000`, `Multiple primary key defined` |
| Missing key column | `1072 / 42000`, `Key column '<name>' doesn't exist in table` |
| Qualified key column | deterministic unsupported key-column diagnostic |
| Multiple key parts | deterministic unsupported one-column diagnostic |
| Unsupported target descriptor type | deterministic unsupported integer-primary-key diagnostic |
| Existing `NULL` target value | `1138 / 22004`, `Invalid use of NULL value` |
| Existing duplicate target value | `1062 / 23000`, `Duplicate entry '<value>' for key '<table>.PRIMARY'` |
| Multi-action `ALTER TABLE` | deterministic unsupported/syntax diagnostic |
| Named constraint, key options, algorithms, locks | deterministic unsupported/syntax diagnostic |
| Allocation failure | `MYLITE_NOMEM`, cleanup-safe |
| Physical SQLite failure | deterministic MyLite execution diagnostic unless mapped above |

## Performance

This DDL is allowed to scan the target table, like MySQL must validate existing
rows before adding a primary key. The implementation must avoid pulling all rows
into C memory. It should use SQLite-side validation queries and materialize only
the first failing value for diagnostics, then create a physical SQLite unique
index so later compatible lookups and DML can stay on SQLite's normal b-tree
path. No SQLite fork patch is justified for this slice.

## Tests

Add a new C test binary, preferably
`packages/libmylite/tests/runtime_alter_table_add_primary_key_test.c`, with
dotted CTest name `libmylite.runtime.alter_table_add_primary_key`.

Coverage:

- parser accepts `ALTER TABLE t ADD PRIMARY KEY (id)` for unqualified and
  schema-qualified table names;
- successful add on empty and nonempty tables;
- target integer families and signed/unsigned boundary values for current
  physical ranges;
- nullable target column becomes `NOT NULL`;
- implicit `DEFAULT NULL`, explicit `DEFAULT NULL`, dropped/no explicit
  default, and preserved non-`NULL` integer defaults;
- future omitted inserts after success for no-default and defaulted primary-key
  columns;
- existing duplicate-row and `NULL`-row validation failures with no mutation;
- target tables that already have primary keys;
- tables with existing supported nonunique and unique secondary indexes;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.COLUMNS`, `STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- DML after success: insert, insert ignore, update, truncate, create-like,
  create-table-select omission, rename/drop, reopen persistence, independent
  file-backed handles, and preamble preservation;
- schema-qualified and unqualified resolution, including missing default
  schema, unknown schema, unknown table, and reserved names;
- unsupported forms: named constraints, composite keys, string/decimal/date/
  datetime/text keys, qualified key parts, multiple actions, key options,
  algorithms, locks, temporary tables, views, standalone index statements, and
  `DROP PRIMARY KEY`;
- zero-initialized cleanup for new plan objects;
- no public API changes or misuse-surface changes.

Add and run
`packages/libmylite/tests/mysql_baseline_alter_table_add_primary_key_expectations.sh`
against MySQL 8.4.9 before implementing runtime behavior.

## Compatibility Documentation

Implementation should update:

- `COMPATIBILITY.md` `ADD PRIMARY KEY` row from `❌` to limited `🟡`;
- `docs/compatibility/sql-indexes-constraints.md` `ADD PRIMARY KEY` row with
  the exact subset;
- `docs/compatibility/sql-table-ddl.md` `ALTER TABLE` row to mention this
  single action;
- `docs/compatibility/metadata-information-schema.md` only if wording needs to
  mention that `TABLE_CONSTRAINTS` and `KEY_COLUMN_USAGE` rows can also come
  from post-create primary keys;
- no docs should claim composite, string, foreign-key, full index, lock,
  algorithm, or `DROP PRIMARY KEY` support.
