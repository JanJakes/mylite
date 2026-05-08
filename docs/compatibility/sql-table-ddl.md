# SQL table DDL

Table lifecycle, table-specific DDL, CREATE TABLE options, and ALTER TABLE
actions.

## Statement surface

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER TABLE` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME [TO\|AS]` only; see [ALTER TABLE actions](#alter-table-actions) |
| `CREATE TABLE` | 🟡 | Limited persistent base-table creation: explicit `INT`/`INTEGER`/`BIGINT` columns, optional `UNSIGNED`, `NULL`/`NOT NULL`, optional explicit `ENGINE [=] InnoDB`, and optional fixed default `utf8mb4` / `utf8mb4_0900_ai_ci` table charset/collation options; no other options, keys, defaults, temporary tables, or `IF NOT EXISTS` |
| `CREATE TEMPORARY TABLE` | ❌ | Session-scoped table lifecycle and name shadowing |
| `CREATE TABLE ... LIKE` | ❌ | Exact metadata cloning rules |
| `CREATE TABLE ... SELECT` | ❌ | CTAS inference and atomicity |
| `DROP TABLE` | 🟡 | Limited single persistent base-table drop without `IF EXISTS`, `TEMPORARY`, multi-table drop, `RESTRICT`, or `CASCADE` |
| `RENAME TABLE` | 🟡 | Limited single-pair persistent base-table rename from `baseline-table-rename-lifecycle`, including unqualified, schema-qualified, and cross-schema names; no multi-table rename, temporary tables, views, triggers, or privilege semantics |
| `TRUNCATE TABLE` | 🟡 | Limited persistent base-table `TRUNCATE [TABLE] table_name` for unqualified and schema-qualified descriptor targets; empties physical rows, preserves descriptors, and reports zero affected rows; no implicit commits, temporary tables, partitions, foreign keys, triggers, auto-increment reset, locks, privileges, or physical storage rebuild semantics |
| Atomic DDL | 🟡 | Catalog descriptor rows and generated physical SQLite table changes commit or roll back atomically for the limited create/drop/rename/truncate and single-action alter-rename subsets only |
| Implicit commit boundaries | ❌ | Implicit commit boundaries |
| `IMPORT TABLE` | ❌ | Transportable tablespace import syntax, diagnostics |

## CREATE TABLE and table options

| Feature | Status | Notes |
| --- | --- | --- |
| Column definition grammar | 🟡 | Limited integer type and nullability grammar only |
| Silent column specification changes | ❌ | Automatic column rewrites |
| Default expressions | ❌ | Literal/expression defaults |
| Generated columns | ❌ | Generated column metadata |
| Invisible columns | ❌ | Implicit column lists, SELECT * behavior, and metadata flags |
| Generated invisible primary keys | ❌ | Invisible primary key metadata |
| AUTO_INCREMENT columns | ❌ | Allocation and metadata |
| Table options: engine | 🟡 | Optional explicit `ENGINE [=] InnoDB` only; no alternate engines, engine substitution, or durable per-table engine metadata |
| Table options: charset/collation | 🟡 | Optional fixed default `CHARSET` / `CHARACTER SET utf8mb4` and `COLLATE utf8mb4_0900_ai_ci` options only for the limited persistent `CREATE TABLE` subset, with matching static `SHOW CHARACTER SET` / `SHOW COLLATION` rows; no alternate defaults, descriptor metadata, string semantics, or full charset/collation catalogs |
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
| `RENAME TO` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME [TO\|AS]` through descriptor rename; supports unqualified, schema-qualified, cross-schema, and same-object no-op forms; no combined actions, temporary tables, views, triggers, options, locks, algorithms, privileges, or metadata side effects |
| `ORDER BY` | ❌ | Physical row ordering syntax and embedded behavior |
| `CONVERT TO CHARACTER SET` | ❌ | Column/table charset and collation conversion semantics |
| `DEFAULT CHARACTER SET` / `COLLATE` | ❌ | Table default charset/collation changes |
| `FORCE` | ❌ | Forced table rebuild semantics |
| `DISCARD TABLESPACE` | ❌ | Tablespace discard syntax |
| `IMPORT TABLESPACE` | ❌ | Tablespace import syntax |
| `ALGORITHM` | ❌ | DEFAULT, INSTANT, INPLACE, COPY handling and diagnostics |
| `LOCK` | ❌ | DEFAULT, NONE, SHARED, EXCLUSIVE handling and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
