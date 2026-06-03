# Baseline CHAR/VARCHAR Key Lifecycle

## Summary

This phase adds the next core schema building block: descriptor-owned
single-column `CHAR` and `VARCHAR` primary and unique keys for persistent
MyLite base tables.

The slice builds on existing `CHAR` / `VARCHAR` storage, descriptor-owned
integer primary keys, descriptor-owned unique indexes, `ALTER TABLE ... ADD`
and `DROP PRIMARY KEY`, `ALTER TABLE ... DROP INDEX`, duplicate-key DML
handling, `SHOW` / `INFORMATION_SCHEMA` metadata, and file-backed `.mylite`
storage.

This is not full MySQL string collation support. MyLite admits only
ASCII-valued non-`NULL` string key values in this first slice, then enforces
MySQL-compatible ASCII equality for the fixed `utf8mb4_0900_ai_ci` baseline:
case-insensitive letters, significant trailing spaces for `VARCHAR`, and the
already canonicalized default-mode trailing-space behavior for `CHAR`. Broader
Unicode collation weights, non-ASCII key values, prefix indexes, composite
string keys, standalone index DDL, and `ALTER TABLE ADD UNIQUE` are handled by
separate slices.

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
- MyLite file format:
  `docs/specs/mylite-file-format/specs.md`
- Baseline `CHAR` type:
  `docs/specs/baseline-char-type/specs.md`
- Baseline `VARCHAR` type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline composite primary key lifecycle:
  `docs/specs/baseline-composite-primary-key-lifecycle/specs.md`
- Baseline unique index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline secondary index lifecycle:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`
- Baseline `ALTER TABLE ... ADD PRIMARY KEY`:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- Baseline `ALTER TABLE ... DROP PRIMARY KEY`:
  `docs/specs/baseline-alter-table-drop-primary-key/specs.md`
- Baseline `ALTER TABLE ... DROP INDEX`:
  `docs/specs/baseline-alter-table-drop-index-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, primary-key and unique constraints:
  <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  <https://dev.mysql.com/doc/refman/8.4/en/char.html>
- MySQL 8.4 Reference Manual, Unicode character sets and collations:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html>
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-columns.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-constraints-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-key-column-usage-table.html>
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_char_varchar_key_lifecycle_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes for this phase establish:

- `CHAR(n)` and `VARCHAR(n)` columns can be primary-key or unique-index key
  columns when `n > 0`.
- `CHAR(0)` and `VARCHAR(0)` cannot be indexed by the tested MySQL 8.4.9
  InnoDB runtime; key definitions fail with `1167 / 42000`.
- `PRIMARY KEY` makes omitted-nullability `CHAR` / `VARCHAR` columns
  `NOT NULL`. `SHOW COLUMNS` reports `NO`, `PRI`, and a displayed default of
  `NULL` when no explicit default exists.
- Explicit `NULL` on a primary-key string column fails with `1171 / 42000`.
  Inline `DEFAULT NULL PRIMARY KEY` fails with `1067 / 42000`; table-level
  `PRIMARY KEY` over an explicitly nullable or `DEFAULT NULL` column fails with
  the primary-key-not-null diagnostic.
- `VARCHAR` and `CHAR` primary-key columns can have explicit string defaults;
  MyLite still defers string defaults in this phase and preserves the current
  no-explicit-default behavior.
- Nullable unique `CHAR` / `VARCHAR` columns permit multiple `NULL` values.
- `SHOW CREATE TABLE` renders inline primary and unique declarations as table
  primary-key and unique-key clauses, with `PRIMARY KEY` before unique indexes.
