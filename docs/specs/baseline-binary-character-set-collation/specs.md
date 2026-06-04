# Baseline Binary Character Set and Collation

## Summary

This phase admits MySQL's `binary` character set and `binary` collation in the
places where MyLite can map them directly onto its existing binary string
descriptors:

- static metadata rows for `SHOW CHARACTER SET`, `SHOW COLLATION`, and the
  supported `INFORMATION_SCHEMA` charset/collation tables;
- explicit column-level `CHARACTER SET binary` or `COLLATE binary` on
  `CHAR`, `VARCHAR`, and `TEXT` family definitions in the current DDL paths,
  normalized to `BINARY`, `VARBINARY`, and BLOB-family descriptors.

The feature does not add binary string literal introducers, binary comparison
semantics, table-default binary charset inheritance, or binary metadata for
`ENUM` / `SET`.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-mysql.html>
  - the `binary` character set:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-binary-set.html>
  - column character set and collation:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-column.html>
  - string data type syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_binary_character_set_collation_expectations.sh`.

The official string-type documentation states that `CHARACTER SET binary` on
character string column types creates the corresponding binary string type.
Runtime probes confirm the exact metadata shape, diagnostics, and table-rendered
DDL for the subset below.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

This feature adds:

- a static `binary` row to `SHOW CHARACTER SET` / `SHOW CHARSET`;
- a static `binary` row to `SHOW COLLATION`;
- `INFORMATION_SCHEMA.CHARACTER_SETS`, `INFORMATION_SCHEMA.COLLATIONS`, and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` rows for
  `binary`;
- explicit column-level `CHARACTER SET binary`, `CHARSET binary`, or
  `COLLATE binary` for current `CHAR`, `VARCHAR`, `TINYTEXT`, `TEXT`,
  `MEDIUMTEXT`, and `LONGTEXT` column definition paths, including the current
  shared `CREATE TABLE`, `ALTER TABLE ADD COLUMN`, `ALTER TABLE MODIFY COLUMN`,
  and `ALTER TABLE CHANGE COLUMN` planners;
- MySQL-compatible mismatch diagnostics for `CHARACTER SET utf8mb4 COLLATE
  binary` and `CHARACTER SET binary COLLATE utf8mb4_*`;
- descriptor normalization before physical table creation and catalog insert,
  so later DML and introspection use existing binary string code paths.

The supported normalizations are:

| Source column declaration | MyLite descriptor after planning |
| --- | --- |
| `CHAR` / `CHAR(1)` with binary charset/collation | `BINARY(1)` |
| `CHAR(n)` with binary charset/collation | `BINARY(n)` |
| `VARCHAR(n)` with binary charset/collation | `VARBINARY(n)` |
| `TINYTEXT` with binary charset/collation | `TINYBLOB` |
| `TEXT` with binary charset/collation | `BLOB` |
| `MEDIUMTEXT` with binary charset/collation | `MEDIUMBLOB` |
| `LONGTEXT` with binary charset/collation | `LONGBLOB` |

`SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, public
result metadata, row-value storage, DML conversion, and file-backed persistence
must observe the normalized binary string descriptors.

## Non-Goals

This feature does not implement:

- table-level `DEFAULT CHARSET=binary` or `COLLATE=binary` inheritance for
  unadorned character columns;
- `ALTER TABLE ... DEFAULT CHARSET=binary`;
- `ENUM` or `SET` `CHARACTER SET binary` / `COLLATE binary` metadata;
- the deprecated column `BINARY` attribute on nonbinary character columns;
- `_binary` introducers, unary `BINARY expr`, or binary string expression
  coercion beyond existing `CAST(... AS BINARY)` / `CONVERT(... USING BINARY)`
  slices;
- `SET NAMES binary`, binary connection character-set state, binary literal
  comparison semantics, bytewise ORDER/GROUP/DISTINCT behavior, or collation
  coercibility;
- binary table/database defaults, full charset catalogs, or SQLite fork
  patches.

Runtime probes show that MySQL 8.4.9 also converts unadorned character columns
under table-level `CHARACTER SET binary`. That is intentionally deferred
because MyLite currently plans columns before applying table options; adding
that safely should be a separate table-default charset slice.

## Ownership Boundary

- Public API remains unchanged. Existing result handles expose rows,
  diagnostics, metadata, affected rows, and warnings.
- Lexer/parser/AST already admit column charset and collation attributes and
  `SHOW CHARACTER SET` / `SHOW COLLATION`; this phase only extends column
  charset/collation option-name parsing so the reserved `BINARY` token can name
  the `binary` charset/collation in those column-attribute positions.
- Analyzer/planner owns binary charset/collation validation and descriptor
  normalization before catalog rows or SQLite SQL are generated.
- Catalog descriptors remain authoritative. Normalized binary columns are stored
  as existing binary string descriptors with empty character-set and collation
  fields, matching MySQL metadata where binary string columns have SQL `NULL`
  charset/collation names.
- Runtime owns static charset/collation metadata rows and mismatch diagnostics.
- SQLite physical storage continues to use existing `BLOB` storage and prepared
  statements; no arbitrary SQLite SQL pass-through or fork hook is needed.
- Storage/VFS and the `.mylite` preamble are unchanged.

## Syntax

The only grammar expansion is permitting the reserved `BINARY` token as the
option name in column charset/collation attributes. Table-level binary defaults
and connection `SET NAMES binary` remain deferred.

```sql
SHOW {CHARACTER SET | CHARSET} [LIKE 'pattern']
SHOW COLLATION [LIKE 'pattern']

column_name character_string_type [CHARACTER SET binary] [COLLATE binary]
column_name character_string_type [COLLATE binary]
```

The independently authored MyLite Lemon-shape remains:

```lemon
column_attribute ::= CHARACTER SET option_name.
column_attribute ::= CHARACTER SET BINARY.
column_attribute ::= CHARSET option_name.
column_attribute ::= CHARSET BINARY.
column_attribute ::= COLLATE option_name.
column_attribute ::= COLLATE BINARY.

column_type ::= char_type.
column_type ::= varchar_type.
column_type ::= text_type.

show_character_set_statement ::= SHOW CHARACTER SET show_catalog_filter_opt.
show_character_set_statement ::= SHOW CHARSET show_catalog_filter_opt.
show_collation_statement ::= SHOW COLLATION show_catalog_filter_opt.

