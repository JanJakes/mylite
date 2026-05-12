# Baseline SERIAL Alias Lifecycle

## Summary

This phase adds the next core MySQL type/key building block: limited
`SERIAL` column-type support for persistent base tables. MySQL expands
`SERIAL` as a `BIGINT UNSIGNED` auto-increment column with a generated unique
key, so this phase also admits create-time `AUTO_INCREMENT` columns backed by a
supported secondary `UNIQUE` or `KEY` descriptor, not only by a primary key.

The slice is intentionally narrow. It does not implement the separate
`SERIAL DEFAULT VALUE` column attribute, generated invisible primary keys,
foreign keys, or a general key/constraint grammar expansion.

## Sources

- MySQL 8.4 Reference Manual, numeric data types:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_serial_alias_lifecycle_expectations.sh`.

This specification is independently authored from public MySQL documentation,
observed MySQL 8.4.9 runtime behavior, public SQLite APIs, and existing MyLite
code. It does not copy MySQL, MariaDB, Percona, SQLite implementation internals,
or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes against MySQL 8.4.9 show:

- `CREATE TABLE t (id SERIAL, v INT)` creates `id` as `bigint unsigned NOT
  NULL AUTO_INCREMENT` with a unique secondary key named `id`.
- `SHOW COLUMNS` reports the `SERIAL` column as type `bigint unsigned`,
  `Null = NO`, `Key = PRI`, `Default = NULL`, and `Extra = auto_increment`.
  The `PRI` value is MySQL's metadata behavior for the first `NOT NULL` unique
  key when no primary key exists.
- `SHOW CREATE TABLE` renders the expanded form, not the spelling `SERIAL`.
- `INFORMATION_SCHEMA.COLUMNS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `STATISTICS` expose the expanded type, `auto_increment`, and unique-key
  descriptor.
- `CREATE TABLE t (id SERIAL NULL, v INT)` is accepted and leaves the column
  nullable while retaining `AUTO_INCREMENT` and the unique key. In that case
  `SHOW COLUMNS` reports `Key = UNI`.
- `CREATE TABLE t (id SERIAL PRIMARY KEY, v INT)` is accepted and creates both
  a primary key and the generated unique key.
- `CREATE TABLE t (id SERIAL UNIQUE, v INT)` and `CREATE TABLE t (id SERIAL
  AUTO_INCREMENT, v INT)` do not add duplicate visible metadata beyond the
  `SERIAL` expansion.
- `CREATE TABLE t (id SERIAL, v INT, KEY(id))` is accepted and adds a nonunique
  secondary key named `id_2` beside the generated unique key.
- `CREATE TABLE t (id INT AUTO_INCREMENT UNIQUE, v INT)` and
  `CREATE TABLE t (id INT AUTO_INCREMENT, KEY(id), v INT)` are accepted even
  without a primary key. Without explicit `NULL`, MySQL renders the auto column
  as `NOT NULL`.
- `CREATE TABLE t (id INT AUTO_INCREMENT NULL UNIQUE, v INT)` and the
  equivalent nonunique-key form are accepted and leave the auto column nullable.
- Table-level `AUTO_INCREMENT=N` works with a `SERIAL` column and controls the
  first generated value.
- Omitted, `NULL`, `0`, and `DEFAULT` values generate auto-increment values
  through the existing MySQL default SQL mode.
- `CREATE TABLE t (id INT AUTO_INCREMENT, v INT)` still fails with
  `1075 / 42000` because the auto column is not indexed.
- `CREATE TABLE t (id SERIAL DEFAULT 7, v INT)` fails with
  `1067 / 42000`.
- `CREATE TABLE t (id SERIAL DEFAULT VALUE, v INT)` is a syntax error; the
  separate `SERIAL DEFAULT VALUE` form applies after an explicit integer type,
  such as `id INT SERIAL DEFAULT VALUE`, and remains deferred by this phase.

## Scope

Supported:

- persistent base tables only;
- `CREATE TABLE ... (column_name SERIAL [column_attribute ...])`;
- `SERIAL` expands to:
  - logical type `BIGINT UNSIGNED`;
  - physical integer storage using the existing integer-family path;
  - `AUTO_INCREMENT`;
  - a generated inline unique-key descriptor on the same column;
  - implicit `NOT NULL` unless the column explicitly says `NULL`;
