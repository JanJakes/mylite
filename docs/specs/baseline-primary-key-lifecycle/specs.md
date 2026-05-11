# Baseline Primary Key Lifecycle

## Status

This feature specifies the first descriptor-owned primary-key slice for
file-backed `.mylite` handles. It builds on the current public execution API,
statement context, parser scaffold, durable schema/table/column descriptors,
persistent base-table lifecycle, integer/`NULL` and `VARCHAR` row values,
descriptor-driven DML, and descriptor-driven SHOW metadata.

The feature is intentionally not full MySQL index or constraint support. It
supports one primary key per persistent base table, and that primary key must
contain exactly one existing MyLite integer-family descriptor column. Composite
keys, `VARCHAR` keys, secondary indexes, `ALTER TABLE ADD/DROP PRIMARY KEY`,
named constraints, prefix lengths, index options, information-schema index
tables, and optimizer assertions are deferred.

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
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline VARCHAR type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline SHOW INDEX empty introspection:
  `docs/specs/baseline-show-index-empty-introspection/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/show-index.html
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_primary_key_lifecycle_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- `id INT PRIMARY KEY` and `PRIMARY KEY (id)` both create a `NOT NULL`
  primary-key column, render `PRI` in `SHOW COLUMNS`, render one `PRIMARY`
  row in `SHOW INDEX`, and render `PRIMARY KEY (`id`)` in
  `SHOW CREATE TABLE`.
- Explicit `NULL` on a primary-key part fails with error `1171`, SQLSTATE
  `42000`, and "All parts of a PRIMARY KEY must be NOT NULL...".
- `DEFAULT NULL` on a primary-key column fails with error `1067`, SQLSTATE
  `42000`. An in-range integer default is accepted and remains visible in
  `SHOW COLUMNS` and `SHOW CREATE TABLE`.
- A duplicate primary-key definition fails with error `1068`, SQLSTATE
  `42000`. A table-level key that names a missing column fails with error
  `1072`, SQLSTATE `42000`.
- Data changes that violate the primary key fail with duplicate-key error
  `1062`, SQLSTATE `23000`, using a message containing
  `Duplicate entry '<value>' for key '<table>.PRIMARY'`.
- `INSERT IGNORE` demotes duplicate-key violations to warning `1062`, inserts
  the nonconflicting rows, and reports affected rows for rows actually inserted.
- `CREATE TABLE ... LIKE` clones the primary key and its metadata. `CREATE
  TABLE ... SELECT` copies column definitions but does not copy the primary key.
- MySQL supports wider forms such as composite primary keys, `VARCHAR` primary
  keys, named primary-key constraints, column-level `KEY` as a primary-key
  synonym, and `ALTER TABLE ADD/DROP PRIMARY KEY`; MyLite defers these in this
  baseline slice.

## Scope

The implementation must add:

- parser and AST support for a limited primary-key definition in
  `CREATE TABLE`;
- inline `column_name integer_type [NOT NULL] [DEFAULT integer] PRIMARY KEY`;
- table-level `PRIMARY KEY (column_name)` over exactly one unqualified column;
- persistent catalog descriptors for primary-key indexes and key parts;
- logical descriptor authority for primary-key metadata independent of SQLite
  schema text and `PRAGMA` metadata;
- physical SQLite enforcement through generated, quoted, stable unique-index
  names over generated MyLite user tables;
- `NOT NULL` physical table creation for primary-key columns;
- schema-qualified and unqualified table lifecycle behavior through existing
  selected-schema policy;
- descriptor-backed `SHOW COLUMNS` `PRI` rendering;
- descriptor-backed `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` primary-key
  rows using the existing MySQL 8.4.9 result shape;
- descriptor-backed `SHOW CREATE TABLE` primary-key rendering after column
  lines and before the fixed InnoDB/charset/collation suffix;
- `CREATE TABLE ... LIKE` cloning of the admitted primary-key descriptor;
- `CREATE TABLE ... SELECT` preserving current MySQL-compatible behavior by not
  copying primary keys;
- duplicate-key and bad-NULL diagnostics for `INSERT ... VALUES`,
  `INSERT ... SET`, and single-assignment `UPDATE`;
- duplicate-key demotion for the current `INSERT IGNORE ... VALUES` and
  `INSERT IGNORE ... SET` paths;
- deterministic unsupported diagnostics for `REPLACE` into key-bearing tables
  until key-aware `REPLACE` semantics are specified;
- reopen persistence, table rename/drop behavior, independent file-backed
  handles, and `.mylite` preamble preservation for admitted primary-key
  descriptors and rows;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider MySQL forms.

## Non-Goals

This feature must not implement:

- composite primary keys;
- primary keys over `VARCHAR`, string-family columns, expression columns, hidden
  columns, generated columns, or arbitrary physical SQLite columns;
- `UNIQUE`, secondary nonunique indexes, full-text indexes, spatial indexes,
  functional key parts, prefix lengths, descending key parts, invisible indexes,
  index comments, index type options, or standalone `CREATE INDEX` /
  `DROP INDEX`;
- `CONSTRAINT name PRIMARY KEY`, column-level `KEY` shorthand, `PRIMARY KEY`
  index options, `USING`, `COMMENT`, `VISIBLE`, `INVISIBLE`, or storage-engine
  attributes;
- `ALTER TABLE ADD PRIMARY KEY`, `ALTER TABLE DROP PRIMARY KEY`, mixed
  ALTER-key actions, key-preserving column rebuilds, or key dependency updates
  for column rename/drop/modify/change beyond deterministic rejection where
  required for safety;
- `AUTO_INCREMENT`, generated invisible primary keys, foreign keys, cascades,
  triggers, check constraints, privileges, locks, table statistics, or optimizer
  index-use guarantees;
- `INSERT ... ON DUPLICATE KEY UPDATE`, key-aware `REPLACE`, `LOAD DATA`,
  multi-table DML, subqueries, or new expression support;
- `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, `mysql` dictionary tables, or protocol
  metadata flags beyond existing public result conventions;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns statement diagnostics, warning count, affected rows,
  previous diagnostics snapshots, and transaction completion. Supported
  successful primary-key DDL and DML report through existing non-row result
  conventions.
