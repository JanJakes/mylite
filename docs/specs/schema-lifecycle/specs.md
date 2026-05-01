# Schema lifecycle and selection

## Scope

This feature establishes MyLite's first schema catalog and session default
schema behavior for:

- `CREATE DATABASE` and `CREATE SCHEMA`
- `ALTER DATABASE` and `ALTER SCHEMA`
- `DROP DATABASE` and `DROP SCHEMA`
- `USE`
- unfiltered `SHOW DATABASES` and `SHOW SCHEMAS`

The implementation is intentionally embedded-compatible. A MyLite database file
stores multiple MySQL schema names in one SQLite payload instead of mapping each
schema to an operating-system directory. Table metadata, information schema
views, schema-qualified table execution, warning storage, and privileges are
later features. Charset/collation validation for the initial registry is
covered by the
[character set/collation foundation spec](../character-set-collation-foundation/specs.md).

## Sources

- MySQL 8.4 Reference Manual, `CREATE DATABASE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- MySQL 8.4 Reference Manual, `ALTER DATABASE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-database.html
- MySQL 8.4 Reference Manual, `DROP DATABASE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-database.html
- MySQL 8.4 Reference Manual, `SHOW DATABASES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-databases.html
- MySQL 8.4 Reference Manual, `USE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/use.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from the official documentation
and observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

- `CREATE SCHEMA` is a synonym for `CREATE DATABASE`.
- `CREATE DATABASE name` creates a schema but does not select it as the session
  default.
- `CREATE DATABASE` fails when the schema already exists unless
  `IF NOT EXISTS` is present. With `IF NOT EXISTS`, MySQL succeeds and records a
  note with code `1007`.
- `CREATE DATABASE` accepts default `CHARACTER SET`, `COLLATE`, and
  `ENCRYPTION` options. These become database defaults for later objects.
- MySQL also accepts `CHARSET` as a synonym for `CHARACTER SET`, and accepts
  quoted strings for character set and collation names.
- `ENCRYPTION` accepts a quoted `Y` or `N` value, case-insensitively, and
  rejects other values.
- `ALTER SCHEMA` is a synonym for `ALTER DATABASE`.
- `ALTER DATABASE name ...` changes schema defaults.
- `ALTER DATABASE ...` without a schema name targets the current default schema.
  It fails with `ERROR 1046 (3D000)` when no default schema is selected.
- `ALTER DATABASE` fails for a missing schema with `ERROR 3503 (42Y07)`.
- `DROP SCHEMA` is a synonym for `DROP DATABASE`.
- `DROP DATABASE name` removes the schema and its objects. It fails when the
  schema does not exist unless `IF EXISTS` is present.
- `DROP DATABASE IF EXISTS missing_name` succeeds and, in MySQL 8.4.9, leaves
  warning count at zero.
- Dropping the current default schema clears the session default schema. A
  following `SELECT DATABASE()` returns `NULL`.
- `USE name` selects an existing schema for subsequent unqualified statements.
  It fails for a missing schema with `ERROR 1049 (42000)`.
- `SHOW SCHEMAS` is a synonym for `SHOW DATABASES`.
- Unfiltered `SHOW DATABASES` returns one column named `Database`.
- `SHOW DATABASES LIKE 'pattern'` names its column as
  `Database (pattern_text)`. `LIKE` and `WHERE` filtering are out of scope for
  this feature.

## MyLite behavior

### Catalog

MyLite stores schema rows in an internal SQLite table inside the single
`.mylite` file. Each row records:

- schema name, case-sensitive and byte-preserving
- default character set text
- default collation text
- default encryption text
- read-only flag
- system-schema flag

The first catalog version seeds these system schema names:

- `information_schema`
- `mysql`
- `performance_schema`
- `sys`

These rows are listed by `SHOW DATABASES` and may be selected by `USE`, but
they cannot be created, altered, or dropped by user SQL in this feature.

### Schema names

Schema names are accepted as unquoted identifiers or backtick-quoted
identifiers. Backtick quoting preserves the identifier bytes and unescapes
double backticks. Unquoted names preserve the bytes written by the user. MyLite
does not case-fold schema names. Nonreserved schema option words such as
`ENCRYPTION` are accepted as unquoted names in unambiguous positions like
`CREATE DATABASE`, `DROP DATABASE`, and `USE`.

Quoted names containing newline bytes are rejected by `USE` because MySQL
documents that `USE` database names must be single-line. Newline rejection for
other schema lifecycle statements may be expanded when the full identifier
policy lands.

### `CREATE DATABASE` / `CREATE SCHEMA`

Supported syntax:

```sql
CREATE { DATABASE | SCHEMA } [ IF NOT EXISTS ] schema_name
    [ [DEFAULT] CHARACTER SET [=] charset_name ]
    [ [DEFAULT] CHARSET [=] charset_name ]
    [ [DEFAULT] COLLATE [=] collation_name ]
    [ [DEFAULT] ENCRYPTION [=] 'Y_or_N' ]
