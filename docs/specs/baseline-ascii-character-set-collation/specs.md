# Baseline ascii Character Set and Collation

## Summary

This phase adds MyLite-owned metadata support for MySQL's explicit `ascii`
character set and its `ascii_general_ci` and `ascii_bin` collations. The slice
extends the existing charset/collation descriptor surfaces for `SHOW`,
`INFORMATION_SCHEMA`, schema defaults, table defaults, and column attributes.

The feature is intentionally metadata-first. It does not implement a full
charset conversion engine or complete collation-sensitive expression semantics.
Current string DML storage keeps using the existing UTF-8 text storage and
fixed ASCII comparison subset already documented for string predicates and
ordering. Non-ASCII rejection for `ascii` columns is deferred until row-value
conversion carries effective table-default charset metadata through every
string validation path.

## References

- MySQL 8.4 Reference Manual, "West European Character Sets":
  <https://dev.mysql.com/doc/refman/8.4/en/charset-we-sets.html>
- MySQL 8.4 Reference Manual, "Table Character Set and Collation":
  <https://dev.mysql.com/doc/refman/8.4/en/charset-table.html>
- MySQL 8.4 Reference Manual, "Column Character Set and Collation":
  <https://dev.mysql.com/doc/refman/8.4/en/charset-column.html>
- MySQL 8.4 Reference Manual, "String Data Type Syntax":
  <https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html>
- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html>
- MySQL 8.4 Reference Manual, `SHOW COLLATION`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-collation.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHARACTER_SETS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-character-sets-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLLATIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html>

## MySQL 8.4.9 Runtime Evidence

Observed against the local `mysql:8.4.9` container named `mylite-mysql-849`.
The expectation script added with this phase keeps these checks executable.

`SHOW CHARACTER SET LIKE 'ascii'` returns one row:

| Charset | Description | Default collation | Maxlen |
| --- | --- | --- | --- |
| `ascii` | `US ASCII` | `ascii_general_ci` | `1` |

`SHOW COLLATION LIKE 'ascii%'` returns:

| Collation | Charset | Id | Default | Compiled | Sortlen | Pad_attribute |
| --- | --- | --- | --- | --- | --- | --- |
| `ascii_bin` | `ascii` | `65` |  | `Yes` | `1` | `PAD SPACE` |
| `ascii_general_ci` | `ascii` | `11` | `Yes` | `Yes` | `1` | `PAD SPACE` |

`CREATE DATABASE d DEFAULT CHARACTER SET ascii` stores schema defaults as
`ascii` / `ascii_general_ci`. `ALTER DATABASE d DEFAULT CHARSET=ascii COLLATE
ascii_bin` stores `ascii` / `ascii_bin`.

`CREATE TABLE t (v VARCHAR(10)) DEFAULT CHARSET=ascii` renders:

```sql
CREATE TABLE `t` (
  `v` varchar(10) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=ascii
```

`CREATE TABLE t (v VARCHAR(10)) DEFAULT COLLATE=ascii_bin` derives table
charset `ascii` and renders the column with inherited `COLLATE ascii_bin`.

Column attributes accept `CHARACTER SET ascii`, `COLLATE ascii_general_ci`, and
`COLLATE ascii_bin` on `CHAR`, `VARCHAR`, and `TEXT` family columns. Explicit
column attributes render as `CHARACTER SET ascii COLLATE ...` in `SHOW CREATE
TABLE`, `SHOW FULL COLUMNS`, and `INFORMATION_SCHEMA.COLUMNS`.

The bare column attribute `ASCII` is not the `ascii` character set. MySQL 8.4
documents and verifies it as a deprecated shorthand for `CHARACTER SET latin1`,
so this phase does not implement it.

Invalid charset/collation pairs produce MySQL error `1253 (42000)`, for example
`CHARACTER SET ascii COLLATE utf8mb4_bin` reports:

```text
COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'ascii'
```

Unknown charset names produce `1115 (42000)`. Unknown collation names produce
`1273 (HY000)`.

## Ownership Boundary

- Public API: no new public ABI. Applications continue to use `mylite_execute`,
  result metadata, diagnostics, and cleanup APIs.
- Parser/AST: existing charset/collation option grammar already admits
  identifier and string-literal names. No new grammar token is required for
  explicit `ascii`.
- Analyzer/planner: validates charset and collation names, normalizes canonical
  lowercase descriptor names, derives default collations from character sets,
  and rejects invalid pairs before generated SQLite SQL is prepared.
- Catalog module: durable schema, table, and column descriptors remain the
  authoritative MyLite metadata. Static charset/collation rows remain owned by
  the runtime metadata catalog.
- Result builder: `SHOW CHARACTER SET`, `SHOW COLLATION`, `SHOW CREATE TABLE`,
  `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.SCHEMATA`, `TABLES`, `COLUMNS`,
  `CHARACTER_SETS`, `COLLATIONS`, and
  `COLLATION_CHARACTER_SET_APPLICABILITY` render from MyLite descriptors.
- Storage/VFS: no `.mylite` file format or VFS change. SQLite stores rows in
  the existing physical text columns behind MyLite descriptors.
- SQLite integration: no SQLite fork patch and no new SQLite extension point.
  This slice is MyLite wrapper/catalog logic over public SQLite execution.

## Supported Syntax

Existing MyLite grammar remains the admitted syntax. The names `ascii`,
`ascii_general_ci`, and `ascii_bin` are now accepted where the existing grammar
accepts charset and collation option names.

