# Baseline National Character Aliases

## Status

This feature specifies a narrow DDL/type-alias slice for MySQL national
character column types. It admits the common `NCHAR`, `NATIONAL CHAR`,
`NATIONAL CHARACTER`, `NVARCHAR`, `NATIONAL VARCHAR`, `NCHAR VARCHAR`,
`NCHAR VARYING`, `NATIONAL CHAR VARYING`, and `NATIONAL CHARACTER VARYING`
forms for persistent base-table descriptors.

MySQL 8.4.9 expands these declarations to `CHAR` / `VARCHAR` columns using the
national character set, currently `utf8mb3`, and emits one deprecation warning
per national column declaration. MyLite stores national aliases as
MyLite-owned logical descriptors (`NCHAR(n)` / `NVARCHAR(n)`) with SQLite
`TEXT` physical storage. The distinct logical descriptor preserves
`utf8mb3` metadata and `SHOW CREATE TABLE` rendering without adding catalog
columns for general per-column character sets.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline `CHAR` type: `docs/specs/baseline-char-type/specs.md`
- Baseline `VARCHAR` type: `docs/specs/baseline-varchar-type/specs.md`
- Baseline character aliases:
  `docs/specs/baseline-character-alias-lifecycle/specs.md`
- Baseline table charset and collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, national character set:
  https://dev.mysql.com/doc/refman/8.4/en/charset-national.html
- MySQL 8.4 Reference Manual, string data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_national_character_aliases_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- `NCHAR`, `NCHAR(n)`, `NATIONAL CHAR(n)`, and `NATIONAL CHARACTER(n)` are
  accepted and render as `char(n) CHARACTER SET utf8mb3 COLLATE
  utf8mb3_general_ci`; bare `NCHAR` has length `1`.
- `NVARCHAR(n)`, `NATIONAL VARCHAR(n)`, `NCHAR VARCHAR(n)`,
  `NCHAR VARYING(n)`, `NATIONAL CHAR VARYING(n)`, and
  `NATIONAL CHARACTER VARYING(n)` are accepted and render as `varchar(n)
  CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci`.
- MySQL emits warning `3720` once for each national column declaration in
  `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, `ALTER TABLE ... MODIFY
  COLUMN`, and `ALTER TABLE ... CHANGE COLUMN`.
- `CREATE TABLE ... LIKE` clones the expanded `utf8mb3` column metadata and
  does not emit a new national-character warning.
- `SHOW FULL COLUMNS` reports `utf8mb3_general_ci` collation for national alias
  columns.
- `INFORMATION_SCHEMA.COLUMNS` reports `CHARACTER_SET_NAME = utf8mb3`,
  `COLLATION_NAME = utf8mb3_general_ci`, `COLUMN_TYPE = char(n)` or
  `varchar(n)`, and `CHARACTER_OCTET_LENGTH = n * 3`.
- DML follows the expanded `CHAR` / `VARCHAR` behavior: `CHAR` readback trims
  trailing spaces in the default SQL mode, while `VARCHAR` preserves them.
- `NVARCHAR` and `NATIONAL VARCHAR` without a length are syntax errors.
- `NCHAR(256)` fails with MySQL error `1074`.
- MySQL accepts wider `NVARCHAR` lengths up to the row-size envelope. MyLite
  intentionally keeps the current `VARCHAR(0..16383)` envelope for this slice.

## Scope

The implementation must add:

- parser support for the admitted national `CHAR` and `VARCHAR` aliases in
  column type positions;
- one warning `3720` for each successfully planned national column declaration
  that appears explicitly in DDL;
- persistent logical descriptors `NCHAR(n)` and `NVARCHAR(n)` with existing
  SQLite `TEXT` physical storage;
- use of existing descriptor-driven `CHAR` / `VARCHAR` storage, defaults,
  DML conversion, predicates, ordering, keys, cloning, and persistence where
  applicable;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW CREATE TABLE`,
  `INFORMATION_SCHEMA.COLUMNS`, and result-column metadata that expose the
  national descriptor's `utf8mb3` character set and collation;
- `CREATE TABLE ... LIKE` preservation without a fresh national warning;
- parser, runtime, MySQL-expectation, documentation, and compatibility-matrix
  coverage for the exact supported subset.

## Non-Goals

This feature must not implement:

- general column-level `CHARACTER SET`, `CHARSET`, or `COLLATE` attributes;
- `N'...'` or `n'...'` national string literal semantics beyond existing lexer
  classification and currently unsupported literal positions;
- utf8mb3 text validation beyond MyLite's current UTF-8 non-`NUL` validation;
- conversion between character sets, introducers, repertoire checks, or
  collation coercibility;
- general utf8mb3/utf8mb4 collation comparison, ordering, grouping, distinct,
  functions, casts, or expression evaluation;
- `NVARCHAR` lengths above the current MyLite `VARCHAR(0..16383)` descriptor
  envelope;
- new catalog table columns, public API, storage file format, or SQLite fork
  patches.

## Ownership Boundary

- Public API behavior is unchanged. `mylite_execute()` owns call validation,
  diagnostics, result ownership, and cleanup.
- Statement context behavior is unchanged. National aliases affect only the
  planned descriptor and warnings for supported DDL.
- Lexer/parser/AST own syntax admission and carry a national-alias marker on
  existing `CHAR` / `VARCHAR` AST node kinds.
- Analyzer/planner code maps marked AST nodes to national logical descriptors
  while retaining SQLite `TEXT` physical storage.
- Catalog descriptors remain the authority. MyLite persists `NCHAR(n)` or
  `NVARCHAR(n)` in the existing logical-type field; SQLite schema text is not
  used for compatibility decisions.
- Result and introspection builders interpret national logical descriptors to
  render MySQL-shaped `utf8mb3` metadata.
- SQLite owns physical row storage through ordinary `TEXT` tables and indexes.
  MyLite continues to generate physical SQL from descriptors, quote generated
  identifiers, and bind values through prepared statements.
- Storage/VFS behavior is unchanged. National alias DDL writes descriptor rows
  and generated SQLite objects inside the shifted SQLite payload without
  changing the `.mylite` preamble or file format.

## Supported SQL Grammar

The feature extends the existing limited column-type grammar:

```sql
char_type:
    CHAR
  | CHAR ( unsigned_decimal_integer_literal )
  | CHARACTER
  | CHARACTER ( unsigned_decimal_integer_literal )
  | NCHAR
  | NCHAR ( unsigned_decimal_integer_literal )
  | NATIONAL CHAR
  | NATIONAL CHAR ( unsigned_decimal_integer_literal )
  | NATIONAL CHARACTER
  | NATIONAL CHARACTER ( unsigned_decimal_integer_literal )

varchar_type:
    VARCHAR ( unsigned_decimal_integer_literal )
  | CHARACTER VARYING ( unsigned_decimal_integer_literal )
  | CHAR VARYING ( unsigned_decimal_integer_literal )
  | NVARCHAR ( unsigned_decimal_integer_literal )
  | NATIONAL VARCHAR ( unsigned_decimal_integer_literal )
  | NCHAR VARCHAR ( unsigned_decimal_integer_literal )
  | NCHAR VARYING ( unsigned_decimal_integer_literal )
  | NATIONAL CHAR VARYING ( unsigned_decimal_integer_literal )
  | NATIONAL CHARACTER VARYING ( unsigned_decimal_integer_literal )
```

`unsigned_decimal_integer_literal` is the existing MyLite integer token used for
type length parsing. Signs, expressions, parameters, empty parentheses, and
non-decimal forms are not admitted.

The aliases do not add row-value grammar. DML values are the existing string,
`NULL`, and `DEFAULT` subset for `CHAR` and `VARCHAR`.

## Semantics

### Logical Descriptors

National `CHAR` aliases map to `NCHAR(n)` descriptors:

- bare `NCHAR`, `NATIONAL CHAR`, and `NATIONAL CHARACTER` have length `1`;
- explicit lengths keep the parsed `0..255` value;
- the physical SQLite column type remains `TEXT`.

National `VARCHAR` aliases map to `NVARCHAR(n)` descriptors:

- a length is mandatory;
- admitted lengths are `0..16383`, matching the current MyLite `VARCHAR`
  envelope;
- the physical SQLite column type remains `TEXT`.

### Metadata

National descriptors render as MySQL's expanded type:

- `SHOW CREATE TABLE` prints `char(n) CHARACTER SET utf8mb3 COLLATE
  utf8mb3_general_ci` or `varchar(n) CHARACTER SET utf8mb3 COLLATE
  utf8mb3_general_ci`;