- `SHOW COLUMNS` reports `PRI` for primary-key columns. In a table with no
  primary key, MySQL may report `PRI` for the first `NOT NULL` unique key and
  `UNI` for later unique keys; MyLite already models this column-key priority
  for descriptor unique indexes and should keep it descriptor-driven.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report one `BTREE` row per
  admitted key part with `Collation = A`, no prefix, and `Null` / `NULLABLE`
  empty for `NOT NULL` parts or `YES` for nullable unique parts.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` report primary-key and unique
  constraints from descriptors, including string-key descriptors.
- The default `utf8mb4_0900_ai_ci` collation is case-insensitive,
  accent-insensitive, and `NO PAD` in MySQL 8.4.9.
- Under `utf8mb4_0900_ai_ci`, `VARCHAR` unique keys treat `'a'` and `'A'` as
  duplicates, treat `'e'` and `'é'` as duplicates, but allow both `'a'` and
  `'a '` because trailing spaces are significant for this `NO PAD` collation.
- `CHAR` storage/readback strips trailing spaces in the default SQL mode used
  by MyLite, so `CHAR` unique keys treat stored `'a'` and attempted `'a '` as
  duplicates with duplicate entry text `'a'`.
- Duplicate `INSERT` / `UPDATE` failures use `1062 / 23000` and include the
  attempted key value plus `table.index` in the message. `INSERT IGNORE`
  demotes duplicate-key rows to `1062` warnings, skips them, and reports
  affected rows for rows actually inserted.
- `ALTER TABLE ... ADD PRIMARY KEY (string_column)` validates existing rows
  using the same string-key equality: existing `'a'` / `'A'` duplicates fail,
  existing `VARCHAR` `'a'` / `'a '` rows succeed, and existing `CHAR` `'a'` /
  `'a '` rows fail because the stored values are canonicalized to the same
  visible string.

## Scope

Supported:

- persistent MyLite base tables only;
- create-time inline `CHAR(n) PRIMARY KEY` and `VARCHAR(n) PRIMARY KEY`;
- create-time table-level `PRIMARY KEY (column_name)` when the single part is
  a `CHAR(n)` or `VARCHAR(n)` descriptor;
- `ALTER TABLE table_name ADD PRIMARY KEY (column_name)` when the single part
  is an existing `CHAR(n)` or `VARCHAR(n)` descriptor with compatible existing
  rows;
- existing integer-family primary-key behavior, including composite integer
  keys, remains supported;
- create-time inline `CHAR(n) UNIQUE`, `CHAR(n) UNIQUE KEY`,
  `VARCHAR(n) UNIQUE`, and `VARCHAR(n) UNIQUE KEY`;
- create-time table-level `UNIQUE [KEY|INDEX] [name] (column_name)` when the
  single part is a `CHAR(n)` or `VARCHAR(n)` descriptor;
- descriptor lengths `1..255`; `CHAR(0)` and `VARCHAR(0)` are rejected when
  used as primary or unique key parts;
- non-`NULL` string key values containing only ASCII bytes `0x01..0x7f`,
  excluding the existing embedded-`NUL` unsupported value;
- default-mode `CHAR` canonicalization already implemented by the `CHAR`
  storage phase before key binding;
- MyLite-owned ASCII equality for key enforcement:
  - ASCII letters compare case-insensitively;
  - all other ASCII bytes compare exactly;
  - `VARCHAR` trailing spaces are significant;
  - `CHAR` trailing spaces are already removed by `CHAR` value conversion;
- multiple `NULL` values in nullable unique string keys;
- duplicate-key enforcement for `INSERT ... VALUES`, `INSERT ... SET`,
  `INSERT IGNORE ... VALUES`, `INSERT IGNORE ... SET`, and single-assignment
  `UPDATE`;
- descriptor-backed `CREATE TABLE ... LIKE` cloning of admitted string primary
  and unique key descriptors;
- descriptor-backed metadata through `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, limited `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`;
- `ALTER TABLE ... DROP PRIMARY KEY` and limited
  `ALTER TABLE ... DROP INDEX|KEY` interaction with admitted string key
  descriptors;
- table rename/drop/truncate interaction, reopen persistence, independent
  file-backed handles, `.mylite` preamble preservation, and no public ABI
  change.

Deferred:

- non-ASCII string key values, including values whose MySQL
  `utf8mb4_0900_ai_ci` equality depends on Unicode accent/case/weight rules;
- full Unicode Collation Algorithm weight tables;
- composite primary keys containing any string key part;
- composite unique string indexes;
- standalone `CREATE [UNIQUE] INDEX`, standalone `DROP INDEX`, and
  `ALTER TABLE ADD UNIQUE` in this phase; later feature slices cover limited
  forms;
- prefix key parts such as `UNIQUE KEY u (v(10))`;
- BINARY/BLOB-family keys without prefixes, `ENUM`, `SET`, `JSON`,
  `CHARACTER`, `CHARACTER VARYING`, `NCHAR`, and `NVARCHAR`; full `TEXT`
  family key parts are covered by the documented WordPress bridge;
