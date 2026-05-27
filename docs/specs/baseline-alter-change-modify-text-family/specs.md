# Baseline ALTER CHANGE/MODIFY TEXT-Family Replacement

## Status

This feature specifies a narrow `ALTER TABLE ... CHANGE` / `MODIFY` widening
slice for existing nonbinary string descriptors. It extends the current
descriptor-driven `CHANGE` and `MODIFY` column replacement path so persistent
base-table `CHAR`, `VARCHAR`, national `CHAR`, national `VARCHAR`, and existing
`TEXT` family columns may be replaced by supported `TEXT` family descriptors.

The feature is intentionally not full MySQL column conversion support. It does
not add expression conversion, charset conversion, BLOB conversion, general
multi-action `ALTER TABLE`, temporary-table ALTER support, or new SQLite fork
patches.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `ALTER TABLE ... MODIFY COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-modify-column/specs.md`
- Baseline `ALTER TABLE ... CHANGE COLUMN` lifecycle:
  `docs/specs/baseline-alter-table-change-column/specs.md`
- Baseline key-aware `ALTER CHANGE` / `MODIFY`:
  `docs/specs/baseline-key-aware-alter-change-modify/specs.md`
- Baseline `TEXT` type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline text-family length arguments:
  `docs/specs/baseline-text-family-length-arguments/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `BLOB` and `TEXT`:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_change_modify_text_family_expectations.sh`
records the runtime probes for this feature. Observed behavior shaping this
slice:

- `ALTER TABLE t MODIFY v TEXT NOT NULL` from `VARCHAR(10) NOT NULL DEFAULT
  'd'` succeeds, removes the visible default, preserves row values, reports
  copied-row affected count for nonempty tables, and reports zero warnings.
- `ALTER TABLE t CHANGE c c_new LONGTEXT NULL` from `CHAR(5) DEFAULT 'xy'`
  succeeds, renames the column, preserves row values, reports copied-row
  affected count for nonempty tables, and reports zero warnings.
- Backtick-quoted hyphenated identifiers are accepted in `CHANGE` old and new
  column positions.
- Replacing a `TEXT` family column with another `TEXT` family descriptor
  succeeds when existing rows fit the target family and requested nullability.
- Replacing a nullable string or text column with a `TEXT` family `NOT NULL`
  descriptor fails on the first stored `NULL` row with MySQL error `1265`,
  SQLSTATE `01000`, and message text shaped as `Data truncated for column
  '<column>' at row N`.
- Shrinking existing `TEXT` family data to a smaller target such as `TINYTEXT`
  fails with error `1406`, SQLSTATE `22001`, and `Data too long for column
  '<column>' at row N` when a stored value exceeds the target byte limit.
- Replacing a full ordinary or unique `VARCHAR` key part with `TEXT` fails
  because `TEXT` columns need a key prefix length for ordinary btree indexes.
- Replacing a prefixed `VARCHAR` key part with `TEXT` succeeds and preserves
  the prefix length.
- Replacing a `VARCHAR` column covered by a `FULLTEXT` index with `TEXT`
  succeeds and preserves metadata.

## Scope

The implementation must add:

- persistent base-table `ALTER TABLE ... MODIFY [COLUMN] column_definition`
  where the existing column is a supported nonbinary string descriptor and the
  replacement descriptor is a supported `TEXT` family descriptor;
- persistent base-table `ALTER TABLE ... CHANGE [COLUMN] old_column
  column_definition` for the same source and target descriptor families;
- source descriptors: `CHAR`, `VARCHAR`, national `CHAR`, national `VARCHAR`,
  `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, `LONGTEXT`, `TEXT(M)` normalized
  descriptors, and `LONG` / `LONG VARCHAR` aliases that already normalize to
  `MEDIUMTEXT`;
- target descriptors: supported bare or normalized `TINYTEXT`, `TEXT`,
  `MEDIUMTEXT`, and `LONGTEXT` definitions admitted by the current column
  definition grammar;
- optional nullability, compatible text-family generated defaults already
  admitted by the column definition grammar, column comments, admitted
  `utf8mb4` / `ascii` charset metadata, matching collation metadata, and
  existing `binary` normalization behavior where the replacement descriptor
  remains inside the supported nonbinary text-family path;
- descriptor-driven table, old-column, replacement-column, `FIRST`, and `AFTER`
  resolution using the existing selected-schema and schema-qualified target
  policy;
- quoted identifier support already available for backtick-quoted names,
  including hyphenated column names;
- validation that existing non-`NULL` row values are valid UTF-8, contain no
  embedded `NUL`, and fit the replacement text-family byte limit;
- validation that existing `NULL` row values fail with MySQL-compatible text
  conversion diagnostics when the replacement is `NOT NULL`;
- existing index descriptor preservation when key descriptors remain valid
  after the replacement, including string prefix keys and metadata-only
  `FULLTEXT` indexes;
- deterministic rejection before mutation when an existing full ordinary,
  unique, or primary key would become an invalid full `TEXT` family key;
- descriptor-driven physical table rebuild, stable physical table naming,
  quoted SQLite identifiers, and generated non-fulltext physical index
  recreation through the existing `CHANGE` / `MODIFY` path;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`,
  `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS` metadata from MyLite
  descriptors after successful replacement;