- Lexer/parser/AST own syntax admission and source spans. They do not inspect
  catalog descriptors, SQLite schema, or physical rows.
- Analyzer/planner code resolves schema names, target tables, columns, and key
  definitions against MyLite descriptors; rejects unsupported key shapes; marks
  admitted key columns effectively `NOT NULL`; plans descriptor rows; and builds
  physical SQLite SQL from descriptors.
- The catalog module owns durable `_mylite_catalog_*` descriptor rows,
  descriptor versions, catalog generation, and descriptor-cache invalidation.
  Primary-key descriptors are first-party MyLite metadata, not reflections of
  SQLite metadata.
- Result and introspection builders render primary-key metadata from catalog
  descriptors. SQLite schema text, `sqlite_schema`, and SQLite pragmas are not
  user-visible MySQL metadata authority.
- SQLite owns physical row storage, b-tree uniqueness enforcement for generated
  prepared statements, and generated physical indexes. MyLite maps expected
  constraint diagnostics to MySQL-compatible errors before surfacing them.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes catalog, table, and index data only inside the shifted
  SQLite payload and must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

The feature extends the current limited `CREATE TABLE` grammar. The grammar is
independent MyLite grammar, not MySQL's complete grammar.

Supported subset:

```sql
CREATE TABLE [IF NOT EXISTS] table_name (
    create_table_item [, create_table_item ...]
) [table_option ...]

create_table_item:
    column_definition
  | PRIMARY KEY ( column_name )

column_definition:
    column_name integer_type [nullability] [integer_default] [PRIMARY KEY]

nullability:
    NOT NULL

integer_default:
    DEFAULT signed_decimal_integer_literal
  | DEFAULT TRUE
  | DEFAULT FALSE
```