- explicit string defaults beyond the existing `CHAR` / `VARCHAR` default
  support already in the row-value/defaults slices;
- named constraints, foreign keys, cascades, check constraints, invisible
  indexes, descending key parts, expression key parts, functional indexes,
  parser/index options, privileges, optimizer hints, or optimizer-plan
  guarantees;
- string comparison predicates, string `ORDER BY`, string `GROUP BY`, string
  `DISTINCT`, `LIKE`, `REGEXP`, collation coercibility, or mutable charset and
  collation state;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public call validation, result
  ownership, diagnostics exposure, and cleanup. No public ABI is added.
- Statement context: owns diagnostics, warnings, affected rows, `ROW_COUNT()`,
  statement atomicity, and cleanup on failure. Supported successful DDL reports
  through existing non-row result conventions; supported in-range DML records
  `warning_count == 0` except existing `INSERT IGNORE` duplicate demotion.
- Lexer/parser/AST: existing grammar already admits inline/table-level primary
  and unique definitions plus `CHAR` / `VARCHAR` column types. Parser output
  remains structural and does not inspect descriptors, collation rules, or
  SQLite schema.
- Analyzer/planner: resolves key definitions against MyLite descriptors,
  validates key shape, validates descriptor length and string-key value
  support, applies existing string value conversion, and builds descriptor
  plans. Unsupported non-ASCII key values are rejected before SQLite mutation
  or duplicate lookup.
- Catalog: existing index descriptors remain authoritative. String primary
  keys use `kind = PRIMARY`, `is_unique = 1`; string unique keys use
  `kind = SECONDARY`, `is_unique = 1`. Index-column descriptors preserve
  ordered key parts. SQLite schema text and `PRAGMA` metadata are not logical
  metadata authority.
- Result and introspection builders: render descriptor-owned key metadata.
  They do not infer primary/unique state from SQLite indexes.
- SQLite registration: MyLite registers a first-party SQLite collation callback
  through public `sqlite3_create_collation_v2()` during connection bootstrap.
  The callback implements the admitted ASCII subset of
  `utf8mb4_0900_ai_ci` equality/order for internal physical indexes and
  duplicate lookup SQL.
- SQLite physical storage: stores `CHAR` / `VARCHAR` as SQLite `TEXT` in
  generated MyLite user tables. Generated physical indexes include a collation
  annotation for string key parts so SQLite uniqueness and MyLite duplicate
  probes use the same comparison semantics.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged. This feature writes only inside the shifted SQLite payload.

## Supported Grammar

The existing grammar is sufficient for the admitted create-time forms:

```lemon
create_table_item ::= column_definition.
create_table_item ::= primary_key_definition.
create_table_item ::= unique_index_definition.

column_definition ::= identifier column_type column_attribute_list_opt.

column_type ::= CHAR.
column_type ::= CHAR LPAREN INTEGER RPAREN.
column_type ::= VARCHAR LPAREN INTEGER RPAREN.

column_attribute ::= PRIMARY KEY.
column_attribute ::= UNIQUE.
column_attribute ::= UNIQUE KEY.

primary_key_definition ::= PRIMARY KEY LPAREN primary_key_part_list RPAREN.
primary_key_part_list ::= primary_key_part.
primary_key_part_list ::= primary_key_part_list COMMA primary_key_part.
primary_key_part ::= identifier.

unique_index_definition ::= UNIQUE unique_index_keyword_opt index_name_opt
                            LPAREN secondary_index_part_list RPAREN.
unique_index_keyword_opt ::= .
unique_index_keyword_opt ::= KEY.
unique_index_keyword_opt ::= INDEX.
secondary_index_part_list ::= secondary_index_part.
secondary_index_part_list ::= secondary_index_part_list COMMA secondary_index_part.
secondary_index_part ::= identifier.
```

Implementation rules narrow the parsed surface:

- string primary keys must have exactly one unqualified key part;
- existing all-integer composite primary keys remain supported;
- unique indexes already require exactly one unqualified key part;
- string key parts must be `CHAR(n)` or `VARCHAR(n)` descriptors with
  `1 <= n <= 255`;
