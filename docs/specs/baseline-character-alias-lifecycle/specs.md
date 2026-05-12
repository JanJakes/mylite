# Baseline CHARACTER Alias Lifecycle

## Status

This feature specifies a narrow string-type alias slice. It admits MySQL's
`CHARACTER` spelling for the existing `CHAR` descriptor path and
`CHARACTER VARYING` / `CHAR VARYING` spellings for the existing `VARCHAR`
descriptor path.

The feature intentionally does not add new storage semantics. Parsed aliases
normalize immediately to the current MyLite `CHAR(n)` and `VARCHAR(n)`
descriptors, so existing row conversion, defaults, DML, `SHOW`, and limited
`INFORMATION_SCHEMA` behavior remain authoritative.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline `CHAR` type:
  `docs/specs/baseline-char-type/specs.md`
- Baseline `VARCHAR` type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- Baseline table charset and collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, string data type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, national character set:
  https://dev.mysql.com/doc/refman/8.4/en/charset-national.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_character_alias_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- `CHARACTER`, `CHARACTER(0)`, and `CHARACTER(n)` are accepted and rendered as
  `char(1)`, `char(0)`, and `char(n)` respectively.
- `CHARACTER VARYING(n)` and `CHAR VARYING(n)` are accepted and rendered as
  `varchar(n)`.
- `SHOW CREATE TABLE` and `INFORMATION_SCHEMA.COLUMNS` report the normalized
  `char` / `varchar` descriptor shape, not the alias spelling used in the input
  DDL.
- Existing `CHAR` trimming and `VARCHAR` trailing-space retention behavior apply
  to the aliases.
- Defaults, `NULL` / `NOT NULL`, and ordinary DML string assignments behave as
  they do for the normalized target type.
- `CHARACTER VARYING` and `CHAR VARYING` without a length are syntax errors.
  Empty and negative length forms are syntax errors.
- `CHARACTER(256)` fails because `CHAR` length is capped at `255` in MySQL.
  `CHARACTER VARYING(256)` is accepted by MySQL, but remains deferred in MyLite
  because the current `VARCHAR` descriptor slice admits only lengths `0..255`.
- `CHARACTER BYTE`, `CHAR BYTE`, column `CHARACTER SET`, `BINARY` attributes,
  and national-character aliases are separate MySQL surfaces and remain
  deferred.

## Scope

The implementation must add:

- parser support for `CHARACTER` and `CHARACTER(length)` as aliases for the
  existing `CHAR` AST node and descriptor path;
- parser support for `CHARACTER VARYING(length)` and `CHAR VARYING(length)` as
  aliases for the existing `VARCHAR` AST node and descriptor path;
- the same admitted length envelope as the normalized descriptor paths:
  `0..255`;
- use of alias forms anywhere the current grammar accepts `CHAR` or `VARCHAR`
  column types, including `CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]`;
- normalized descriptor storage, metadata, readback, defaults, DML assignment,
  key participation, clone/copy behavior, and diagnostics through the existing
  `CHAR` / `VARCHAR` code paths;
- parser tests for alias AST shape and spans;
- fast runtime tests for `CREATE TABLE`, `ALTER TABLE ... ADD`, metadata,
  defaults, DML, persistence, and deterministic unsupported forms;
- MySQL 8.4.9 expectation coverage for accepted aliases, normalized metadata,
  DML behavior, and deferred syntax.

## Non-Goals

This feature must not implement:

- `NCHAR`, `NATIONAL CHAR`, `NVARCHAR`, `NATIONAL VARCHAR`,
  `NATIONAL CHARACTER`, `NATIONAL CHARACTER VARYING`, or other national
  character aliases;
- `CHAR BYTE`, `CHARACTER BYTE`, `BINARY`, `VARBINARY`, `BLOB`, `ENUM`, `SET`,
  `JSON`, or other string/binary-family types;
- column-level `CHARACTER SET`, `CHARSET`, `COLLATE`, `BINARY`, `ASCII`,
  `UNICODE`, or binary-collation attributes;
- `VARCHAR` / `CHARACTER VARYING` lengths above `255`;
- new collation comparison, string ordering, grouping, distinct, functions,
  casts, parameters, or expression evaluation;
- new public API, catalog columns, physical storage formats, indexes, or SQLite
  fork patches.

## Ownership Boundary

- Public API behavior is unchanged. `mylite_execute()` continues to own call
  validation, result ownership, diagnostics exposure, and cleanup.
- Statement context behavior is unchanged. Supported in-range alias operations
  use the existing `CHAR` / `VARCHAR` statement results and warnings.
- Lexer/parser/AST own alias syntax admission. They normalize aliases to the
  existing AST node kinds so later layers do not need alias-specific branching.
- Analyzer/planner code remains descriptor-driven. Alias AST nodes are mapped
  by the existing `CHAR` / `VARCHAR` descriptor builders and validators.
