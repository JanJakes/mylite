# Baseline ALTER TABLE ADD INDEX Lifecycle

## Summary

This phase adds the next core schema-management building block for persistent
MyLite base tables: descriptor-owned single-column nonunique secondary indexes
added after table creation with:

```sql
ALTER TABLE table_name ADD INDEX [index_name] (column_name)
ALTER TABLE table_name ADD KEY [index_name] (column_name)
```

The goal is to support common migration DDL, keep MyLite catalog descriptors as
the logical authority, create matching physical SQLite indexes for efficient
compatible scans, and expose the new index through existing descriptor-driven
`SHOW`, `CREATE TABLE ... LIKE`, DML, auto-increment, and
`INFORMATION_SCHEMA.STATISTICS` paths.

This is intentionally not full MySQL index DDL. This phase did not add
`ADD UNIQUE`; that later surface is specified separately in
`docs/specs/baseline-alter-table-add-unique-lifecycle/specs.md`. It also does
not add index drops or renames, composite indexes, prefix indexes, functional
key parts, index comments, visibility, fulltext/spatial indexes, or optimizer
guarantees.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing secondary and unique index specs:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Existing `ALTER TABLE ... ADD PRIMARY KEY` and `DROP PRIMARY KEY` specs:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`,
  `docs/specs/baseline-alter-table-drop-primary-key/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_add_index_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `ALTER TABLE t ADD INDEX k_v (v)` and `ALTER TABLE t ADD KEY k_v (v)`
  succeed on existing InnoDB tables, report `ROW_COUNT() == 0`, and leave
  `@@warning_count == 0` for the plain admitted form.
- `INDEX` and `KEY` are synonyms for the nonunique secondary-index form.
- Omitting the index name derives a name from the key column. Repeating the
  unnamed same-column index appends `_2` for the second index in the tested
  subset.
- `SHOW INDEX` and `SHOW CREATE TABLE` expose the new secondary index after
  the existing primary key and existing indexes. `INFORMATION_SCHEMA.STATISTICS`
  reports `NON_UNIQUE = 1`, `SEQ_IN_INDEX = 1`, and the descriptor column name.
- Integer-family, exact decimal, canonical temporal, `CHAR(1..255)`, and
  `VARCHAR(1..255)` columns can be indexed without a prefix for this subset.
  `TEXT` without a prefix fails with `1170 / 42000`, and `CHAR(0)` /
  `VARCHAR(0)` fail with `1167 / 42000`.
- A duplicate explicit index name fails with `1061 / 42000`.
- A quoted index name `PRIMARY` fails with `1280 / 42000`; unquoted
  `PRIMARY` in the index-name position is a syntax error.
- An unknown key column fails with `1072 / 42000`.
- Missing default schema, unknown schema, and unknown table use the existing
  MySQL diagnostics for `ALTER TABLE` target resolution.
- MySQL accepts wider forms such as multiple alter actions, composite key
  parts, unique indexes, index options, comments, visibility, prefixes, and
  functional key parts. They remain outside this nonunique-index phase.

## Scope

Supported:

- persistent MyLite base tables only;
- one `ALTER TABLE table_name ADD INDEX [index_name] (column_name)` action;
- one `ALTER TABLE table_name ADD KEY [index_name] (column_name)` action;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- optional explicit index names using the existing descriptor identifier policy;
- omitted index names generated from the key column with MySQL-compatible `_N`
  suffixes for collisions in the admitted subset;
- exactly one unqualified descriptor column in the key-part list;
- supported key target descriptors:
  - integer-family and integer aliases, including `BOOL` / `BOOLEAN`;
  - exact `DECIMAL` / `NUMERIC` / `FIXED`;
  - canonical `DATE`, `DATETIME`, and `TIMESTAMP`;
  - `CHAR(1..255)` and `VARCHAR(1..255)`;
- nullable and `NOT NULL` key target columns;
- empty and nonempty tables;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `CREATE TABLE ... LIKE`, DML, `ALTER TABLE ... DROP PRIMARY KEY`
  auto-increment key checks, and limited `INFORMATION_SCHEMA.STATISTICS` after
  the add;