`PRIMARY KEY` may appear on one integer-family column definition, or as one
table-level key definition naming one already-declared integer-family column.
The supported semantic subset does not admit explicit `NULL` on the key column,
`DEFAULT NULL` on the key column, table-qualified key parts, multiple key parts,
named constraints, key options, secondary indexes, `UNIQUE`, `KEY`, or `INDEX`
items.

The parser may admit a small amount of syntax that the analyzer rejects
deterministically, such as multiple primary-key definitions or a table-level key
over more than one key part, if doing so keeps unsupported diagnostics clear.

### MyLite Lemon-Syntax Snippet

```lemon
create_table_statement ::=
    CREATE TABLE create_if_not_exists_opt table_name LPAREN
    create_table_item_list RPAREN table_option_list_opt.

create_table_item_list ::= create_table_item.
create_table_item_list ::= create_table_item_list COMMA create_table_item.

create_table_item ::= column_definition.
create_table_item ::= primary_key_definition.

column_definition ::=
    identifier column_type nullability_opt column_default_opt
    column_primary_key_opt.

column_primary_key_opt ::= .
column_primary_key_opt ::= PRIMARY KEY.

primary_key_definition ::= PRIMARY KEY LPAREN primary_key_part_list RPAREN.

primary_key_part_list ::= primary_key_part.
primary_key_part_list ::= primary_key_part_list COMMA primary_key_part.

primary_key_part ::= identifier.
primary_key_part ::= qualified_identifier.
```

The implemented semantic subset requires one unqualified key part.

## Catalog Design

The catalog schema advances to the next schema version and adds two durable
descriptor tables:

```sql
_mylite_catalog_indexes(
    index_id INTEGER PRIMARY KEY,
    table_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    kind INTEGER NOT NULL,
    is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),
    physical_name TEXT NOT NULL UNIQUE,
    descriptor_version INTEGER NOT NULL,
    created_catalog_generation INTEGER NOT NULL,
    updated_catalog_generation INTEGER NOT NULL,
    UNIQUE(table_id, name)
)

_mylite_catalog_index_columns(
    index_column_id INTEGER PRIMARY KEY,
    index_id INTEGER NOT NULL,
    table_id INTEGER NOT NULL,
    column_id INTEGER NOT NULL,
    ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),
    descriptor_version INTEGER NOT NULL,
    created_catalog_generation INTEGER NOT NULL,
    updated_catalog_generation INTEGER NOT NULL,
    UNIQUE(index_id, ordinal_position),
    UNIQUE(index_id, column_id)
)
```

The first admitted index kind is `PRIMARY`. Its logical name is exactly
`PRIMARY`, `is_unique` is `1`, and the physical name is generated as
`_mylite_user_index_<index_id>`. The index-column descriptor references the
stable column id, not only the column name, so future column rename support can
update metadata without losing key identity. This slice may reject key-bearing
column rename/drop/modify/change operations until that dependency handling is
specified.

Catalog descriptors remain authoritative. `SHOW INDEX`, `SHOW COLUMNS`,
`SHOW CREATE TABLE`, duplicate-key diagnostics, and DML planning must read
MyLite descriptors, never SQLite `PRAGMA index_list`, `PRAGMA index_info`, or
`sqlite_schema` text.

## Type and Nullability Semantics