```

Behavior:

- Creates a schema catalog row.
- Does not change the session default schema.
- Defaults omitted options to `utf8mb4`, `utf8mb4_0900_ai_ci`, and `N`.
- Stores option text as authored after identifier/string unquoting.
- Returns a duplicate-schema execution error when the schema exists and
  `IF NOT EXISTS` is absent.
- Returns success when the schema exists and `IF NOT EXISTS` is present.

Compatibility gaps:

- No warning is exposed for duplicate `CREATE DATABASE IF NOT EXISTS` because
  the public diagnostics API does not yet expose warning records.
- Charset and collation values are validated and normalized for the initial
  `utf8mb4`, `utf8mb3`, `latin1`, and `binary` registry. Full MySQL charset
  and collation catalog coverage is a later feature.
- Encryption values are limited to quoted `Y` and `N` values.
- Privilege checks and `LOCK TABLES` interactions are not implemented.

### `ALTER DATABASE` / `ALTER SCHEMA`

Supported syntax:

```sql
ALTER { DATABASE | SCHEMA } [ schema_name ]
    alter_schema_option [ alter_schema_option ... ]

alter_schema_option ::= [DEFAULT] CHARACTER SET [=] charset_name
alter_schema_option ::= [DEFAULT] CHARSET [=] charset_name
alter_schema_option ::= [DEFAULT] COLLATE [=] collation_name
alter_schema_option ::= [DEFAULT] ENCRYPTION [=] 'Y_or_N'
alter_schema_option ::= READ ONLY [=] { DEFAULT | 0 | 1 }
```

Behavior:

- Updates the schema catalog row.
- Uses the current default schema when the statement omits `schema_name`.
- Fails if the statement omits `schema_name` and no default schema is selected.
- Fails if the target schema does not exist.
- Stores non-conflicting options in the catalog.
- `READ ONLY DEFAULT` and `READ ONLY = 0` store `0`; `READ ONLY = 1` stores `1`.
- Other `READ ONLY` numeric values fail during execution.

Compatibility gaps:

- Charset and collation side effects on existing routines do not apply because
  stored routines are not implemented.
- Full `READ ONLY` enforcement against object DDL/DML is deferred until table
  metadata and table execution exist.
- Conflicting duplicate options are not fully diagnosed yet.
- Ambiguous unquoted `ALTER DATABASE` targets that have the same text as an
  option starter, such as `encryption`, currently need backtick quoting.
- Privilege and replication/binlog behavior is not implemented.

### `DROP DATABASE` / `DROP SCHEMA`

Supported syntax:

```sql
DROP { DATABASE | SCHEMA } [ IF EXISTS ] schema_name
```

Behavior:

- Deletes the schema catalog row.
- Clears the session default schema when it names the dropped schema.
- Returns a missing-schema execution error when the schema does not exist and
  `IF EXISTS` is absent.
- Returns success when the schema does not exist and `IF EXISTS` is present.

Compatibility gaps:

- No tables or temporary tables are removed because table metadata is a later
  feature.
- Privileges are not implemented.
- Filesystem-directory behavior does not apply to MyLite's single-file model.

### `USE`

Supported syntax:

```sql
USE schema_name
```

Behavior:

- Selects an existing catalog schema as the session default.
- Leaves the selected schema unchanged when the target does not exist.
- Fails if the schema name contains a newline.
- Does not prevent future statements from referencing other schemas once
  schema-qualified object access exists.

### `SHOW DATABASES` / `SHOW SCHEMAS`

Supported syntax:

```sql
SHOW { DATABASES | SCHEMAS }
```

Behavior:

- Returns one text column named `Database`.
- Returns seeded system schemas and user-created schemas.
- Sorts by schema name using bytewise order.

Compatibility gaps:

- `LIKE` and `WHERE` filtering are deferred.
- Privilege filtering and `skip_show_database` behavior are not implemented.
- The result is backed by the MyLite catalog rather than filesystem directory
  enumeration.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
statement ::= create_schema_statement.
statement ::= alter_schema_statement.
statement ::= drop_schema_statement.
statement ::= show_schemas_statement.
statement ::= use_statement.

create_schema_statement ::= CREATE schema_keyword opt_if_not_exists identifier
                            schema_create_option_list.
schema_keyword ::= DATABASE.
schema_keyword ::= SCHEMA.
opt_if_not_exists ::= .
opt_if_not_exists ::= IF NOT EXISTS.

schema_create_option_list ::= .
schema_create_option_list ::= schema_create_option_list schema_create_option.
schema_create_option ::= opt_default CHARACTER SET opt_equal schema_option_value.
schema_create_option ::= opt_default CHARSET opt_equal schema_option_value.
schema_create_option ::= opt_default COLLATE opt_equal schema_option_value.
schema_create_option ::= opt_default ENCRYPTION opt_equal STRING.

alter_schema_statement ::= ALTER schema_keyword opt_identifier schema_alter_option_list.
opt_identifier ::= .
opt_identifier ::= identifier.
schema_alter_option_list ::= schema_alter_option.
schema_alter_option_list ::= schema_alter_option_list schema_alter_option.
schema_alter_option ::= opt_default CHARACTER SET opt_equal schema_option_value.
schema_alter_option ::= opt_default CHARSET opt_equal schema_option_value.
schema_alter_option ::= opt_default COLLATE opt_equal schema_option_value.
schema_alter_option ::= opt_default ENCRYPTION opt_equal STRING.
schema_alter_option ::= READ ONLY opt_equal read_only_value.
read_only_value ::= DEFAULT.
read_only_value ::= INTEGER.
schema_option_value ::= identifier.
schema_option_value ::= STRING.

drop_schema_statement ::= DROP schema_keyword opt_if_exists identifier.
opt_if_exists ::= .
opt_if_exists ::= IF EXISTS.

show_schemas_statement ::= SHOW show_schema_keyword.
show_schema_keyword ::= DATABASES.
show_schema_keyword ::= SCHEMAS.

use_statement ::= USE identifier.

opt_default ::= .
opt_default ::= DEFAULT.
opt_equal ::= .
opt_equal ::= EQ.
```