- reopen persistence, row-value preservation, default behavior after the
  replacement, independent file-backed handle isolation, and `.mylite`
  preamble preservation;
- MySQL 8.4.9 runtime-verified expectations for supported success and error
  cases.

## Non-Goals

This feature must not implement:

- arbitrary type conversion to or from numeric, temporal, `ENUM`, `SET`,
  `JSON`, spatial, `BIT`, binary string, BLOB family, or unsupported string
  families;
- `TEXT` to `CHAR` / `VARCHAR` shrinking;
- BLOB to `TEXT`, `TEXT` to BLOB, or charset transcoding;
- generated columns, invisible column-definition syntax, auto-increment
  changes, inline primary-key addition, new keys, key prefix rewriting,
  foreign-key or CHECK dependency rewrites, or unsupported object kinds;
- warning-producing trailing-space truncation or non-strict conversion changes;
- double-quoted identifier repair under `ANSI_QUOTES`;
- multi-action `ALTER TABLE` with `CHANGE` or `MODIFY`;
- temporary tables, views, triggers, routines, privileges, metadata locks,
  online DDL scheduling, or implicit-commit emulation beyond the current ALTER
  statement boundary;
- physical schema introspection from SQLite as metadata authority;
- SQLite fork patches.

## Ownership Boundary

- Public API behavior remains unchanged. `mylite_execute()` owns public call
  validation, result ownership, and cleanup after failed statements.
- Statement context owns diagnostics reset, warning count, affected rows, and
  statement completion.
- Lexer/parser/AST own syntax admission and source spans. They already admit
  the relevant `CHANGE` and `MODIFY` column-definition shapes; this feature
  does not add new grammar tokens.
- Analyzer/planner code resolves descriptors, classifies source/target
  descriptor compatibility, validates existing rows, validates key descriptor
  compatibility, and chooses no-op, metadata-only, or physical-rebuild
  execution.
- Catalog code remains authoritative for logical type, physical type,
  nullability, default metadata, charset/collation/comment metadata, column
  ordinals, visibility, indexes, and descriptor versions.
- Result builders and metadata statements render MySQL-shaped output from
  descriptors, not SQLite schema text.
- SQLite owns physical row storage and generated physical rebuild statements.
  MyLite must quote every generated SQLite identifier and rely on prepared
  statement execution, not interpolated user data.
- Storage/VFS owns the shifted `.mylite` SQLite payload and preamble. This
  feature must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

This feature reuses the existing `CHANGE` / `MODIFY` grammar and expands only
the allowed descriptor replacement matrix:

```sql
ALTER TABLE table_name MODIFY [COLUMN] column_definition
ALTER TABLE table_name CHANGE [COLUMN] old_column_name column_definition

column_definition:
    column_name text_family_type [NULL | NOT NULL] [compatible_column_attrs]

text_family_type:
    TINYTEXT
  | TEXT
  | MEDIUMTEXT
  | LONGTEXT
  | TEXT(unsigned_decimal_integer_literal)
```

The current column-definition grammar also admits existing alias and charset
forms that normalize to a supported text-family descriptor. Unsupported
trailing clauses continue to fail through the existing parser or planner.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar surface for this slice. It
is not MySQL grammar text:

```lemon
statement ::= alter_table_modify_column_statement.
statement ::= alter_table_change_column_statement.

alter_table_modify_column_statement ::=
    ALTER TABLE table_name MODIFY column_keyword_opt column_definition.

alter_table_change_column_statement ::=
    ALTER TABLE table_name CHANGE column_keyword_opt identifier column_definition.

column_keyword_opt ::= .
column_keyword_opt ::= COLUMN.

column_definition ::= identifier text_family_type column_attribute_list_opt.

text_family_type ::= TINYTEXT.
text_family_type ::= TEXT.
text_family_type ::= TEXT LP unsigned_integer RP.
text_family_type ::= MEDIUMTEXT.
text_family_type ::= LONGTEXT.
```

## Resolution And Case Policy

Unqualified target tables use the selected schema. Qualified target tables use
the explicit schema and do not require a selected schema. Missing selected
schema, unknown schemas, unknown tables, reserved `_mylite_*` names, and
unsupported object kinds use the existing `CHANGE` / `MODIFY` diagnostics.

Column resolution remains descriptor-driven and case-insensitive under the
current MyLite descriptor-name policy. SQLite column metadata and `PRAGMA`
output are not used for logical resolution. Replacement names are checked for
duplicates in the same descriptor table, except exact same-name and case-only
renames that the existing `CHANGE` path accepts.

## Row Conversion And Nullability

Existing stored row values are read from the current physical column before the
catalog mutation is committed.

- `NULL` remains `NULL` for nullable replacement descriptors.
- `NULL` into a text-family `NOT NULL` replacement fails with `1265 / 01000`
  and does not mutate row data, descriptors, index descriptors, catalog
  generation, or SQLite schema generation.
