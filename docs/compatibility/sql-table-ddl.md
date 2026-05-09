# SQL table DDL

Table lifecycle, table-specific DDL, CREATE TABLE options, and ALTER TABLE
actions.

## Statement surface

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER TABLE` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME [TO\|AS]`, append-only `ALTER TABLE ... ADD [COLUMN]`, single-column `ALTER TABLE ... DROP [COLUMN]`, single-column `ALTER TABLE ... RENAME COLUMN ... TO ...`, single-column `ALTER TABLE ... MODIFY [COLUMN] ...`, and single-column `ALTER TABLE ... CHANGE [COLUMN] old_col new_col ...` forms only; see [ALTER TABLE actions](#alter-table-actions) |
| `CREATE TABLE` | 🟡 | Limited persistent base-table creation: optional `IF NOT EXISTS`, explicit integer-family columns (`TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`), optional single `SIGNED` or `UNSIGNED`, `NULL`/`NOT NULL`, optional explicit `ENGINE [=] InnoDB`, and optional fixed default `utf8mb4` / `utf8mb4_0900_ai_ci` table charset/collation options; existing-table `IF NOT EXISTS` is a no-op with `Note 1050`; no other options, keys, defaults, temporary tables, or generated invisible primary keys |
| `CREATE TEMPORARY TABLE` | ❌ | Session-scoped table lifecycle and name shadowing |
| `CREATE TABLE ... LIKE` | ❌ | Exact metadata cloning rules |
| `CREATE TABLE ... SELECT` | ❌ | CTAS inference and atomicity |
| `DROP TABLE` | 🟡 | Limited persistent base-table drop with comma-separated target lists and optional `IF EXISTS`; missing-table and missing-explicit-schema `IF EXISTS` forms are no-ops with `Note 1051`, duplicate targets fail before mutation, and non-`IF EXISTS` missing targets fail atomically; no `TEMPORARY`, `RESTRICT`, or `CASCADE` |
| `RENAME TABLE` | 🟡 | Limited persistent base-table rename with one or more left-to-right pairs from `baseline-table-rename-lifecycle` and `baseline-multi-table-rename-lifecycle`, including unqualified, schema-qualified, and cross-schema names plus statement rollback on pair failure; no temporary tables, views, triggers, metadata locks, foreign keys, or privilege semantics |
| `TRUNCATE TABLE` | 🟡 | Limited persistent base-table `TRUNCATE [TABLE] table_name` for unqualified and schema-qualified descriptor targets; empties physical rows, preserves descriptors, and reports zero affected rows; no implicit commits, temporary tables, partitions, foreign keys, triggers, auto-increment reset, locks, privileges, or physical storage rebuild semantics |
| Atomic DDL | 🟡 | Catalog descriptor rows and generated physical SQLite table changes commit or roll back atomically for the limited create/drop/rename/truncate and single-action alter rename/add-column/drop-column/rename-column/modify-column/change-column subsets only |
| Implicit commit boundaries | ❌ | Implicit commit boundaries |
| `IMPORT TABLE` | ❌ | Transportable tablespace import syntax, diagnostics |

## CREATE TABLE and table options

| Feature | Status | Notes |
| --- | --- | --- |
| Column definition grammar | 🟡 | Limited integer-family type, optional single `SIGNED` or `UNSIGNED`, and nullability grammar only |
| Silent column specification changes | ❌ | Automatic column rewrites |
| Default expressions | ❌ | Literal/expression defaults |
| Generated columns | ❌ | Generated column metadata |
| Invisible columns | ❌ | Implicit column lists, SELECT * behavior, and metadata flags |
| Generated invisible primary keys | ❌ | Invisible primary key metadata and hidden `my_row_id` table creation; limited scalar `@@sql_generate_invisible_primary_key` reads expose the fixed disabled baseline only |
| Primary key requirement enforcement | ❌ | `@@sql_require_primary_key` DDL effects are not implemented; limited scalar reads expose the fixed disabled baseline only |
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
| `ADD COLUMN` | 🟡 | Limited append-only `ALTER TABLE table_name ADD [COLUMN] column_name integer_type [NULL\|NOT NULL]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, and `BIGINT` with optional single `SIGNED` or `UNSIGNED`, nullable existing-row `NULL` backfill, and non-null existing-row `0` backfill; no defaults, positioning, multiple actions, parenthesized lists, non-integer types, generated/invisible/auto-increment columns, keys, constraints, algorithms, locks, temporary tables, or views |
| `DROP COLUMN` | 🟡 | Limited single-column `ALTER TABLE table_name DROP [COLUMN] column_name` for persistent base tables; removes the MyLite descriptor column, compacts later descriptor ordinals, preserves remaining row values, and emits MySQL-compatible diagnostics for unknown columns and attempts to drop the last column; no multiple actions, table-qualified drop targets, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
| `RENAME COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name RENAME COLUMN old_col TO new_col` for persistent base tables; preserves column id, ordinal position, integer type, nullability, and row values, supports exact same-name no-op and case-only spelling changes, and rejects duplicate or unknown columns; no multiple actions, table-qualified column names, type changes, dependency updates, algorithms, locks, temporary tables, or views |
| `CHANGE COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name CHANGE [COLUMN] old_col new_col integer_type [NULL\|NOT NULL]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, and `BIGINT` with optional single `SIGNED` or `UNSIGNED`, preserves column id and ordinal position, replaces name, type, and nullability from the replacement definition, treats omitted nullability as nullable, supports exact same-definition no-op and case-only spelling changes, validates existing integer/`NULL` row values inside the catalog mutation before descriptor replacement, and rebuilds descriptor-backed physical tables when type or nullability changes; rebuilds currently require at least one unshadowed SQLite rowid alias; no defaults, positioning, non-integer types, multiple actions, table-qualified column names, generated/invisible/auto-increment columns, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
| `MODIFY COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name MODIFY [COLUMN] column_name integer_type [NULL\|NOT NULL]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, and `BIGINT` with optional single `SIGNED` or `UNSIGNED`, preserves column id and ordinal position, replaces nullability exactly as specified, supports exact same-definition no-op and case-only spelling changes, validates existing integer/`NULL` row values inside the catalog mutation before descriptor replacement, and rebuilds descriptor-backed physical tables when type or nullability changes; rebuilds currently require at least one unshadowed SQLite rowid alias; no defaults, positioning, non-integer types, multiple actions, table-qualified column names, generated/invisible/auto-increment columns, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
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
