# Baseline Composite String Primary Key Lifecycle

## Summary

This phase extends the existing descriptor-owned primary-key lifecycle so table
level primary keys may contain more than one supported key part when one or more
parts are baseline `CHAR` or `VARCHAR` columns. The supported surface covers
persistent base tables created by `CREATE TABLE ... PRIMARY KEY (...)` and
existing persistent base tables changed by `ALTER TABLE ... ADD PRIMARY KEY
(...)`.

The implementation remains descriptor-driven. MyLite resolves table and column
names against its catalog descriptors, stores the ordered primary-key parts in
the catalog, marks each key column `NOT NULL`, and creates one generated SQLite
unique index over the stable physical table. SQLite enforces the physical
uniqueness, but public metadata, diagnostics, and SQL rendering come from
MyLite descriptors.

## Sources

Compatibility expectations are based on independently written MyLite design,
official MySQL 8.4 documentation, and observed MySQL 8.4.9 runtime behavior:

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `ALTER TABLE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- MySQL 8.4.9 runtime probes captured by
  `packages/libmylite/tests/mysql_baseline_composite_string_primary_key_lifecycle_expectations.sh`.

The spec text and Lemon-style grammar snippets below are independently authored
for MyLite. They do not copy MySQL grammar or implementation sources.

## Supported Surface

This phase supports:

- `CREATE TABLE table_name (..., PRIMARY KEY (key_part[, ...]))` where every key
  part is an unqualified existing column descriptor.
- `ALTER TABLE table_name ADD PRIMARY KEY (key_part[, ...])` for a persistent
  base table without an existing primary key.
- Ordered key parts containing any mix of the currently supported integer-family
  descriptors and baseline `CHAR(1..255)` / `VARCHAR(1..255)` descriptors.
- Single-column integer primary keys and create-time composite integer primary
  keys already supported by earlier phases.
- Single-column `CHAR` / `VARCHAR` primary keys already supported by earlier
  phases.
- Valid UTF-8 string key participation through MyLite's shared limited
  `utf8mb4_0900_ai_ci` service.
- Descriptor-driven rendering in `SHOW CREATE TABLE`, `SHOW COLUMNS`,
  `SHOW INDEX`, `CREATE TABLE ... LIKE`, and limited
  `INFORMATION_SCHEMA.COLUMNS`, `INFORMATION_SCHEMA.STATISTICS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`.
- Duplicate-key enforcement for existing row insert and update paths that
  already consult descriptor-owned primary and unique key metadata.
- `INSERT IGNORE` duplicate handling for current supported insert forms.
- `ALTER TABLE ... DROP PRIMARY KEY` for keys created by this phase, preserving
  the existing rule that dropped key columns remain `NOT NULL`.

This phase intentionally defers:

- Primary-key prefix parts such as `PRIMARY KEY (name(10))`, even though MySQL
  accepts them for compatible string types.
- Unique prefix parts and composite unique string indexes.
- Complete UCA 9.0 collation weights and locale tailoring.
- `TEXT`, `BLOB`, binary-string, approximate-number, temporal, `DECIMAL`, `BIT`,
  JSON, spatial, generated, or expression primary-key parts beyond already
  supported integer and `CHAR` / `VARCHAR` descriptors.
- Descending key parts, table-qualified key parts, expression key parts, ordinal
  key parts, invisible or generated invisible primary keys, named constraints,
  index options, parser options, fulltext/spatial indexes, foreign keys,
  cascades, triggers, optimizer guarantees, and protocol-complete metadata.
- Broadening `INSERT ... ON DUPLICATE KEY UPDATE`; it remains limited to the
  existing single-column conflict target subset.
- Converting an existing column to or from `AUTO_INCREMENT`.

## Runtime Observations

The MySQL 8.4.9 probes establish the visible behavior MyLite follows for this
slice:

- `CREATE TABLE cspk (a VARCHAR(10), b VARCHAR(10), v INT, PRIMARY KEY (a,b))`
  succeeds. `a` and `b` are rendered as `NOT NULL`, `SHOW COLUMNS` marks both
  as `PRI`, and `SHOW INDEX` reports ordered `PRIMARY` rows with
  `Seq_in_index` values `1` and `2`.
- `VARCHAR` key comparison is case-insensitive under MySQL's default
  `utf8mb4_0900_ai_ci` collation. Inserting `('a','b')` and then `('A','b')`
  fails with duplicate-key error `1062 / 23000`.
- Distinct trailing spaces in `VARCHAR` key parts remain distinct in the probed
  forms. Rows such as `('a','b')`, `('a','b ')`, and `('a ','b')` can coexist.
- Explicit `NULL` or `DEFAULT NULL` on a table-level primary-key part fails
  during `CREATE TABLE` with error `1171 / 42000`.
- A composite primary key whose total key-part byte budget exceeds InnoDB's
  3072-byte limit fails with error `1071 / 42000`.
- `ALTER TABLE ... ADD PRIMARY KEY (a,b)` over existing compatible rows returns
  zero affected rows and zero warnings, makes key columns `NOT NULL`, stores
  ordered key metadata, and renders the same `PRIMARY` key shape as create-time
  keys.
- `ALTER TABLE ... ADD PRIMARY KEY` fails before mutation when existing rows
  contain `NULL` in any key part (`1138 / 22004`) or duplicate key tuples
  (`1062 / 23000`).
- MySQL accepts primary prefix parts such as `PRIMARY KEY (a(3), b)` for
  compatible string columns; this remains outside this MyLite slice and is
  rejected deterministically as unsupported syntax or unsupported primary-key
  key-part shape.

## Grammar

No new grammar class is required. This phase widens the semantic validation of
the existing key-part lists from "integer composite or single string key" to
"supported integer/string key-part tuple".

The admitted MyLite grammar shape is:

```lemon
table_constraint ::= PRIMARY KEY LP key_part_list RP.