## Runtime and storage impact

- Opening a MyLite handle initializes the internal schema catalog if needed.
- The current default schema is session state stored on `mylite_db`.
- Schema lifecycle statements are custom MyLite statements. Their side effects
  occur during `mylite_step()`, matching the existing prepare/step API shape.
- `SHOW DATABASES` can be lowered to a read-only SQLite query over the internal
  catalog.
- Schema lifecycle statements should remain O(log n) or O(n) over the catalog
  table depending on the SQLite query plan. The schema name is the primary key.

## MySQL 8.4.9 verified expectations

The following observations were verified against `mylite-mysql-849`:

| SQL | Expected behavior |
| --- | --- |
| `CREATE DATABASE mylite_schema_lifecycle_a;` | Succeeds and does not select the schema. |
| `CREATE DATABASE mylite_schema_lifecycle_a;` again | Fails with duplicate database error `1007`. |
| `CREATE SCHEMA IF NOT EXISTS mylite_schema_lifecycle_a; SHOW WARNINGS;` | Succeeds and reports note `1007` in MySQL. MyLite succeeds but has no warning API yet. |
| `CREATE DATABASE mylite_schema_lifecycle_b DEFAULT CHARSET 'utf8mb4' COLLATE 'utf8mb4_bin';` | Succeeds and stores the named defaults. |
| `CREATE DATABASE mylite_schema_lifecycle_c ENCRYPTION='X';` | Fails with invalid encryption value error `1525`. |
| `USE mylite_schema_lifecycle_a; SELECT DATABASE();` | Returns `mylite_schema_lifecycle_a` in MySQL. MyLite stores the same session default; `DATABASE()` is a later expression feature. |
| `USE mylite_schema_lifecycle_missing;` | Fails with unknown database error `1049`. |
| `ALTER SCHEMA DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;` after `USE` | Succeeds and changes catalog defaults in MySQL. MyLite updates stored option text. |
| `ALTER DATABASE DEFAULT CHARACTER SET utf8mb4;` with no selected schema | Fails with no database selected error `1046`. |
| `ALTER DATABASE mylite_schema_lifecycle_missing DEFAULT CHARACTER SET utf8mb4;` | Fails with missing database error `3503`. |
| `ALTER DATABASE mylite_schema_lifecycle_a READ ONLY = 2;` | Fails with a syntax error in MySQL. MyLite accepts the literal in the early grammar and reports an execution error. |
| `SHOW DATABASES;` | Returns one column named `Database` and lists visible schemas. |
| `SHOW SCHEMAS;` | Same result shape as `SHOW DATABASES`. |
| `DROP SCHEMA mylite_schema_lifecycle_a; SELECT DATABASE();` after selecting it | Drops the schema and clears the current default schema. |
| `DROP DATABASE mylite_schema_lifecycle_missing;` | Fails with missing database error `1008`. |
| `DROP DATABASE IF EXISTS mylite_schema_lifecycle_missing; SHOW COUNT(*) WARNINGS;` | Succeeds with warning count `0` in MySQL 8.4.9. |

