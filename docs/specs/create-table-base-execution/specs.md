# CREATE TABLE base execution

## Scope

This feature moves MyLite's supported `CREATE TABLE` table-definition subset
from parse-only `MYLITE_UNSUPPORTED` behavior to executable DDL. It builds on
the existing schema lifecycle, core metadata catalog, column type, column
attribute, primary-key, and create-table index specs.

In scope:

- `CREATE TABLE [IF NOT EXISTS] table_name (...)`
- schema-qualified and default-schema table creation
- actual SQLite user-table creation inside the single `.mylite` file
- internal rows for `__mylite_table_catalog`, `__mylite_column_catalog`, and
  `__mylite_index_catalog`
- `INFORMATION_SCHEMA.TABLES`, `COLUMNS`, and `STATISTICS` reflection through
  the existing metadata views
- table options `ENGINE [=] InnoDB`, `[DEFAULT] CHARACTER SET [=] charset`,
  `[DEFAULT] CHARSET [=] charset`, `[DEFAULT] COLLATE [=] collation`,
  `COMMENT [=] 'string'`, and `AUTO_INCREMENT [=] integer`
- inline `PRIMARY KEY`, inline `KEY`, inline `UNIQUE`, inline `UNIQUE KEY`,
  table-level primary keys, table-level unique indexes, and table-level
  secondary indexes over identifier key parts
- critical deterministic validation for missing schemas, system schemas,
  duplicate tables, empty table bodies, duplicate column names, duplicate
  primary keys, duplicate explicit index names, and unknown indexed columns
- atomic DDL side effects
- MySQL-compatible implicit commit before non-temporary `CREATE TABLE` inside
  an explicit transaction

Out of scope:

- row insertion, default expression evaluation, auto-increment allocation, and
  write-time constraint enforcement
- standalone `CREATE INDEX`
- broad `CREATE TABLE ... SELECT` definition merging; the first no-definition
  CTAS slice is specified separately in
  [CREATE TABLE ... SELECT](../create-table-select/specs.md)
- temporary-table transaction exceptions
- full engine-specific validation, prefix-length suitability, functional key
  parts, generated-column runtime evaluation, executable references,
  executable checks, partitions, and unsupported table options