alter_table_action ::= ADD PRIMARY KEY LP key_part_list RP.

key_part_list ::= key_part.
key_part_list ::= key_part_list COMMA key_part.

key_part ::= identifier.
```

The parser may already recognize a larger key-part grammar for nonunique
indexes. For primary keys, semantic analysis for this phase admits only an
unqualified column identifier. Prefix lengths, direction keywords, table
qualification, and expression key parts are rejected for primary keys.

## Name Resolution and Catalog Rules

Target table resolution follows the existing table DDL policy:

- Unqualified table names require the selected/default schema.
- Schema-qualified table names resolve against the named schema.
- Unknown schemas and unknown tables use existing MySQL-compatible diagnostics
  for DDL target resolution.
- Reserved `_mylite_*` schema, table, physical table, index, and generated
  object names are rejected before SQLite SQL is generated.
- Temporary tables and views are not admitted by this baseline path.

Primary-key part names are resolved against the ordered MyLite column
descriptors for the target table. Resolution is descriptor-owned and independent
of SQLite metadata. Current descriptor name matching follows the existing MyLite
identifier matching policy used by table DDL and index descriptors.

Duplicate key-part names in the same primary key are rejected with the existing
duplicate-column-in-key diagnostic. Unknown key-part names are rejected with the
existing unknown-column diagnostic for key definitions.

## Type and Value Semantics

Each primary-key part must be a descriptor family already admitted by primary
keys:

- signed and unsigned integer-family columns in the current physical ranges;
- `CHAR(1..255)` and `VARCHAR(1..255)` columns.

All primary-key columns become logically `NOT NULL` in the MyLite descriptor
catalog. During `CREATE TABLE`, an explicit `NULL` attribute or explicit
`DEFAULT NULL` on a table-level primary-key part fails with the existing
MySQL-shaped `1171 / 42000` diagnostic. During `ALTER TABLE ... ADD PRIMARY
KEY`, existing `NULL` values in any key part fail with `1138 / 22004` before
catalog or physical index mutation.

String key values use MyLite's shared limited UTF-8 collation service for
primary and unique keys:

- ASCII text without embedded `NUL` is admitted.
- Non-ASCII text and embedded `NUL` in string key values are rejected before
  mutation with the existing deterministic MyLite string-key diagnostic.
- MyLite registers its `utf8mb4_0900_ai_ci` SQLite collation callback for string
  key parts. It covers the documented Unicode case/accent/canonical-equivalence
  subset and does not claim complete UCA 9.0 equivalence.

The aggregate key length must fit the current InnoDB-compatible 3072-byte
budget. MyLite computes the descriptor key-byte budget before creating or
adding the primary key. String descriptors use the existing utf8mb4
four-bytes-per-character envelope. If the aggregate key is too large, MyLite
returns error `1071 / 42000` with the existing "Specified key was too long"
message shape.

## Metadata

Descriptor-owned metadata must remain authoritative:

- `SHOW COLUMNS` returns `PRI` for each primary-key column.
- `SHOW INDEX` returns one row per primary-key part with `Key_name='PRIMARY'`,
  `Non_unique=0`, ascending `Seq_in_index`, `Collation='A'`, `Sub_part=NULL`,
  `Index_type='BTREE'`, and `Visible='YES'`.
- `SHOW CREATE TABLE` renders one table-level `PRIMARY KEY (`...`)` clause using
  the ordered descriptor key parts.
- `CREATE TABLE ... LIKE` clones the ordered primary-key descriptor.
- `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY` reports `PRI` for each key column.
- `INFORMATION_SCHEMA.STATISTICS` and `KEY_COLUMN_USAGE` expose ordered key-part
  rows.
- `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` exposes one enforced `PRIMARY KEY`
  constraint row.

Metadata does not claim optimizer statistics, full collation metadata,
privilege filtering, invisible generated keys, or protocol-complete field flags.

## Physical SQLite Handling

MyLite continues to use SQLite through public APIs and MyLite-owned SQL
translation. No SQLite fork patch is required for this phase.

The generated physical table remains a normal SQLite rowid table created by
MyLite. The primary key is represented by a generated SQLite unique index whose
name is derived from the stable MyLite table descriptor id. Generated SQL uses
only MyLite-controlled physical table and column names, quotes every SQLite
identifier, and never reuses user SQL text directly.

For string key parts, the generated index expression applies the registered
MyLite collation name. Integer key parts use their existing physical column
shape. SQLite physical uniqueness is only one enforcement layer; MyLite still
validates descriptor support, `NULL` admissibility, supported string-key values,
and aggregate key length before creating the descriptor/index pair.

`ALTER TABLE ... ADD PRIMARY KEY` validates existing rows before mutating
catalog rows. It scans only the candidate key columns needed for validation and
uses grouped SQLite queries to find the first duplicate key tuple. It then
stores catalog descriptors and creates the generated physical unique index
inside the existing DDL transaction boundary.

## Diagnostics

The feature uses existing diagnostics whenever possible:

- Syntax outside admitted primary-key key-part shapes: existing parser or
  unsupported-syntax diagnostic.
- Missing default schema, unknown schema, unknown table, unsupported object kind,
  reserved `_mylite_*` names: existing DDL diagnostics.
- Unknown key-part column: existing key-definition unknown-column diagnostic.
- Duplicate key-part column: existing duplicate-column-in-key diagnostic.
- Unsupported descriptor family: existing primary-key unsupported-type
  diagnostic.
- String key descriptor length outside the current `CHAR(1..255)` /
  `VARCHAR(1..255)` key envelope: existing string-key diagnostics.
- Explicit `NULL` or `DEFAULT NULL` on a create-time table primary-key part:
  `1171 / 42000`.
- Existing `NULL` values during `ALTER TABLE ... ADD PRIMARY KEY`:
  `1138 / 22004`.
- Duplicate key tuples on insert, update, or alter-add validation:
  `1062 / 23000`, using the existing key tuple formatting.
- Aggregate key length above 3072 bytes: `1071 / 42000`.
- Unsupported string key value: existing deterministic MyLite string-key
  diagnostic before mutation.
- Physical SQLite failure, allocation failure, or public API misuse: existing
  runtime diagnostics and cleanup rules.

Successful `CREATE TABLE` and `ALTER TABLE ... ADD PRIMARY KEY` follow the
existing DDL result convention. `ALTER TABLE ... ADD PRIMARY KEY` reports zero
affected rows and zero warnings for supported successful forms.

## Performance and Storage Notes

This phase does not materialize normal DML row sets in MyLite for key
enforcement. Insert and update conflict checks continue to use descriptor-built
SQLite probes and generated unique indexes. The only additional MyLite-side row
iteration is the DDL-time `ALTER TABLE ... ADD PRIMARY KEY` validation of
existing rows, which is unavoidable for preexisting table contents and limited
to the candidate key columns.

The `.mylite` file preamble, shifted SQLite payload, catalog generation,
descriptor cache invalidation, and VFS invariants are unchanged. The phase adds
no new storage format version and no SQLite fork patch.

## Tests

Tests must cover:

- Create-time composite `VARCHAR` primary keys and mixed integer/string primary
  keys.
- `SHOW COLUMNS`, `SHOW INDEX`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA` metadata for ordered string key parts.
- Insert and update duplicate handling through descriptor-owned keys, including
  ASCII case-insensitive comparisons and distinct `VARCHAR` trailing-space
  cases.
- `INSERT IGNORE` duplicate handling and warning count for composite string
  primary keys.
- `ALTER TABLE ... ADD PRIMARY KEY (a,b)` success over existing rows, duplicate
  failure, `NULL` failure, unsupported string-key value failure, and aggregate
  key-length failure.
- Create-time explicit `NULL`, explicit `DEFAULT NULL`, unsupported prefix
  primary-key parts, and aggregate key-length failure.
- `CREATE TABLE ... LIKE`, drop primary key, table rename, reopen persistence,
  and independent file-backed handles for keys containing string parts.
- Existing primary-key, unique-index, nonunique-index, string type, insert,
  update, metadata, file-format, and workflow tests still pass.
