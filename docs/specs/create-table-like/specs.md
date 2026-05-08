# CREATE TABLE ... LIKE

## Scope

This feature adds executable support for MySQL-compatible table cloning through
`CREATE TABLE ... LIKE`.

In scope:

- `CREATE TABLE [IF NOT EXISTS] target LIKE source`
- `CREATE TEMPORARY TABLE [IF NOT EXISTS] target LIKE source`
- schema-qualified and default-schema target and source names
- source resolution for persistent and session-local temporary base tables
- empty target physical table creation
- catalog cloning for table options, columns, indexes, and check constraints
- MySQL-compatible omission of foreign-key definitions from the clone
- MySQL-compatible duplicate-target notes and same-name alias errors
- MySQL-compatible implicit commit before non-temporary `CREATE TABLE ... LIKE`

Out of scope:

- cloning views
- `LOCK TABLES` interactions
- `DATA DIRECTORY`, `INDEX DIRECTORY`, `ENGINE_ATTRIBUTE`, and
  `SECONDARY_ENGINE_ATTRIBUTE`
- revalidating historic definitions against changed SQL modes beyond MyLite's
  current create-table validation surface
- broader `CREATE TABLE ... SELECT`

## Sources

- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## MySQL 8.4.9 behavior summary

`CREATE TABLE dst LIKE src` creates an empty base table with the source table's
supported definition. It clones column attributes, indexes, generated-column
metadata, expression defaults, table collation, and table comments.

Foreign-key definitions are not cloned. Indexes that exist on the source table
are cloned, including indexes that support foreign keys. Check constraints are
cloned with generated names based on the target table, such as
`dst_chk_1`, while preserving the check clause and enforcement state.

The destination table does not inherit `TEMPORARY` from a temporary source. A
temporary destination is created only when the statement uses
`CREATE TEMPORARY TABLE ... LIKE`.

The destination is empty. Source rows are not copied. For an `AUTO_INCREMENT`
column, the column attribute is preserved, while the destination sequence starts
at the initial value rather than the source table's current next value.

Name resolution and diagnostics observed against MySQL 8.4.9:

- unqualified target and source names require a selected schema
- a fully qualified target and source work without a selected schema
- an unqualified source without a selected schema fails with error 1046
- an unqualified target without a selected schema fails with error 1046
- a missing source table fails with error 1146
- `CREATE TABLE IF NOT EXISTS existing LIKE missing` still fails with 1146
  because MySQL validates the source before skipping the existing target
- same-schema `CREATE TABLE t LIKE t` fails with error 1066,
  `Not unique table/alias: 't'`, even with `IF NOT EXISTS`
- cross-schema `CREATE TABLE d2.t LIKE d1.t` succeeds when `d1.t` exists and
  `d2.t` does not
- when the source exists and the target exists without `IF NOT EXISTS`, MySQL
  fails with error 1050, `Table 'target' already exists`
- when the source exists and the target exists, `IF NOT EXISTS` succeeds as a
  no-op note 1050

## MyLite behavior

### Parser and AST

MyLite extends the existing `CREATE TABLE` statement node with a
`create_table_like` flag. In the LIKE form, the node children are:

1. target table name
2. source table name
3. optional `IF NOT EXISTS`

The table-definition form keeps its existing children unchanged.

Supported grammar:

```lemon
create_table_statement ::= CREATE opt_temporary TABLE opt_if_not_exists
                           table_name LIKE table_name.
```

The existing `create_table_temporary` flag records whether the destination is
temporary.

### Runtime execution

`CREATE TABLE ... LIKE` prepares as a custom `CREATE TABLE` statement. Prepare
does not mutate storage or metadata.

Execution order:

1. Non-temporary destinations commit an active explicit transaction before
   validation and execution, matching MySQL DDL implicit-commit behavior.
2. Target and source schema names are resolved from explicit qualifiers or the
   selected default schema.
3. Missing selected schema reports `No database selected`.
4. A same-schema target/source table name reports error 1066,
   `Not unique table/alias`.
5. The target schema must exist and must not be a system schema.
6. The source must be an existing base table, resolving temporary metadata
   before persistent metadata.
7. Only after source validation does `IF NOT EXISTS` skip an existing target
   with note 1050.
8. The physical destination table and catalog clone are created atomically.

Physical storage is created empty from the source physical table shape. The
target receives no rows.

Catalog cloning:

- table catalog rows are cloned into the destination catalog with target schema
  and table name, copied engine, row format, table collation, and comment
- table row and byte counters start at zero placeholders
- `AUTO_INCREMENT` current state does not copy the source's current next value
- column catalog rows are copied with target schema and table name
- index catalog rows are copied with target schema, target table name, target
  index schema, and zero cardinality
- check constraint rows are copied with generated target names
- foreign-key catalog rows are intentionally not copied

Temporary destinations write only temporary catalog rows and create a SQLite
temporary physical table. Persistent destinations write only persistent catalog
rows and create a persistent physical table. Source tables may be temporary or
persistent independently of the destination mode.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `CREATE TABLE clone LIKE src` | Creates an empty persistent table with source columns, indexes, checks, collation, and comment. |
| `CREATE TEMPORARY TABLE tmp_clone LIKE src` | Creates a temporary clone and `SHOW CREATE TABLE` emits `CREATE TEMPORARY TABLE`. |
| clone from temporary source without `TEMPORARY` on the destination | Creates a persistent destination, not a temporary destination. |
| source table with foreign key | Destination has no foreign-key metadata, but cloned source indexes remain. |
| source table with named check | Destination check name is generated from the target table name. |
| source table with `AUTO_INCREMENT` column and advanced sequence | Destination preserves `auto_increment` column metadata but does not copy the source next value. |
| `CREATE TABLE IF NOT EXISTS existing LIKE src` | Succeeds as no-op note 1050 when `src` exists. |
| `CREATE TABLE IF NOT EXISTS existing LIKE missing` | Fails with missing-source error 1146. |
| `CREATE TABLE t LIKE t` and `CREATE TABLE IF NOT EXISTS t LIKE t` | Fail with error 1066 in the same schema. |
| `CREATE TABLE d2.t LIKE d1.t` | Succeeds when source and target schemas differ. |
| fully qualified target and source with no selected schema | Succeeds. |
| unqualified source or target with no selected schema | Fails with error 1046. |

## Test plan

- Parser tests for persistent, temporary, `IF NOT EXISTS`, qualified target,
  qualified source, and same-name syntax.
- Runtime tests for successful persistent and temporary clones.
- Runtime tests for cloning from a temporary source into a persistent
  destination.
- Metadata tests for `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW INDEX`,
  `INFORMATION_SCHEMA.TABLES`, `COLUMNS`, `STATISTICS`,
  `TABLE_CONSTRAINTS`, `CHECK_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
  `REFERENTIAL_CONSTRAINTS`.
- Error tests for missing selected schema, missing source, same-schema
  target/source names, missing target schema, system target schema, duplicate
  target, and `IF NOT EXISTS` ordering.
- Transaction tests showing non-temporary `CREATE TABLE ... LIKE` commits an
  active explicit transaction before execution, while temporary destinations do
  not force that commit boundary.
