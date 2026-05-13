# Baseline utf8mb4 Legacy Collations

## Summary

This phase expands MyLite's fixed `utf8mb4_0900_ai_ci` charset/collation
surface into a narrow metadata-preserving `utf8mb4` collation slice for common
legacy schemas:

```sql
CREATE TABLE t (id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
CREATE TABLE t (id INT) COLLATE=utf8mb4_unicode_520_ci
ALTER TABLE t DEFAULT COLLATE utf8mb4_general_ci
SET NAMES utf8mb4 COLLATE utf8mb4_bin
```

The goal is to accept and preserve table default collation names that appear in
real MySQL-oriented schemas while keeping the string comparison engine honest:
this feature does not implement full Unicode collation semantics, charset
conversion, or column-level collation attributes.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/baseline-show-character-set-collation/specs.md`
  - `docs/specs/baseline-alter-table-default-charset-collation/specs.md`
  - `docs/specs/baseline-char-varchar-key-lifecycle/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Official MySQL 8.4 Reference Manual:
  - table character set and collation:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-table.html>
  - Unicode character sets:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html>
  - `SHOW COLLATION`:
    <https://dev.mysql.com/doc/refman/8.4/en/show-collation.html>
  - `INFORMATION_SCHEMA.COLLATIONS`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html>
  - `SHOW CREATE TABLE`:
    <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_utf8mb4_legacy_collations_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for the supported slice:

- Every table has a table default character set and collation.
- `DEFAULT CHARSET=utf8mb4` without `COLLATE` uses MySQL's `utf8mb4` default
  collation, `utf8mb4_0900_ai_ci`.
- `COLLATE=utf8mb4_general_ci`, `utf8mb4_bin`, `utf8mb4_unicode_ci`,
  `utf8mb4_unicode_520_ci`, or `utf8mb4_0900_ai_ci` succeeds on a table
  definition and `SHOW CREATE TABLE` preserves the selected collation.
- `COLLATE=<utf8mb4 collation>` without `CHARSET` infers table charset
  `utf8mb4`.
- Repeating the same table collation succeeds. Repeating different collation
  options succeeds and the last collation is visible in `SHOW CREATE TABLE`.
- `CREATE TABLE clone LIKE source` clones the source table's default
  charset/collation metadata.
- `ALTER TABLE target DEFAULT COLLATE utf8mb4_unicode_ci` changes the visible
  table collation, reports `ROW_COUNT() = 0`, and leaves
  `@@warning_count = 0`.
- `SET NAMES utf8mb4 COLLATE <utf8mb4 collation>` changes
  `@@character_set_client`, `@@character_set_connection`,
  `@@character_set_results`, and `@@collation_connection` to the selected
  admitted values with zero affected rows and warnings.
- `SHOW COLLATION LIKE '<collation>'` exposes each admitted collation with
  MySQL's catalog metadata:
  - `utf8mb4_general_ci`: id `45`, sort length `1`, `PAD SPACE`;
  - `utf8mb4_bin`: id `46`, sort length `1`, `PAD SPACE`;
  - `utf8mb4_unicode_ci`: id `224`, sort length `8`, `PAD SPACE`;
  - `utf8mb4_unicode_520_ci`: id `246`, sort length `8`, `PAD SPACE`;
  - `utf8mb4_0900_ai_ci`: id `255`, default `Yes`, sort length `0`, `NO PAD`.
- `INFORMATION_SCHEMA.COLLATIONS` exposes the same row attributes.
- A nonexistent charset fails with `1115 / 42000`.
- A nonexistent collation fails with `1273 / HY000`.
- A collation not valid for an explicitly selected charset fails with
  `1253 / 42000`.

## Supported Surface

MyLite supports:

- persistent base-table DDL only;
- `CREATE TABLE` table options already admitted by the current charset/collation
  grammar:
  - `[DEFAULT] CHARSET [=] utf8mb4`;
  - `[DEFAULT] CHARACTER SET [=] utf8mb4`;
  - `[DEFAULT] COLLATE [=] admitted_utf8mb4_collation`;
- `ALTER TABLE table_name [DEFAULT] CHARSET ...` and
  `ALTER TABLE table_name [DEFAULT] COLLATE ...` for the same admitted table
  option forms;
