# Baseline String Defaults

## Status

This feature specifies explicit literal defaults for MyLite's existing
descriptor-owned `CHAR` and `VARCHAR` string columns. It builds on the current
string storage, DML `DEFAULT` keyword, `ALTER TABLE ... ALTER COLUMN
SET/DROP DEFAULT`, `CREATE TABLE ... LIKE`, and introspection paths.

This is not a full default-expression or collation feature. It admits ordinary
literal defaults for `CHAR` and `VARCHAR`. Nonempty `TEXT` family literal
defaults remain rejected in this slice; the separate WordPress indexes-bucket
bridge accepts only empty ordinary `TEXT` family defaults and records a MySQL
`1101` warning.

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
- Baseline `VARCHAR` type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline `CHAR` type:
  `docs/specs/baseline-char-type/specs.md`
- Baseline `TEXT` type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline DML default keyword values:
  `docs/specs/baseline-dml-default-keyword-values/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CHAR` and `VARCHAR`:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, string type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, `BLOB` and `TEXT`:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_string_defaults_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- `CHAR` and `VARCHAR` columns can have literal string defaults, including the
  empty string.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` report the
  decoded default text, not the quoted SQL literal.
- `SHOW CREATE TABLE` renders explicit string defaults as quoted literals.
- `CHAR` default values are normalized through the same default-mode `CHAR`
  storage rules as DML values. A trailing-space default that fits after `CHAR`
  trimming is accepted and renders without the trailing spaces.
- `VARCHAR` default values preserve trailing spaces when they fit. Excess
  trailing-space defaults are accepted by MySQL with note `1265` and truncated.
  MyLite continues to reject overlength values rather than adding
  warning-producing truncation in this slice.
- Nonspace-overlength string defaults fail with error `1067 / 42000`.
- `CHAR(0)` and `VARCHAR(0)` accept only the empty string default. A nonempty
  default fails with error `1067 / 42000`.
- `DEFAULT NULL` on a `NOT NULL` string column fails with error
  `1067 / 42000`.
- Omitted-column `INSERT`, explicit `DEFAULT` in `INSERT`, `REPLACE`, and
  single-assignment `UPDATE`, and `ALTER TABLE ... ADD COLUMN ... DEFAULT`
  materialize the descriptor default.
- `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT string_literal` updates only
  catalog metadata and affects later default materialization.
- `ALTER TABLE ... ALTER COLUMN ... DROP DEFAULT` removes the explicit default.
  A later strict omitted-column insert into a nullable column with no explicit
  default fails like MySQL for the admitted baseline.
- `CREATE TABLE ... LIKE` preserves literal string defaults.
- `CREATE TABLE ... SELECT` preserves selected source column defaults in the
  current descriptor-inference shape.
- `TEXT DEFAULT 'abc'` is rejected by MySQL as a literal default; expression
  defaults such as `TEXT DEFAULT ('abc')` are accepted upstream. MyLite's
  WordPress bridge additionally accepts only `TEXT DEFAULT ''`, warns, and
  suppresses the visible metadata default.

## Scope

The implementation must add:

- `CREATE TABLE` support for explicit literal defaults on existing `CHAR` and
  `VARCHAR` descriptor columns;
- `ALTER TABLE ... ADD [COLUMN]` support for explicit literal defaults on
  existing `CHAR` and `VARCHAR` descriptor columns, including existing-row
  backfill through generated SQLite DDL;
- `ALTER TABLE ... ALTER [COLUMN] column SET DEFAULT string_literal` support
  for existing `CHAR` and `VARCHAR` descriptor columns;
- unchanged `ALTER TABLE ... ALTER [COLUMN] column DROP DEFAULT` behavior,
  verified for string columns;
- descriptor materialization of string defaults through omitted-column
  `INSERT`, explicit `DEFAULT` in admitted `INSERT`, `REPLACE`, and
  single-assignment `UPDATE` forms;
- descriptor cloning/copying of `CHAR` and `VARCHAR` string defaults through
  `CREATE TABLE ... LIKE` and compatible `CREATE TABLE ... SELECT`;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and
  limited `INFORMATION_SCHEMA.COLUMNS` rendering for explicit `CHAR` and
  `VARCHAR` defaults;
- correct quoting for generated SQLite default literals and MySQL-style
  `SHOW CREATE TABLE` rendering, including single quotes and backslash-decoded
  text from admitted string literals;
- persistent storage, reopen behavior, table rename/drop behavior, `.mylite`
  preamble preservation, and independent file-backed handle behavior for
  admitted string default metadata and default-generated row values;
- MySQL 8.4.9 expectation coverage for supported behavior and deliberately
  deferred wider default-expression behavior.

## Non-Goals

This feature must not implement:

- nonempty literal defaults for `TEXT`, `TINYTEXT`, `MEDIUMTEXT`, or
  `LONGTEXT`;
- expression defaults such as `DEFAULT ('abc')`, function defaults, column
  references, `DEFAULT(col_name)`, parameters, user variables, or subqueries;
- warning-producing truncation of overlength `CHAR` or `VARCHAR` defaults;
- string-to-integer or integer-to-string default conversion;
- non-default character sets, column-level collations, collation comparison,
  collation-aware uniqueness, or string primary keys;
- `ALTER TABLE ... MODIFY [COLUMN]` or `CHANGE [COLUMN]` string type/default
  replacement beyond existing behavior;
- public API changes, catalog schema migrations, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  result ownership, public misuse behavior, and failure cleanup.
- Statement context owns diagnostics, warnings, affected rows, and transaction
  completion. Supported string-default paths report `warning_count == 0`.
  MyLite continues to reject overlength string defaults instead of emitting
  MySQL truncation notes.
- Lexer/parser/AST already admit `DEFAULT string_literal` in column
  definitions and `ALTER ... SET DEFAULT`. They store source spans and
  structural payloads only; they do not resolve descriptors or decode string
  values.
- Analyzer/planner code resolves target schemas, tables, and columns through
  MyLite catalog descriptors, converts admitted string defaults with the same
  MyLite-owned string conversion used for row values, and rejects unsupported
  defaults before SQLite SQL is generated.
- The catalog remains authoritative for logical type, physical type,
  nullability, visibility, default kind, and default text. This feature reuses
  the existing text default descriptor field and may widen the in-memory
  capacity so `CHAR(255)` and `VARCHAR(255)` defaults fit; the on-disk catalog
  column is already SQLite `TEXT`, so no catalog schema migration is required.
  Catalog validation accepts ordinary string defaults for `CHAR` and `VARCHAR`
  logical descriptors and only the documented empty-literal bridge for `TEXT`
  family descriptors; decimal and temporal text-backed defaults must stay
  nonempty.
- Result and introspection builders render defaults from MyLite descriptors.
  SQLite schema text and `sqlite_schema` are not metadata authority.
- SQLite owns physical row storage and DDL execution for generated statements.
  MyLite must quote generated SQLite identifiers, quote generated SQL string
  literals correctly, and bind row values in DML through prepared statements.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The existing parser already admits the syntax needed by this feature:

```sql
column_definition:
    column_name char_or_varchar_type [NULL | NOT NULL] DEFAULT string_literal
  | column_name char_or_varchar_type [NULL | NOT NULL] DEFAULT NULL
  | column_name char_or_varchar_type [NULL | NOT NULL]