show_catalog_filter_opt ::= .
show_catalog_filter_opt ::= LIKE STRING.
show_catalog_filter_opt ::= WHERE predicate.
```

## Semantics

### Static Metadata

`SHOW CHARACTER SET LIKE 'binary'` returns:

| Charset | Description | Default collation | Maxlen |
| --- | --- | --- | --- |
| `binary` | `Binary pseudo charset` | `binary` | `1` |

`SHOW COLLATION LIKE 'binary'` returns:

| Collation | Charset | Id | Default | Compiled | Sortlen | Pad_attribute |
| --- | --- | --- | --- | --- | --- | --- |
| `binary` | `binary` | `63` | `Yes` | `Yes` | `1` | `NO PAD` |

`INFORMATION_SCHEMA.CHARACTER_SETS` and `INFORMATION_SCHEMA.COLLATIONS` expose
the same row values. `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`
maps `binary` to `binary`.

Existing `SHOW LIKE` behavior applies: ASCII case-insensitive name matching,
existing string-literal decoding, warning count `0`, and row-count `-1` for
successful result sets.

### Column Normalization

If a supported character string descriptor has explicit `CHARACTER SET binary`
or explicit `COLLATE binary`, the planner converts the column descriptor to the
corresponding binary descriptor before default validation, index validation,
catalog persistence, and physical SQL generation.

The conversion clears column `character_set_name` and `collation_name`, because
MySQL reports binary string columns with `NULL` character set and collation in
`INFORMATION_SCHEMA.COLUMNS` and blank `Collation` in `SHOW FULL COLUMNS`.

`COLLATE binary` without `CHARACTER SET` implies the associated `binary`
character set for this conversion. `CHARACTER SET binary` without `COLLATE`
uses default collation `binary`.

If both clauses are present, the charset and collation must belong together:

- `CHARACTER SET binary COLLATE binary` is accepted;
- `CHARACTER SET utf8mb4 COLLATE binary` fails with `1253 / 42000`;
- `CHARACTER SET binary COLLATE utf8mb4_bin` fails with `1253 / 42000`.

Length limits and descriptor choices follow the already-supported binary string
families: `BINARY(0..255)`, `VARBINARY(0..65535)` within the current MyLite row
size limit, and BLOB-family descriptors. Values, defaults, DML, indexes, and
result metadata reuse the existing binary string implementation for the
normalized descriptor.

For `ALTER TABLE ... MODIFY COLUMN` and `ALTER TABLE ... CHANGE COLUMN`, the
existing table rebuild path converts the replaced column's physical value to a
BLOB when the replacement descriptor is binary. Text source values are copied as
their stored bytes; fixed `BINARY(n)` replacements apply the existing NUL
padding and too-long validation before the physical copy.

## Diagnostics

| Condition | Diagnostic |
| --- | --- |
| Supported binary charset/collation metadata query | Result set, warning count `0` |
| Supported column binary charset/collation conversion | Success, normal DDL affected rows and warning count |
| `CHARACTER SET utf8mb4 COLLATE binary` | `1253 / 42000`, collation not valid for character set |
| `CHARACTER SET binary COLLATE utf8mb4_bin` | `1253 / 42000`, collation not valid for character set |
| `CHARACTER SET binary` on unsupported descriptor families | Existing unsupported column charset/collation diagnostics |
| Unknown charset or collation | Existing unknown charset/collation diagnostics |
| Table-level binary charset/collation | Existing unsupported diagnostics until the follow-up slice |
| Allocation, catalog, SQLite, or public API failure | Existing MyLite diagnostics |

## Physical SQLite Handling

Generated physical SQL must contain the normalized binary string physical type
and no MySQL charset/collation attributes. All identifiers remain
descriptor-derived and quoted. DML continues to bind BLOB values through
existing prepared-statement paths.

Rebuild execution for supported ALTER conversions materializes rows through
MyLite's existing descriptor-aware value conversion path when the replaced
column becomes binary. The temporary physical table insert uses a prepared
statement and binds BLOB values, so fixed `BINARY(n)` NUL padding is preserved
without exposing SQLite expression behavior as a public SQL feature.

No catalog schema migration is required because existing column descriptor rows
already represent binary string types and optional charset/collation metadata.

## Tests

The MySQL expectation script records:

- binary `SHOW CHARACTER SET`, `SHOW COLLATION`, and `INFORMATION_SCHEMA` rows;
- explicit `CHAR`, `VARCHAR`, and TEXT-family binary charset/collation
  conversion, including shared `ALTER TABLE ADD` / `MODIFY` / `CHANGE`
  planning paths, as rendered by `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS`;
- accepted `COLLATE binary` without explicit charset;
- mismatch diagnostics for binary/utf8mb4 charset-collation combinations;
- observed `ENUM` / `SET` and table-default binary behavior as deferred
  follow-up evidence.

Fast C tests should cover:

- static metadata result rows and filters;
- persistent and temporary `CREATE TABLE` descriptor normalization;
- `ALTER TABLE ADD` / `MODIFY` / `CHANGE` descriptor normalization through the
  shared column planner;
- `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and
  public result metadata for normalized columns;
- insert/select/update/reopen persistence through existing binary string DML;
- mismatch and unsupported diagnostics;
- file preamble preservation and independent file-backed handles;
- existing charset/collation, binary string, parser, DDL, DML, and metadata
  tests.

## Compatibility Notes

This feature treats `binary` charset/collation as a descriptor-normalization
surface, not as a new general expression-collation engine. It improves common
DDL and metadata compatibility while keeping binary comparison, coercibility,
and table-default inheritance for separate, better-scoped phases.