- `SET NAMES utf8mb4 COLLATE admitted_utf8mb4_collation`;
- `SET NAMES DEFAULT`, `SET CHARACTER SET utf8mb4`, and
  `SET CHARACTER SET DEFAULT` as existing fixed-baseline forms;
- option names as unquoted identifiers, quoted identifiers, single-quoted
  strings, or double-quoted strings under the current SQL mode;
- ASCII case-insensitive matching and canonical lowercase metadata rendering;
- these admitted `utf8mb4` collations:
  - `utf8mb4_0900_ai_ci`;
  - `utf8mb4_general_ci`;
  - `utf8mb4_bin`;
  - `utf8mb4_unicode_ci`;
  - `utf8mb4_unicode_520_ci`;
- table descriptor storage for default charset `utf8mb4` and the selected table
  default collation;
- descriptor-driven `SHOW CREATE TABLE` rendering of the stored table default
  collation;
- `CREATE TABLE ... LIKE` cloning of table default charset/collation metadata;
- limited `INFORMATION_SCHEMA.TABLES.TABLE_COLLATION` rendering from the table
  descriptor;
- static `SHOW COLLATION` and `INFORMATION_SCHEMA.COLLATIONS` rows for the
  admitted collations;
- no-result statement result shape for successful `CREATE TABLE`, `ALTER TABLE`,
  and `SET NAMES` forms, with `affected_rows == 0` and `warning_count == 0`;
- reopen persistence, independent handle state, and `.mylite` preamble
  preservation.

## Deferred Surface

This feature intentionally does not support:

- non-`utf8mb4` table charsets;
- `utf8mb3`, `latin1`, `ascii`, or `binary` table/column storage;
- mutable server, database, or column charset/collation defaults;
- `CREATE DATABASE ... DEFAULT CHARSET/COLLATE`, `ALTER DATABASE`, or database
  default metadata;
- column-level `CHARACTER SET` or `COLLATE` attributes;
- string literal introducers, character-set conversion, or protocol character
  set metadata beyond existing public result conventions;
- full collation-aware equality, ordering, grouping, distinct, aggregate,
  prefix-index, primary-key, or unique-key semantics;
- PAD SPACE versus NO PAD comparison differences beyond the existing
  descriptor-specific string storage/readback behavior;
- `COLLATE` expression operators, coercibility rules, `COLLATION()`,
  `CHARSET()`, or `WEIGHT_STRING()`;
- full `SHOW COLLATION` / `INFORMATION_SCHEMA.COLLATIONS` catalogs beyond the
  admitted rows;
- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` or
  `mysql.collations`;
- SQLite fork patches.

Admitting `utf8mb4_bin` and legacy Unicode collation names is therefore a
metadata compatibility improvement, not a claim that MyLite currently compares
strings exactly as MySQL does under those collations.

## Ownership Boundaries

- Public API: unchanged. Callers continue to use `mylite_execute()` and the
  existing result/diagnostic APIs.
- Statement context: owns diagnostics reset, statement result state, warning
  count, affected rows, `ROW_COUNT()`, and cleanup.
- Lexer/parser/AST: the existing table-option and `SET NAMES` grammar remains
  the syntax admission point. Parser code stores option names and does not bind
  charsets, collations, schemas, tables, or descriptors.
- Analyzer/planner: decodes option names under the current SQL mode, validates
  admitted charset/collation combinations, canonicalizes table collation names,
  resolves target/source tables through MyLite descriptors, and rejects
  unsupported names before SQLite SQL is generated.
- Catalog: table descriptors own the default charset and collation metadata.
  Existing column and index descriptors remain the authority for row storage
  and supported key semantics.
- Result builder/introspection: `SHOW CREATE TABLE`, `SHOW COLLATION`, and
  `INFORMATION_SCHEMA` render descriptor/static collation metadata without
  consulting SQLite schema text.
- Storage/VFS/file format: unchanged except for the MyLite-owned catalog schema
  version bump. The `.mylite` preamble and shifted SQLite payload invariant are
  preserved.
- SQLite physical storage: existing row tables and generated indexes remain
  SQLite storage artifacts. No new SQLite collation registration or fork patch
  is required for this metadata-only slice; existing ASCII string-key collation
  behavior stays fixed to its current limited implementation.

## Grammar

No new parser grammar is required beyond the existing table-option and
connection-charset productions. This feature widens runtime validation for
existing parsed forms.

MyLite's supported table-option grammar remains:

```lemon
table_option ::= ENGINE equal_opt option_name.
table_option ::= default_opt charset_keyword equal_opt option_name.
table_option ::= default_opt COLLATE equal_opt option_name.