```lemon
schema_option ::= DEFAULT CHARSET equal_opt option_name.
schema_option ::= DEFAULT CHARACTER SET equal_opt option_name.
schema_option ::= DEFAULT COLLATE equal_opt option_name.

table_option ::= DEFAULT CHARSET equal_opt option_name.
table_option ::= DEFAULT CHARACTER SET equal_opt option_name.
table_option ::= DEFAULT COLLATE equal_opt option_name.

alter_table_default_charset_collation_option ::= DEFAULT CHARSET equal_opt option_name.
alter_table_default_charset_collation_option ::= DEFAULT CHARACTER SET equal_opt option_name.
alter_table_default_charset_collation_option ::= DEFAULT COLLATE equal_opt option_name.

column_attribute ::= CHARACTER SET option_name.
column_attribute ::= CHARSET option_name.
column_attribute ::= COLLATE option_name.
```

`option_name` is an unquoted identifier or a string literal decoded by existing
table-option name rules. Name matching is ASCII case-insensitive; descriptors
store canonical lowercase names.

## Supported Behavior

- Static metadata includes:
  - `ascii` character set: default collation `ascii_general_ci`, description
    `US ASCII`, maxlen `1`;
  - `ascii_general_ci`: charset `ascii`, id `11`, default `Yes`, compiled
    `Yes`, sortlen `1`, pad attribute `PAD SPACE`;
  - `ascii_bin`: charset `ascii`, id `65`, default empty, compiled `Yes`,
    sortlen `1`, pad attribute `PAD SPACE`.
- Schema defaults accept explicit `ascii` charset and `ascii_*` collations in
  `CREATE DATABASE` / `CREATE SCHEMA` and `ALTER DATABASE` / `ALTER SCHEMA`.
- Table defaults accept explicit `ascii` charset and `ascii_*` collations in
  persistent and temporary `CREATE TABLE`, `CREATE TABLE ... LIKE`, and the
  existing `ALTER TABLE ... DEFAULT CHARSET/COLLATE` metadata path.
- Column attributes accept explicit `CHARACTER SET ascii`, `CHARSET ascii`,
  `COLLATE ascii_general_ci`, and `COLLATE ascii_bin` for the existing
  supported `CHAR`, `VARCHAR`, and bare `TEXT` family descriptors.
- Charset-only declarations derive the default collation for that charset:
  `ascii` derives `ascii_general_ci`; `utf8mb4` still derives
  `utf8mb4_0900_ai_ci`; `binary` behavior remains limited to the existing
  binary-normalization path where admitted.
- Collation-only declarations derive the associated character set.
- Mixed valid pair checks are descriptor-driven:
  `ascii` pairs only with `ascii_general_ci` or `ascii_bin`;
  `utf8mb4` pairs only with admitted `utf8mb4_*` collations; `binary` keeps the
  existing binary-only rules.
- `SHOW CREATE TABLE` omits inherited table-default charset/collation where
  MySQL omits it and renders explicit or inherited nondefault column collations
  from MyLite descriptors.
- `SHOW FULL COLUMNS`, result metadata, and `INFORMATION_SCHEMA.COLUMNS` report
  effective charset/collation names from explicit column descriptors first,
  otherwise table descriptors.

## Deferred Behavior

- Bare `ASCII` column attribute. It is a deprecated MySQL shorthand for
  `CHARACTER SET latin1`, not this feature's `ascii` charset.
- `latin1` charset/collation support.
- Full charset conversion, warning-producing conversions, and non-strict
  demotion behavior.
- Strict rejection or substitution of non-ASCII values for columns whose
  effective charset is `ascii`. Existing row-value conversion validates UTF-8
  and NUL/length constraints; table-default charset-aware conversion is a later
  storage slice.
- Full `ascii_general_ci` and `ascii_bin` comparison, ordering, grouping, and
  uniqueness semantics. Current string predicates and string keys continue to
  use MyLite's documented fixed ASCII subset.
- `SET NAMES ascii` / connection character set changes.
- Full MySQL charset/collation catalogs beyond currently admitted rows.

## Diagnostics

- Unknown character set: MySQL-compatible `1115 (42000)` with
  `Unknown character set: '<name>'`.
- Unknown collation: MySQL-compatible `1273 (HY000)` with
  `Unknown collation: '<name>'`.
- Invalid charset/collation pair: MySQL-compatible `1253 (42000)` with
  `COLLATION '<collation>' is not valid for CHARACTER SET '<charset>'`.
- Duplicate column charset/collation attributes, NUL bytes in option names,
  unsupported column families, allocation failures, and physical SQLite errors
  keep the existing MyLite diagnostics.

## Testing

- MySQL expectation script verifies static metadata rows, schema defaults,
  table defaults, column metadata, case-insensitive option names, diagnostics,
  and the deferred bare `ASCII` shorthand behavior.
- Runtime tests cover:
  - `SHOW CHARACTER SET` / `SHOW COLLATION` rows and `LIKE` filters;
  - static `INFORMATION_SCHEMA` charset/collation/applicability rows;
  - schema, table, column, `CREATE TABLE ... LIKE`, and `ALTER TABLE` metadata;
  - uppercase accepted names and canonical lowercase rendering;
  - invalid pair and unknown-name diagnostics;
  - reopen persistence, independent file-backed handles, and preamble safety.