- prefix lengths, qualified key parts, sort direction, expression key parts,
  named constraints, and index options remain unsupported or syntax errors
  according to the existing parser surface.

`ALTER TABLE ... ADD PRIMARY KEY (column_name)` reuses the existing
single-action alter grammar and admits one existing `CHAR(n)` or `VARCHAR(n)`
descriptor column when all validation passes.

## Descriptor Resolution and Defaults

Name resolution follows existing descriptor policy:

- target table names use selected/default schema resolution for unqualified
  names and explicit-schema resolution for qualified names;
- reserved `_mylite_*` schema, table, column, and index names are rejected
  before physical SQL generation;
- key-part column names are unqualified and resolved case-insensitively against
  MyLite descriptors;
- unknown key columns fail with MySQL-compatible `1072 / 42000`;
- duplicate key definitions, duplicate key names, and reserved `PRIMARY`
  secondary-index names reuse existing primary/unique/index diagnostics.

Primary-key nullability/default behavior:

- an omitted-nullability `CHAR` / `VARCHAR` primary-key column becomes
  effectively `NOT NULL`;
- explicit `NULL` on a string primary-key column fails with `1171 / 42000`;
- explicit `DEFAULT NULL` in an inline string primary-key column fails with
  `1067 / 42000`;
- table-level `PRIMARY KEY` over a nullable or default-`NULL` string column
  fails with the primary-key-not-null diagnostic;
- string primary-key columns with no explicit default keep the existing
  no-explicit-default descriptor state. Omitted-column inserts into such
  columns fail through existing no-default logic.

Unique-key nullability/default behavior:

- nullable unique string columns remain nullable and allow multiple `NULL`
  rows;
- `NOT NULL UNIQUE` string columns with no explicit default keep the existing
  no-explicit-default descriptor state;
- explicit string defaults are not introduced by this phase. Existing supported
  `CHAR` / `VARCHAR` defaults continue to work where already implemented, but
  this feature does not expand default-expression syntax.

## String Key Value Semantics

Every non-`NULL` value written to a descriptor-owned string primary key or
unique key must first pass existing `CHAR` / `VARCHAR` value conversion:

- string literal decoding;
- UTF-8 validation;
- embedded-`NUL` rejection;
- declared length validation;
- default-mode `CHAR` trailing-space canonicalization;
- existing nullability/default/range diagnostics.

After conversion, MyLite applies the string-key support gate:

- accepted key bytes are ASCII `0x01..0x7f`;
- `0x00` remains unsupported through the existing string-value policy;
- any byte with the high bit set is rejected with a deterministic MyLite
  unsupported diagnostic before physical mutation.

This ASCII gate is deliberate. MySQL 8.4.9 `utf8mb4_0900_ai_ci` treats many
non-ASCII strings as equal to ASCII or other Unicode strings. Raw SQLite text
comparison would over-admit duplicates, and MyLite should not claim support
until it owns the needed Unicode collation weights or another equivalent
strategy.

For admitted ASCII key values, MyLite implements:

- case-insensitive comparison for ASCII letters `A..Z` and `a..z`;
- byte-exact comparison for digits, spaces, and punctuation after ASCII
  lowercase folding;
- significant trailing spaces for `VARCHAR`;
- stored canonical visible values for `CHAR`, so excess trailing spaces already
  removed by `CHAR` conversion do not create distinct key values.

## Physical SQLite Handling

MyLite uses public SQLite APIs only:

- register the fixed MyLite collation callback with
  `sqlite3_create_collation_v2()` during connection bootstrap;
- create physical SQLite unique indexes over descriptor physical table/column
  names;
- quote every generated SQLite identifier;
- bind row values and duplicate-probe values as prepared-statement parameters;
- use `sqlite3_bind_text(..., SQLITE_TRANSIENT)` for admitted string key
  values and `sqlite3_bind_null()` for nullable unique `NULL`s;
- never interpolate user SQL literals into generated physical SQL;
- never inspect SQLite schema text as logical metadata.

Generated physical index shapes:

```sql
CREATE UNIQUE INDEX "_mylite_user_index_<index_id>"
ON "_mylite_user_table_<table_id>"
("_logical_column_name" COLLATE "utf8mb4_0900_ai_ci")
```