## Test plan

- Parser tests:
  - parse `CREATE DATABASE`, `CREATE SCHEMA IF NOT EXISTS`, and create options
  - parse `CHARSET` and quoted character set/collation option values
  - parse `ALTER DATABASE` with explicit and omitted schema names
  - parse `DROP DATABASE IF EXISTS`
  - parse `SHOW DATABASES` and `SHOW SCHEMAS`
  - preserve quoted schema spans
  - preserve nonreserved schema names such as `encryption`
  - reject unsupported `SHOW DATABASES LIKE` until filtering is implemented
- Runtime tests:
  - initial `SHOW DATABASES` includes seeded system schemas and has column name
    `Database`
  - `CREATE DATABASE` adds a row and does not select it
  - duplicate create fails without `IF NOT EXISTS` and succeeds with it
  - `USE` succeeds for an existing schema and fails for a missing schema
  - `ALTER DATABASE` succeeds with explicit and default schema targets, and
    fails without a selected schema
  - `DROP DATABASE` removes the row and clears the selected schema
  - `DROP DATABASE IF EXISTS` succeeds for missing schemas
  - system schemas cannot be created, altered, or dropped
  - quoted names preserve case and escaped backticks
  - `ENCRYPTION` rejects values other than quoted `Y` or `N`
  - `READ ONLY` rejects numeric values other than `0` or `1`
