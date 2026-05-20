# Baseline Unique Binary Prefix Indexes

## Summary

MyLite extends the existing descriptor-owned secondary-index lifecycle to admit
unique prefix indexes on binary string descriptor columns. The feature is narrow:
it covers persistent base tables and the already supported `CREATE TABLE`,
`ALTER TABLE ... ADD UNIQUE`, and standalone `CREATE UNIQUE INDEX` forms whose
key parts are unqualified descriptor columns. It does not add primary prefix
keys, spatial indexes, functional key parts, optimizer guarantees, or broader
binary comparison semantics.

The implementation stays in MyLite's parser/planner/catalog/execution layers.
SQLite stores and enforces the generated physical unique expression indexes, but
MyLite descriptors remain the authority for table names, column names, key
parts, metadata, duplicate diagnostics, and public behavior.

## Compatibility Sources

- Official MySQL 8.4 Reference Manual:
  - https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html
  - https://dev.mysql.com/doc/refman/8.4/en/create-index.html
  - https://dev.mysql.com/doc/refman/8.4/en/show-index.html
  - https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
  - https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
  - https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4.9 runtime probes captured in
  `packages/libmylite/tests/mysql_baseline_unique_binary_prefix_indexes_expectations.sh`.

The MySQL documentation describes prefix key parts for `BINARY`, `VARBINARY`,
and BLOB-family columns, requires prefixes for BLOB-family key parts, measures
binary prefixes in bytes, exposes prefixes through `SHOW INDEX.Sub_part` and
`INFORMATION_SCHEMA.STATISTICS.SUB_PART`, and treats unique prefix keys as real
uniqueness constraints. MyLite's text here is independently authored and uses
the documentation plus runtime probes only as behavior evidence.

## Public Scope

Supported:

- Persistent base tables only.
- `CREATE TABLE` secondary unique key definitions:
  - inline `UNIQUE` forms that already lower to secondary unique descriptors;
  - table-level `UNIQUE [KEY|INDEX] [name] (key_part[, ...])`;
  - table-level `CONSTRAINT symbol UNIQUE [KEY|INDEX] [name] (key_part[, ...])`.
- `ALTER TABLE table_name ADD UNIQUE [INDEX|KEY] [name] (key_part[, ...])`.
- `ALTER TABLE table_name ADD CONSTRAINT [symbol] UNIQUE [INDEX|KEY] [name] (key_part[, ...])`.
- `CREATE UNIQUE INDEX index_name ON table_name (key_part[, ...])`.
- Existing limited index options for non-fulltext secondary indexes:
  `USING BTREE`, `USING HASH` without ordered key parts, `COMMENT`, and
  `VISIBLE` / `INVISIBLE`.
- Binary prefix key parts on `BINARY`, `VARBINARY`, `TINYBLOB`, `BLOB`,
  `MEDIUMBLOB`, and `LONGBLOB` descriptor columns.
- Composite unique keys that mix binary prefix key parts with already supported
  full scalar key parts and string prefix key parts.
- Existing `ASC` / `DESC` key-part metadata.
- Descriptor-backed metadata in `SHOW CREATE TABLE`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`,
  `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and `SHOW COLUMNS` / `COLUMNS.COLUMN_KEY`.
- `CREATE TABLE ... LIKE`, rename/drop/visibility index operations, table
  rename/drop, persistence across close/reopen, and independent file handles
  using the existing descriptor lifecycle.
- Duplicate enforcement for existing rows at index creation time and for the
  current descriptor-driven `INSERT`, `INSERT IGNORE`,
  `INSERT ... ON DUPLICATE KEY UPDATE`, `REPLACE`, and `UPDATE` subsets.
- Multiple `NULL` values in unique key parts. As in MySQL, a unique key tuple
  with any `NULL` key part does not conflict with another tuple.

Deferred:

- Primary-key prefix parts.
- Full-column `BINARY` / `VARBINARY` unique key parts unless already supported
  by another unique-index slice.