The admitted primary-key column must be one of the existing integer-family
descriptors currently backed by signed SQLite `INTEGER`: `TINYINT`,
`SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, `INT1`/`INT2`/`INT3`/
`INT4`/`INT8`, `BOOL`/`BOOLEAN`, and their currently supported `UNSIGNED`
forms within MyLite's physical range.

For an admitted primary-key column:

- omitted nullability becomes `NOT NULL` in the column descriptor and physical
  table;
- explicit `NOT NULL` is accepted;
- explicit `NULL` is rejected with MySQL error `1171` / SQLSTATE `42000`;
- `DEFAULT NULL` is rejected with MySQL error `1067` / SQLSTATE `42000`;
- supported in-range integer, `TRUE`, and `FALSE` defaults are accepted and
  remain descriptor-owned.

`VARCHAR` primary keys are deferred because MyLite currently exposes
collation metadata without collation-aware comparison and uniqueness semantics.
Rejecting them keeps the first primary-key enforcement path correct for the
admitted subset.

## Physical SQLite Handling

Generated MyLite user tables remain ordinary SQLite rowid tables. MyLite must
not model a MySQL integer primary key as a SQLite `INTEGER PRIMARY KEY` rowid
alias, because that would import SQLite rowid assignment and update behavior
that is not MySQL `AUTO_INCREMENT` semantics.

For an admitted primary key, MyLite generates:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>" ("<column_name>")
```

All generated identifiers are quoted with MyLite's SQLite identifier quoting
helper. Runtime DML values continue to be bound through prepared statements.
MyLite performs all admitted integer conversion and `NULL` validation before
binding values. SQLite enforces uniqueness, while MyLite owns descriptor
resolution and MySQL-shaped diagnostics.

This feature uses public SQLite SQL and prepared-statement APIs only. It does
not require a targeted SQLite fork hook.

## DML Semantics

For admitted `INSERT ... VALUES`, `INSERT ... SET`, and one-assignment
`UPDATE` statements:

- `NULL` assigned to a primary-key column fails as a not-null violation with
  the existing MySQL-compatible column-null diagnostic.
- A duplicate primary-key value fails with error `1062`, SQLSTATE `23000`, and
  a deterministic MySQL-shaped duplicate-entry message using the logical table
  name and `PRIMARY` key name.
- Successful in-range writes report existing affected-row semantics and
  `warning_count == 0`.
- `INSERT IGNORE` demotes admitted duplicate-key violations to warning `1062`,
  skips the conflicting row, inserts nonconflicting rows, and reports affected
  rows for rows inserted.
- `UPDATE` duplicate-key conflicts roll back the statement atomically.

`REPLACE` into a key-bearing table is rejected with an unsupported diagnostic
until a dedicated key-aware `REPLACE` slice specifies delete/insert ordering,
affected rows, warnings, trigger interactions, and conflict selection. Existing
no-key `REPLACE` behavior remains unchanged for tables without primary keys.

`INSERT ... SELECT`, `REPLACE ... SELECT`, and `CREATE TABLE ... SELECT` into
or from key-bearing tables must either preserve the existing documented no-key
behavior where no key is involved, or reject key-bearing targets with a
deterministic unsupported diagnostic until duplicate-key and key-clone
semantics are expanded. `CREATE TABLE ... SELECT` targets do not receive a
primary-key descriptor.

## Metadata Semantics

`SHOW COLUMNS` / `SHOW FIELDS` sets `Key` to `PRI` for the primary-key column
and keeps `Key` empty for all other columns.

`SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` returns one row for the admitted
primary key:

- `Table`: logical table name;
- `Non_unique`: `0`;
- `Key_name`: `PRIMARY`;
- `Seq_in_index`: `1`;
- `Column_name`: logical column name;
- `Collation`: `A`;
- `Cardinality`: deterministic baseline `0`;
- `Sub_part`: SQL `NULL`;
- `Packed`: SQL `NULL`;
- `Null`: empty string;
- `Index_type`: `BTREE`;
- `Comment`: empty string;
- `Index_comment`: empty string;
- `Visible`: `YES`;
- `Expression`: SQL `NULL`.

`SHOW CREATE TABLE` renders the key after all column definitions:

```sql
CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
```