Non-string key parts keep the existing generated shape without a collation
annotation. Current string-key scope is single-column, but helper code should
format key parts from descriptors so future mixed/composite design does not
hardcode parser text.

Duplicate lookup and existing-row validation SQL must use the same collation
annotation for string key parts. For example, duplicate lookup over a string
unique key is shaped like:

```sql
SELECT 1
FROM "_mylite_user_table_<table_id>"
WHERE "_logical_column_name" COLLATE "utf8mb4_0900_ai_ci" = ?1
LIMIT 1
```

`ALTER TABLE ... ADD PRIMARY KEY` existing-row duplicate validation groups by
descriptor key expressions with the same collation annotation. This prevents a
table containing existing `'a'` and `'A'` rows from acquiring a string primary
key.

No SQLite fork patch is required. If later full Unicode collation support is
too expensive through SQLite callbacks or causes unavoidable index overhead,
that later decision must be specified separately.

## DML Semantics

Supported:

- `INSERT ... VALUES` and `INSERT ... SET` into admitted string primary/unique
  keys fail atomically on duplicate non-`NULL` key values with
  `1062 / 23000`;
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` demote duplicate
  non-`NULL` string key values to warnings, skip duplicate rows, and continue
  with later rows;
- nullable unique string keys admit multiple `NULL` values;
- single-assignment `UPDATE` fails atomically when the assignment would create
  a duplicate string primary or unique key;
- no-op string-key assignments report affected rows through the existing
  changed-row semantics;
- unsupported non-ASCII key values fail before SQLite mutation and before
  duplicate-key demotion.

Deferred:

- `INSERT ... SELECT` into key-bearing string-key targets remains governed by
  the existing key-bearing target restrictions until source-stream duplicate
  handling is separately specified;
- key-aware `REPLACE` remains deferred;
- `ON DUPLICATE KEY UPDATE` remains unsupported;
- triggers, cascades, foreign keys, generated columns, and privilege semantics
  remain unsupported.

Duplicate diagnostics use the existing MySQL-shaped message:

```text
Duplicate entry '<attempted-value>' for key '<table-name>.<index-name>'
```

For `CHAR`, the attempted value in diagnostics is the post-conversion stored
visible value. MySQL reports `'a'` for an attempted duplicate `'a '` into
`CHAR(5)` because that value canonicalizes to `'a'`.

## Metadata Semantics

Existing descriptor-driven metadata paths should require little shape change.
Once string key descriptors exist, they must be visible through:

- `SHOW COLUMNS`: `PRI` for primary-key string columns, `UNI` for nullable
  unique string columns, and existing first-`NOT NULL`-unique `PRI` priority
  when no primary key exists;
- `SHOW CREATE TABLE`: render string primary keys after columns and before
  unique/nonunique secondary indexes; render string unique indexes as
  `UNIQUE KEY \`name\` (\`column\`)`;
- `SHOW INDEX`: one row per descriptor key part with `Non_unique = 0`,
  `Collation = A`, `Sub_part = NULL`, `Index_type = BTREE`,
  `Visible = YES`, and `Expression = NULL`;