- existing `NULL` / `NOT NULL` column attributes after `SERIAL`;
- existing inline `PRIMARY KEY`, inline `UNIQUE`, and explicit
  `AUTO_INCREMENT` attributes after `SERIAL`, without duplicate visible unique
  or auto-increment descriptors;
- table-level primary, unique, and nonunique index definitions that reference a
  `SERIAL` column within the currently supported one-column key subsets;
- create-time `AUTO_INCREMENT` on one integer-family column backed by a
  supported single-column secondary `UNIQUE` or nonunique `KEY` descriptor;
- descriptor-owned `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, limited
  `INFORMATION_SCHEMA`, `CREATE TABLE ... LIKE`, `TRUNCATE`, table rename/drop,
  reopen persistence, and independent file-backed handle behavior through the
  existing descriptor paths;
- generated and explicit auto-increment values in existing `INSERT ... VALUES`
  and `INSERT ... SET` paths;
- table-level `AUTO_INCREMENT=N` on the existing supported nonnegative integer
  option subset.

Deferred:

- `SERIAL DEFAULT VALUE` as a column attribute after explicit integer types;
- accepting `SERIAL` in `ALTER TABLE ADD`, `MODIFY`, or `CHANGE` execution;
- default values other than the existing hidden `ALTER ... SET DEFAULT`
  behavior for auto-increment columns;
- multiple auto-increment columns;
- auto-increment on non-integer, generated, invisible-generated, temporary, or
  view columns;
- full constraint names, foreign keys, check constraints, generated invisible
  primary keys, `NO_AUTO_VALUE_ON_ZERO`, replication lock modes, concurrency
  guarantees, protocol insert-id metadata, or privilege semantics.

## Ownership Boundaries

- Public API: no ABI change. Applications continue to use `mylite_execute()`
  and the existing result/diagnostic accessors.
- Statement context: owns diagnostics, warning count, affected rows, and
  transaction cleanup. Successful `CREATE TABLE` and inserts use existing
  non-row result conventions.
- Parser/AST: admits the `SERIAL` type token in column definitions and carries
  a type-alias marker. The parser does not inspect descriptors or SQLite
  schema.
- Analyzer/planner: expands `SERIAL` into an existing integer descriptor plus
  auto-increment and unique-key plan fields. It validates that auto-increment
  columns are indexed by a supported primary or secondary key before physical
  SQL is generated.
- Catalog module: durable table, column, and index descriptors remain the
  MySQL metadata authority. `SERIAL` itself is not persisted as a spelling;
  the expanded descriptors are persisted.
- Result/introspection builders: render metadata from descriptors. SQLite
  schema text, `PRAGMA`, `sqlite_sequence`, and rowid state are not metadata
  authority.
- SQLite physical storage: uses the existing generated MyLite rowid table and
  generated SQLite indexes. MyLite binds generated and explicit values as
  ordinary prepared-statement parameters.
- Storage/VFS: no `.mylite` file-format or VFS change. The MyLite preamble
  remains untouched.

No SQLite fork patch is required. This phase is MyLite parser/planner/catalog
translation plus existing SQLite row storage and index enforcement.

## Supported Grammar

Independent MyLite Lemon-style grammar snippets:

```lemon
column_type ::= serial_type.

serial_type ::= SERIAL.