alter_table_set_default_statement:
    ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT string_literal
  | ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT NULL

insert_value:
    DEFAULT
  | string_literal
  | NULL
  | existing_admitted_value

update_value:
    DEFAULT
  | string_literal
  | NULL
  | existing_admitted_value
```

`string_literal` is the existing ordinary MyLite string token: single-quoted or
double-quoted text with admitted MySQL-style quote and backslash escape
decoding under the fixed default SQL mode. National strings, introducers,
adjacent literal concatenation, hex strings, bit strings, parameters, and
expression defaults are not admitted.

## Default Conversion

For `CHAR(n)`:

- decode the source string literal using the existing MyLite string-literal
  decoder;
- reject embedded `NUL` bytes;
- validate UTF-8 using the current string-storage policy;
- apply the existing `CHAR` conversion, including default-mode trailing-space
  canonicalization;
- reject nonempty defaults for `CHAR(0)`;
- reject converted text that does not fit in MyLite's descriptor default-text
  capacity;
- store the converted text in the descriptor as
  `MYLITE_CATALOG_COLUMN_DEFAULT_TEXT`.

For `VARCHAR(n)`:

- decode the source string literal using the existing MyLite string-literal
  decoder;
- reject embedded `NUL` bytes;
- validate UTF-8 using the current string-storage policy;
- preserve trailing spaces when the value fits the declared character length;
- reject any decoded value whose character length exceeds `n`, including
  values that MySQL would truncate with a note;
- reject nonempty defaults for `VARCHAR(0)`;
- reject converted text that does not fit in MyLite's descriptor default-text
  capacity;
- store the converted text in the descriptor as
  `MYLITE_CATALOG_COLUMN_DEFAULT_TEXT`.

`DEFAULT NULL` is accepted only for nullable columns. `DEFAULT NULL` on a
`NOT NULL` column fails with `1067 / 42000`.

Nonempty `TEXT` family literal defaults stay rejected with the existing
deterministic invalid-default diagnostic. Empty ordinary text defaults are
accepted only by the documented WordPress bridge.

## Physical SQLite Handling

Generated SQLite physical tables remain descriptor-driven:

```sql
CREATE TABLE "_mylite_user_table_<id>" (
    "column_name" TEXT [NOT NULL],
    ...
);
```

Physical `CREATE TABLE` intentionally omits string default clauses. The MyLite
catalog descriptor remains authoritative, and DML default materialization binds
descriptor-owned values explicitly. `ALTER TABLE ... ADD COLUMN` emits a
physical `DEFAULT` for this slice because SQLite uses that clause to backfill
existing rows and enforce `NOT NULL` added columns:

```sql
ALTER TABLE "_mylite_user_table_<id>"
ADD COLUMN "column_name" TEXT [NOT NULL] DEFAULT '<escaped default>';
```

Every generated identifier is quoted. Generated SQLite string literals use a
MyLite helper that doubles embedded single quotes and preserves decoded
backslash bytes as ordinary text. `SHOW CREATE TABLE` uses MySQL-style
quoted rendering for descriptor defaults.
DML row values continue to use prepared statements and bound text values rather
than interpolated literals.

## Result And Metadata Behavior

Successful string-default DDL returns through the existing public result API
for non-row statements with no result rows, `affected_rows == 0`, and
`warning_count == 0` for the admitted subset.

`SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` report the decoded descriptor
default text. Empty string defaults render as empty text, distinct from SQL
`NULL`.

`SHOW CREATE TABLE` renders explicit string defaults as quoted SQL literals:
single quotes inside default text are doubled, and other stored bytes are
rendered through the current admitted text policy.

`CREATE TABLE ... LIKE` copies explicit string defaults. Compatible
`CREATE TABLE ... SELECT` preserves selected source descriptor defaults in the
current MyLite descriptor-inference shape.

## Diagnostics

The supported diagnostics for this slice are:

- syntax errors: existing parser syntax diagnostics;
- unsupported expression defaults: existing deterministic parser/runtime
  diagnostics;
- missing default schema, unknown schema, unknown table, reserved names, and
  unknown columns: existing catalog diagnostics;
- `DEFAULT NULL` on `NOT NULL`: `1067 / 42000`;
- non-string default on `CHAR` or `VARCHAR`: `1067 / 42000`;
- overlength `CHAR` or `VARCHAR` default: `1067 / 42000`;
- nonempty `CHAR(0)` or `VARCHAR(0)` default: `1067 / 42000`;
- embedded `NUL` or invalid UTF-8 default text: deterministic MyLite
  diagnostics matching current string row-value policy;
- nonempty `TEXT` family literal default: existing deterministic
  invalid-default diagnostic;
- physical SQLite failures and allocation failures: existing internal
  diagnostics, with cleanup leaving catalog and physical storage unchanged.

## Performance

Default conversion is performed once during DDL planning or catalog-only
`ALTER ... SET DEFAULT`. Row DML default materialization copies the descriptor
text into the existing planned value path and still executes physical row
mutation through generated SQLite prepared statements. This slice does not add
table-wide scans except existing `ALTER TABLE ... ADD COLUMN` physical work
required to backfill a new column.

## Test Plan

Add MySQL-runtime-verified expectation coverage for:

- `CREATE TABLE` with `CHAR`, `CHAR(0)`, `VARCHAR`, and `VARCHAR(0)` literal
  defaults;
- empty defaults, quotes, decoded backslash escapes, `CHAR` trailing-space
  normalization, and `VARCHAR` trailing-space preservation;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`
  default metadata;
- omitted-column `INSERT`, explicit DML `DEFAULT`, `REPLACE`, and
  single-assignment `UPDATE`;
- `ALTER TABLE ... ADD COLUMN ... DEFAULT`, `ALTER ... SET DEFAULT`, and
  `ALTER ... DROP DEFAULT`;
- `CREATE TABLE ... LIKE` and compatible `CREATE TABLE ... SELECT`;
- reopen persistence, table rename/drop, independent handles, and `.mylite`
  preamble preservation;
- invalid defaults: `DEFAULT NULL` on `NOT NULL`, non-string values,
  nonspace-overlength strings, nonempty zero-length columns, embedded `NUL`,
  invalid UTF-8 where constructible, nonempty `TEXT DEFAULT 'x'`, expression
  defaults, functions, parameters, and `DEFAULT(col_name)`;
- existing `CHAR`, `VARCHAR`, `TEXT`, parser, DML default, `SHOW`, catalog,
  file-backed, and compatibility lifecycle tests still pass.
