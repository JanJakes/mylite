# Baseline Binary Full-Column Indexes

## Summary

MyLite extends the existing descriptor-owned secondary-index lifecycle to admit
full-column `BINARY(N)` and `VARBINARY(N)` key parts in the current nonunique
and unique secondary-index forms. This removes the deliberate gap left by the
binary prefix index slices while keeping BLOB-family columns prefix-only.

This slice is metadata and storage oriented. It does not add binary primary
keys, full-column BLOB indexes, binary predicate semantics, optimizer promises,
or new SQLite fork hooks. MyLite descriptors remain authoritative for index
definitions, generated physical SQLite index SQL, metadata rendering, duplicate
checks, and diagnostics.

## Compatibility Sources

- Official MySQL 8.4 Reference Manual:
  - https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html
  - https://dev.mysql.com/doc/refman/8.4/en/create-table.html
  - https://dev.mysql.com/doc/refman/8.4/en/create-index.html
  - https://dev.mysql.com/doc/refman/8.4/en/show-index.html
  - https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
  - https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4.9 runtime probes captured in
  `packages/libmylite/tests/mysql_baseline_binary_full_column_indexes_expectations.sh`.

The MySQL documentation states that `BLOB` and `TEXT` index parts require an
explicit prefix, while prefixes are optional for `CHAR`, `VARCHAR`, `BINARY`,
and `VARBINARY`. It also documents that binary prefix lengths are byte counts
and that `SHOW INDEX.Sub_part` is `NULL` when the whole column is indexed. The
MyLite text here is independently authored from those public docs and observed
MySQL 8.4.9 behavior.

## Public Scope

Supported:

- Persistent base tables only.
- Full-column `BINARY(N)` and `VARBINARY(N)` secondary key parts in:
  - table-level `KEY` / `INDEX`;
  - inline and table-level `UNIQUE` / `UNIQUE KEY` / `UNIQUE INDEX`;
  - named table-level `CONSTRAINT name UNIQUE [KEY|INDEX]`;
  - standalone `CREATE INDEX` and `CREATE UNIQUE INDEX`;
  - single-action `ALTER TABLE ... ADD INDEX|KEY`;
  - single-action `ALTER TABLE ... ADD UNIQUE [INDEX|KEY]`;
  - single-action `ALTER TABLE ... ADD CONSTRAINT ... UNIQUE [INDEX|KEY]`.
- Composite secondary and unique indexes mixing full binary key parts with
  already supported full scalar, string full, string prefix, and binary prefix
  key parts.
- Existing `ASC` / `DESC`, `USING BTREE`, limited `USING HASH`, `COMMENT`, and
  `VISIBLE` / `INVISIBLE` metadata on supported non-fulltext indexes.
- Descriptor-backed rendering in `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `SHOW COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`,
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` where those surfaces already include
  supported indexes.
- Existing descriptor-driven DML enforcement for supported `INSERT`,
  `INSERT IGNORE`, `INSERT ... ON DUPLICATE KEY UPDATE`, `REPLACE`, and
  `UPDATE`.
- Existing index clone, drop, rename, visibility, table rename/drop, reopen
  persistence, and independent-handle behavior.

Deferred:

- Primary keys over binary string columns.
- Full-column `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, or `LONGBLOB` indexes.
- Spatial, functional, expression, table-qualified, generated-column, and
  multi-valued key parts.
- Broad binary comparison predicates, binary ordering in query execution, and
  optimizer/index-selection behavior.
- Redundant-index warnings and privilege semantics.

## Grammar

No new grammar is required. The existing index grammar already admits unprefixed
key parts; this feature changes semantic validation for the `BINARY` and
`VARBINARY` descriptor families.

Representative MyLite Lemon-style snippets:

```lemon
secondary_index_def ::= INDEX index_name_opt LP key_part_list RP index_option_list_opt.
secondary_index_def ::= UNIQUE unique_keyword_opt index_name_opt
                        LP key_part_list RP index_option_list_opt.
secondary_index_def ::= CONSTRAINT ident UNIQUE unique_keyword_opt index_name_opt
                        LP key_part_list RP index_option_list_opt.
alter_table_action ::= ADD INDEX index_name_opt LP key_part_list RP index_option_list_opt.
alter_table_action ::= ADD UNIQUE unique_keyword_opt index_name_opt
                       LP key_part_list RP index_option_list_opt.
create_index_stmt ::= CREATE unique_opt INDEX ident ON table_name
                      LP key_part_list RP index_option_list_opt.