- warning records for `IF NOT EXISTS` and storage-engine normalization
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW INDEX`, and result-set protocol
  metadata beyond the current public API

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html
- Existing MyLite specs:
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/schema-lifecycle/specs.md`
  - `docs/specs/integer-boolean-column-types/specs.md`
  - `docs/specs/string-binary-column-types/specs.md`
  - `docs/specs/numeric-column-types/specs.md`
  - `docs/specs/temporal-column-types/specs.md`
  - `docs/specs/column-attributes/specs.md`
  - `docs/specs/primary-keys-auto-increment/specs.md`
  - `docs/specs/create-table-indexes/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, including the independent Task 11 probe set supplied by
  Jan.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

### Schema resolution and table existence

- `CREATE DATABASE d` does not select `d`.
- Unqualified `CREATE TABLE t (a INT)` fails with error 1046 when no default
  schema is selected.
- Qualified `CREATE TABLE d.t (a INT)` succeeds when `d` exists, even without a
  selected default schema.
- Qualified creation in a missing schema fails with error 1049.
- Creating in `information_schema` returns MySQL-compatible access denied.
  MyLite does not implement privileges, but system schemas must remain not
  writable.
- Duplicate table creation fails with error 1050.
- Duplicate column, key-name, and primary-key declarations fail with errors
  1060, 1061, and 1068 respectively.
- `CREATE TABLE IF NOT EXISTS existing_table (...)` succeeds as a no-op,
  records note 1050, and must not mutate existing metadata or storage.

### Metadata for the supported subset

For a representative table:

```sql
CREATE TABLE simple_create (
  id INT NOT NULL,
  name VARCHAR(20) DEFAULT 'x' COMMENT 'name col',
  created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  amount DECIMAL(10,2),
  flag BOOL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_name (name),
  KEY amount_idx (amount)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
  COMMENT='hello table' AUTO_INCREMENT=10
```

MySQL reports `INFORMATION_SCHEMA.TABLES` with catalog `def`, type
`BASE TABLE`, engine `InnoDB`, version `10`, zero estimated rows, initial
`AUTO_INCREMENT=10`, table collation `utf8mb4_bin`, empty create options, and
the authored table comment.

`INFORMATION_SCHEMA.COLUMNS` reports:

- primary-key column `id` as non-nullable with `COLUMN_KEY='PRI'`
- `name` as `varchar`, length `20`, octet length `80` under `utf8mb4`,
  charset/collation `utf8mb4`/`utf8mb4_bin`, default `x`, key marker `UNI`,
  and comment `name col`
- `TIMESTAMP DEFAULT CURRENT_TIMESTAMP` with default text `CURRENT_TIMESTAMP`,
  datetime precision `0`, and `EXTRA='DEFAULT_GENERATED'`
- `TIMESTAMP DEFAULT NOW()` with default text `CURRENT_TIMESTAMP`, while
  parenthesized `DEFAULT (NOW())` records default text `now()`, matching
  MySQL's `SHOW CREATE TABLE` normalization
- fractional `NOW(fsp)` defaults record `CURRENT_TIMESTAMP(fsp)` for bare
  defaults and `now(fsp)` for parenthesized defaults
- `DECIMAL(10,2)` with numeric precision `10` and scale `2`
- `BOOL` as `tinyint(1)`

`INFORMATION_SCHEMA.STATISTICS` reports primary, unique, and secondary index
parts. Primary-key rows have `INDEX_NAME='PRIMARY'` and `NON_UNIQUE=0`. Unique
rows have `NON_UNIQUE=0`. Secondary rows have `NON_UNIQUE=1`. Normal InnoDB
indexes report `INDEX_TYPE='BTREE'` and `IS_VISIBLE='YES'`.

### Table options

- `ENGINE=InnoDB` is accepted and reported as `InnoDB`.
- `DEFAULT CHARACTER SET = latin1 COLLATE = latin1_swedish_ci COMMENT =
  'comment' AUTO_INCREMENT = 42` is accepted. Metadata reports the selected
  collation, initial auto-increment value, and table comment.
- `DEFAULT CHARSET utf8mb4` is accepted.
- Charset/collation mismatches fail, such as `DEFAULT CHARSET latin1 COLLATE
  utf8mb4_bin`.
- Unknown charsets fail.
- Unsupported engines parse as storage-engine names and fail during DDL
  validation for this MyLite feature.

### Validation boundaries

- `CREATE TABLE d.empty_columns ()` is a syntax error.
- Column names are duplicate-checked case-insensitively.
- Explicit index names are duplicate-checked case-insensitively.
- Duplicate primary-key declarations fail.
- Inline unique indexes normalize into index metadata named from the column.
  If that name collides with an explicit index name, MySQL reports duplicate
  key name.
- Multiple unnamed indexes on the same first column are allowed in MySQL and
  auto-named as `name`, `name_2`, and so on. MyLite supports deterministic
  `name`, `name_2`, ... auto-naming for unnamed inline and table-level unique
  and secondary indexes in this feature.

## MyLite behavior

### Parser and AST

MyLite extends the existing `CREATE TABLE` AST without changing the first two
children used by earlier parser tests:

1. table name
2. table element list
3. optional `IF NOT EXISTS`
4. table option list

The table option list is present even when empty. Each table option stores its
option kind and a single child value when the option has one.

Supported syntax:

```sql
CREATE TABLE [IF NOT EXISTS] table_name (
    table_element [, table_element ...]
) table_option_list

table_option ::= ENGINE [=] identifier
table_option ::= [DEFAULT] CHARACTER SET [=] table_option_value
table_option ::= [DEFAULT] CHARSET [=] table_option_value
table_option ::= [DEFAULT] COLLATE [=] table_option_value
table_option ::= COMMENT [=] string_literal
table_option ::= AUTO_INCREMENT [=] integer_literal
```

`table_option_value` accepts unquoted identifiers, quoted strings, `binary`,
and `DEFAULT`. `DEFAULT` means the target schema default for charset and the
effective table charset default collation for collation.

Malformed option shapes remain parse errors. Unsupported table options remain
parse errors until they are specified.

### Runtime execution

`CREATE TABLE` prepares to a custom statement handle and performs side effects
on the first `mylite_step()`.

Schema resolution:

- `db.table` targets `db`.
- Unqualified names target the selected default schema.
- A missing selected schema fails with `MYLITE_EXEC_ERROR` and a
  "No database selected" diagnostic.
- A missing target schema fails with `MYLITE_EXEC_ERROR`.
- System schemas are rejected as not writable.

Existence:

- Existing target table plus no `IF NOT EXISTS` fails.
- Existing target table plus `IF NOT EXISTS` returns success with note 1050 and
  no metadata or storage mutation.

Storage:

- The user table is created in SQLite under an internal physical name derived
  from a hex encoding of schema and table names:
  `__mylite_user_<schema_hex>__<table_hex>`.
- The strategy cannot collide with MyLite catalog tables and preserves
  case-sensitive, byte-preserving MySQL schema/table names.
- The SQLite table contains one column per supported MySQL column. SQLite type
  affinities are selected from the current MyLite type descriptor domain:
  integer/boolean as `INTEGER`, approximate numeric as `REAL`, exact numeric as
  `NUMERIC`, binary/blob as `BLOB`, and character/temporal as `TEXT`.
- For non-temporary tables, `CREATE TABLE` commits an active explicit
  transaction before validation and execution. This matches MySQL's
  implicit-commit boundary: later `ROLLBACK` does not undo earlier work, the
  created table, or later autocommit writes.
- Physical SQLite defaults, constraints, and indexes are deferred until write
  execution semantics can enforce them with MySQL-compatible conversions and
  diagnostics. Metadata rows still reflect the supported definitions.

Metadata:

- `__mylite_table_catalog` receives one `BASE TABLE` row with catalog `def`,
  engine `InnoDB`, version `10`, row estimate `0`, effective table collation,
  comment, and initial `AUTO_INCREMENT` when supplied.
- `__mylite_column_catalog` receives one row per column with ordinal position,
  default text, nullability, type metadata from the current descriptor,
  `COLUMN_KEY`, `EXTRA`, comment, and empty generation expression.
- Primary-key key parts mark their columns non-nullable in metadata, matching
  MySQL's implicit `NOT NULL` normalization for accepted primary-key
  definitions.
- `__mylite_index_catalog` receives one row per supported primary, unique, and
  secondary index key part. Inline primary and unique attributes are normalized
  into index rows.

Validation:

- Empty table element lists are syntax errors through grammar.
- Duplicate column names are detected case-insensitively.
- Duplicate primary-key declarations fail.
- Explicit duplicate index names are detected case-insensitively.
- Generated names for omitted index names use the first key-part column plus
  `_2`, `_3`, ... as needed to avoid collisions.
- Key parts must name existing columns.
- Full type suitability for primary keys, prefix lengths, `AUTO_INCREMENT`,
  nullable primary-key syntax conflicts, and engine-specific option effects are
  deterministic deferrals. MyLite records metadata for the parsed supported
  subset and leaves write-time enforcement to later insert/index work.

Atomicity:

- SQLite physical table creation and all catalog writes are wrapped in one
  SQLite transaction.
- If any step fails, MyLite rolls the transaction back and leaves no partial
  physical table or catalog/index rows for the attempted table.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
create_table_statement ::= CREATE TABLE opt_if_not_exists table_name LPAREN
                           table_element_list RPAREN table_option_list.

table_option_list ::= .
table_option_list ::= table_option_list table_option.

table_option ::= ENGINE opt_equal identifier.
table_option ::= opt_default CHARACTER SET opt_equal table_option_value.
table_option ::= opt_default CHARSET opt_equal table_option_value.
table_option ::= opt_default COLLATE opt_equal table_option_value.
table_option ::= COMMENT opt_equal STRING.
table_option ::= AUTO_INCREMENT opt_equal INTEGER.

table_option_value ::= identifier.
table_option_value ::= STRING.
table_option_value ::= BINARY.
table_option_value ::= DEFAULT.
```

The table element, column type, column attribute, key-part, and index-option
productions are the previously specified MyLite grammar for Tasks 4 through 10.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `CREATE TABLE no_default_table (a INT)` with no selected schema | Execution error containing `No database selected`; no metadata row. |
| `CREATE TABLE existing_schema.t (a INT)` with no selected schema | Succeeds if `existing_schema` exists. |
| `CREATE TABLE missing_schema.t (a INT)` | Execution error for unknown/missing database. |
| `CREATE TABLE information_schema.should_fail (a INT)` | Execution error; system schema remains unmodified. |
| representative `simple_create` statement above | Succeeds; `TABLES`, `COLUMNS`, and `STATISTICS` reflect table, columns, and indexes. |
| duplicate `CREATE TABLE simple_create (a INT)` | Execution error; original metadata unchanged. |
| `CREATE TABLE IF NOT EXISTS simple_create (a INT)` | Success no-op; original metadata unchanged. |
| `CREATE TABLE IF NOT EXISTS new_table (a INT)` | Creates normally. |
| valid table-option permutations with optional `=` | Parse and execute when charset/collation are supported and compatible. |
| `DEFAULT CHARSET latin1 COLLATE utf8mb4_bin` | Execution error; no partial table/catalog rows. |
| `DEFAULT CHARSET unknown_charset` | Execution error; no partial table/catalog rows. |
| duplicate columns `(a INT, A INT)` | Execution error; no partial rows. |
| duplicate explicit indexes `KEY idx (a), KEY IDX (b)` | Execution error; no partial rows. |
| duplicate primary keys | Execution error; no partial rows. |
| inline primary/unique/key attributes | Normalize to `PRIMARY`, generated unique index names, and generated secondary index names. |

## Test plan

- Parser tests:
  - `CREATE TABLE IF NOT EXISTS`
  - schema-qualified names with table options
  - option permutations with and without `=`
  - malformed option syntax
  - unsupported option boundaries
- Runtime tests:
  - successful unqualified and schema-qualified creation
  - missing selected schema, missing schema, and system schema errors
  - duplicate table and `IF NOT EXISTS` no-op behavior
  - no mutation on validation and execution failures
  - physical SQLite table existence via the expected internal name
  - `INFORMATION_SCHEMA.TABLES` table type, engine, collation, comment, and
    auto-increment values
  - `INFORMATION_SCHEMA.COLUMNS` type, nullability, default, comment, key, and
    extra values for the supported subset
  - `INFORMATION_SCHEMA.STATISTICS` rows for primary, unique, secondary,
    inline, named, unnamed, prefix, order, visibility, and comments where the
    current AST has enough information
  - atomic rollback when a later catalog/index validation fails

`CREATE TABLE ... LIKE` is implemented separately in
`docs/specs/create-table-like/specs.md`.