charset_keyword ::= CHARSET.
charset_keyword ::= CHARACTER SET.

option_name ::= identifier.
option_name ::= STRING_LITERAL.
```

Analyzer acceptance narrows names to:

```lemon
admitted_table_charset ::= utf8mb4.

admitted_utf8mb4_collation ::= utf8mb4_0900_ai_ci.
admitted_utf8mb4_collation ::= utf8mb4_general_ci.
admitted_utf8mb4_collation ::= utf8mb4_bin.
admitted_utf8mb4_collation ::= utf8mb4_unicode_ci.
admitted_utf8mb4_collation ::= utf8mb4_unicode_520_ci.
```

Connection charset grammar remains:

```lemon
set_statement ::= SET NAMES set_character_set_target set_names_collation_opt.
set_statement ::= SET CHARACTER SET set_character_set_target.

set_names_collation_opt ::= .
set_names_collation_opt ::= COLLATE option_name.
```

`SET CHARACTER SET ... COLLATE ...` remains a syntax error as before.

## Table Option Resolution

Planning scans the table-option list left to right:

1. `CHARSET` / `CHARACTER SET` must decode to `utf8mb4`.
2. `COLLATE` must decode to one admitted `utf8mb4` collation.
3. `COLLATE` without an explicit charset still sets table charset `utf8mb4`.
4. Repeated charset declarations are accepted only when all decoded names are
   `utf8mb4`; any other charset fails with the existing unknown-character-set
   diagnostic.
5. Repeated collation declarations are accepted; the last admitted collation
   becomes the stored table default.
6. If no collation is specified, the stored default is `utf8mb4_0900_ai_ci`.
7. If an explicit non-`utf8mb4` charset is paired with an admitted `utf8mb4`
   collation, MyLite reports the MySQL-compatible invalid charset/collation
   pairing diagnostic when it can classify both names. Unknown charsets still
   report unknown charset first.

The canonical stored names are lowercase ASCII. Descriptor name matching and
catalog object name case behavior are otherwise unchanged.

## Catalog And Metadata

Catalog schema version `13` adds table-level metadata to
`_mylite_catalog_tables`:

```sql
default_charset TEXT NOT NULL
default_collation TEXT NOT NULL
```

Existing tables migrating from schema version `12` are assigned:

```text
default_charset = utf8mb4
default_collation = utf8mb4_0900_ai_ci
```

Fresh table descriptors are created with those defaults unless a supported
table option supplies a different admitted collation.

`ALTER TABLE ... DEFAULT CHARSET/COLLATE` updates only the table descriptor
metadata when the effective charset/collation differs from the stored values.
It must not rewrite user rows, change column descriptors, change index
descriptors, or modify physical SQLite table definitions. It may bump the
table descriptor version and catalog generation because the table descriptor's
visible metadata changed.

`CREATE TABLE ... LIKE` clones the source table default charset/collation while
keeping its existing auto-increment counter reset behavior.

`SHOW CREATE TABLE` renders:

```sql
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=<stored_collation>
```

`INFORMATION_SCHEMA.TABLES.TABLE_COLLATION` renders the stored table collation
for persistent base tables.

Column metadata remains at the current fixed `utf8mb4` /
`utf8mb4_0900_ai_ci` baseline until column-level charset/collation descriptors
are designed. This avoids implying full column collation semantics from a table
default metadata slice.

## Static Collation Catalog Rows

The admitted collation rows are:

| Collation | Charset | Id | Default | Compiled | Sortlen | Pad_attribute |
| --- | --- | ---: | --- | --- | ---: | --- |
| `utf8mb4_general_ci` | `utf8mb4` | 45 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_bin` | `utf8mb4` | 46 | `` | `Yes` | 1 | `PAD SPACE` |
| `utf8mb4_unicode_ci` | `utf8mb4` | 224 | `` | `Yes` | 8 | `PAD SPACE` |
| `utf8mb4_unicode_520_ci` | `utf8mb4` | 246 | `` | `Yes` | 8 | `PAD SPACE` |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 | `Yes` | `Yes` | 0 | `NO PAD` |

`SHOW CHARACTER SET` remains a one-row `utf8mb4` surface with default collation
`utf8mb4_0900_ai_ci`, matching MySQL's default collation for `utf8mb4`.