- BLOB-family key parts without an explicit prefix.
- Spatial, fulltext search behavior, functional, expression, table-qualified,
  generated-column, multi-valued, and prefix-on-expression key parts.
- New optimizer/index-use promises.
- Unicode-complete string collation behavior and broad binary comparison
  predicate semantics outside uniqueness enforcement.
- Redundant-index warnings and privilege semantics.

## Grammar

The existing index grammar is reused. The new behavior is semantic: binary
string descriptor columns with prefix lengths are no longer rejected solely
because the index is unique.

Representative MyLite Lemon-style snippets:

```lemon
secondary_index_def ::= UNIQUE unique_index_name_opt LP key_part_list RP index_option_list_opt.
secondary_index_def ::= CONSTRAINT ident UNIQUE unique_keyword_opt index_name_opt
                        LP key_part_list RP index_option_list_opt.
alter_table_action ::= ADD UNIQUE unique_keyword_opt index_name_opt
                       LP key_part_list RP index_option_list_opt.
alter_table_action ::= ADD CONSTRAINT ident UNIQUE unique_keyword_opt index_name_opt
                       LP key_part_list RP index_option_list_opt.
create_index_stmt ::= CREATE UNIQUE INDEX ident ON table_name
                      LP key_part_list RP index_option_list_opt.
key_part ::= ident prefix_length_opt sort_direction_opt.
prefix_length_opt ::= .
prefix_length_opt ::= LP unsigned_integer_literal RP.
sort_direction_opt ::= .
sort_direction_opt ::= ASC.
sort_direction_opt ::= DESC.
```

Unsupported grammar shapes continue to be rejected by the existing parser or
planner paths.

## Name Resolution and Descriptors

Table resolution follows the existing selected/default schema policy for
unqualified and schema-qualified DDL. Reserved `_mylite_*` schema/table names,
unknown schemas, unknown tables, temporary tables where unsupported, and views
continue to use the existing descriptor-driven diagnostics.

Key-part column resolution is descriptor-driven and case-insensitive according
to the current catalog name policy. SQLite metadata is not consulted to decide
whether a column exists, which type family it belongs to, whether it is
nullable, or how it should render in MySQL metadata.

Each admitted unique binary prefix key part stores:

- the descriptor column id;
- ordinal position;
- positive byte prefix length;
- sort direction;
- existing index visibility, comment, type, uniqueness, and generated physical
  name metadata.

No catalog schema version is needed beyond existing secondary-index descriptor
rows because prefix length and sort direction already exist in the catalog.

## Prefix Length Semantics

Binary prefix lengths are byte counts. MyLite admits positive decimal integer
literal prefixes after the existing parser normalization.

Validation remains MySQL-shaped:

- `BINARY(N)` and `VARBINARY(N)` prefixes must be between `1` and `N`.
- `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB` key parts must specify a
  positive prefix.
- BLOB-family prefixes are bounded by the current descriptor type maximum when
  smaller than the engine key limit, such as `255` bytes for `TINYBLOB`.
- Aggregate key bytes across the composite key must not exceed the current
  InnoDB-shaped `3072` byte envelope.
- Prefix length `0`, non-integer prefixes, negative prefixes, over-declared
  `BINARY` / `VARBINARY` prefixes, missing BLOB prefixes, unknown columns,
  duplicate key parts, duplicate index names, and reserved names keep the
  existing MySQL-shaped diagnostics.

## Duplicate Semantics and Diagnostics

The unique key value for a binary prefix key part is the first `N` bytes of the
stored value. If the stored value is shorter than `N`, the whole stored value is
used. `NULL` key parts do not participate in duplicate conflicts.

SQLite enforces the physical unique expression index, but MyLite formats public
duplicate-key diagnostics from descriptor key parts and values. For binary
prefix parts, diagnostic values use the indexed byte prefix:

- printable ASCII bytes `0x20` through `0x7e` render as those bytes;
- every other byte renders as uppercase `\xHH`;
- composite key-part displays are joined by `-`;
- the existing duplicate-key display length envelope is preserved.

Observed MySQL 8.4.9 examples include:

- `X'61626364'` conflicting with `X'616263FF'` on `VARBINARY(8)(3)` reports
  `Duplicate entry 'abc'`.
- `X'0001AA'` conflicting with `X'0001BB'` on `VARBINARY(4)(2)` reports
  `Duplicate entry '\x00\x01'`.
- `X'FF01AA'` conflicting with `X'FF01BB'` on `VARBINARY(4)(2)` reports
  `Duplicate entry '\xFF\x01'`.

## Physical SQLite Handling

Generated physical table names remain stable MyLite names such as
`_mylite_user_table_<table_id>`. Generated physical unique index names remain
descriptor-owned MyLite names.

For a binary prefix key part, generated SQLite index expressions use:

```sql
substr("column_name", 1, <prefix_length>)
```

No text collation is appended for binary string descriptor columns. Existing
string prefix key parts continue to append the MyLite ASCII
`utf8mb4_0900_ai_ci` collation annotation.

MyLite continues to quote every generated SQLite identifier and binds DML values
through prepared statements. No SQLite fork patch is required for this slice:
SQLite expression indexes and BLOB parameter binding are sufficient public
SQLite behavior. The MyLite layer owns metadata, validation, and diagnostics.

## Result and Warning Behavior

Successful DDL follows existing result conventions:

- successful `CREATE TABLE`, `ALTER TABLE ... ADD UNIQUE`, and
  `CREATE UNIQUE INDEX` report zero affected rows and zero warnings except for
  already supported `USING HASH` notes;
- successful DML reports the existing MySQL-compatible affected rows and
  warning counts for supported `INSERT`, `INSERT IGNORE`, ODKU, `REPLACE`, and
  `UPDATE` subsets;
- successful metadata statements return row result sets through the existing
  public result API.

`INSERT IGNORE` converts duplicate-key errors on unique binary prefix keys into
warnings using the existing warning machinery. `INSERT ... ON DUPLICATE KEY
UPDATE` detects the conflicting unique binary prefix key through descriptor key
matching and executes the existing ODKU update subset.

## Diagnostics

The supported subset must produce deterministic diagnostics for:

- syntax errors and unsupported key-part grammar;
- missing default schema, unknown schema, unknown table, reserved target names,
  unsupported object kinds, and temporary-table exclusions;
- unknown columns and duplicate key parts;
- duplicate index names and reserved index names;
- missing BLOB-family prefix length;
- zero prefix length;
- over-declared `BINARY` / `VARBINARY` prefixes;
- BLOB-family prefix lengths over type caps or the aggregate key-length cap;
- existing duplicate rows when adding or creating a unique index;
- DML duplicate-key conflicts for inserted or updated rows;
- allocation failures and physical SQLite failures.

Unsupported shapes keep existing diagnostics and are not broadened by this
slice.

## Tests

Add MySQL-runtime expectation coverage for:

- create-time, alter-added, and standalone unique binary prefix index metadata;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`;
- `BINARY`, `VARBINARY`, `TINYBLOB`, and `BLOB` prefixes;
- composite unique keys with binary prefixes;
- duplicate insert errors, nonprintable-byte duplicate diagnostics,
  `INSERT IGNORE`, ODKU, duplicate update errors, and successful update;
- existing-row duplicate validation for `ALTER TABLE ... ADD UNIQUE` and
  `CREATE UNIQUE INDEX`;
- prefix length diagnostics.

Add fast C runtime coverage for:

- metadata and physical index creation;
- binary-byte DML storage/readback;
- duplicate diagnostics including nonprintable bytes;
- `INSERT IGNORE`, ODKU, and `UPDATE` enforcement;
- close/reopen persistence and file preamble preservation;
- independent file-backed handles;
- removing the prior unsupported diagnostic for unique binary prefix keys.