`CREATE TABLE ... LIKE` clones the primary-key descriptor and creates a new
physical unique index for the destination table. `CREATE TABLE ... SELECT`
keeps the current MySQL-compatible behavior of copying selected column
descriptors and rows without copying keys or constraints.

## Diagnostics

Supported and explicitly handled diagnostics include:

- syntax errors and unsupported grammar: existing parse error `1064` or a
  MyLite unsupported diagnostic when the parser admits a deferred shape;
- missing default schema: existing `1046`;
- unknown schema: existing `1049`;
- unknown table: existing `1146` or context-specific existing table errors;
- reserved `_mylite_*` schema/table/column names: existing reserved-name
  diagnostics before generated SQLite SQL;
- unsupported object kind: existing persistent-base-table diagnostic;
- duplicate column names: existing `1060`;
- multiple primary keys: MySQL error `1068` / SQLSTATE `42000`;
- missing primary-key column: MySQL error `1072` / SQLSTATE `42000`;
- explicit `NULL` primary-key part: MySQL error `1171` / SQLSTATE `42000`;
- invalid `DEFAULT NULL` on a primary-key column: MySQL error `1067` /
  SQLSTATE `42000`;
- unsupported primary-key type or key shape: deterministic MyLite unsupported
  diagnostic;
- duplicate primary-key DML value: MySQL error `1062` / SQLSTATE `23000`, or
  warning `1062` for admitted `INSERT IGNORE`;
- `NULL` into a primary-key column: existing error `1048` / SQLSTATE `23000`;
- physical SQLite failures: existing physical SQLite diagnostics unless MyLite
  has already mapped the failure to a more precise MySQL-compatible diagnostic;
- allocation failures: existing `MYLITE_NOMEM` and diagnostics;
- public API misuse: unchanged existing public API behavior.

## Tests

Tests must cover:

- parser acceptance for inline and table-level single-column integer primary
  keys;
- parser or analyzer rejection for composite keys, table-qualified key parts,
  named constraints, column-level `KEY`, secondary `KEY`/`INDEX`/`UNIQUE`,
  prefix lengths, index options, `ALTER TABLE ADD/DROP PRIMARY KEY`, and
  `VARCHAR` primary keys;
- successful creation over `INT`, `INTEGER`, `BIGINT`, and their admitted
  `UNSIGNED` forms;
- omitted and explicit `NOT NULL` nullability becoming `NOT NULL`;
- explicit `NULL`, `DEFAULT NULL`, missing key columns, and multiple primary
  keys with MySQL-verified diagnostics;
- in-range integer defaults on primary-key columns;
- `SHOW COLUMNS`, `SHOW INDEX`, and `SHOW CREATE TABLE` metadata;
- duplicate-key errors for `INSERT ... VALUES`, `INSERT ... SET`, and
  `UPDATE`;
- `INSERT IGNORE` duplicate-key warning demotion for the current values and set
  paths;
- primary-key lookup after `UPDATE`, `DELETE`, `TRUNCATE`, table rename, and
  table drop where existing statements are admitted;
- `CREATE TABLE ... LIKE` primary-key cloning and `CREATE TABLE ... SELECT`
  omission of primary keys;
- reopen persistence, independent file-backed handles, and `.mylite` preamble
  preservation;
- zero-initialized cleanup for new key planner/catalog/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, DDL, DML, SELECT, UPDATE, DELETE, VARCHAR, client-data, and
  registration tests still passing.

## Compatibility

`COMPATIBILITY.md`, `docs/compatibility/sql-indexes-constraints.md`,
`docs/compatibility/sql-table-ddl.md`, and
`docs/compatibility/sql-show-statements.md` must be updated after
implementation to mark only this partial primary-key slice as supported. The
docs must continue to state that secondary indexes, composite keys,
`AUTO_INCREMENT`, information-schema index metadata, full key DDL, and
key-aware `REPLACE` remain unsupported.