- `SHOW COLUMNS` prints `char(n)` or `varchar(n)`;
- `SHOW FULL COLUMNS` prints `utf8mb3_general_ci`;
- `INFORMATION_SCHEMA.COLUMNS` reports `DATA_TYPE` as `char` / `varchar`,
  `COLUMN_TYPE` as `char(n)` / `varchar(n)`, `CHARACTER_SET_NAME` as
  `utf8mb3`, `COLLATION_NAME` as `utf8mb3_general_ci`, and octet length as
  `n * 3`;
- public result metadata for descriptor-backed result columns uses the
  `utf8mb3_general_ci` collation id for national descriptors and a display
  length of `n * 3`.

### Defaults, Nullability, and DML

National aliases use the existing normalized target rules:

- nullable columns with no explicit default report `DEFAULT NULL`;
- string defaults are decoded and validated through the existing string
  conversion path;
- `NULL` into `NOT NULL`, overlength values, embedded `NUL`, `INSERT IGNORE`
  adjustment, and duplicate-key behavior reuse current `CHAR` / `VARCHAR`
  diagnostics and warnings;
- `NCHAR` values use default-mode `CHAR` canonicalization;
- `NVARCHAR` values preserve trailing spaces like `VARCHAR`.

### Warnings

Each successfully planned explicit national column declaration appends:

- level: `Warning`
- code: `3720`
- SQLSTATE: `HY000`
- message: `NATIONAL/NCHAR/NVARCHAR implies the character set UTF8MB3, which
  will be replaced by UTF8MB4 in a future release. Please consider using
  CHAR(x) CHARACTER SET UTF8MB4 in order to be unambiguous.`

`CREATE TABLE ... LIKE` copies descriptors and does not re-append this warning.

For `ALTER TABLE ... MODIFY COLUMN` and `ALTER TABLE ... CHANGE COLUMN`,
MyLite keeps the current limited replacement path's public result convention:
metadata-only replacements report zero affected rows, while descriptor-backed
physical rebuilds report the copied/validated row count. The MySQL expectation
script still records MySQL's `ROW_COUNT()` behavior so this MyLite public result
choice remains explicit rather than accidental.

### Diagnostics

The feature uses existing diagnostics where possible:

- missing `VARCHAR` lengths, empty parentheses, signed lengths, unsupported
  attributes, or malformed syntax produce the current parse diagnostic;
- `NCHAR(256)` uses the existing `CHAR` length diagnostic;
- `NVARCHAR(16384)` and larger use MyLite's current `VARCHAR` length diagnostic
  and remain a documented envelope gap versus MySQL;
- invalid string assignment, `NULL` into `NOT NULL`, duplicate keys, unknown
  tables/columns, physical SQLite failures, allocation failures, and public API
  misuse use existing descriptor-driven diagnostics.

## Performance and SQLite

This feature stays on the existing fast path:

- no SQLite fork patch is needed;
- row storage remains SQLite `TEXT`;
- `SELECT`, `INSERT`, `REPLACE`, `UPDATE`, and index maintenance use the same
  descriptor-driven physical SQL and prepared statements as `CHAR` / `VARCHAR`;
- no query results are materialized beyond existing result-building behavior
  for the statement being executed;
- the only extra runtime branching is descriptor-prefix recognition for
  metadata, length, and warning behavior.

## Tests

Add focused coverage, preferably by extending
`packages/libmylite/tests/runtime_character_alias_lifecycle_test.c`, for:

- all admitted national alias spellings in `CREATE TABLE`;
- one warning per explicit national declaration and no warning for
  `CREATE TABLE ... LIKE`;
- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` utf8mb3 metadata;
- public result metadata for descriptor-backed national columns, including the
  MySQL 8.4.9 mysqli behavior where selected `NCHAR` / `NVARCHAR` columns use
  the session result collation and byte width even though stored descriptor
  metadata remains `utf8mb3`;
- string defaults, nullable/not-null behavior, `INSERT`, `UPDATE`, readback,
  affected rows, and warning counts;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN` national
  declarations;
- clone/persistence and file preamble preservation;
- deterministic rejection of omitted/empty/signed lengths, `NCHAR(256)`,
  `NVARCHAR(16384)`, and unsupported character-set/collation attributes;
- existing parser, runtime, metadata, storage, and workflow tests.

Run:

1. `packages/libmylite/tests/mysql_baseline_national_character_aliases_expectations.sh`
2. focused parser/runtime CTests for character aliases
3. `cmake --workflow --preset check`