identifier ::= SERIAL.
```

`SERIAL` remains a nonreserved keyword. It is accepted as an identifier where
the grammar expects ordinary identifiers.

The separately documented MySQL spelling below is intentionally not admitted by
this phase:

```sql
column_name integer_type SERIAL DEFAULT VALUE
```

## Descriptor Expansion

For `column_name SERIAL` the planner behaves as if the user had provided:

```sql
column_name BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE
```

with these MySQL-compatible adjustments:

- explicit `NULL` after `SERIAL` overrides the implicit `NOT NULL`;
- explicit `NOT NULL`, `AUTO_INCREMENT`, or `UNIQUE` after `SERIAL` is
  redundant and leaves one auto-increment column and one generated unique key;
- explicit inline or table-level `PRIMARY KEY` creates a primary-key descriptor
  in addition to the generated unique descriptor;
- the generated unique key name follows existing MyLite/MySQL name derivation
  from the column name and `_2`, `_3`, ... suffixing when needed;
- `SERIAL` is not retained as a logical type string. Metadata renders
  `bigint unsigned`, `AUTO_INCREMENT`, and key descriptors from catalog data.

For non-`SERIAL` `AUTO_INCREMENT` columns, `CREATE TABLE` now accepts the
column if the planned descriptors contain one supported index whose first key
part is the auto-increment column. This may be the existing primary-key subset,
an inline/table-level supported unique key, or a supported nonunique secondary
key. If no such key exists, MyLite keeps the existing `1075` diagnostic.

## DML Semantics

`SERIAL` and secondary-key auto-increment columns use the existing
descriptor-owned auto-increment allocator:

- omitted, `NULL`, `0`, and `DEFAULT` values generate the next value;
- explicit positive values may advance the table counter;
- explicit nonpositive values are stored through the existing integer
  conversion path and do not update `LAST_INSERT_ID()`;
- successful generated inserts update `LAST_INSERT_ID()` after commit;
- mixed explicit/generated multi-row auto-increment behavior remains deferred
  exactly as in the existing auto-increment baseline;
- duplicate-key detection is driven by descriptor-owned unique indexes.

`INSERT ... SELECT` and `REPLACE` retain their current key-bearing target
limits. This phase does not widen those DML paths.

## Metadata Semantics

`SHOW CREATE TABLE` renders the expanded column and key descriptors. Examples:

```sql
CREATE TABLE `t` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
```

`SHOW COLUMNS` uses the existing MySQL-compatible key text rules:

- `PRI` for actual primary-key columns;
- `PRI` for the first `NOT NULL` unique secondary key when no primary key
  exists;
- `UNI` for nullable or non-first unique secondary key columns;
- `MUL` for nonunique secondary key columns.

`INFORMATION_SCHEMA.COLUMNS.EXTRA` reports `auto_increment`, and limited
constraint/statistics views expose the generated unique descriptor exactly as
other supported descriptor-owned unique keys.

## Diagnostics

The phase reuses existing diagnostics where possible:

- syntax errors: MySQL-compatible `1064 / 42000`;
- missing selected/default schema: existing selected-schema diagnostic;
- unknown schema/table: existing MySQL-compatible diagnostics;
- reserved `_mylite_*` schema/table/column names: existing MyLite reserved-name
  diagnostics before physical SQL generation;
- unsupported object kind: existing persistent-base-table unsupported
  diagnostic;
- auto-increment without a key: MySQL-compatible `1075 / 42000`;
- more than one auto-increment column: MySQL-compatible `1075 / 42000`;
- non-integer auto-increment column: MySQL-compatible `1063 / 42000`;
- invalid non-`NULL` auto-increment default: MySQL-compatible `1067 / 42000`;
- unknown key column, duplicate key name, unsupported key type, duplicate key
  value, allocation failure, and SQLite physical failures: existing descriptor
  key and runtime diagnostics.

Successful supported statements report warning count `0` unless they use an
already-warning-producing existing feature such as deprecated integer display
width. No new warnings are introduced.

## Performance

This phase does not add query-time materialization. `SERIAL` is expanded once
during DDL planning. Inserts continue to use prepared SQLite inserts and
generated physical indexes. Metadata queries iterate descriptor rows through
existing synthetic information-schema builders.

## Tests

Add MySQL-runtime expectation coverage and fast C tests for:

- parser acceptance of `SERIAL` as a column type and as a nonreserved
  identifier;
- parser rejection/deferral of `SERIAL DEFAULT VALUE`;
- `CREATE TABLE` metadata for `SERIAL`, `SERIAL NULL`, `SERIAL PRIMARY KEY`,
  secondary-key auto-increment unique, and secondary-key auto-increment
  nonunique forms;
- generated inserts, explicit inserts, `LAST_INSERT_ID()`, affected rows,
  warning count, and `AUTO_INCREMENT=N`;
- descriptor metadata through `SHOW COLUMNS`, `SHOW CREATE TABLE`,
  `SHOW INDEX`, `SHOW TABLE STATUS`, and limited `INFORMATION_SCHEMA`;
- `CREATE TABLE ... LIKE`, rename/drop/truncate, reopen persistence, and
  independent file-backed handles through existing descriptor paths;
- diagnostics for unindexed auto-increment, multiple auto-increment columns,
  non-integer auto-increment, invalid defaults, and unsupported `ALTER` uses;
- `.mylite` preamble preservation and unchanged SQLite payload invariants.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/type-system-literals-conversion.md`;
- `docs/compatibility/sql-table-ddl.md`;
- related index/metadata compatibility docs only where the supported surface
  changes.

Do not claim full `SERIAL DEFAULT VALUE`, full auto-increment semantics,
foreign-key behavior, invisible generated primary keys, or general constraint
support.