- `INFORMATION_SCHEMA.STATISTICS`: one descriptor row per key part;
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`: primary and unique constraint rows;
- `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`: primary and unique key-column rows;
- `CREATE TABLE ... LIKE`: cloned string key descriptors and new physical
  indexes with target physical names;
- `CREATE TABLE ... SELECT`: existing key-free behavior preserved.

Metadata must not claim prefix lengths, expression keys, non-default
collations, visibility controls, or optimizer statistics.

## Diagnostics

Use MySQL-compatible diagnostics where MySQL behavior is in scope:

- missing default schema: existing no-database diagnostic;
- unknown schema: existing unknown-database diagnostic;
- unknown table: existing table-does-not-exist diagnostic;
- reserved logical names: existing reserved-name diagnostic;
- unsupported object kind: existing persistent-base-table unsupported
  diagnostic;
- unknown key column: `1072 / 42000`;
- duplicate primary key definition: `1068 / 42000`;
- explicit nullable primary-key part: `1171 / 42000`;
- inline `DEFAULT NULL PRIMARY KEY`: `1067 / 42000`;
- `CHAR(0)` / `VARCHAR(0)` key part: `1167 / 42000`;
- duplicate key name: `1061 / 42000`;
- invalid secondary name `PRIMARY`: `1280 / 42000`;
- duplicate key value: `1062 / 23000`;
- `NULL` into primary-key or `NOT NULL` key column during DML: existing
  `1048 / 23000`;
- omitted no-default primary/unique `NOT NULL` string key value: existing
  `1364 / HY000`;
- unsupported non-ASCII string key value: deterministic MyLite unsupported
  diagnostic;
- unsupported key shape, prefix, expression, qualified key part, and
  unsupported standalone or alter index DDL forms: existing parse or
  unsupported diagnostics;
- SQLite physical failures, allocation failures, and public API misuse:
  existing internal/`MYLITE_NOMEM`/`MYLITE_MISUSE` policies.

Supported successful DDL and DML should not emit warnings except for existing
`INSERT IGNORE` duplicate-key warning demotion and other already-specified
`IGNORE` value-adjustment behavior.

## Performance and Architecture

This feature must stay close to SQLite's optimal path:

- no table-wide materialization for ordinary `INSERT`, `UPDATE`, `DELETE`, or
  `SELECT`;
- physical unique indexes enforce admitted uniqueness in SQLite;
- duplicate diagnostics may run focused indexed lookup SQL using the same
  string collation;
- `ALTER TABLE ... ADD PRIMARY KEY` may scan/group existing rows once because
  MySQL also validates existing data before adding the key;
- result and metadata paths remain descriptor-driven and do not query SQLite
  schema as authority.

The current SQLite fork remains untouched. The right extension point for this
slice is SQLite's public collation registration API plus MyLite-owned
descriptor planning.

## Test Plan

Add a new fast C runtime test, preferably
`runtime_char_varchar_key_lifecycle_test.c`, and register it with a dotted
CTest name.

Cover:

- create-time inline and table-level `CHAR` / `VARCHAR` primary keys;
- `ALTER TABLE ... ADD PRIMARY KEY` for existing `CHAR` / `VARCHAR` columns;
- create-time inline and table-level `CHAR` / `VARCHAR` unique keys;
- `CHAR(1)`, `CHAR(3)`, `VARCHAR(1)`, and `VARCHAR(10)` key descriptors;
- rejection of `CHAR(0)` and `VARCHAR(0)` key parts;
- case-insensitive ASCII duplicates for primary and unique keys;
- `VARCHAR` trailing-space distinctness and `CHAR` trailing-space
  canonical duplicate behavior;
- nullable unique multiple `NULL`s;
- empty string as a real unique key value;
- `INSERT`, `INSERT SET`, `INSERT IGNORE`, and `UPDATE` duplicate handling;
- non-ASCII string key values rejected deterministically in MyLite, with
  MySQL expectation coverage documenting broader upstream behavior;
- missing default schema, unknown schema, unknown table, reserved names,
  unknown assignment/key columns, duplicate key names, duplicate primary-key
  definitions, nullable primary-key parts, and no-default DML diagnostics;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, and
  `KEY_COLUMN_USAGE`;
- `CREATE TABLE ... LIKE` cloning and `CREATE TABLE ... SELECT` key omission;
- update/drop/rename/truncate interactions already covered by key lifecycle
  tests, extended for string key descriptors where relevant;
- reopen persistence and independent file-backed handles;
- `.mylite` preamble preservation;
- zero-initialized cleanup for new planner/collation registration objects;
- existing parser, runtime, descriptor, DML, metadata, storage, VFS, and
  SQLite bootstrap tests.

Run:

1. `cmake --build --preset dev`
2. focused CTest entries for parser, `CHAR`, `VARCHAR`, primary-key,
   unique-index, secondary-index, `ALTER TABLE ADD/DROP PRIMARY KEY`,
   `ALTER TABLE DROP INDEX`, DML, and the new runtime test
3. `packages/libmylite/tests/mysql_baseline_char_varchar_key_lifecycle_expectations.sh`
4. `cmake --workflow --preset check`

Review the final diff for MySQL behavior, ASCII collation correctness,
explicit non-ASCII scope control, descriptor authority, generated SQLite SQL
quoting/collation annotations, duplicate diagnostics, metadata accuracy,
file-format safety, performance, cleanup on failure, and compatibility docs.