## SET NAMES Semantics

`SET NAMES utf8mb4 COLLATE admitted_utf8mb4_collation`:

- sets the session connection charset fields to `utf8mb4`;
- sets `@@collation_connection` to the canonical admitted collation name;
- returns the existing no-row `SET` result shape with `affected_rows == 0`;
- leaves `@@warning_count == 0`.

`SET NAMES utf8mb4` and `SET NAMES DEFAULT` preserve the existing fixed default
connection state: `utf8mb4` / `utf8mb4_0900_ai_ci`.

This state affects scalar system-variable readback only. It does not change
string literal conversion, column storage, comparison semantics, or result
metadata in this slice.

## Diagnostics

Required diagnostics:

- syntax errors: existing parser error `1064 / 42000`;
- missing default schema, unknown schema/table, reserved names, unsupported
  object kind: existing table DDL diagnostics;
- unknown charset: `1115 / 42000`, `Unknown character set: '<name>'`;
- unknown collation: `1273 / HY000`, `Unknown collation: '<name>'`;
- invalid charset/collation pairing when explicitly detected:
  `1253 / 42000`, `COLLATION '<collation>' is not valid for CHARACTER SET '<charset>'`;
- NUL bytes in option names: existing deterministic MyLite unsupported
  diagnostic for charset/collation option names;
- physical SQLite failure: existing physical row/schema error path;
- allocation failure: `MYLITE_NOMEM` with handle diagnostics;
- public API misuse: unchanged existing public API behavior.

Unsupported valid MySQL collations outside the admitted set use the unknown
collation diagnostic for now. That is MyLite-specific and must remain documented
until the catalog grows.

## Performance And SQLite Fit

This feature is metadata-only. It adds two small table-descriptor text fields
and extends static result sets by four rows. It does not materialize user rows,
does not scan data for DDL, does not add physical indexes, and does not route
string comparisons through MyLite outside existing key enforcement paths.

The implementation is MyLite wrapper/catalog work using public SQLite DDL and
prepared statements for catalog mutations. No SQLite extension point or fork
patch is needed.

## Tests

Add or extend plain C tests under `packages/libmylite/tests/` and keep existing
dotted CTest names where possible. Cover:

- `CREATE TABLE` with each admitted legacy collation;
- `COLLATE` without explicit `CHARSET`;
- uppercase, quoted identifier, and string option names;
- repeated collation options with last value visible;
- default charset without collation still rendering `utf8mb4_0900_ai_ci`;
- `SHOW CREATE TABLE` preservation before and after reopen;
- `INFORMATION_SCHEMA.TABLES.TABLE_COLLATION`;
- `CREATE TABLE ... LIKE` cloning;
- `ALTER TABLE ... DEFAULT COLLATE` metadata update and no-row result shape;
- no-op `ALTER TABLE ... DEFAULT CHARSET=utf8mb4` preserving existing rows;
- `SET NAMES utf8mb4 COLLATE <admitted>` session readback;
- `SHOW COLLATION` / `LIKE` / `WHERE` and
  `INFORMATION_SCHEMA.COLLATIONS` rows for admitted collations;
- unknown charset, unknown collation, invalid charset/collation pairing,
  unsupported collations outside the admitted set, NUL option names, reserved
  targets, unknown schemas/tables, and syntax errors;
- existing charset/collation, table lifecycle, show, information schema,
  string key, prefix index, DML, file-backed opening, and VFS tests;
- `.mylite` preamble preservation and independent file-backed handles.

Run:

1. `packages/libmylite/tests/mysql_baseline_utf8mb4_legacy_collations_expectations.sh`
2. `cmake --build --preset dev`
3. focused CTests for table charset/collation, show charset/collation, alter
   table default charset/collation, information schema, create-table-like, and
   string/index lifecycle tests
4. `cmake --workflow --preset check`

## Compatibility Docs

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/character-sets.md`;
- `docs/compatibility/collations.md`;
- `docs/compatibility/sql-set-statements.md`;
- `docs/compatibility/sql-table-ddl.md`;
- `docs/compatibility/metadata-information-schema.md`;
- `docs/compatibility/sql-show-statements.md`.

Use limited wording. Do not claim full collation support, column collations,
charset conversion, mutable server/database defaults, full catalogs, or
collation-aware string comparison semantics.
