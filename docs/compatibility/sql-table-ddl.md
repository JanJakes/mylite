# SQL table DDL

Table lifecycle, table-specific DDL, CREATE TABLE options, and ALTER TABLE
actions.

## Statement surface

| Feature | Status | Notes |
| --- | --- | --- |
| `ALTER TABLE` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME [TO\|AS]`, append-only `ALTER TABLE ... ADD [COLUMN]`, single-column `ALTER TABLE ... DROP [COLUMN]`, single-column `ALTER TABLE ... RENAME COLUMN ... TO ...`, single-column `ALTER TABLE ... MODIFY [COLUMN] ...`, single-column `ALTER TABLE ... CHANGE [COLUMN] old_col new_col ...`, and single-column `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT ...` forms only; see [ALTER TABLE actions](#alter-table-actions) |
| `CREATE TABLE` | 🟡 | Limited persistent base-table creation: optional `IF NOT EXISTS`, explicit integer-family columns (`TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`) and the `INT1`/`INT2`/`INT3`/`INT4`/`INT8` plus `BOOL`/`BOOLEAN` aliases, optional deprecated display width `0..255` before optional single `SIGNED` or `UNSIGNED` for integer-family and `INT*` aliases, `NULL`/`NOT NULL`, optional `DEFAULT NULL`, optional descriptor-owned decimal integer/`TRUE`/`FALSE` defaults within the current signed-64 physical range, optional explicit `ENGINE [=] InnoDB`, and optional fixed default `utf8mb4` / `utf8mb4_0900_ai_ci` table charset/collation options; display width emits warning 1681 and only signed `TINYINT(1)`/`INT1(1)` plus `BOOL`/`BOOLEAN` persist as `tinyint(1)` metadata; existing-table `IF NOT EXISTS` is a no-op with `Note 1050`; no other options, keys, expression defaults, temporary tables, or generated invisible primary keys |
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
| Column definition grammar | 🟡 | Limited integer-family type, `INT1`/`INT2`/`INT3`/`INT4`/`INT8` aliases, bare `BOOL`/`BOOLEAN` aliases, optional unsigned decimal display width before optional single `SIGNED` or `UNSIGNED` for integer-family and `INT*` aliases, nullability, and optional trailing `DEFAULT NULL` or decimal integer/`TRUE`/`FALSE` default literal |
| Silent column specification changes | ❌ | Automatic column rewrites |
| Default expressions | 🟡 | Limited descriptor-owned defaults in supported integer-family column definitions and `ALTER TABLE ... ALTER [COLUMN] column_name SET DEFAULT ...` / `DROP DEFAULT`: `DEFAULT NULL` for nullable columns plus decimal integer literals with optional unary sign and `TRUE`/`FALSE` within MyLite's current physical signed-64 range, with dropped-default metadata preserved distinctly from implicit nullable defaults; supported by `CREATE TABLE`, `ADD COLUMN`, `MODIFY COLUMN`, `CHANGE COLUMN`, omitted-column `INSERT`, `SHOW COLUMNS`, and `SHOW CREATE TABLE`; no string/decimal/float/hex/bit defaults, general expression defaults, or DML `DEFAULT` keyword values |
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
| `ADD COLUMN` | 🟡 | Limited append-only `ALTER TABLE table_name ADD [COLUMN] column_name integer_type [NULL\|NOT NULL] [DEFAULT NULL\|DEFAULT integer_literal\|DEFAULT TRUE\|DEFAULT FALSE]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, `INT1`/`INT2`/`INT3`/`INT4`/`INT8` aliases, and bare `BOOL`/`BOOLEAN` aliases with optional deprecated display width `0..255` before optional single `SIGNED` or `UNSIGNED` for integer-family and `INT*` aliases; existing rows backfill descriptor defaults, nullable no-default columns backfill `NULL`, and not-null no-default columns preserve the current `0` backfill; no positioning, multiple actions, parenthesized lists, non-integer types, generated/invisible/auto-increment columns, keys, constraints, algorithms, locks, temporary tables, or views |
| `DROP COLUMN` | 🟡 | Limited single-column `ALTER TABLE table_name DROP [COLUMN] column_name` for persistent base tables; removes the MyLite descriptor column, compacts later descriptor ordinals, preserves remaining row values, and emits MySQL-compatible diagnostics for unknown columns and attempts to drop the last column; no multiple actions, table-qualified drop targets, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
| `RENAME COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name RENAME COLUMN old_col TO new_col` for persistent base tables; preserves column id, ordinal position, integer type, nullability, and row values, supports exact same-name no-op and case-only spelling changes, and rejects duplicate or unknown columns; no multiple actions, table-qualified column names, type changes, dependency updates, algorithms, locks, temporary tables, or views |
| `CHANGE COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name CHANGE [COLUMN] old_col new_col integer_type [NULL\|NOT NULL] [DEFAULT NULL\|DEFAULT integer_literal\|DEFAULT TRUE\|DEFAULT FALSE]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, `INT1`/`INT2`/`INT3`/`INT4`/`INT8` aliases, and bare `BOOL`/`BOOLEAN` aliases with optional deprecated display width `0..255` before optional single `SIGNED` or `UNSIGNED` for integer-family and `INT*` aliases, preserves column id and ordinal position, replaces name/type/nullability/default from the replacement definition, treats omitted nullability as nullable, supports exact same-definition no-op and case-only spelling changes, validates existing integer/`NULL` row values inside the catalog mutation before descriptor replacement, keeps display-width-only, bool-alias range-equivalent, and default-only changes metadata-only except for physical column renames, and rebuilds descriptor-backed physical tables when type range or nullability changes; rebuilds currently require at least one unshadowed SQLite rowid alias; no positioning, non-integer types, multiple actions, table-qualified column names, generated/invisible/auto-increment columns, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
| `MODIFY COLUMN` | 🟡 | Limited single-action `ALTER TABLE table_name MODIFY [COLUMN] column_name integer_type [NULL\|NOT NULL] [DEFAULT NULL\|DEFAULT integer_literal\|DEFAULT TRUE\|DEFAULT FALSE]` for persistent base tables; supports `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`/`INTEGER`, `BIGINT`, `INT1`/`INT2`/`INT3`/`INT4`/`INT8` aliases, and bare `BOOL`/`BOOLEAN` aliases with optional deprecated display width `0..255` before optional single `SIGNED` or `UNSIGNED` for integer-family and `INT*` aliases, preserves column id and ordinal position, replaces nullability/default exactly as specified, supports exact same-definition no-op and case-only spelling changes, validates existing integer/`NULL` row values inside the catalog mutation before descriptor replacement, keeps display-width-only, bool-alias range-equivalent, and default-only changes metadata-only, and rebuilds descriptor-backed physical tables when type range or nullability changes; rebuilds currently require at least one unshadowed SQLite rowid alias; no positioning, non-integer types, multiple actions, table-qualified column names, generated/invisible/auto-increment columns, keys, constraints, dependency checks, algorithms, locks, temporary tables, or views |
| `ALTER COLUMN SET DEFAULT` | 🟡 | Limited catalog-only `ALTER TABLE table_name ALTER [COLUMN] column_name SET DEFAULT value` for persistent base tables and one unqualified integer-family descriptor column; `value` may be `NULL` for nullable columns, an in-range decimal integer literal with optional unary sign, `TRUE`, or `FALSE`; preserves existing row values and physical SQLite schema, reports zero affected rows and warnings, and updates descriptor defaults for later omitted-column inserts plus `SHOW COLUMNS` / `SHOW CREATE TABLE`; no expression/string/decimal/float/hex/bit defaults, table-qualified columns, multiple actions, visibility changes, temporary tables, or views |
| `ALTER COLUMN DROP DEFAULT` | 🟡 | Limited catalog-only `ALTER TABLE table_name ALTER [COLUMN] column_name DROP DEFAULT` for persistent base tables and one unqualified descriptor column; preserves existing row values and physical SQLite schema, reports zero affected rows and warnings, stores a dropped-default descriptor state distinct from implicit nullable `DEFAULT NULL`, makes later omitted-column inserts fail, and renders MySQL-compatible `SHOW COLUMNS` / `SHOW CREATE TABLE` metadata; no table-qualified columns, multiple actions, visibility changes, temporary tables, or views |
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
