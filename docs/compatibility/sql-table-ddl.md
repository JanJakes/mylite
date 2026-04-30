# SQL table DDL

Table lifecycle, table-specific DDL, CREATE TABLE options, and ALTER TABLE
actions.

## Statement surface

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER TABLE` | ❌ | See [ALTER TABLE actions](#alter-table-actions) |
| `CREATE TABLE` | ❌ | Table creation and linked options |
| `CREATE TEMPORARY TABLE` | ❌ | Session-scoped table lifecycle and name shadowing |
| `CREATE TABLE ... LIKE` | ❌ | Exact metadata cloning rules |
| `CREATE TABLE ... SELECT` | ❌ | CTAS inference and atomicity |
| `DROP TABLE` | ❌ | Drop semantics and warnings |
| `RENAME TABLE` | ❌ | Atomic multi-table rename semantics |
| `TRUNCATE TABLE` | ❌ | DDL truncate semantics |
| Atomic DDL | ❌ | DDL atomicity expectations |
| Implicit commit boundaries | ❌ | Implicit commit boundaries |
| `IMPORT TABLE` | ❌ | Transportable tablespace import syntax, diagnostics |

## CREATE TABLE and table options

| Feature | Status | Notes |
| --- | --- | --- |
| Column definition grammar | ❌ | Column attributes and constraints |
| Silent column specification changes | ❌ | Automatic column rewrites |
| Default expressions | ❌ | Literal/expression defaults |
| Generated columns | ❌ | Generated column metadata |
| Invisible columns | ❌ | Implicit column lists, SELECT * behavior, and metadata flags |
| Generated invisible primary keys | ❌ | Invisible primary key metadata |
| AUTO_INCREMENT columns | ❌ | Allocation and metadata |
| Table options: engine | ❌ | Engine option metadata |
| Table options: charset/collation | ❌ | Charset/collation options |
| Table options: storage | ❌ | Storage option metadata |
| Table options: statistics | ❌ | Statistics option metadata |
| Table options: misc | ❌ | Misc table options |
| NDB comment options | ❌ | NDB comment syntax |
| Temporary table metadata | ❌ | Session isolation, name shadowing, and cleanup |

## ALTER TABLE actions

| Feature | Status | Notes |
| --- | --- | --- |
| `ADD COLUMN` | ❌ | Column positioning and defaults |
| `DROP COLUMN` | ❌ | Dependency checks and errors |
| `RENAME COLUMN` | ❌ | Metadata rewrite and dependency updates |
| `CHANGE COLUMN` | ❌ | Rename plus type/attribute change semantics |
| `MODIFY COLUMN` | ❌ | Type/attribute change without rename |
| `ALTER COLUMN SET DEFAULT` | ❌ | Default mutation semantics |
| `ALTER COLUMN DROP DEFAULT` | ❌ | Default removal semantics |
| `ALTER COLUMN SET VISIBLE` / `SET INVISIBLE` | ❌ | Column visibility changes and restrictions |
| `RENAME TO` | ❌ | Table rename via ALTER TABLE |
| `ORDER BY` | ❌ | Physical row ordering syntax and embedded behavior |
| `CONVERT TO CHARACTER SET` | ❌ | Column/table charset and collation conversion semantics |
| `DEFAULT CHARACTER SET` / `COLLATE` | ❌ | Table default charset/collation changes |
| `FORCE` | ❌ | Forced table rebuild semantics |
| `DISCARD TABLESPACE` | ❌ | Tablespace discard syntax |
| `IMPORT TABLESPACE` | ❌ | Tablespace import syntax |
| `ALGORITHM` | ❌ | DEFAULT, INSTANT, INPLACE, COPY handling and diagnostics |
| `LOCK` | ❌ | DEFAULT, NONE, SHARED, EXCLUSIVE handling and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
