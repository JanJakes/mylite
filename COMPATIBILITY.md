# MyLite ↔ MySQL 8.4.9 compatibility

This document tracks MyLite's first baseline compatibility target against
MySQL 8 LTS, currently MySQL 8.4.9. The baseline tables below are the focused
near-term implementation contract. The complete compatibility inventory remains
in the [detailed compatibility tables](#detailed-compatibility-tables).

## Legend

| Mark | Meaning |
| :-: | --- |
| ✅ | Supported with MySQL 8.4.9-compatible behavior and runtime comparison tests for relevant results, errors, warnings, metadata, affected rows, side effects, and protocol details. |
| 🟡 | Implemented with documented compatibility gaps, reduced fidelity, or MyLite-specific behavior that is covered by tests. |
| ⚪ | Accepted at parse or API level and intentionally handled as an embedded-compatible no-op, placeholder, warning, or diagnostic. |
| ❌ | Not implemented, rejected, or not yet MySQL-runtime verified in MyLite. |

## Baseline Compatibility

Each row tracks one baseline behavior or feature. A row moves out of `❌` only
when the listed surface is implemented and covered by MySQL 8.4.9 comparison
tests.

### Runtime, Identity, and Diagnostics

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| Dynamic current database | 🟡 | `USE` selects catalog schemas; `DATABASE()` / `SCHEMA()` return the selected schema or `NULL` in the limited scalar-select slice; dropping the selected schema clears the selection. | [SQL schemas](docs/compatibility/sql-schemas.md), [runtime session state](docs/compatibility/runtime-session-sql-modes.md), [system functions](docs/compatibility/functions-system.md) |
| Database listing baseline | 🟡 | Descriptor-driven `SHOW DATABASES` / `SHOW SCHEMAS` for MyLite catalog schemas only; no system schemas, filters, or privileges. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md), [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| Current user identity | 🟡 | Limited scalar `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`, and bare `CURRENT_USER` expose MyLite's embedded `root@%` session identity; no accounts, authentication, roles, privileges, definer semantics, `IGNORE_SPACE`, or stored-function resolution. | [SQL users, roles, and privileges](docs/compatibility/sql-users-privileges.md), [system functions](docs/compatibility/functions-system.md) |
| Current role function | 🟡 | Limited scalar `CURRENT_ROLE()` exposes MyLite's no-active-role value `NONE`; no role catalog, role grants, default roles, `SET ROLE`, privileges, or bare `CURRENT_ROLE`. | [SQL users, roles, and privileges](docs/compatibility/sql-users-privileges.md), [system functions](docs/compatibility/functions-system.md) |
| Version function | 🟡 | Limited scalar `VERSION()` exposes MyLite's engine version string; no MySQL server-version impersonation or protocol handshake version reporting. | [system functions](docs/compatibility/functions-system.md) |
| Version system variables | 🟡 | Limited scalar reads for `@@version` and `@@version_comment` expose MyLite's engine version and fixed MyLite comment; no MySQL server-version impersonation, protocol handshake behavior, compile variables, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [system functions](docs/compatibility/functions-system.md) |
| Diagnostics count variables | 🟡 | Limited scalar `SELECT @@warning_count` and `SELECT @@error_count` read the previous diagnostics snapshot and then clear it like MySQL nondiagnostic `SELECT`; supports `session`/`local` qualifiers only. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| Character set system variables | 🟡 | Limited scalar reads for `@@character_set_client`, `@@character_set_connection`, `@@character_set_results`, and `@@collation_connection` expose MyLite's fixed `utf8mb4` / `utf8mb4_0900_ai_ci` connection baseline; no `SET`, `SET NAMES`, conversion, or full charset state. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [character sets](docs/compatibility/character-sets.md), [collations](docs/compatibility/collations.md) |
| Server character set system variables | 🟡 | Limited scalar reads for `@@character_set_server` and `@@collation_server` expose MyLite's fixed embedded server defaults `utf8mb4` / `utf8mb4_0900_ai_ci`; no `SET`, startup options, mutable global state, database defaults, conversion, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [character sets](docs/compatibility/character-sets.md), [collations](docs/compatibility/collations.md) |
| Database character set system variables | 🟡 | Limited scalar reads for `@@character_set_database` and `@@collation_database` expose MyLite's fixed current-database defaults `utf8mb4` / `utf8mb4_0900_ai_ci`; no `SET`, `CREATE DATABASE` options, `ALTER DATABASE`, mutable schema defaults, conversion, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL schemas](docs/compatibility/sql-schemas.md), [character sets](docs/compatibility/character-sets.md), [collations](docs/compatibility/collations.md) |
| System character set system variable | 🟡 | Limited scalar reads for global-only `@@character_set_system` expose MyLite's fixed identifier-system charset placeholder `utf8mb3`; no `SET`, mutable state, string conversion, identifier conversion, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [character sets](docs/compatibility/character-sets.md) |
| Filesystem character set system variable | 🟡 | Limited scalar reads for `@@character_set_filesystem` expose MyLite's fixed file-name charset placeholder `binary`; no `SET`, mutable state, server-side file operations, file-name conversion, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [character sets](docs/compatibility/character-sets.md), [SQL file output](docs/compatibility/sql-file-output.md) |
| Default storage engine system variable | 🟡 | Limited scalar reads for `@@default_storage_engine` expose MyLite's fixed embedded permanent-table default `InnoDB`; no `SET`, mutable engine state, temporary-table defaults, alternate engines, plugins, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [embedded-design decisions](docs/compatibility/embedded-design-decisions.md) |
| Autocommit system variable | 🟡 | Limited scalar reads for `@@autocommit` expose MyLite's fixed enabled statement-atomicity baseline `1`; no `SET`, mutable autocommit state, explicit transactions, session-state tracking, or protocol status flags. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [runtime session state](docs/compatibility/runtime-session-sql-modes.md), [SQL transactions](docs/compatibility/sql-transactions.md) |
| SQL quote SHOW CREATE system variable | 🟡 | Limited scalar reads for `@@sql_quote_show_create` expose MyLite's fixed enabled quoted-SHOW-CREATE baseline `1`; no `SET`, mutable quote-control state, disabled rendering, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| Foreign key checks system variable | 🟡 | Limited scalar reads for `@@foreign_key_checks` expose MyLite's fixed enabled foreign-key-checking baseline `1`; no `SET`, mutable checking state, foreign key DDL, enforcement, cascades, metadata, or dependency checks. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Unique checks system variable | 🟡 | Limited scalar reads for `@@unique_checks` expose MyLite's fixed enabled unique-checking baseline `1`; no `SET`, mutable checking state, unique index DDL, duplicate-key enforcement, index metadata, optimizer effects, or import optimizations. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Updatable views with LIMIT system variable | 🟡 | Limited scalar reads for `@@updatable_views_with_limit` expose MyLite's fixed enabled view-updatability baseline `YES`; no `SET`, mutable checking state, view DDL, view metadata, view DML, check options, or privileges. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL views](docs/compatibility/sql-views.md) |
| SQL auto IS NULL system variable | 🟡 | Limited scalar reads for `@@sql_auto_is_null` expose MyLite's fixed disabled auto-increment lookup baseline `0`; no `SET`, mutable state, `AUTO_INCREMENT`, `LAST_INSERT_ID()`, or changed `IS NULL` predicate behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| SQL big selects system variable | 🟡 | Limited scalar reads for `@@sql_big_selects` expose MyLite's fixed enabled big-select baseline `1`; no `SET`, mutable state, `max_join_size`, optimizer row-estimate aborts, or changed `SELECT` behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| SQL buffer result system variable | 🟡 | Limited scalar reads for `@@sql_buffer_result` expose MyLite's fixed disabled result-buffering baseline `0`; no `SET`, mutable result-buffering state, temporary result tables, lock-release behavior, optimizer effects, or changed `SELECT` behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| SQL generate invisible primary key system variable | 🟡 | Limited scalar reads for `@@sql_generate_invisible_primary_key` expose MyLite's fixed disabled GIPK baseline `0`; no `SET`, mutable state, hidden `my_row_id` columns, generated primary keys, or changed `CREATE TABLE` behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| SQL log bin system variable | 🟡 | Limited scalar reads for session-only `@@sql_log_bin` expose MyLite's fixed enabled binary-logging baseline `1`; no `SET`, mutable session state, global scope, binary log writes, GTID behavior, or replication side effects. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL replication](docs/compatibility/sql-replication.md) |
| SQL log off system variable | 🟡 | Limited scalar reads for `@@sql_log_off` expose MyLite's fixed disabled general-query-log suppression baseline `0`; no `SET`, mutable state, general query log files, `mysql.general_log`, privileges, or logging side effects. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [mysql schema](docs/compatibility/metadata-mysql-schema.md) |
| SQL mode system variable | 🟡 | Limited scalar reads for `@@sql_mode` expose MySQL 8.4.9's default SQL-mode string; no `SET`, mutable mode state, mode-dependent parsing, mode-dependent coercion, or changed statement behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| SQL require primary key system variable | 🟡 | Limited scalar reads for `@@sql_require_primary_key` expose MyLite's fixed disabled primary-key-requirement baseline `0`; no `SET`, mutable state, primary-key constraints, DDL enforcement, replication policy, or privilege semantics. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL table DDL](docs/compatibility/sql-table-ddl.md), [SQL replication](docs/compatibility/sql-replication.md) |
| SQL replica skip counter system variable | 🟡 | Limited scalar reads for global-only `@@sql_replica_skip_counter` expose MyLite's fixed replica-event-skip counter baseline `0`; no `SET`, mutable state, replica SQL thread control, relay log/event skipping, channels, GTID restrictions, or replication side effects. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL replication](docs/compatibility/sql-replication.md) |
| SQL slave skip counter system variable | 🟡 | Limited scalar reads for deprecated global-only `@@sql_slave_skip_counter` expose the fixed replica-event-skip counter baseline `0` and emit MySQL-compatible deprecation warnings; no `SET`, mutable state, replica SQL thread control, relay log/event skipping, channels, GTID restrictions, or replication side effects. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL replication](docs/compatibility/sql-replication.md) |
| SQL safe updates system variable | 🟡 | Limited scalar reads for `@@sql_safe_updates` expose MyLite's fixed disabled safe-updates baseline `0`; no `SET`, mutable safe-updates state, `sql_select_limit`, `max_join_size`, key-aware DML checks, or changed `UPDATE`/`DELETE` behavior. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL table DML](docs/compatibility/sql-table-dml.md) |
| SQL select limit system variable | 🟡 | Limited scalar reads for `@@sql_select_limit` expose MyLite's fixed no-limit baseline `18446744073709551615`; no `SET`, mutable global/session state, implicit row caps for descriptor-backed `SELECT`, safe-updates initialization, or `SHOW VARIABLES`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| SQL notes system variable | 🟡 | Limited scalar reads for `@@sql_notes` expose MyLite's fixed enabled note-recording baseline `1`; note-level diagnostics are currently produced only for limited table-existence DDL no-ops; no `SET`, mutable note state, note suppression, or `max_error_count`. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| SQL warnings system variable | 🟡 | Limited scalar reads for `@@sql_warnings` expose MyLite's fixed disabled single-row insert warning-reporting baseline `0`; no `SET`, mutable warning state, warning-producing DML conversions, `INSERT IGNORE`, strict warning demotion, or protocol information strings. | [runtime system variables](docs/compatibility/runtime-system-variables.md), [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md), [SQL table DML](docs/compatibility/sql-table-dml.md) |
| SQLSTATE values | ❌ | MySQL-compatible SQLSTATEs. | [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| Error numbers | ❌ | MySQL-compatible error codes. | [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| Warning numbers | ❌ | MySQL-compatible warning codes. | [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| Warning count and order | ❌ | Diagnostics area behavior. | [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md), [runtime session state](docs/compatibility/runtime-session-sql-modes.md) |
| Column flags | ❌ | Protocol and result flag bits. | [wire protocol](docs/compatibility/wire-protocol.md), [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| Result column metadata | ❌ | Names, types, lengths, charset, decimals. | [wire protocol](docs/compatibility/wire-protocol.md), [error, warning, and result semantics](docs/compatibility/error-warning-result-semantics.md) |
| Origin metadata | ❌ | Schema, table, and origin names. | [wire protocol](docs/compatibility/wire-protocol.md), [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| InnoDB-only engine surface | 🟡 | Limited embedded InnoDB-compatible surface: `CREATE TABLE ... ENGINE [=] InnoDB` is accepted for the current persistent base-table subset, `SHOW CREATE TABLE` emits the fixed InnoDB suffix, `SHOW [STORAGE] ENGINES` exposes a single InnoDB default row, and `@@default_storage_engine` reports `InnoDB`; no alternate engines, plugins, `INFORMATION_SCHEMA.ENGINES`, or engine internals. | [embedded-design decisions](docs/compatibility/embedded-design-decisions.md), [mysql schema](docs/compatibility/metadata-mysql-schema.md), [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |

### Schema Objects

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| Base tables | 🟡 | Persistent base-table descriptors and physical tables for the limited create/drop/rename and integer/null row lifecycle subsets only. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Temporary tables | ❌ | Session-scoped tables and name shadowing. | [SQL table DDL](docs/compatibility/sql-table-ddl.md), [runtime session state](docs/compatibility/runtime-session-sql-modes.md) |
| Views | ❌ | View DDL, metadata, and query behavior. | [SQL views](docs/compatibility/sql-views.md) |

### Table DDL

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `CREATE TABLE` | 🟡 | Limited persistent base-table creation with optional `IF NOT EXISTS`, explicit `INT`/`INTEGER`/`BIGINT` columns, optional `UNSIGNED`, `NULL`/`NOT NULL`, optional explicit `ENGINE [=] InnoDB`, and optional fixed default `utf8mb4` / `utf8mb4_0900_ai_ci` table charset/collation options; existing-table `IF NOT EXISTS` is a no-op with a MySQL-compatible note; no other options, keys, defaults, or temporary tables. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `CREATE TEMPORARY TABLE` | ❌ | Session-scoped creation. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `CREATE TABLE ... LIKE` | ❌ | Metadata cloning. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `CREATE TABLE ... SELECT` | ❌ | CTAS inference and atomicity. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Column definition grammar | 🟡 | Limited integer column descriptors and nullability only. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Default expressions | ❌ | Literal and expression defaults. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Table engine options | 🟡 | Optional explicit `ENGINE [=] InnoDB` only for the limited persistent `CREATE TABLE` subset; non-InnoDB engines are rejected with MyLite's embedded InnoDB-only diagnostic. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Table charset and collation options | 🟡 | Optional fixed default `CHARSET` / `CHARACTER SET utf8mb4` and `COLLATE utf8mb4_0900_ai_ci` options only for the limited persistent `CREATE TABLE` subset, with matching static `SHOW CHARACTER SET` / `SHOW COLLATION` rows; no non-default charsets/collations, descriptor metadata, string semantics, or full charset/collation catalogs. | [SQL table DDL](docs/compatibility/sql-table-ddl.md), [collations](docs/compatibility/collations.md) |
| `ADD COLUMN` | 🟡 | Limited append-only persistent base-table `ALTER TABLE ... ADD [COLUMN]` for one integer-family column with optional nullability; existing nullable rows backfill `NULL`, existing `NOT NULL` integer rows backfill `0`; no defaults, positioning, multiple actions, non-integer types, keys, constraints, temporary tables, views, algorithms, locks, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `DROP COLUMN` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... DROP [COLUMN] column_name` removes one descriptor-backed column and compacts remaining column ordinals; no multi-action ALTER, dependency checks, keys, constraints, algorithms, locks, temporary tables, views, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `RENAME COLUMN` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME COLUMN old_col TO new_col`; preserves column id, ordinal position, integer type, nullability, and row values; supports exact same-name no-op and case-only spelling changes; no multiple actions, table-qualified column names, type changes, dependency updates, temporary tables, views, algorithms, locks, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `CHANGE COLUMN` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... CHANGE [COLUMN] old_col new_col integer_type [NULL\|NOT NULL]`; preserves column id and ordinal, supports integer-family name/type/nullability replacement and case-only spelling changes, treats omitted nullability as nullable, validates existing integer/`NULL` rows, rebuilds the physical table when needed, and reports MySQL-compatible affected-row counts for the tested subset; rebuilds currently require at least one unshadowed SQLite rowid alias; no defaults, positioning, non-integer types, multiple actions, table-qualified column names, algorithms, locks, temporary tables, views, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `MODIFY COLUMN` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... MODIFY [COLUMN] column_name integer_type [NULL\|NOT NULL]`; preserves column id and ordinal, supports integer-family type/nullability replacement and case-only spelling changes, validates existing integer/`NULL` rows, rebuilds the physical table when needed, and reports MySQL-compatible affected-row counts for the tested subset; rebuilds currently require at least one unshadowed SQLite rowid alias; no defaults, positioning, non-integer types, multiple actions, table-qualified column names, algorithms, locks, temporary tables, views, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `ALTER COLUMN SET DEFAULT` | ❌ | Default mutation. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `ALTER COLUMN DROP DEFAULT` | ❌ | Default removal. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `RENAME TO` | 🟡 | Limited single-action persistent base-table `ALTER TABLE ... RENAME [TO\|AS]` with unqualified, schema-qualified, and cross-schema names; no combined `ALTER TABLE` actions, temporary tables, views, triggers, options, locks, algorithms, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `ORDER BY` | ❌ | Accepted table-rebuild syntax. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `DEFAULT CHARACTER SET` / `COLLATE` | ❌ | Table default charset and collation changes. | [SQL table DDL](docs/compatibility/sql-table-ddl.md), [collations](docs/compatibility/collations.md) |
| `TRUNCATE TABLE` | 🟡 | Limited persistent base-table `TRUNCATE [TABLE] table_name` empties descriptor-backed physical rows and reports zero affected rows; no implicit commits, temporary tables, partitions, foreign keys, triggers, auto-increment reset, locks, privileges, or storage rebuild semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `DROP TABLE` | 🟡 | Limited single persistent base-table drop with optional `IF EXISTS`; missing-table and missing-explicit-schema `IF EXISTS` forms are no-ops with MySQL-compatible notes; no `TEMPORARY`, multi-table drop, `RESTRICT`, or `CASCADE`. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `RENAME TABLE` | 🟡 | Limited single-pair persistent base-table rename with unqualified, schema-qualified, and cross-schema names; no multi-table rename, temporary tables, views, triggers, or privilege semantics. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Atomic DDL | 🟡 | Catalog descriptors and generated physical SQLite table changes commit or roll back atomically for the limited create/drop/rename/truncate and single-action alter rename/add-column/drop-column/rename-column/modify-column/change-column subsets only. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| Implicit commit boundaries | ❌ | MySQL DDL commit behavior. | [SQL table DDL](docs/compatibility/sql-table-ddl.md), [SQL transactions](docs/compatibility/sql-transactions.md) |

### Auto Increment

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `AUTO_INCREMENT` column DDL | ❌ | Creation and alteration. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `AUTO_INCREMENT` allocation | ❌ | Insert ids and sequence behavior. | [SQL table DML](docs/compatibility/sql-table-dml.md), [system functions](docs/compatibility/functions-system.md) |
| `AUTO_INCREMENT` table option | ❌ | Counter initialization and alteration. | [SQL table DDL](docs/compatibility/sql-table-ddl.md) |
| `AUTO_INCREMENT` in `SHOW CREATE TABLE` | ❌ | MySQL-style DDL output. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `AUTO_INCREMENT` in `SHOW TABLE STATUS` | ❌ | Counter metadata. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `AUTO_INCREMENT` in `INFORMATION_SCHEMA` | ❌ | Table and column metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |

### Indexes, Constraints, and Foreign Keys

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `CREATE INDEX` | ❌ | Standalone index creation. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `DROP INDEX` | ❌ | Standalone index removal. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Primary keys | ❌ | Definitions, metadata, errors. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Unique indexes | ❌ | NULLs, prefixes, functional parts. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Nonunique indexes | ❌ | BTREE/HASH clauses and metadata. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Descending indexes | ❌ | DESC key-part syntax. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Prefix indexes | ❌ | Prefix length semantics. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Functional key parts | ❌ | Expression key parts. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Multi-valued indexes | ❌ | JSON array indexes. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| CHECK constraints | ❌ | Enforcement, names, metadata. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Constraint naming | ❌ | Names, scope, `SHOW CREATE`. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ADD PRIMARY KEY` | ❌ | Add and validate primary keys. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `DROP PRIMARY KEY` | ❌ | Remove primary keys. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ADD UNIQUE` | ❌ | Add unique indexes. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ADD INDEX` / `ADD KEY` | ❌ | Add secondary indexes. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `DROP INDEX` / `DROP KEY` | ❌ | Remove table indexes. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `RENAME INDEX` / `RENAME KEY` | ❌ | Rename indexes. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ALTER INDEX VISIBLE` / `INVISIBLE` | ❌ | Index visibility metadata. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ADD CONSTRAINT CHECK` | ❌ | Add check constraints. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `DROP CHECK` | ❌ | Remove check constraints. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `ALTER CHECK ENFORCED` / `NOT ENFORCED` | ❌ | Toggle check enforcement. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| Foreign keys | ❌ | Definitions, actions, metadata. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md), [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `ADD CONSTRAINT FOREIGN KEY` | ❌ | Add and validate foreign keys. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |
| `DROP FOREIGN KEY` | ❌ | Remove foreign keys. | [SQL indexes and constraints](docs/compatibility/sql-indexes-constraints.md) |

### Types and Literals

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `TINYINT` | ❌ | Ranges, display width, metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `SMALLINT` | ❌ | Ranges, display width, metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `MEDIUMINT` | ❌ | Ranges, display width, metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `INT` / `INTEGER` | 🟡 | Limited DDL descriptors, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT` and `UPDATE` assignment, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion, and single-column sort support; no general expression semantics, display width, or protocol-grade result metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `BIGINT` | 🟡 | Limited DDL descriptors, `ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` descriptor replacement, integer/`NULL` `INSERT` and `UPDATE` assignment, text readback, descriptor-driven filtered `SELECT`/`DELETE`/`UPDATE` predicate conversion, and single-column sort support; `BIGINT UNSIGNED` is capped at the signed 64-bit SQLite integer range in this slice. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| Integer type aliases | ❌ | Alias rewrites and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `DECIMAL` / `NUMERIC` | ❌ | Exact math and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `FIXED` | ❌ | Alias rewrites and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `FLOAT` | ❌ | Approximate numeric metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `DOUBLE` / `REAL` | ❌ | Approximate numeric metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `FLOAT4` / `FLOAT8` | ❌ | Alias rewrites and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `BIT` | ❌ | Bit storage and conversion. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `BOOL` / `BOOLEAN` | ❌ | Alias and truth semantics. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `SERIAL` | ❌ | BIGINT AUTO_INCREMENT alias. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `DATE` | ❌ | Date range and formatting. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `TIME` | ❌ | Time range and formatting. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `DATETIME` | ❌ | Datetime range and defaults. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `TIMESTAMP` | ❌ | UTC conversion and defaults. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `YEAR` | ❌ | Year storage and display. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `CHAR` | ❌ | Padding, charsets, metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `VARCHAR` | ❌ | Length, charsets, metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `CHARACTER` / `CHARACTER VARYING` | ❌ | CHAR/VARCHAR aliases. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `NCHAR` / `NATIONAL CHAR` | ❌ | National-character aliases. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `NVARCHAR` / `NATIONAL VARCHAR` | ❌ | National-character aliases. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `BINARY` | ❌ | Fixed-length binary semantics. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `VARBINARY` | ❌ | Variable binary semantics. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `CHAR BYTE` | ❌ | Alias for `BINARY`. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `TINYBLOB` | ❌ | BLOB length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `BLOB` | ❌ | BLOB length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `MEDIUMBLOB` | ❌ | BLOB length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `LONGBLOB` | ❌ | BLOB length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `TINYTEXT` | ❌ | TEXT length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `TEXT` | ❌ | TEXT length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `MEDIUMTEXT` | ❌ | TEXT length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `LONGTEXT` | ❌ | TEXT length and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `LONG` / `LONG VARCHAR` | ❌ | Alias rewrites and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `LONG VARBINARY` | ❌ | Alias rewrites and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `ENUM` | ❌ | Indexing, sorting, invalid values. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `SET` | ❌ | Bitmap membership metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `JSON` | ❌ | Validation and metadata. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md), [JSON functions and operators](docs/compatibility/functions-json.md) |
| Numeric literals | 🟡 | Decimal integer literals with optional unary sign only as supported `INSERT ... VALUES`, `INSERT ... SET`, and single-table `UPDATE` assignment inputs plus supported filtered `SELECT`/`DELETE`/`UPDATE` predicate right operands; unsigned decimal integer literals for supported `SELECT` `LIMIT`/`OFFSET` and `DELETE`/`UPDATE LIMIT`; no expression-level numeric semantics. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| String literals | ❌ | Escapes, introducers, SQL modes. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| Temporal literals | ❌ | DATE/TIME/TIMESTAMP syntax. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| JSON path literals | ❌ | Path grammar and errors. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md), [JSON functions and operators](docs/compatibility/functions-json.md) |
| Type conversion | 🟡 | Limited strict assignment conversion for inserted and updated integer/`NULL` values, descriptor-driven integer predicate conversion for `SELECT`/`DELETE`/`UPDATE`, and unsigned signed-64 range conversion for supported `SELECT` `LIMIT`/`OFFSET` plus `DELETE`/`UPDATE LIMIT` literals only. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| Collation coercibility | ❌ | Coercibility and diagnostics. | [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md), [collations](docs/compatibility/collations.md) |

### Character Sets, Collations, and SET

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `ascii` character set | ❌ | Baseline character set. | [character sets](docs/compatibility/character-sets.md) |
| `binary` character set | 🟡 | Limited scalar `@@character_set_filesystem` placeholder value only; no column, literal, conversion, collation, metadata, file-name conversion, or general charset semantics. | [character sets](docs/compatibility/character-sets.md), [runtime system variables](docs/compatibility/runtime-system-variables.md) |
| `utf8mb3` character set | 🟡 | Limited scalar `@@character_set_system` placeholder value only; no column, literal, conversion, collation, metadata, or general charset semantics. | [character sets](docs/compatibility/character-sets.md), [runtime system variables](docs/compatibility/runtime-system-variables.md) |
| `utf8mb4` character set | 🟡 | Limited static `SHOW CHARACTER SET` row, fixed `CREATE TABLE` option acceptance, scalar connection charset variable reads, scalar `@@character_set_server`, and scalar `@@character_set_database` reads only; no conversions, string types, mutable connection/server/database charset state, or full catalog semantics. | [character sets](docs/compatibility/character-sets.md) |
| `ascii_general_ci` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `ascii_bin` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `binary` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `utf8mb4_general_ci` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `utf8mb4_bin` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `utf8mb4_unicode_ci` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `utf8mb4_0900_ai_ci` collation | 🟡 | Limited static `SHOW COLLATION` row, fixed `CREATE TABLE` option acceptance, scalar `@@collation_connection`, scalar `@@collation_server`, and scalar `@@collation_database` reads only; no comparison semantics, coercibility, mutable connection/server/database collation state, string metadata, or full catalog semantics. | [collations](docs/compatibility/collations.md) |
| `utf8mb4_0900_bin` collation | ❌ | Baseline collation. | [collations](docs/compatibility/collations.md) |
| `SET` | ❌ | Session variable assignment. | [SQL SET statements](docs/compatibility/sql-set-statements.md), [runtime system variables](docs/compatibility/runtime-system-variables.md) |
| `SET NAMES` | ❌ | Client character-set state. | [SQL SET statements](docs/compatibility/sql-set-statements.md), [character sets](docs/compatibility/character-sets.md) |
| `SET CHARACTER SET` | ❌ | Character-set state reset. | [SQL SET statements](docs/compatibility/sql-set-statements.md), [character sets](docs/compatibility/character-sets.md) |

### SQL Modes

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| MySQL default SQL modes | 🟡 | Limited scalar `@@sql_mode` reads expose the default 8.4.9 mode set; no mutable mode state or mode effects. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md), [runtime system variables](docs/compatibility/runtime-system-variables.md) |
| `NO_AUTO_VALUE_ON_ZERO` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `NO_BACKSLASH_ESCAPES` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `NO_ZERO_DATE` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `NO_ZERO_IN_DATE` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `ONLY_FULL_GROUP_BY` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `PIPES_AS_CONCAT` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `STRICT_ALL_TABLES` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `STRICT_TRANS_TABLES` | ❌ | On and off behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `ANSI` | ❌ | Composite mode behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |
| `ANSI_QUOTES` | ❌ | Identifier quoting behavior. | [runtime session state and SQL modes](docs/compatibility/runtime-session-sql-modes.md) |

### Operators

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `&` | ❌ | Bitwise AND. | [operators](docs/compatibility/operators.md) |
| `>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support. | [operators](docs/compatibility/operators.md) |
| `>>` | ❌ | Right shift. | [operators](docs/compatibility/operators.md) |
| `>=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support. | [operators](docs/compatibility/operators.md) |
| `<` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support. | [operators](docs/compatibility/operators.md) |
| `<>, !=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support. | [operators](docs/compatibility/operators.md) |
| `<<` | ❌ | Left shift. | [operators](docs/compatibility/operators.md) |
| `<=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support. | [operators](docs/compatibility/operators.md) |
| `<=>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` integer right operands only. | [operators](docs/compatibility/operators.md) |
| `%, MOD` | ❌ | Modulo. | [operators](docs/compatibility/operators.md), [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `*` | ❌ | Multiplication. | [operators](docs/compatibility/operators.md) |
| `+` | ❌ | Addition. | [operators](docs/compatibility/operators.md) |
| `-` (binary) | ❌ | Subtraction. | [operators](docs/compatibility/operators.md) |
| `-` (unary) | ❌ | Sign negation. | [operators](docs/compatibility/operators.md) |
| `/` | ❌ | Division. | [operators](docs/compatibility/operators.md) |
| `:=` | ❌ | Assignment expression. | [operators](docs/compatibility/operators.md), [SQL SET statements](docs/compatibility/sql-set-statements.md) |
| `=` (assignment) | 🟡 | One unqualified descriptor-column single-table `UPDATE` assignment to a supported decimal integer literal or `NULL`; `SET` statements and expression assignments remain unsupported. | [operators](docs/compatibility/operators.md), [SQL SET statements](docs/compatibility/sql-set-statements.md), [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `=` (comparison) | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` integer right operands only. | [operators](docs/compatibility/operators.md) |
| `^` | ❌ | Bitwise XOR. | [operators](docs/compatibility/operators.md) |
| `AND`, `&&` | ❌ | Logical AND. | [operators](docs/compatibility/operators.md) |
| `BETWEEN ... AND ...` | ❌ | Range test. | [operators](docs/compatibility/operators.md) |
| `BINARY` | ❌ | Binary-string cast. | [operators](docs/compatibility/operators.md), [type system, literals, and conversion](docs/compatibility/type-system-literals-conversion.md) |
| `CASE` | ❌ | Case operator. | [operators](docs/compatibility/operators.md) |
| `DIV` | ❌ | Integer division. | [operators](docs/compatibility/operators.md) |
| `EXISTS()` | ❌ | Query existence test. | [operators](docs/compatibility/operators.md), [SQL subqueries](docs/compatibility/sql-subqueries.md) |
| `IN()` | ❌ | Set membership. | [operators](docs/compatibility/operators.md) |
| `IS` | ❌ | Boolean/null test. | [operators](docs/compatibility/operators.md) |
| `IS NOT` | ❌ | Negated boolean/null test. | [operators](docs/compatibility/operators.md) |
| `IS NOT NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only. | [operators](docs/compatibility/operators.md) |
| `IS NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only. | [operators](docs/compatibility/operators.md) |
| `LIKE` | ❌ | Simple pattern matching. | [operators](docs/compatibility/operators.md) |
| `NOT`, `!` | ❌ | Logical negation. | [operators](docs/compatibility/operators.md) |
| `NOT BETWEEN ... AND ...` | ❌ | Negated range test. | [operators](docs/compatibility/operators.md) |
| `NOT EXISTS()` | ❌ | Negated query existence test. | [operators](docs/compatibility/operators.md), [SQL subqueries](docs/compatibility/sql-subqueries.md) |
| `NOT IN()` | ❌ | Negated set membership. | [operators](docs/compatibility/operators.md) |
| `NOT LIKE` | ❌ | Negated pattern matching. | [operators](docs/compatibility/operators.md) |
| `NOT REGEXP` | ❌ | Negated regular expression match. | [operators](docs/compatibility/operators.md), [string functions](docs/compatibility/functions-string.md) |
| `OR`, `\|\|` | ❌ | Logical OR. | [operators](docs/compatibility/operators.md) |
| `REGEXP` | ❌ | Regular expression match. | [operators](docs/compatibility/operators.md), [string functions](docs/compatibility/functions-string.md) |
| `RLIKE` | ❌ | Regular expression match. | [operators](docs/compatibility/operators.md), [string functions](docs/compatibility/functions-string.md) |
| `XOR` | ❌ | Logical XOR. | [operators](docs/compatibility/operators.md) |
| `\|` | ❌ | Bitwise OR. | [operators](docs/compatibility/operators.md) |
| `~` | ❌ | Bitwise inversion. | [operators](docs/compatibility/operators.md) |

### Query Expressions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `SELECT` | 🟡 | Descriptor-driven single persistent base-table `SELECT *` or unqualified column-list reads with optional limited `WHERE`, single-column `ORDER BY`, and `LIMIT`/`OFFSET`, plus limited one-item `COUNT(*)`; no joins, grouping, aliases, general expression projection, or locking clauses. | [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| Projection list | 🟡 | Wildcard uses catalog ordinal order; explicit projections resolve unqualified descriptor column names only; limited one-item `COUNT(*)` is supported; duplicate projected columns are allowed, with no aliases or table-qualified references. | [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| `WHERE` | 🟡 | One unqualified descriptor column predicate on supported integer/`NULL` columns for filtered table `SELECT`, `DELETE`, and `UPDATE`; no boolean composition, literal-left comparisons, table-qualified columns, or general expression predicates. | [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| `ORDER BY` | 🟡 | One unqualified descriptor column for supported base-table `SELECT`, `DELETE`, and `UPDATE`; optional `ASC`/`DESC`, MySQL-compatible `NULL` placement, and no tie-order guarantee without additional keys. | [SQL query expressions](docs/compatibility/sql-query-expressions.md) |
| `LIMIT` / `OFFSET` | 🟡 | Supported base-table `SELECT` forms are `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count`; supported single-table `DELETE`/`UPDATE` admits `LIMIT row_count` only, with unsigned decimal literals in signed 64-bit range. | [SQL query expressions](docs/compatibility/sql-query-expressions.md) |

### Table DML

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `DELETE` (single-table) | 🟡 | Limited persistent base-table `DELETE FROM table` with optional baseline `WHERE`, one unqualified descriptor `ORDER BY` column, and signed-64-range `LIMIT row_count`; no aliases, partitions, modifiers, joined deletes, full ordering, or offset forms. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `DELETE` (multi-table) | ❌ | Multi-table forms. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `DELETE` with joins | ❌ | Joined delete target semantics. | [SQL table DML](docs/compatibility/sql-table-dml.md), [SQL joins](docs/compatibility/sql-joins.md) |
| `INSERT ... VALUES` | 🟡 | Limited single- and multi-row inserts into persistent base tables with integer/`NULL` values, descriptor column resolution, strict range checks, affected rows, and statement atomicity; no defaults, keys, generated values, or insert ids. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `INSERT ... SET` | 🟡 | Limited one-row `INSERT [INTO] table SET column = integer_or_NULL[, ...]` into persistent base tables with descriptor assignment resolution, omitted nullable columns stored as `NULL`, strict required-column/range/nullability diagnostics, affected rows, and statement atomicity; no modifiers, qualified assignment targets, expressions, defaults, generated values, or insert ids. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `INSERT ... SELECT` | ❌ | Query insert and metadata inference. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `INSERT ... ON DUPLICATE KEY UPDATE` | ❌ | Duplicate-key update semantics. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `INSERT IGNORE` | ❌ | Warning demotion rules. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `REPLACE ... VALUES` | ❌ | Delete-insert semantics. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `REPLACE ... SET` | ❌ | SET-form replace. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `REPLACE ... SELECT` | ❌ | Replace from query expression. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `UPDATE` (single-table) | 🟡 | Limited persistent base-table `UPDATE table SET column = integer_or_NULL` with optional baseline `WHERE`, one unqualified descriptor `ORDER BY` column, and signed-64-range `LIMIT row_count`; changed-row affected counts; no aliases, partitions, modifiers, joins, multiple assignments, expression assignments, full ordering, or offset forms. | [SQL table DML](docs/compatibility/sql-table-dml.md) |
| `UPDATE` (multi-table) | ❌ | Joined update semantics. | [SQL table DML](docs/compatibility/sql-table-dml.md), [SQL joins](docs/compatibility/sql-joins.md) |
| `UPDATE` with joins | ❌ | Joined target and assignment behavior. | [SQL table DML](docs/compatibility/sql-table-dml.md), [SQL joins](docs/compatibility/sql-joins.md) |

### Aggregate Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `AVG()` | ❌ | Average value. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `COUNT()` | 🟡 | Limited `COUNT(*)` in one-item `SELECT` with no source, `FROM DUAL`, or one descriptor-backed persistent base table with optional baseline `WHERE`; no `COUNT(expr)`, `DISTINCT`, grouping, aliases, ordering, limiting, or window forms. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `COUNT(DISTINCT)` | ❌ | Count distinct values. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `GROUP_CONCAT()` | ❌ | Concatenated aggregate string. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `MAX()` | ❌ | Maximum value. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `MIN()` | ❌ | Minimum value. | [aggregate functions](docs/compatibility/functions-aggregate.md) |
| `SUM()` | ❌ | Sum. | [aggregate functions](docs/compatibility/functions-aggregate.md) |

### Cast, Control-Flow, Comparison, and Default-Value Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `CAST()` | ❌ | Explicit conversion. | [cast functions](docs/compatibility/functions-casts.md) |
| `COALESCE()` | ❌ | First non-NULL argument. | [control-flow functions](docs/compatibility/functions-control-flow.md) |
| `CONVERT()` | ❌ | Explicit conversion. | [cast functions](docs/compatibility/functions-casts.md) |
| `GREATEST()` | ❌ | Largest argument. | [comparison functions](docs/compatibility/functions-comparison.md) |
| `IF()` | ❌ | If/else expression. | [control-flow functions](docs/compatibility/functions-control-flow.md) |
| `IFNULL()` | ❌ | NULL fallback expression. | [control-flow functions](docs/compatibility/functions-control-flow.md) |
| `INTERVAL()` | ❌ | Argument interval index. | [comparison functions](docs/compatibility/functions-comparison.md) |
| `ISNULL()` | ❌ | NULL test function. | [comparison functions](docs/compatibility/functions-comparison.md) |
| `LEAST()` | ❌ | Smallest argument. | [comparison functions](docs/compatibility/functions-comparison.md) |
| `NULLIF()` | ❌ | NULL if arguments compare equal. | [control-flow functions](docs/compatibility/functions-control-flow.md) |
| `VALUES()` | ❌ | INSERT value reference. | [default-value functions](docs/compatibility/functions-default-values.md) |

### JSON Functions and Operators

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `->` | ❌ | JSON path extraction. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `->>` | ❌ | JSON path extraction and unquote. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_ARRAY()` | ❌ | Create JSON array. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_CONTAINS()` | ❌ | JSON containment test. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_CONTAINS_PATH()` | ❌ | JSON path existence test. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_EXTRACT()` | ❌ | Extract from JSON document. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_INSERT()` | ❌ | Insert JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_KEYS()` | ❌ | Object key list. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_LENGTH()` | ❌ | JSON element count. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_OBJECT()` | ❌ | Create JSON object. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_QUOTE()` | ❌ | Quote JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_REMOVE()` | ❌ | Remove JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_REPLACE()` | ❌ | Replace JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_SET()` | ❌ | Set JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_TYPE()` | ❌ | JSON value type. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_UNQUOTE()` | ❌ | Unquote JSON value. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_VALID()` | ❌ | JSON validity check. | [JSON functions and operators](docs/compatibility/functions-json.md) |
| `JSON_VALUE()` | ❌ | JSON path value extraction. | [JSON functions and operators](docs/compatibility/functions-json.md) |

### Numeric and Math Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `ABS()` | ❌ | Absolute value. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `ACOS()` | ❌ | Arc cosine. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `ASIN()` | ❌ | Arc sine. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `ATAN()` | ❌ | Arc tangent. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `ATAN2(), ATAN()` | ❌ | Two-argument arc tangent. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `BIN()` | ❌ | Binary string for number. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `BIT_COUNT()` | ❌ | Count set bits. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `CEIL()` | ❌ | Ceiling. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `CEILING()` | ❌ | Ceiling synonym. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `CONV()` | ❌ | Convert number base. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `COS()` | ❌ | Cosine. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `COT()` | ❌ | Cotangent. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `CRC32()` | ❌ | CRC32 checksum. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `DEGREES()` | ❌ | Radians to degrees. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `EXP()` | ❌ | Exponential. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `FLOOR()` | ❌ | Floor. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `FORMAT()` | ❌ | Localized decimal formatting. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `LN()` | ❌ | Natural logarithm. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `LOG()` | ❌ | Logarithm. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `LOG10()` | ❌ | Base-10 logarithm. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `LOG2()` | ❌ | Base-2 logarithm. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `MOD()` | ❌ | Remainder. | [numeric and math functions](docs/compatibility/functions-numeric-math.md), [operators](docs/compatibility/operators.md) |
| `OCT()` | ❌ | Octal string for number. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `PI()` | ❌ | Pi constant. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `POW()` | ❌ | Power. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `POWER()` | ❌ | Power synonym. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `RADIANS()` | ❌ | Degrees to radians. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `RAND()` | ❌ | Random value. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `ROUND()` | ❌ | Round value. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `SIGN()` | ❌ | Sign of argument. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `SIN()` | ❌ | Sine. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `SQRT()` | ❌ | Square root. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `TAN()` | ❌ | Tangent. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |
| `TRUNCATE()` | ❌ | Decimal truncation. | [numeric and math functions](docs/compatibility/functions-numeric-math.md) |

### String and Regular Expression Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `ASCII()` | ❌ | Left-most character code. | [string functions](docs/compatibility/functions-string.md) |
| `BIT_LENGTH()` | ❌ | Length in bits. | [string functions](docs/compatibility/functions-string.md) |
| `CHAR()` | ❌ | Characters from integers. | [string functions](docs/compatibility/functions-string.md) |
| `CHAR_LENGTH()` | ❌ | Character count. | [string functions](docs/compatibility/functions-string.md) |
| `CHARACTER_LENGTH()` | ❌ | Character count synonym. | [string functions](docs/compatibility/functions-string.md) |
| `CHARSET()` | ❌ | Argument character set. | [string functions](docs/compatibility/functions-string.md) |
| `COERCIBILITY()` | ❌ | Collation coercibility. | [string functions](docs/compatibility/functions-string.md), [collations](docs/compatibility/collations.md) |
| `COLLATION()` | ❌ | Argument collation. | [string functions](docs/compatibility/functions-string.md), [collations](docs/compatibility/collations.md) |
| `CONCAT()` | ❌ | Concatenate strings. | [string functions](docs/compatibility/functions-string.md) |
| `CONCAT_WS()` | ❌ | Concatenate with separator. | [string functions](docs/compatibility/functions-string.md) |
| `ELT()` | ❌ | String at index. | [string functions](docs/compatibility/functions-string.md) |
| `EXPORT_SET()` | ❌ | Bitmask-to-string mapping. | [string functions](docs/compatibility/functions-string.md) |
| `FIELD()` | ❌ | Argument position. | [string functions](docs/compatibility/functions-string.md) |
| `FIND_IN_SET()` | ❌ | Comma-list position. | [string functions](docs/compatibility/functions-string.md) |
| `FROM_BASE64()` | ❌ | Base64 decode. | [string functions](docs/compatibility/functions-string.md) |
| `HEX()` | ❌ | Hex representation. | [string functions](docs/compatibility/functions-string.md) |
| `INSERT()` | ❌ | Insert substring. | [string functions](docs/compatibility/functions-string.md) |
| `INSTR()` | ❌ | First substring index. | [string functions](docs/compatibility/functions-string.md) |
| `LCASE()` | ❌ | Lowercase synonym. | [string functions](docs/compatibility/functions-string.md) |
| `LEFT()` | ❌ | Left substring. | [string functions](docs/compatibility/functions-string.md) |
| `LENGTH()` | ❌ | Byte length. | [string functions](docs/compatibility/functions-string.md) |
| `LOCATE()` | ❌ | Substring position. | [string functions](docs/compatibility/functions-string.md) |
| `LOWER()` | ❌ | Lowercase conversion. | [string functions](docs/compatibility/functions-string.md) |
| `LPAD()` | ❌ | Left padding. | [string functions](docs/compatibility/functions-string.md) |
| `LTRIM()` | ❌ | Trim leading spaces. | [string functions](docs/compatibility/functions-string.md) |
| `MAKE_SET()` | ❌ | Bitmask-selected set. | [string functions](docs/compatibility/functions-string.md) |
| `MID()` | ❌ | Substring synonym. | [string functions](docs/compatibility/functions-string.md) |
| `OCTET_LENGTH()` | ❌ | Byte length synonym. | [string functions](docs/compatibility/functions-string.md) |
| `ORD()` | ❌ | Leftmost character code. | [string functions](docs/compatibility/functions-string.md) |
| `POSITION()` | ❌ | LOCATE synonym. | [string functions](docs/compatibility/functions-string.md) |
| `QUOTE()` | ❌ | SQL string quoting. | [string functions](docs/compatibility/functions-string.md) |
| `REGEXP_INSTR()` | ❌ | Regex match index. | [string functions](docs/compatibility/functions-string.md) |
| `REGEXP_LIKE()` | ❌ | Regex match predicate. | [string functions](docs/compatibility/functions-string.md) |
| `REGEXP_REPLACE()` | ❌ | Regex replacement. | [string functions](docs/compatibility/functions-string.md) |
| `REGEXP_SUBSTR()` | ❌ | Regex substring. | [string functions](docs/compatibility/functions-string.md) |
| `REPEAT()` | ❌ | Repeat string. | [string functions](docs/compatibility/functions-string.md) |
| `REPLACE()` | ❌ | Replace substrings. | [string functions](docs/compatibility/functions-string.md) |
| `REVERSE()` | ❌ | Reverse string. | [string functions](docs/compatibility/functions-string.md) |
| `RIGHT()` | ❌ | Right substring. | [string functions](docs/compatibility/functions-string.md) |
| `RPAD()` | ❌ | Right padding. | [string functions](docs/compatibility/functions-string.md) |
| `RTRIM()` | ❌ | Trim trailing spaces. | [string functions](docs/compatibility/functions-string.md) |
| `SOUNDEX()` | ❌ | Soundex string. | [string functions](docs/compatibility/functions-string.md) |
| `SPACE()` | ❌ | Repeated spaces. | [string functions](docs/compatibility/functions-string.md) |
| `STRCMP()` | ❌ | String comparison. | [string functions](docs/compatibility/functions-string.md) |
| `SUBSTR()` | ❌ | Substring synonym. | [string functions](docs/compatibility/functions-string.md) |
| `SUBSTRING()` | ❌ | Substring. | [string functions](docs/compatibility/functions-string.md) |
| `SUBSTRING_INDEX()` | ❌ | Delimiter-count substring. | [string functions](docs/compatibility/functions-string.md) |
| `TO_BASE64()` | ❌ | Base64 encode. | [string functions](docs/compatibility/functions-string.md) |
| `TRIM()` | ❌ | Trim spaces. | [string functions](docs/compatibility/functions-string.md) |
| `UCASE()` | ❌ | Uppercase synonym. | [string functions](docs/compatibility/functions-string.md) |
| `UNHEX()` | ❌ | Hex decode. | [string functions](docs/compatibility/functions-string.md) |
| `UPPER()` | ❌ | Uppercase conversion. | [string functions](docs/compatibility/functions-string.md) |
| `WEIGHT_STRING()` | ❌ | Collation weight string. | [string functions](docs/compatibility/functions-string.md), [collations](docs/compatibility/collations.md) |

### System Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `DATABASE()` | 🟡 | Limited one-row scalar `SELECT DATABASE()` with optional `FROM DUAL`; returns connection-local selected schema or `NULL`. | [system functions](docs/compatibility/functions-system.md) |
| `SCHEMA()` | 🟡 | Limited synonym for `DATABASE()` in the same scalar-select slice. | [system functions](docs/compatibility/functions-system.md) |
| `CONNECTION_ID()` | 🟡 | Limited one-row scalar select returns a MyLite process-local nonzero handle id; no server thread, process-list, Performance Schema, or `pseudo_thread_id` semantics. | [system functions](docs/compatibility/functions-system.md) |
| `CURRENT_ROLE()` | 🟡 | Limited one-row scalar select returns MyLite's no-active-role value `NONE`; no role catalog, role grants, `SET ROLE`, or bare `CURRENT_ROLE`. | [system functions](docs/compatibility/functions-system.md), [SQL users, roles, and privileges](docs/compatibility/sql-users-privileges.md) |
| `FOUND_ROWS()` | ❌ | Rows before LIMIT. | [system functions](docs/compatibility/functions-system.md) |
| `CURRENT_USER()` / `CURRENT_USER` | 🟡 | Limited one-row scalar select returns MyLite's embedded current identity `root@%`; no account or definer semantics. | [system functions](docs/compatibility/functions-system.md) |
| `LAST_INSERT_ID()` | ❌ | Last auto-increment value. | [system functions](docs/compatibility/functions-system.md) |
| `ROW_COUNT()` | 🟡 | Limited one-row scalar select returns connection-local row-count state for supported baseline statements; no protocol OK-packet parity, `CLIENT_FOUND_ROWS`, aliases, table-backed evaluation, or full diagnostics area support. | [system functions](docs/compatibility/functions-system.md) |
| `SESSION_USER()` | 🟡 | Limited no-whitespace one-row scalar select returns MyLite's embedded client identity `root@%`; no `IGNORE_SPACE`, stored-function resolution, authentication, or host matching. | [system functions](docs/compatibility/functions-system.md) |
| `SYSTEM_USER()` | 🟡 | Limited no-whitespace one-row scalar select returns MyLite's embedded client identity `root@%`; no `SYSTEM_USER` privilege semantics, authentication, or host matching. | [system functions](docs/compatibility/functions-system.md) |
| `USER()` | 🟡 | Limited one-row scalar select returns MyLite's embedded client identity `root@%`; no authentication or host matching. | [system functions](docs/compatibility/functions-system.md) |
| `VERSION()` | 🟡 | Limited one-row scalar select returns MyLite's engine version string; no MySQL server-version impersonation. | [system functions](docs/compatibility/functions-system.md) |

### Temporal Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `ADDDATE()` | ❌ | Add interval to date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `ADDTIME()` | ❌ | Add time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CONVERT_TZ()` | ❌ | Convert time zone. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CURDATE()` | ❌ | Current date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CURRENT_DATE()` / `CURRENT_DATE` | ❌ | Current date synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CURRENT_TIME()` / `CURRENT_TIME` | ❌ | Current time synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CURRENT_TIMESTAMP()` / `CURRENT_TIMESTAMP` | ❌ | Current timestamp synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `CURTIME()` | ❌ | Current time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DATE()` | ❌ | Extract date part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DATE_ADD()` | ❌ | Add interval to date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DATE_FORMAT()` | ❌ | Format date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DATE_SUB()` | ❌ | Subtract interval from date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DATEDIFF()` | ❌ | Date difference. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DAY()` | ❌ | Day-of-month synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DAYNAME()` | ❌ | Weekday name. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DAYOFMONTH()` | ❌ | Day of month. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DAYOFWEEK()` | ❌ | Weekday index. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `DAYOFYEAR()` | ❌ | Day of year. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `EXTRACT()` | ❌ | Extract date part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `FROM_DAYS()` | ❌ | Day number to date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `FROM_UNIXTIME()` | ❌ | Unix timestamp to date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `GET_FORMAT()` | ❌ | Date format string. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `HOUR()` | ❌ | Hour part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `LAST_DAY()` | ❌ | Last day of month. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `LOCALTIME()` / `LOCALTIME` | ❌ | Current timestamp synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `LOCALTIMESTAMP()` / `LOCALTIMESTAMP` | ❌ | Current timestamp synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MAKEDATE()` | ❌ | Year plus day-of-year date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MAKETIME()` | ❌ | Time from parts. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MICROSECOND()` | ❌ | Microsecond part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MINUTE()` | ❌ | Minute part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MONTH()` | ❌ | Month part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `MONTHNAME()` | ❌ | Month name. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `NOW()` | ❌ | Current date and time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `PERIOD_ADD()` | ❌ | Add months to period. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `PERIOD_DIFF()` | ❌ | Months between periods. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `QUARTER()` | ❌ | Quarter part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `SEC_TO_TIME()` | ❌ | Seconds to time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `SECOND()` | ❌ | Second part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `STR_TO_DATE()` | ❌ | Parse date string. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `SUBDATE()` | ❌ | Subtract interval synonym. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `SUBTIME()` | ❌ | Subtract time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `SYSDATE()` | ❌ | Execution-time timestamp. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIME()` | ❌ | Extract time part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIME_FORMAT()` | ❌ | Format time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIME_TO_SEC()` | ❌ | Time to seconds. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIMEDIFF()` | ❌ | Time difference. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIMESTAMP()` | ❌ | Timestamp cast or addition. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIMESTAMPADD()` | ❌ | Add timestamp interval. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TIMESTAMPDIFF()` | ❌ | Timestamp difference. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TO_DAYS()` | ❌ | Date to day number. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `TO_SECONDS()` | ❌ | Date/time to seconds. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `UNIX_TIMESTAMP()` | ❌ | Unix timestamp. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `UTC_DATE()` | ❌ | Current UTC date. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `UTC_TIME()` | ❌ | Current UTC time. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `UTC_TIMESTAMP()` | ❌ | Current UTC timestamp. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `WEEK()` | ❌ | Week number. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `WEEKDAY()` | ❌ | Weekday index. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `WEEKOFYEAR()` | ❌ | Calendar week. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `YEAR()` | ❌ | Year part. | [temporal functions](docs/compatibility/functions-temporal.md) |
| `YEARWEEK()` | ❌ | Year and week. | [temporal functions](docs/compatibility/functions-temporal.md) |

### Window Functions

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `CUME_DIST()` | ❌ | Cumulative distribution. | [window functions](docs/compatibility/functions-window.md) |
| `DENSE_RANK()` | ❌ | Rank without gaps. | [window functions](docs/compatibility/functions-window.md) |
| `FIRST_VALUE()` | ❌ | First frame value. | [window functions](docs/compatibility/functions-window.md) |
| `LAG()` | ❌ | Previous-row value. | [window functions](docs/compatibility/functions-window.md) |
| `LAST_VALUE()` | ❌ | Last frame value. | [window functions](docs/compatibility/functions-window.md) |
| `LEAD()` | ❌ | Next-row value. | [window functions](docs/compatibility/functions-window.md) |
| `NTH_VALUE()` | ❌ | N-th frame value. | [window functions](docs/compatibility/functions-window.md) |
| `NTILE()` | ❌ | Bucket number. | [window functions](docs/compatibility/functions-window.md) |
| `PERCENT_RANK()` | ❌ | Percentage rank. | [window functions](docs/compatibility/functions-window.md) |
| `RANK()` | ❌ | Rank with gaps. | [window functions](docs/compatibility/functions-window.md) |
| `ROW_NUMBER()` | ❌ | Row number in partition. | [window functions](docs/compatibility/functions-window.md) |

### Information Schema

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | ❌ | Character set catalog. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | ❌ | Check constraint metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.COLLATIONS` | ❌ | Collation catalog. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.COLUMNS` | ❌ | Column metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | ❌ | Key column metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.KEYWORDS` | ❌ | Keyword catalog. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.SCHEMATA` | ❌ | Schema listing. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.STATISTICS` | ❌ | Index metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | ❌ | Constraint metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.TABLES` | ❌ | Table metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.VIEWS` | ❌ | View metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | ❌ | View dependency metadata. | [INFORMATION_SCHEMA tables](docs/compatibility/metadata-information-schema.md) |

### SHOW and Introspection Statements

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `SHOW CHARACTER SET` / `SHOW CHARSET` | 🟡 | Limited static `utf8mb4` row with MySQL 8.4.9 column labels and `LIKE` filters; no `WHERE`, alternate charsets, privileges, or `INFORMATION_SCHEMA`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md), [character sets](docs/compatibility/character-sets.md) |
| `SHOW COLLATION` | 🟡 | Limited static `utf8mb4_0900_ai_ci` row with MySQL 8.4.9 column labels and `LIKE` filters; no `WHERE`, alternate collations, privileges, or `INFORMATION_SCHEMA`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md), [collations](docs/compatibility/collations.md) |
| `SHOW COUNT(*) ERRORS` | 🟡 | Limited previous-statement error-condition count with MySQL 8.4.9 column label; counts MyLite's previous error condition only. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW COUNT(*) WARNINGS` | 🟡 | Limited previous-statement diagnostics count with MySQL 8.4.9 column label; counts MyLite previous error conditions and warning/note records only. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW COLUMNS` | 🟡 | Limited descriptor-driven column listing for persistent base tables with integer-family descriptors; supports `FROM`/`IN`, schema-qualified targets, explicit schema forms, and `LIKE` filters, but no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, indexes, defaults, or hidden columns. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW FIELDS` | 🟡 | Limited alias for the supported `SHOW COLUMNS` subset. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Limited descriptor-driven schema DDL rendering with MySQL 8.4.9 columns, fixed default charset/collation/encryption text, and fixed enabled `sql_quote_show_create` quoting for current optionless schema descriptors; no `IF NOT EXISTS`, schema options, privileges, system schemas, mutable quote-control state, or disabled rendering. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md), [SQL schemas](docs/compatibility/sql-schemas.md) |
| `SHOW CREATE TABLE` | 🟡 | Limited descriptor-driven MySQL-style DDL for persistent base tables with current integer-family descriptors, nullability, fixed InnoDB/utf8mb4 suffix, and fixed enabled `sql_quote_show_create` quoting; no views, temporary tables, indexes, defaults, constraints, generated columns, auto-increment, privileges, mutable quote-control state, or disabled rendering. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW DATABASES` | 🟡 | Limited descriptor-driven catalog schema listing with `LIKE` filters; no NUL-producing pattern escapes, system schemas, `WHERE`, or privileges. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW ERRORS` | 🟡 | Limited previous-statement error-condition rows with MySQL 8.4.9 columns and unsigned decimal `LIMIT` slicing; reports MyLite's previous error condition only. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW EVENTS` | 🟡 | Limited empty event introspection with MySQL 8.4.9 column labels and `LIKE` filters; unknown explicit schemas are empty successes; no event descriptors, event rows, event DDL, `SHOW CREATE EVENT`, `WHERE`, Event Scheduler, privileges, or `INFORMATION_SCHEMA.EVENTS`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW FUNCTION STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE` filters; global and default-schema independent; no routine descriptors, routine rows, routine DDL, `SHOW CREATE FUNCTION`, `WHERE`, privileges, or `INFORMATION_SCHEMA.ROUTINES`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | 🟡 | Limited descriptor-resolved persistent base-table introspection with MySQL 8.4.9 column labels and zero rows for current no-index tables; no index descriptors, indexed rows, `EXTENDED`, `WHERE`, temporary tables, views, privileges, or statistics. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW OPEN TABLES` | 🟡 | Limited embedded empty open-table introspection with MySQL 8.4.9 column labels and `LIKE` filters; unknown explicit schemas are empty successes; no table-cache rows, lock counts, name-lock state, temporary tables, `HANDLER`, `WHERE`, privileges, or performance-schema metadata. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW PROCEDURE STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE` filters; global and default-schema independent; no routine descriptors, routine rows, routine DDL, `SHOW CREATE PROCEDURE`, `WHERE`, privileges, or `INFORMATION_SCHEMA.ROUTINES`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW PROCESSLIST` | 🟡 | Limited current embedded-handle row for `SHOW [FULL] PROCESSLIST` with MySQL 8.4.9 columns, selected-schema `db`, `Info` truncation, and default process-list warning count; no server-wide threads, privileges, filters, Performance Schema, `INFORMATION_SCHEMA.PROCESSLIST`, or `KILL`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW TABLES` | 🟡 | Limited descriptor-driven table listing with optional `FROM`/`IN` schema and `LIKE` filters; no NUL-producing pattern escapes, `FULL`, `EXTENDED`, `WHERE`, views, privileges, or temporary tables. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW TABLE STATUS` | 🟡 | Limited descriptor-driven persistent base-table status rows with MySQL 8.4.9 column labels, schema forms, `LIKE` filters, exact physical row counts, and deterministic placeholder statistics; no views, temporary tables, `WHERE`, privileges, timestamps, auto-increment metadata, full storage statistics, or `INFORMATION_SCHEMA`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW TRIGGERS` | 🟡 | Limited schema-resolved empty trigger introspection with MySQL 8.4.9 column labels, optional `FULL`, and `LIKE` filters; no trigger descriptors, trigger rows, trigger DDL, `SHOW CREATE TRIGGER`, `WHERE`, privileges, or `INFORMATION_SCHEMA.TRIGGERS`. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `SHOW WARNINGS` | 🟡 | Limited previous-statement diagnostics rows with MySQL 8.4.9 columns and unsigned decimal `LIMIT` slicing; reports MyLite previous error conditions plus warning/note records only. | [SQL SHOW statements](docs/compatibility/sql-show-statements.md) |
| `DESCRIBE` | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset; no column filters or execution-plan `EXPLAIN` behavior. | [SQL utility statements](docs/compatibility/sql-utility-statements.md) |
| `DESC` | 🟡 | Limited alias for the supported table-only `DESCRIBE` subset. | [SQL utility statements](docs/compatibility/sql-utility-statements.md) |
| `EXPLAIN` for tables | 🟡 | Limited table-column introspection alias for the supported `SHOW COLUMNS` subset; supports only `EXPLAIN table_name`, not column filters, wildcard filters, execution plans, `FORMAT`, `ANALYZE`, or `FOR CONNECTION`. | [SQL utility statements](docs/compatibility/sql-utility-statements.md) |

### Transactions, Locking, and Maintenance

| Feature | Status | Notes | Full table |
| --- | :-: | --- | --- |
| `START TRANSACTION` | ❌ | Start transaction. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `BEGIN` | ❌ | Start transaction. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `BEGIN WORK` | ❌ | Start transaction synonym. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `COMMIT` | ❌ | Commit transaction. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `ROLLBACK` | ❌ | Roll back transaction. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `SAVEPOINT` | ❌ | Create or replace savepoint. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `ROLLBACK TO SAVEPOINT` | ❌ | Partial rollback. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `RELEASE SAVEPOINT` | ❌ | Release savepoint. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `SET TRANSACTION` | ❌ | Isolation and access scope. | [SQL transactions](docs/compatibility/sql-transactions.md) |
| `LOCK TABLES` | ❌ | Table lock syntax and behavior. | [SQL locking](docs/compatibility/sql-locking.md) |
| `UNLOCK TABLES` | ❌ | Release table locks. | [SQL locking](docs/compatibility/sql-locking.md) |
| `ANALYZE TABLE` | ❌ | Table analysis result shape. | [SQL table maintenance](docs/compatibility/sql-table-maintenance.md) |
| `CHECK TABLE` | ❌ | Table check result shape. | [SQL table maintenance](docs/compatibility/sql-table-maintenance.md) |
| `OPTIMIZE TABLE` | ❌ | Table optimize result shape. | [SQL table maintenance](docs/compatibility/sql-table-maintenance.md) |
| `REPAIR TABLE` | ❌ | Table repair result shape. | [SQL table maintenance](docs/compatibility/sql-table-maintenance.md) |

## Detailed Compatibility Tables

These tables track the full MySQL compatibility inventory beyond the baseline
target above.

| Surface | Full table |
| --- | --- |
| SQL table DDL | [docs/compatibility/sql-table-ddl.md](docs/compatibility/sql-table-ddl.md) |
| SQL table DML | [docs/compatibility/sql-table-dml.md](docs/compatibility/sql-table-dml.md) |
| SQL table maintenance | [docs/compatibility/sql-table-maintenance.md](docs/compatibility/sql-table-maintenance.md) |
| SQL utility statements | [docs/compatibility/sql-utility-statements.md](docs/compatibility/sql-utility-statements.md) |
| SQL indexes and constraints | [docs/compatibility/sql-indexes-constraints.md](docs/compatibility/sql-indexes-constraints.md) |
| SQL partitioning | [docs/compatibility/sql-partitioning.md](docs/compatibility/sql-partitioning.md) |
| SQL schemas | [docs/compatibility/sql-schemas.md](docs/compatibility/sql-schemas.md) |
| SQL views | [docs/compatibility/sql-views.md](docs/compatibility/sql-views.md) |
| SQL spatial reference systems | [docs/compatibility/sql-spatial-reference-systems.md](docs/compatibility/sql-spatial-reference-systems.md) |
| SQL routines | [docs/compatibility/sql-routines.md](docs/compatibility/sql-routines.md) |
| SQL stored programs | [docs/compatibility/sql-stored-programs.md](docs/compatibility/sql-stored-programs.md) |
| SQL triggers | [docs/compatibility/sql-triggers.md](docs/compatibility/sql-triggers.md) |
| SQL events | [docs/compatibility/sql-events.md](docs/compatibility/sql-events.md) |
| SQL users, roles, and privileges | [docs/compatibility/sql-users-privileges.md](docs/compatibility/sql-users-privileges.md) |
| SQL transactions | [docs/compatibility/sql-transactions.md](docs/compatibility/sql-transactions.md) |
| SQL locking | [docs/compatibility/sql-locking.md](docs/compatibility/sql-locking.md) |
| SQL XA transactions | [docs/compatibility/sql-xa-transactions.md](docs/compatibility/sql-xa-transactions.md) |
| SQL prepared statements | [docs/compatibility/sql-prepared-statements.md](docs/compatibility/sql-prepared-statements.md) |
| SQL server administration | [docs/compatibility/sql-server-administration.md](docs/compatibility/sql-server-administration.md) |
| SQL tablespaces | [docs/compatibility/sql-tablespaces.md](docs/compatibility/sql-tablespaces.md) |
| SQL foreign servers | [docs/compatibility/sql-foreign-servers.md](docs/compatibility/sql-foreign-servers.md) |
| SQL file output | [docs/compatibility/sql-file-output.md](docs/compatibility/sql-file-output.md) |
| SQL replication | [docs/compatibility/sql-replication.md](docs/compatibility/sql-replication.md) |
| SQL resource groups | [docs/compatibility/sql-resource-groups.md](docs/compatibility/sql-resource-groups.md) |
| SQL components and plugins | [docs/compatibility/sql-components-plugins.md](docs/compatibility/sql-components-plugins.md) |
| SQL SET statements | [docs/compatibility/sql-set-statements.md](docs/compatibility/sql-set-statements.md) |
| SQL SHOW statements | [docs/compatibility/sql-show-statements.md](docs/compatibility/sql-show-statements.md) |
| SQL query expressions | [docs/compatibility/sql-query-expressions.md](docs/compatibility/sql-query-expressions.md) |
| SQL joins | [docs/compatibility/sql-joins.md](docs/compatibility/sql-joins.md) |
| SQL common table expressions | [docs/compatibility/sql-ctes.md](docs/compatibility/sql-ctes.md) |
| SQL subqueries | [docs/compatibility/sql-subqueries.md](docs/compatibility/sql-subqueries.md) |
| SQL query hints | [docs/compatibility/sql-query-hints.md](docs/compatibility/sql-query-hints.md) |
| SQL full-text search | [docs/compatibility/sql-fulltext-search.md](docs/compatibility/sql-fulltext-search.md) |
| Type system, literals, and conversion | [docs/compatibility/type-system-literals-conversion.md](docs/compatibility/type-system-literals-conversion.md) |
| Character sets | [docs/compatibility/character-sets.md](docs/compatibility/character-sets.md) |
| Collations and comparison behavior | [docs/compatibility/collations.md](docs/compatibility/collations.md) |
| Operators | [docs/compatibility/operators.md](docs/compatibility/operators.md) |
| Cast functions | [docs/compatibility/functions-casts.md](docs/compatibility/functions-casts.md) |
| Control-flow functions | [docs/compatibility/functions-control-flow.md](docs/compatibility/functions-control-flow.md) |
| Comparison functions | [docs/compatibility/functions-comparison.md](docs/compatibility/functions-comparison.md) |
| Default-value functions | [docs/compatibility/functions-default-values.md](docs/compatibility/functions-default-values.md) |
| Numeric and math functions | [docs/compatibility/functions-numeric-math.md](docs/compatibility/functions-numeric-math.md) |
| String functions | [docs/compatibility/functions-string.md](docs/compatibility/functions-string.md) |
| Temporal functions | [docs/compatibility/functions-temporal.md](docs/compatibility/functions-temporal.md) |
| Aggregate functions | [docs/compatibility/functions-aggregate.md](docs/compatibility/functions-aggregate.md) |
| Window functions | [docs/compatibility/functions-window.md](docs/compatibility/functions-window.md) |
| JSON functions and operators | [docs/compatibility/functions-json.md](docs/compatibility/functions-json.md) |
| Spatial functions | [docs/compatibility/functions-spatial.md](docs/compatibility/functions-spatial.md) |
| Crypto and password functions | [docs/compatibility/functions-crypto-password.md](docs/compatibility/functions-crypto-password.md) |
| Compression functions | [docs/compatibility/functions-compression.md](docs/compatibility/functions-compression.md) |
| UUID functions | [docs/compatibility/functions-uuid.md](docs/compatibility/functions-uuid.md) |
| System functions | [docs/compatibility/functions-system.md](docs/compatibility/functions-system.md) |
| Internal functions | [docs/compatibility/functions-internal.md](docs/compatibility/functions-internal.md) |
| Replication functions | [docs/compatibility/functions-replication.md](docs/compatibility/functions-replication.md) |
| INFORMATION_SCHEMA tables | [docs/compatibility/metadata-information-schema.md](docs/compatibility/metadata-information-schema.md) |
| Performance Schema tables | [docs/compatibility/metadata-performance-schema.md](docs/compatibility/metadata-performance-schema.md) |
| sys schema objects | [docs/compatibility/metadata-sys-schema.md](docs/compatibility/metadata-sys-schema.md) |
| mysql system schema and data dictionary | [docs/compatibility/metadata-mysql-schema.md](docs/compatibility/metadata-mysql-schema.md) |
| Runtime session state and SQL modes | [docs/compatibility/runtime-session-sql-modes.md](docs/compatibility/runtime-session-sql-modes.md) |
| Server system variables | [docs/compatibility/runtime-system-variables.md](docs/compatibility/runtime-system-variables.md) |
| Server status variables | [docs/compatibility/runtime-status-variables.md](docs/compatibility/runtime-status-variables.md) |
| Wire protocol | [docs/compatibility/wire-protocol.md](docs/compatibility/wire-protocol.md) |
| Error, warning, and result semantics | [docs/compatibility/error-warning-result-semantics.md](docs/compatibility/error-warning-result-semantics.md) |
| Embedded-design compatibility decisions | [docs/compatibility/embedded-design-decisions.md](docs/compatibility/embedded-design-decisions.md) |

## Source inventory

- [MySQL 8.4 Reference Manual](https://dev.mysql.com/doc/refman/8.4/en/)
- [MySQL 8.4.9 release notes](https://dev.mysql.com/doc/relnotes/mysql/8.4/en/news-8-4-9.html)
- [What Is New in MySQL 8.4 since MySQL 8.0](https://dev.mysql.com/doc/refman/8.4/en/mysql-nutshell.html)
- [MySQL 8.4 supported character sets and collations](https://dev.mysql.com/doc/refman/8.4/en/charset-charsets.html)
- [MySQL 8.4 `SHOW CHARACTER SET` statement](https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html)
- [MySQL 8.4 `SHOW COLLATION` statement](https://dev.mysql.com/doc/refman/8.4/en/show-collation.html)
- [MySQL 8.4 SQL statement manual](https://dev.mysql.com/doc/refman/8.4/en/sql-statements.html)
- [MySQL 8.4 built-in function and operator reference](https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html)
- [MySQL 8.4 data type manual](https://dev.mysql.com/doc/refman/8.4/en/data-types.html)
- [MySQL 8.4 `INFORMATION_SCHEMA` table reference](https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html)
- [MySQL 8.4 Performance Schema table descriptions](https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-descriptions.html)
- [MySQL 8.4 Performance Schema table reference](https://dev.mysql.com/doc/refman/8.4/en/performance-schema-table-reference.html)
- [MySQL 8.4 `sys` schema object reference](https://dev.mysql.com/doc/refman/8.4/en/sys-schema-reference.html)
- [MySQL 8.4 `mysql` system schema reference](https://dev.mysql.com/doc/refman/8.4/en/system-schema.html)
- [MySQL 8.4 server system variable reference](https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html)
- [MySQL 8.4 server status variable reference](https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html)

## Maintenance rules

- Keep this file as an implementation contract, not a marketing scorecard.
- Split rows when syntax, behavior, metadata, warnings, or side effects need independent tests.
- Document deliberate embedded-design incompatibilities explicitly instead of silently dropping them.
- Keep this baseline tracker, `docs/compatibility/`, feature guides, design notes, and MySQL-runtime tests in sync.
- Prefer MySQL 8.4.9 behavior over convenience, SQLite defaults, or older MySQL/MariaDB behavior.
- Every implemented row needs MySQL 8.4.9 comparison tests.
- Tests must cover results, errors, warnings, metadata, affected rows, inserted ids, session state, and side effects.
- Syntax-only compatibility must still test parser acceptance and exact warning, error, or placeholder behavior.
- Add focused guide documentation when behavior needs design rationale.
- When MySQL behavior depends on platform or storage engine, tests must document the MyLite contract and observed MySQL baseline.