- Catalog descriptors remain authoritative and store normalized logical type
  text such as `CHAR(2)` or `VARCHAR(3)`. Alias spelling is not persisted.
- Result and introspection builders render normalized descriptor metadata. They
  do not inspect SQLite schema text for type names.
- SQLite owns physical `TEXT` row storage for normalized string descriptors.
  MyLite continues to bind values through prepared statements and does not
  add a SQLite fork hook for aliases.
- Storage/VFS behavior is unchanged. Alias DDL writes only descriptor rows and
  generated SQLite objects inside the shifted SQLite payload.

## Supported SQL Grammar

The feature extends the existing limited column-type grammar:

```sql
column_type:
    existing_integer_or_decimal_or_temporal_or_text_type
  | char_type
  | varchar_type

char_type:
    CHAR
  | CHAR ( unsigned_decimal_integer_literal )
  | CHARACTER
  | CHARACTER ( unsigned_decimal_integer_literal )

varchar_type:
    VARCHAR ( unsigned_decimal_integer_literal )
  | CHARACTER VARYING ( unsigned_decimal_integer_literal )
  | CHAR VARYING ( unsigned_decimal_integer_literal )
```

`unsigned_decimal_integer_literal` is an existing MyLite integer token used only
for type length parsing. Signs, expressions, parameters, empty parentheses, and
non-decimal forms are not admitted.

The aliases do not add value grammar. All row DML values are the existing
`CHAR` / `VARCHAR` string, `NULL`, and `DEFAULT` subset.

## Semantics

### Normalization

`CHARACTER` and `CHARACTER(n)` normalize to the existing `CHAR` descriptor:

- bare `CHARACTER` has effective length `1`;
- explicit `CHARACTER(n)` keeps the parsed length when `n` is in `0..255`;
- metadata renders `char(n)`.

`CHARACTER VARYING(n)` and `CHAR VARYING(n)` normalize to the existing
`VARCHAR` descriptor:

- a length is mandatory;
- admitted lengths are `0..255`;
- metadata renders `varchar(n)`.

The original alias spelling is not stored in the catalog and is not returned by
`SHOW CREATE TABLE`, `SHOW COLUMNS`, or `INFORMATION_SCHEMA.COLUMNS`.

### Defaults and Nullability

Alias columns use the existing normalized target rules:

- nullable columns with no explicit default report `DEFAULT NULL`;
- `NOT NULL` columns with no explicit default use the existing no-default
  behavior for `CHAR` / `VARCHAR`;
- string defaults are decoded and validated through the normalized target
  descriptor;
- `NULL` into `NOT NULL` fails or is adjusted by existing `INSERT IGNORE`
  behavior exactly as the normalized target type does.

### Row Values and DML

All DML behavior is inherited from the normalized target descriptor:

- `CHARACTER` values use default-mode `CHAR` canonicalization, including
  trailing-space trimming and silent excess-trailing-space truncation;
- `CHARACTER VARYING` and `CHAR VARYING` values preserve trailing spaces like
  `VARCHAR`;
- `INSERT`, `REPLACE`, `UPDATE`, `SELECT`, `WHERE IS NULL`, `WHERE IS NOT NULL`,
  clone/copy, keys, and persistence reuse existing descriptor-driven behavior.

### Diagnostics

The feature uses existing diagnostics where possible:

- missing `VARYING` length, empty parentheses, signed lengths, unsupported
  alias attributes, or malformed syntax produce the current parse diagnostic;
- `CHARACTER(256)` and `CHARACTER VARYING(256)` use existing MyLite length
  diagnostics for `CHAR` and `VARCHAR`;
- invalid string assignment, embedded `NUL`, overlength values, `NULL` into
  `NOT NULL`, duplicate keys, unknown tables/columns, physical SQLite failures,
  allocation failures, and public API misuse use existing normalized target
  diagnostics.

## Tests

Add a focused runtime test, preferably
`packages/libmylite/tests/runtime_character_alias_lifecycle_test.c`, covering:

- `CREATE TABLE` with `CHARACTER`, `CHARACTER(0)`, `CHARACTER(n)`,
  `CHARACTER VARYING(n)`, and `CHAR VARYING(n)`;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  normalized metadata;
- string defaults, `NULL` / `NOT NULL`, `INSERT`, `UPDATE`, readback,
  affected rows, and warning counts through the normalized target paths;
- `ALTER TABLE ... ADD [COLUMN]` with admitted aliases;
- `CREATE TABLE ... LIKE` and reopen persistence;
- deterministic rejection of omitted/empty/signed alias lengths, lengths above
  the current MyLite envelope, `CHARACTER BYTE`, `CHAR BYTE`,
  `CHARACTER SET`, national aliases, and binary/collation attributes;
- no `.mylite` preamble mutation.

Run the MySQL expectation script, targeted parser/runtime CTests, and
`cmake --workflow --preset check` before marking the feature complete.