key_part ::= ident prefix_length_opt sort_direction_opt.
prefix_length_opt ::= .
prefix_length_opt ::= LP unsigned_integer_literal RP.
sort_direction_opt ::= .
sort_direction_opt ::= ASC.
sort_direction_opt ::= DESC.
```

Unsupported key-part shapes continue through existing parser or planner
diagnostics.

## Name Resolution and Descriptors

Target table resolution follows the existing selected/default schema policy.
Reserved `_mylite_*` schema and table names, unknown schemas, unknown tables,
temporary tables where not admitted, and views use the current descriptor-driven
diagnostics.

Key-part columns resolve against MyLite catalog descriptors with the current
case-insensitive descriptor name policy. SQLite schema text is not consulted to
decide whether a key part exists, which type family it belongs to, or how it
should render in MySQL metadata.

For admitted full binary key parts, the index-column descriptor stores:

- column id;
- ordinal position;
- no prefix length;
- sort direction;
- existing index kind, uniqueness, visibility, comment, and physical-name
  metadata.

No catalog format change is required.

## Key Length Semantics

Full `BINARY(N)` key bytes equal `N`. Full `VARBINARY(N)` key bytes equal `N`;
the MySQL key-length budget does not include MyLite's row-storage length prefix.
Zero-length `BINARY(0)` and `VARBINARY(0)` full key parts fail with the current
storage-engine-cannot-index-column diagnostic.

The total key bytes for all parts in an index must not exceed the current
InnoDB-shaped 3072-byte envelope. Therefore:

- `VARBINARY(3072), KEY (v)` is admitted.
- `VARBINARY(3073), KEY (v)` fails with key-too-long diagnostics.
- `VARBINARY(0), KEY (v)` fails with `1167 / 42000`.
- composite full binary key parts whose aggregate declared length exceeds
  3072 bytes fail before catalog or SQLite mutation.

BLOB-family full key parts continue to fail with MySQL-compatible
`1170 / 42000` because MySQL requires a prefix length for `BLOB` and `TEXT`
columns.

## Duplicate Semantics and Diagnostics

Full binary unique key parts compare the stored byte sequence. `VARBINARY`
values are compared by their actual byte length. `BINARY(N)` values are stored
right-padded with `0x00` bytes to `N` and compare including those stored bytes.

As in MySQL, unique key tuples with any SQL `NULL` key part do not conflict.

Duplicate-key diagnostics are formatted by MyLite from descriptor key parts,
not by exposing SQLite error text:

- printable ASCII bytes `0x20` through `0x7e` render as characters;
- other bytes render as uppercase `\xHH`;
- fixed `BINARY(N)` displays trim trailing `0x00` bytes before escaping;
- composite parts are joined with `-`;
- the existing duplicate-key display truncation envelope is preserved.

Observed MySQL 8.4.9 examples:

- `VARBINARY(4)` value `X'0001AA'` reports `\x00\x01\xAA`.
- `BINARY(4)` value `X'41420000'` reports `AB`.
- `BINARY(4)` value `X'41004200'` reports `A\x00B`.

## Physical SQLite Handling

Generated physical table names and index names remain stable MyLite-owned
identifiers such as `_mylite_user_table_<table_id>` and
`_mylite_user_index_<index_id>`.

For full binary key parts, generated SQLite index SQL references the quoted
physical column directly:

```sql
CREATE INDEX "_mylite_user_index_N"
ON "_mylite_user_table_M" ("binary_column")
```

No MyLite text collation is appended for binary string descriptors. Prefix key
parts continue to use `substr(...)`, and nonbinary string key parts continue to
use the existing MyLite ASCII collation hook.

SQLite's ordinary BLOB values and B-tree indexes are sufficient for this slice.
No SQLite fork change or extension point is required.

## Result and Warning Behavior

Successful DDL keeps existing result conventions:

- successful `CREATE TABLE`, `ALTER TABLE ... ADD INDEX`, and
  `CREATE [UNIQUE] INDEX` report zero affected rows and zero warnings except
  for already supported `USING HASH` fallback notes;
- successful DML uses existing affected-row, warning, insert-id, and
  diagnostics behavior;
- `SHOW` and `INFORMATION_SCHEMA` surfaces return row results through the
  existing public result API.

## Diagnostics

The supported subset must produce deterministic diagnostics for:

- missing default schema;
- unknown schema or table;
- reserved `_mylite_*` targets;
- unsupported object kinds;
- unknown key columns;
- duplicate key part columns;
- duplicate index names;
- BLOB-family full key parts without a prefix; full `TEXT` family key parts
  are covered by the documented WordPress bridge;
- `VARBINARY` or composite full binary key parts exceeding 3072 bytes;
- unsupported primary binary keys;
- unsupported `BIT`, `ENUM`, `SET`, JSON, spatial, functional, expression, or
  table-qualified key parts;
- duplicate existing rows when adding or creating a unique full binary index;
- duplicate DML rows on a unique full binary index;
- physical SQLite failures and allocation failures.

## Performance and Storage

MyLite does not materialize indexed rows to maintain the full binary index.
Index creation emits one descriptor-built SQLite index statement and lets
SQLite build the B-tree. Existing-row duplicate validation for unique indexes
uses descriptor-built SQLite grouping queries, as in existing unique-index
slices. DML duplicate checks reuse the current descriptor key-probe path and
bind BLOB values through prepared statements.

The `.mylite` preamble and shifted SQLite payload invariants are unchanged.

## Tests

Add MySQL-runtime expectation coverage and C runtime coverage for:

- create-time, alter-added, and standalone full binary nonunique indexes;
- create-time, alter-added, and standalone full binary unique indexes;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS.SUB_PART = NULL`;
- `ASC` / `DESC` direction metadata;
- `BINARY` right-padding readback and `VARBINARY` length preservation;
- duplicate enforcement, duplicate diagnostics including fixed-binary trailing
  NUL trimming, `INSERT IGNORE`, ODKU, `REPLACE`, and `UPDATE`;
- multiple `NULL` unique-key values;
- existing-row duplicate validation for `ALTER TABLE ... ADD UNIQUE` and
  `CREATE UNIQUE INDEX`;
- BLOB full key rejection, zero-length binary full key rejection, key-length
  boundary `3072`, and aggregate key-length overflow;
- persistence after close/reopen, `CREATE TABLE ... LIKE`, drop/rename index,
  table rename/drop, independent handles, and preamble preservation.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-indexes-constraints.md`
to describe full `BINARY` / `VARBINARY` secondary and unique key support while
still documenting BLOB-family full key parts, binary primary keys, optimizer
semantics, and broader binary query behavior as deferred.