- row-value preservation, reopen persistence, independent file-backed handles,
  and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- `ADD UNIQUE` and `ADD CONSTRAINT UNIQUE` in this phase; see the later limited
  `ALTER TABLE ... ADD UNIQUE` slice for the supported unique-index and named
  unique-constraint subset;
- `ADD FULLTEXT`, `ADD SPATIAL`, `ADD FOREIGN KEY`, and check constraints;
- `DROP INDEX` / `DROP KEY`, `RENAME INDEX` / `RENAME KEY`, and index
  visibility changes;
- multi-action `ALTER TABLE`;
- multiple key parts, duplicate key parts, prefix lengths, descending key
  parts, functional key parts, expression key parts, table-qualified key parts,
  ordinal key parts, and string-literal key parts;
- index type clauses, comments, parser options, `KEY_BLOCK_SIZE`, visibility,
  engine attributes, algorithms, locks, partitions, temporary tables, views,
  foreign keys, cascades, triggers, privileges, and implicit-commit emulation;
- `TEXT` family key parts until prefix-length semantics are implemented;
- string unique or primary-key semantics and collation-aware comparison;
- optimizer/index-use guarantees beyond creating a physical SQLite index that
  SQLite may use.

## Ownership Boundaries

- Public API: no new public ABI. Callers continue to use `mylite_execute()` and
  existing result/diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and statement cleanup.
- Lexer/parser/AST: owns syntax admission for the narrow `ALTER TABLE` action
  and preserves index name, keyword spelling, and key column nodes without
  depending on runtime or SQLite.
- Analyzer/planner/runtime: resolves schema, table, index names, and key
  columns against MyLite descriptors before generating any SQLite SQL.
- Catalog: MyLite index descriptors and index-column descriptors are
  authoritative for logical metadata. SQLite schema text is not used to infer
  logical indexes.
- Result builder/introspection: existing `SHOW` and `INFORMATION_SCHEMA`
  surfaces render from descriptors.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite creates one ordinary SQLite index using
  stable generated physical table, index, and column identifiers. No SQLite
  fork patch is required.

## Supported SQL Grammar

MyLite admits only a single nonunique index action:

```sql
ALTER TABLE table_name ADD INDEX [index_name] (column_name)
ALTER TABLE table_name ADD KEY [index_name] (column_name)
```

Both target table names may be unqualified or schema-qualified. The index name,
when present, is one identifier or quoted identifier. The key column is one
unqualified identifier or quoted identifier.

MyLite Lemon-syntax sketch:

```lemon
statement(A) ::= alter_table_add_index_statement(B). {
    A = B;
}

alter_table_add_index_statement(A) ::=
    ALTER TABLE table_name(T) ADD alter_add_index_keyword(K)
    index_name_opt(N) LPAREN index_key_part(P) RPAREN(R). {
    A = mylite_sql_parser_make_alter_table_add_index_statement(
        state, T, K, N, P, R);
}

alter_add_index_keyword ::= INDEX.
alter_add_index_keyword ::= KEY.

index_name_opt ::= .
index_name_opt ::= identifier.

index_key_part ::= identifier.
```

The implementation may reuse existing secondary-index AST node shapes if the
statement kind still distinguishes this `ALTER TABLE` action from create-time
index definitions.

## Resolution Semantics

Target table resolution follows the existing policy:

- an unqualified table requires the selected/default schema;
- a schema-qualified table uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with MySQL-compatible `1049 / 42000`;
- unknown tables fail with MySQL-compatible `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL is
  generated.

The target object must be a persistent base table. Future non-base descriptors
must fail with the matching MySQL diagnostic for this supported surface.

Index name resolution is table-local:

- explicit names must not collide case-insensitively with any existing primary,
  unique, or nonunique index descriptor;
- duplicate explicit names fail with `1061 / 42000`;
- quoted `PRIMARY` fails with `1280 / 42000`;
- unquoted `PRIMARY` remains a syntax error in the admitted grammar;
- omitted names derive from the target column name and append `_2`, `_3`, ...
  until no descriptor name collides in the table-local index namespace.

Column resolution is descriptor-owned:

- the key column must be an existing unqualified descriptor column;
- unknown columns fail with `1072 / 42000`;
- supported invisible columns may be explicitly indexed because existing
  descriptor DML and metadata paths already allow explicit references;
- current descriptor name comparison follows the existing catalog
  case-insensitive policy. This phase does not add collation-aware identifier
  comparison.

## Descriptor and Catalog Semantics

On success, MyLite creates:

- one `_mylite_catalog_indexes` row with secondary kind and `is_unique = 0`;
- one `_mylite_catalog_index_columns` row with sequence `1`, referencing the
  resolved descriptor column;
- a new generated physical index name `_mylite_user_index_<index_id>`;
- updated table descriptor identity/generation so descriptor caches observe the
  new index;
- an incremented SQLite schema generation because physical SQLite schema
  changed.

The table descriptor, column descriptors, row values, table physical name,
auto-increment counter, and existing index descriptors are otherwise
preserved. `CREATE TABLE ... LIKE` clones the newly added descriptor using
existing index clone behavior. `CREATE TABLE ... SELECT` continues to omit
indexes. `DROP TABLE`, `RENAME TABLE`, `TRUNCATE`, DML, reopen, and
independent handles observe the new descriptor through existing catalog paths.

Failed planning or execution must leave no catalog rows and no physical SQLite
index.

## Physical SQLite Handling

For one admitted index, MyLite generates standard SQLite DDL from descriptors:

```sql
CREATE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("physical_column_name")
```

Rules:

- quote every generated identifier;
- never interpolate user SQL literals into physical SQL;
- allocate the logical index id and physical name through the catalog mutation
  path;
- execute the physical index creation before committing the catalog mutation;
- if physical SQLite creation fails, roll back the catalog mutation and surface
  a deterministic MyLite physical-SQL diagnostic unless a clearer MySQL
  diagnostic is already recorded;
- do not scan or materialize table rows in MyLite for nonunique index creation;
- use public SQLite DDL support only. No SQLite fork patch, virtual table,
  custom function, trigger, or storage format change is required.

SQLite may use the created physical index for compatible generated SQL. MyLite
does not expose optimizer-plan guarantees in this slice.

## Type and Value Semantics

This phase does not introduce new row-value conversions. It admits only columns
whose stored physical representation is already canonical for indexing in the
current nonunique descriptor subset:

- integer-family values are SQLite integers after existing MyLite conversion;
- exact decimal values are descriptor-canonical fixed-scale text;
- temporal values are canonical text;
- `CHAR` and `VARCHAR` values are existing MyLite UTF-8 text storage;
- `NULL` values are indexed normally by SQLite and rendered as nullable
  metadata.

`TEXT` family columns are rejected without prefix support because MySQL requires
a key length for those columns in ordinary InnoDB indexes. This phase does not
implement prefix truncation, byte/character prefix accounting, or warning/error
differences by SQL mode.

## Introspection

Existing descriptor-driven metadata must observe the added index:

- `SHOW CREATE TABLE` appends `KEY `index_name` (`column_name`)` after existing
  primary, unique, and secondary index descriptors in logical descriptor order;
- `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` emits one row with `Non_unique =
  1`, `Seq_in_index = 1`, `Collation = A`, `Sub_part = NULL`,
  `Index_type = BTREE`, `Visible = YES`, and current deterministic cardinality
  placeholders;
- `INFORMATION_SCHEMA.STATISTICS` emits the matching descriptor row;
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` report `MUL` for
  columns that are now only nonunique-indexed, while preserving `PRI` and `UNI`
  precedence from existing key metadata rules;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and `KEY_COLUMN_USAGE` remain
  unchanged because nonunique indexes are not constraints.

## Diagnostics

Supported diagnostics:

- syntax errors and unsupported grammar: existing parser diagnostic
  `1064 / 42000`;
- missing default schema: existing `1046 / 3D000`;
- unknown explicit schema: `1049 / 42000`;
- unknown target table: `1146 / 42S02`;
- reserved `_mylite_*` schema/table names: existing MyLite reserved-name
  diagnostics;