- Existing SQLite `TEXT` values must decode as the current admitted MyLite
  UTF-8 non-`NUL` string subset and fit the target text-family byte limit.
- Existing SQLite `INTEGER`, `REAL`, or unsupported physical values fail as
  physical row corruption for this path.
- Existing `TEXT` values that exceed the target family limit fail with
  `1406 / 22001`.

This slice does not add warning-producing truncation or non-strict conversion
for stored rows.

## Index Compatibility

Existing index descriptors are preserved by column id when valid after
replacement:

- prefix key parts on `CHAR`, `VARCHAR`, and text-family descriptors remain
  valid when their prefix length is still positive and inside current aggregate
  key-length limits;
- metadata-only `FULLTEXT` key parts remain valid over supported nonzero-length
  text-family descriptors;
- full ordinary or unique key parts that would become full `TEXT` family key
  parts are rejected before mutation with the existing MySQL-shaped key
  diagnostic;
- primary keys over text-family descriptors remain unsupported.

This feature does not rewrite existing prefix lengths, synthesize new prefix
lengths, or change index names.

## Physical SQLite Handling

The implementation must reuse the descriptor-built physical rebuild path:

1. build a temporary physical table from the replacement descriptor list;
2. validate existing source rows before descriptor replacement;
3. copy rows with quoted physical identifiers in descriptor order;
4. drop the old generated physical table;
5. rename the temporary physical table to the stable generated name;
6. recreate non-fulltext generated SQLite indexes from MyLite descriptors.

No SQLite `ALTER COLUMN` extension, SQLite schema-text parsing, `PRAGMA`
metadata reliance, or fork patch is required. The generated MyLite physical
tables remain ordinary rowid tables; that invariant stays internal to
descriptor-driven rebuild planning.

## Results, Warnings, And Metadata

Successful statements return the existing empty DDL result object convention,
`warning_count == 0`, and MySQL-compatible affected rows for the admitted
subset:

- no-op replacements report zero rows;
- metadata-only replacement shapes report zero rows unless the existing
  baseline already reports a copied-row physical rebuild for that shape;
- physical type replacement over nonempty tables reports the copied-row count;
- physical type replacement over empty tables reports zero rows.

After success, descriptor-driven `SHOW COLUMNS`, `SHOW CREATE TABLE`, limited
`INFORMATION_SCHEMA.COLUMNS`, `SHOW INDEX`, and limited
`INFORMATION_SCHEMA.STATISTICS` must reflect the replacement descriptor and
preserved key metadata. Later omitted-column inserts must use the replacement
descriptor's default policy.

## Diagnostics

The implementation must preserve existing diagnostics for:

- syntax errors and unsupported trailing grammar;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` names;
- unsupported object kinds;
- unknown old, target, or position columns;
- duplicate replacement column names;
- unsupported replacement families;
- unsupported defaults, generated columns, invisible syntax, auto-increment,
  inline keys, foreign-key or CHECK constrained tables, and temporary tables;
- invalid charset or collation attributes;
- invalid full-text/ordinary/unique/primary key compatibility after
  replacement;
- `NULL` into text-family `NOT NULL` replacement (`1265 / 01000`);
- stored text too long for the replacement family (`1406 / 22001`);
- invalid stored text bytes, physical SQLite row corruption, SQLite execution
  failures, allocation failures, and public API misuse.

Supported in-range replacements produce no warnings.

## Performance And Storage

This path should stay close to SQLite for row movement. Existing `CHAR`,
`VARCHAR`, and text-family descriptors all store nonbinary values as SQLite
`TEXT`, so successful replacements can use the existing SQLite-side
`INSERT INTO temp SELECT ...` copy path after MyLite validates row
compatibility. MyLite must not materialize whole tables in memory. Row
validation may stream one source column through a prepared statement, matching
the existing `CHANGE` / `MODIFY` validation strategy.

## Test Requirements

Tests must cover:

- `MODIFY` from `VARCHAR` to `TEXT` preserving rows, removing incompatible
  visible defaults, preserving metadata, and persisting after reopen;
- `CHANGE` from `CHAR` to `LONGTEXT` with rename and row preservation;
- backtick-quoted hyphenated column names in `CHANGE`;
- existing `TEXT` family to `TEXT` family replacement;
- nullable source rows into nullable target and `NULL` into `NOT NULL` target
  diagnostics;
- target byte-limit overflow diagnostics when shrinking to `TINYTEXT`;
- prefix-index preservation and `SHOW INDEX` / `SHOW CREATE TABLE` metadata;
- rejection of full ordinary/unique/primary key descriptors that would become
  invalid full `TEXT` keys;
- metadata-only `FULLTEXT` descriptor preservation;
- omitted-column insert/default behavior after replacement;
- `.mylite` preamble preservation and independent file-backed handles where
  existing lifecycle tests do not already cover the path;
- existing parser, runtime ALTER, text-type, key-aware ALTER, metadata, and
  file-format tests continue to pass.