- unsupported object kind: future view/non-base-table diagnostic matching the
  surrounding `ALTER TABLE` policy;
- duplicate explicit index name: `1061 / 42000`;
- quoted `PRIMARY` index name: `1280 / 42000`;
- unknown key column: `1072 / 42000`;
- unsupported key target type, including `TEXT` without prefix:
  `1170 / 42000` where MySQL has an equivalent, or an explicit MyLite
  unsupported diagnostic for future descriptor kinds;
- `CHAR(0)` / `VARCHAR(0)` key part: `1167 / 42000`;
- unsupported key expression, key prefix, direction, index option, multi-key
  part, multi-action alter, unique-index actions, comments, visibility, algorithms,
  locks, partitions, temporary tables, and views:
  deterministic syntax or unsupported diagnostics;
- physical SQLite failure: deterministic internal/physical SQL diagnostic;
- allocation failure: `MYLITE_NOMEM` and handle-owned diagnostics.

Successful admitted forms produce no warnings.

## Tests

Add a fast C runtime test, preferably
`packages/libmylite/tests/runtime_alter_table_add_index_test.c`, registered as
`libmylite.runtime.alter_table_add_index`.

Coverage must include:

- successful `ADD INDEX` and `ADD KEY` on existing empty and nonempty tables;
- schema-qualified and unqualified target resolution;
- supported key target families: integer aliases, exact decimal, `DATE`,
  `DATETIME`, `TIMESTAMP`, `CHAR`, and `VARCHAR`;
- nullable and `NOT NULL` key columns;
- explicit and omitted index names, including `_2` suffix collision behavior;
- coexistence with primary keys, unique indexes, and existing nonunique
  indexes;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS` after the add;
- `CREATE TABLE ... LIKE` cloning after `ADD INDEX`;
- `CREATE TABLE ... SELECT` still omitting indexes;
- DML after index add, including `INSERT`, `UPDATE`, `DELETE`, and sorted or
  filtered `SELECT` observing unchanged row values;
- adding a secondary index to an `AUTO_INCREMENT` column, then dropping the
  primary key and continuing generated inserts;
- reopen persistence and independent file-backed handles;
- physical SQLite index existence without modifying the `.mylite` preamble;
- cleanup-safe zero initialization for any new planner/result objects;
- diagnostics for missing default schema, unknown schema, unknown table,
  reserved names, duplicate explicit names, quoted `PRIMARY`, unknown key
  columns, `TEXT` without prefix, zero-length `CHAR` / `VARCHAR` key columns,
  and unsupported syntax;
- unsupported forms for this nonunique-index phase: `ADD FULLTEXT`,
  `ADD SPATIAL`, multi-action alter, multiple key parts, key prefixes, table-qualified key
  parts, functional key parts, expression key parts, ordinal parts, `USING`,
  comments, visibility, algorithm/lock options, and partitions;
- existing parser, runtime handle, diagnostics, statement context, result
  metadata, file-backed opening, VFS, catalog, create/drop/rename, primary-key,
  secondary-index, unique-index, auto-increment, and information-schema tests
  still pass.

The MySQL comparison script
`packages/libmylite/tests/mysql_baseline_alter_table_add_index_expectations.sh`
must pass against MySQL 8.4.9 before implementation behavior is claimed.

## Compatibility Documentation

Implementation must update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-indexes-constraints.md`;
- `docs/compatibility/sql-table-ddl.md`;

with limited wording for the exact `ALTER TABLE ... ADD INDEX` / `ADD KEY`
subset. Do not claim full index drops, renames, comments, visibility,
composite/prefix/functional/descending indexes,
foreign keys, optimizer guarantees, or full metadata parity.

## Verification

Before the feature is complete, run:

1. `cmake --build --preset dev`
2. focused CTest entries for parser, secondary indexes, unique indexes,
   primary keys, auto-increment, information schema, and the new runtime test;
3. `packages/libmylite/tests/mysql_baseline_alter_table_add_index_expectations.sh`
4. `cmake --workflow --preset check`
