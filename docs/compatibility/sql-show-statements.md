# SQL SHOW statements

MySQL SHOW statement result shapes, filters, privileges, and compatibility diagnostics.

| SHOW statement | Status | Notes |
| --- | --- | --- |
| `SHOW BINARY LOG STATUS` | 🟡 | Limited embedded placeholder with MySQL 8.4.9 column labels `File`, `Position`, `Binlog_Do_DB`, `Binlog_Ignore_DB`, and `Executed_Gtid_Set`, returning one synthetic `binlog.000001` row at position `4` with empty database filters and empty GTID set; no physical binary log file, live source position, privileges, filters, or modifiers |
| `SHOW BINARY LOGS` | 🟡 | Limited embedded placeholder with MySQL 8.4.9 column labels `Log_name`, `File_size`, and `Encrypted`, returning one synthetic `binlog.000001` row with file size `4` and `Encrypted = No`; no physical binary log files, rotation, purge, privileges, filters, or modifiers |
| `SHOW BINLOG EVENTS` | 🟡 | Limited base-form placeholder exposes MySQL 8.4.9 column labels and one synthetic `Format_desc` event for `binlog.000001`; no `IN`, `FROM`, `LIMIT`, live event stream, binary log files, filters, privileges, or mutable replication state |
| `SHOW CHARACTER SET` / `SHOW CHARSET` | 🟡 | Metadata-only MySQL 8.4.9 catalog rows for all 41 character sets with MySQL column labels and `LIKE 'pattern'` filters; catalog visibility does not imply DDL admission, conversion, comparison semantics, `WHERE`, or privileges |
| `SHOW COLLATION` | 🟡 | Metadata-only MySQL 8.4.9 catalog rows for all 286 collations with MySQL column labels and `LIKE 'pattern'` filters; catalog visibility does not imply DDL admission, conversion, comparison semantics, `WHERE`, or privileges |
| `SHOW COLUMNS` / `SHOW FIELDS` | 🟡 | Limited descriptor-driven column listing for persistent base tables, baseline views, and shadowing session temporary tables, plus MySQL-shaped column metadata for supported `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, the `mysql.time_zone*` table family, `mysql.innodb_table_stats`, `mysql.innodb_index_stats`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.memory_by_thread_by_current_bytes`, `sys.memory_by_user_by_current_bytes`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$memory_by_thread_by_current_bytes`, `sys.x$memory_by_user_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` system objects; supports optional ignored `EXTENDED`, optional `FULL`, `FROM`/`IN`, schema-qualified targets, explicit schema forms, `LIKE 'pattern'` filters, and limited trailing `WHERE` predicates over displayed output columns using string/`NULL` literals, `LIKE`, `REGEXP`/`RLIKE` over the baseline ASCII pattern subset, `IN`, comparisons, `IS NULL`, and boolean connectives; displays `Field`, integer, `decimal(M,D)`, `decimal(M,D) unsigned`, `float`, `float unsigned`, `double`, `double unsigned`, `year`, `date`, `time`, `datetime`, `timestamp`, `char(n)`, `varchar(n)`, baseline `TEXT` family, admitted binary string and `BIT`, limited `enum('label',...)`, limited `set('member',...)`, and limited spatial `GEOMETRY` family `Type`, `Null`, `Key` including `PRI` for current primary-key descriptor columns or the first suitable `NOT NULL` unique descriptor when no primary key exists, `UNI` for supported single-column unique-index columns, and `MUL` for leftmost supported composite unique/nonunique secondary-index columns plus first key parts of metadata-only `FULLTEXT` and `SPATIAL` descriptors, SQL `NULL`, canonical integer, canonical decimal, canonical approximate, decoded `CHAR`/`VARCHAR` string or limited generated `_utf8mb4`/`NULL` character default, matching enum label string, matching set member-list string, canonical year-string or limited generated integer/`NULL` `YEAR` default, canonical date-string, canonical time-string, canonical datetime-string, canonical timestamp-string, compatible `BIT` default including limited generated integer/`NULL` defaults, `BINARY`/`VARBINARY` string or hex default, limited BLOB-family hex/`NULL` generated default, or generated `curdate()`, `curtime()`, or `CURRENT_TIMESTAMP` `Default`, and `Extra`, including generated-column storage text, `DEFAULT_GENERATED`, `on update CURRENT_TIMESTAMP`, `auto_increment`, and `INVISIBLE` for descriptor attributes; `FULL` adds `Collation`, `Privileges`, and `Comment`, using explicit column collation when present, otherwise table-default collation for current nonbinary character descriptors except national `CHAR` / `VARCHAR` aliases report `utf8mb3_general_ci`, fixed embedded privileges, descriptor column comments, and fixed supported `mysql` / `sys` system-object metadata; no NUL-producing pattern escapes, warning-producing numeric filter coercions, full ICU regex semantics, `ORDER BY`, `LIMIT`, privilege filtering, unsupported index metadata, unsupported `mysql` system-table column catalogs, BLOB-family defaults beyond the limited parenthesized hex/`NULL` generated subset, explicit `TEXT`-family string defaults, string/hex defaults for `BIT`, numeric or expression enum/set defaults, non-`NULL` spatial defaults, empty-string set members, warning-producing overlength string-default truncation, general expression defaults beyond the admitted integer, `BIT`, `YEAR`, `CHAR`/`VARCHAR` string/`NULL`, `TEXT` family string/`NULL`, and current date/time/timestamp subsets, generated hidden columns, or full `INFORMATION_SCHEMA` parity |
| `SHOW COUNT(*) ERRORS` | 🟡 | Limited previous-statement error-condition count with MySQL 8.4.9 column label `@@session.error_count`; counts MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no notes, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW COUNT(*) WARNINGS` | 🟡 | Limited previous-statement diagnostics count with MySQL 8.4.9 column label; counts MyLite's previous error condition plus stored warning/note records; missing-schema `DROP DATABASE IF EXISTS` intentionally leaves no stored count after reporting a statement warning count; no `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW CREATE DATABASE` / `SHOW CREATE SCHEMA` | 🟡 | Limited descriptor-driven schema DDL rendering with MySQL 8.4.9 columns, stored schema default charset/collation metadata, fixed default-encryption text, and fixed enabled `sql_quote_show_create` quoting; `binary` schema defaults render without an explicit `COLLATE binary` clause; no `IF NOT EXISTS`, encryption mutation, privileges, system schemas, mutable quote-control state, or disabled rendering |
| `SHOW CREATE EVENT` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE FUNCTION` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE PROCEDURE` | 🟡 | Limited MySQL-shaped six-column metadata for session-local no-argument single-`SELECT` procedures created by the current routine bridge; no persistent routine catalog, privilege filtering, or full stored-program support |
| `SHOW CREATE TABLE` | 🟡 | Limited descriptor-driven MySQL-style DDL for persistent base tables, shadowing session temporary tables, baseline metadata-only views, and the built-in `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.memory_by_thread_by_current_bytes`, `sys.memory_by_user_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$memory_by_thread_by_current_bytes`, `sys.x$memory_by_user_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` views; base and temporary tables render the current supported column/index/constraint/default/table-option subset, while view targets render MySQL-shaped `CREATE VIEW` metadata using stored view descriptors or the supported synthetic sys view definition. Base-table support includes current integer-family, exact `DECIMAL`, approximate `FLOAT`/`DOUBLE`, canonical temporal, character, text, binary, `BIT`, `ENUM`, `SET`, spatial, generated-column, key, foreign-key, check, auto-increment, charset/collation, comment, storage/statistics, and fixed InnoDB suffix metadata described by the table DDL surface; unsupported index/default/type/table-option details remain omitted. View support is limited to baseline direct-projection view descriptors, `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.memory_by_thread_by_current_bytes`, `sys.memory_by_user_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$memory_by_thread_by_current_bytes`, `sys.x$memory_by_user_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans`, and does not imply view execution, updatable views, privileges, mutable quote-control state, or disabled rendering |
| `SHOW CREATE TRIGGER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE USER` | ❌ | Result shape, filters, privileges |
| `SHOW CREATE VIEW` | 🟡 | Limited descriptor-driven output for baseline metadata-only views plus synthetic `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.memory_by_thread_by_current_bytes`, `sys.memory_by_user_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$memory_by_thread_by_current_bytes`, `sys.x$memory_by_user_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` output, with MySQL-shaped columns `View`, `Create View`, `character_set_client`, and `collation_connection`; rejects base tables as not-view and does not support view options beyond fixed descriptor or supported sys metadata, privilege filtering, mutable quote-control state, or executable/updatable views |
| `SHOW DATABASES` / `SHOW SCHEMAS` | 🟡 | Limited schema listing for synthetic built-in schemas `information_schema`, `mysql`, `performance_schema`, and `sys` plus descriptor-owned user schemas, with `LIKE 'pattern'` filters, MySQL-shaped column labels, and limited trailing `WHERE` predicates over the displayed `Database` column using string/`NULL` literals, `LIKE`, `REGEXP`/`RLIKE` over the baseline ASCII pattern subset, `IN`, comparisons, `IS NULL`, and boolean connectives; no NUL-producing pattern escapes, warning-producing numeric coercions, `ORDER BY`, `LIMIT`, or privileges |
| `SHOW ENGINE` | 🟡 | Limited to `SHOW ENGINE InnoDB STATUS`, with `SHOW ENGINE <engine> LOGS` and `SHOW ENGINE <engine> MUTEX` parsed and rejected through explicit unsupported-utility diagnostics; non-`InnoDB` engine status names are rejected by MyLite's embedded InnoDB-only policy; no Performance Schema engine status, alternate engines, filters, privileges, or live engine internals |
| `SHOW ENGINE LOGS` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no server log rows, filters, privileges, or engine internals |
| `SHOW ENGINE MUTEX` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no mutex rows, filters, privileges, or engine internals |
| `SHOW ENGINE STATUS` | 🟡 | Limited `SHOW ENGINE InnoDB STATUS` exposes MySQL 8.4.9 columns `Type`, `Name`, and `Status` with one synthetic `InnoDB` row and stable MyLite-owned status text; no live InnoDB monitor output, non-`InnoDB` status rows, filters, privileges, or mutable engine state |
| `SHOW ENGINES` | 🟡 | Limited `SHOW [STORAGE] ENGINES` exposes one embedded InnoDB default row with MySQL 8.4.9 column labels; related `INFORMATION_SCHEMA.ENGINES` is a separate limited one-row synthetic catalog; no alternate engines, filters, privileges, or plugins |
| `SHOW ERRORS` | 🟡 | Limited previous-statement error-condition rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite's previous error condition only; related scalar `@@error_count` is limited separately; no warning/note rows, `WHERE`, `LIKE`, expression filters, `max_error_count`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |
| `SHOW EVENTS` | 🟡 | Limited empty event introspection with MySQL 8.4.9 column labels, `LIKE 'pattern'` filters, and admitted trailing `WHERE` predicates; unknown explicit schemas are empty successes; related empty `INFORMATION_SCHEMA.EVENTS` metadata is queryable, but there are no NUL-producing pattern escapes, event descriptors, event rows, event DDL, `SHOW CREATE EVENT`, row predicate evaluation, Event Scheduler, or privileges |
| `SHOW FUNCTION CODE` | ⚪ | Parsed and rejected with stored-program unsupported diagnostics; no routine bytecode or debugger rows |
| `SHOW FUNCTION STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` or `WHERE` filters; global and default-schema independent; related empty `INFORMATION_SCHEMA.ROUTINES` metadata is queryable, but there are no NUL-producing pattern escapes, routine descriptors, routine rows, routine DDL, `SHOW CREATE FUNCTION`, or privileges |
| `SHOW GRANTS` | 🟡 | Limited current-user forms `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`, `SHOW GRANTS FOR CURRENT_USER()`, and named-root account spellings that resolve to `root@%` return two synthetic MySQL 8.4.9-shaped global grant rows for MyLite's embedded identity; other named accounts return `1141 / 42000`; parser-admitted `USING` role clauses over embedded root return `3530 / HY000` because MyLite has no granted roles. No account storage, role graph, role-expanded output, grant descriptors, privilege enforcement, filters, ordering, or limits |
| `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` | 🟡 | Limited descriptor-resolved persistent base-table and shadowing session temporary-table introspection with MySQL 8.4.9 column labels, optional ignored `EXTENDED`, zero rows for no-index tables and supported synthetic no-index sys views, one `PRIMARY` BTREE row per ordered primary-key part, one BTREE row per supported unique or nonunique secondary key part including `Sub_part` for current supported string and binary prefix parts, `Collation` values `A` / `D` for stored ascending / descending key-part direction, `Index_comment` from descriptor index comments, and `Visible` from descriptor visibility metadata, plus one metadata-only `FULLTEXT` row per key part with `Index_type = FULLTEXT`, `Collation = NULL`, ignored prefixes reported as `Sub_part = NULL`, descriptor comments, and descriptor visibility, one metadata-only `SPATIAL` row with `Index_type = SPATIAL`, `Collation = A`, `Sub_part = 32`, descriptor comments, and descriptor visibility, MySQL-shaped `PRIMARY` BTREE metadata for `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, the `mysql.time_zone*` table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats`, and zero-row index metadata for supported no-index `mysql.general_log` and `mysql.slow_log`; `Comment` remains the fixed empty storage-engine placeholder. Limited trailing `WHERE` predicates over displayed output columns use string/`NULL` literals, `LIKE`, `REGEXP`/`RLIKE` over the baseline ASCII pattern subset, `IN`, comparisons, `IS NULL`, and boolean connectives; no unsupported `mysql` system-table index catalogs, functional index metadata, `LIKE` filter clause, `ORDER BY`, `LIMIT`, unsupported views, privileges, real storage statistics, full ICU regex semantics, full-text search behavior, or spatial search behavior |
| `SHOW MASTER STATUS` | ❌ | Removed; use SHOW BINARY LOG STATUS |
| `SHOW OPEN TABLES` | 🟡 | Limited embedded empty open-table introspection with MySQL 8.4.9 column labels, `LIKE 'pattern'` filters, and admitted trailing `WHERE` predicates; unknown explicit schemas are empty successes; no NUL-producing pattern escapes, table-cache rows, `In_use` counts, `Name_locked` state, temporary tables, `HANDLER`, table locks, row predicate evaluation, privileges, or performance-schema metadata |
| `SHOW PARSE_TREE` | ❌ | Conditional parse-tree debug output |
| `SHOW PLUGINS` | 🟡 | Limited one-row result for MyLite's embedded active `InnoDB` storage-engine plugin surface with MySQL 8.4.9 column labels; related `INFORMATION_SCHEMA.PLUGINS` is a separate limited synthetic catalog; no filters, privileges, loadable plugins, disabled plugin rows, alternate engines, or complete MySQL plugin inventory |
| `SHOW PRIVILEGES` | 🟡 | Limited static MySQL 8.4.9-shaped privilege-name catalog with columns `Privilege`, `Context`, and `Comment` and the observed 73 rows for the target runtime; no filters, account storage, grant descriptors, privilege enforcement, mutable dynamic privileges, ordering, or limits |
| `SHOW PROCEDURE CODE` | ⚪ | Parsed and rejected with stored-program unsupported diagnostics; no routine bytecode or debugger rows |
| `SHOW PROCEDURE STATUS` | 🟡 | Limited empty routine introspection with MySQL 8.4.9 column labels and `LIKE 'pattern'` or `WHERE` filters; global and default-schema independent; related empty `INFORMATION_SCHEMA.ROUTINES` metadata is queryable, but there are no NUL-producing pattern escapes, persistent routine descriptors, routine rows, session-local procedure bridge rows, or privileges |
| `SHOW PROCESSLIST` | 🟡 | Limited current embedded-handle row for `SHOW [FULL] PROCESSLIST` with MySQL 8.4.9 columns, selected-schema `db`, `Info` truncation, and default process-list warning count; related limited `INFORMATION_SCHEMA.PROCESSLIST` metadata is queryable, but there are no server-wide threads, sleeping/background rows, filters, privileges, Performance Schema, sys-schema views, or `KILL` |
| `SHOW PROFILE` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no profiler collection, result rows, filters, or privileges |
| `SHOW PROFILES` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no profiler collection, result rows, filters, or privileges |
| `SHOW RELAYLOG EVENTS` | ❌ | Result shape, filters, privileges |
| `SHOW REPLICA STATUS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels, affected rows `0`, warning count `0`, and following `ROW_COUNT() = -1`; no `FOR CHANNEL`, filters, privileges, channel state, relay logs, source topology, or replication effects |
| `SHOW REPLICAS` | 🟡 | Limited no-replication empty result set with MySQL 8.4.9 column labels `Server_Id`, `Host`, `Port`, `Source_Id`, and `Replica_UUID`, affected rows `0`, warning count `0`, and following `ROW_COUNT() = -1`; no registered replica rows, filters, privileges, source topology, or replication effects |
| `SHOW STATUS` | 🟡 | Limited runtime-owned status registry with MySQL 8.4.9 column labels, optional `GLOBAL`/`SESSION`/`LOCAL` scope, `LIKE 'pattern'` filters, and limited trailing `WHERE` predicates over `Variable_name` and `Value`; values are embedded placeholders for current common rows and do not implement live counters, arbitrary `WHERE` expressions, Performance Schema status tables, privileges, or full status-variable coverage |
| `SHOW TABLE STATUS` | 🟡 | Limited descriptor-driven persistent base-table and baseline view status rows plus metadata-only built-in schema directory status rows, with MySQL 8.4.9 column labels, `FROM`/`IN` schema forms, `LIKE 'pattern'` filters, limited `WHERE` predicates over displayed output columns and one-level `SUBSTRING()` / `SUBSTR()` / `MID()` operands over those columns using string/`NULL` literals plus unsigned integer literals for numeric status columns, `LIKE`, `REGEXP`/`RLIKE` over the baseline ASCII pattern subset, `IN`, comparisons, `IS NULL`, and `AND`/`OR`/`NOT`; user base tables expose exact physical row counts and descriptor-owned status metadata, while built-in rows expose directory placeholders and representative MySQL-shaped engine/type/comment/collation fields, with MySQL-observed status fields for supported `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, the `mysql.time_zone*` table family, `mysql.innodb_table_stats`, `mysql.innodb_index_stats`, `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.memory_by_thread_by_current_bytes`, `sys.memory_by_user_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$memory_by_thread_by_current_bytes`, `sys.x$memory_by_user_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` rows; no temporary tables, arbitrary `WHERE` expressions beyond the documented substring operand subset, warning-producing numeric coercions, privileges, checksums, full ICU regex semantics, or full storage-engine statistics |
| `SHOW TABLES` | 🟡 | Limited descriptor-driven `SHOW TABLES` for MyLite user schemas and metadata-only directory listing for built-in `information_schema`, `mysql`, `performance_schema`, and `sys` schemas, with optional ignored `EXTENDED`, optional `FULL`, `FROM`/`IN` schema, `LIKE 'pattern'` filters, and limited trailing `WHERE` predicates over displayed output columns using string/`NULL` literals, `LIKE`, `REGEXP`/`RLIKE` over the baseline ASCII pattern subset, `IN`, comparisons, `IS NULL`, and `AND`/`OR`/`NOT`; `FULL` reports `BASE TABLE`, `VIEW`, or `SYSTEM VIEW` from descriptors or the built-in directory and exposes `Table_type` to `WHERE`; no NUL-producing pattern escapes, `LIKE ... WHERE`, `ORDER BY`, `LIMIT`, privileges, temporary-table rows, or queryable unsupported system tables |
| `SHOW TRIGGERS` | 🟡 | Limited schema-resolved trigger introspection with MySQL 8.4.9 column labels, optional `FULL`, and `LIKE 'pattern'` filters over table names; returns metadata-only rows for the two built-in `sys.sys_config` triggers and empty user rows otherwise; no NUL-producing pattern escapes, user-created trigger descriptors, trigger execution, trigger DDL, `SHOW CREATE TRIGGER`, evaluated `WHERE` predicates, or privileges |
| `SHOW VARIABLES` | 🟡 | Limited runtime-owned rows for MyLite's current system-variable registry with MySQL 8.4.9 column labels, optional `GLOBAL`/`SESSION`/`LOCAL` scope, `LIKE 'pattern'` filters, limited `WHERE` predicates over `Variable_name` and `Value`, and session-local readback for the currently mutable variables; no full MySQL variable catalog, arbitrary expressions, warning-producing numeric filter coercions, privileges, persisted variables, Performance Schema variable tables, or mutable global state |
| `SHOW WARNINGS` | 🟡 | Limited previous-statement diagnostics rows with `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing; reports MyLite previous error conditions and stored warning/note records only; missing-schema `DROP DATABASE IF EXISTS` intentionally stores no warning row after reporting a statement warning count; no `WHERE`, `LIKE`, expression filters, `max_error_count`, mutable `sql_notes`, `GET DIAGNOSTICS`, privileges, or full diagnostics-area behavior |

The `baseline-sys-sys-config-table` slice extends the `SHOW COLUMNS` /
`SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` /
`SHOW KEYS`, and `SHOW TABLE STATUS` rows above with MySQL-shaped metadata for
`sys.sys_config`. Other `sys` objects remain metadata-directory rows only until
their own slices document support.

The `baseline-sys-version-view` slice extends the `SHOW COLUMNS` /
`SHOW FULL COLUMNS` / `DESCRIBE`, empty `SHOW INDEX` / `SHOW INDEXES` /
`SHOW KEYS`, and `SHOW TABLE STATUS` rows above with MySQL-shaped metadata for
the supported synthetic `sys.version` view. The
`baseline-sys-version-view-definition` slice adds MySQL-shaped
`SHOW CREATE VIEW` and `SHOW CREATE TABLE` rows for that synthetic view without
adding persisted sys view descriptors or the broader sys view catalog.

The `baseline-sys-ps-check-lost-instrumentation-view` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.ps_check_lost_instrumentation` view, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add Performance Schema lost-instrumentation counters,
positive status rows, privilege checks, or broader sys view execution.

The `baseline-sys-host-summary-views` slice adds the same SHOW metadata
surface for the supported empty synthetic `sys.host_summary` and
`sys.x$host_summary` views, including selected-schema access, empty view index
metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata. It does
not add Performance Schema account, statement, file-I/O, or memory summary
collection, live host rows, raw-view warning production, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-host-summary-by-file-io-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.host_summary_by_file_io` and `sys.x$host_summary_by_file_io` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
host wait-summary collection, live file-I/O rows, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-host-summary-by-file-io-type-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.host_summary_by_file_io_type` and
`sys.x$host_summary_by_file_io_type` views, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add Performance Schema host/event wait-summary
collection, live file-I/O type rows, sys helper-function execution, privilege
checks, or broader sys view execution.

The `baseline-sys-host-summary-by-stages-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.host_summary_by_stages` and `sys.x$host_summary_by_stages` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
host/stage summary collection, live stage rows, sys helper-function execution,
privilege checks, or broader sys view execution.

The `baseline-sys-host-summary-by-statement-latency-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.host_summary_by_statement_latency` and
`sys.x$host_summary_by_statement_latency` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema host
statement-summary collection, live statement rows, latency or row counters, sys
helper-function execution, privilege checks, or broader sys view execution.

The `baseline-sys-host-summary-by-statement-type-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.host_summary_by_statement_type` and
`sys.x$host_summary_by_statement_type` views, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add Performance Schema host statement-summary
collection, live statement-type rows, latency or row counters, sys
helper-function execution, privilege checks, or broader sys view execution.

The `baseline-sys-innodb-buffer-stats-by-schema-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.innodb_buffer_stats_by_schema` and
`sys.x$innodb_buffer_stats_by_schema` views, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add live InnoDB buffer-pool page inventory, schema page
accounting, sys helper-function execution, privilege checks, or broader sys
view execution.

The `baseline-sys-innodb-buffer-stats-by-table-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.innodb_buffer_stats_by_table` and
`sys.x$innodb_buffer_stats_by_table` views, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add live InnoDB buffer-pool page inventory, table page
accounting, sys helper-function execution, privilege checks, or broader sys
view execution.

The `baseline-sys-innodb-lock-waits-views` slice adds the same SHOW metadata
surface for the supported empty synthetic `sys.innodb_lock_waits` and
`sys.x$innodb_lock_waits` views, including selected-schema access, empty view
index metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata. It
does not add live InnoDB lock-wait collection, Performance Schema data-lock
rows, sys helper-function execution, privilege checks, or broader sys view
execution.

The `baseline-sys-latest-file-io-views` slice adds the same SHOW metadata
surface for the supported empty synthetic `sys.latest_file_io` and
`sys.x$latest_file_io` views, including selected-schema access, empty view
index metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata. It
does not add Performance Schema file-I/O wait collection, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-memory-by-host-by-current-bytes-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.memory_by_host_by_current_bytes` and
`sys.x$memory_by_host_by_current_bytes` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema memory-summary
collection, live host memory rows, sys helper-function execution, privilege
checks, or broader sys view execution.

The `baseline-sys-memory-by-thread-by-current-bytes-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.memory_by_thread_by_current_bytes` and
`sys.x$memory_by_thread_by_current_bytes` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema memory-summary
collection, live thread rows, sys helper-function execution, privilege checks,
or broader sys view execution.

The `baseline-sys-memory-by-user-by-current-bytes-views` slice adds the same
SHOW metadata surface for the supported empty synthetic
`sys.memory_by_user_by_current_bytes` and
`sys.x$memory_by_user_by_current_bytes` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema memory-summary
collection, live user rows, sys helper-function execution, privilege checks,
or broader sys view execution.

The `baseline-sys-io-by-thread-by-latency-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.io_by_thread_by_latency` and `sys.x$io_by_thread_by_latency` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
wait-summary collection, live thread/file-I/O latency rows, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-io-global-by-file-by-bytes-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.io_global_by_file_by_bytes` and `sys.x$io_global_by_file_by_bytes` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
file-summary collection, live byte/average/write percentage rows, sys
helper-function execution, privilege checks, or broader sys view execution.

The `baseline-sys-io-global-by-file-by-latency-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.io_global_by_file_by_latency` and `sys.x$io_global_by_file_by_latency`
views, including selected-schema access, empty view index metadata, view status
rows, and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance
Schema file-summary collection, live event/latency rows, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-io-global-by-wait-by-bytes-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.io_global_by_wait_by_bytes` and `sys.x$io_global_by_wait_by_bytes` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
file-summary collection, live event/byte/latency rows, sys helper-function
execution, privilege checks, or broader sys view execution.

The `baseline-sys-io-global-by-wait-by-latency-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.io_global_by_wait_by_latency` and
`sys.x$io_global_by_wait_by_latency` views, including selected-schema access,
empty view index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add Performance Schema file-summary collection, live
event/latency/byte rows, sys helper-function execution, privilege checks, or
broader sys view execution.

The `baseline-sys-x-ps-schema-table-statistics-io-view` slice adds the same
SHOW metadata surface for the supported synthetic
`sys.x$ps_schema_table_statistics_io` helper view, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema file-summary
collection, sys helper-function execution, privilege checks, or broader sys
view execution.

The `baseline-sys-schema-auto-increment-columns-view` slice adds the same SHOW
metadata surface for the supported synthetic
`sys.schema_auto_increment_columns` view, including selected-schema access,
empty index metadata, view status rows, and MySQL-shaped `SHOW CREATE`
metadata. It does not add physical sys views, privilege checks, or broader sys
view execution.

The `baseline-sys-schema-index-statistics-views` slice adds the same SHOW
metadata surface for the supported synthetic `sys.schema_index_statistics` and
`sys.x$schema_index_statistics` views, including selected-schema access, empty
view index metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata.
It does not add Performance Schema wait collection, real latency accumulation,
physical sys views, privilege checks, or broader sys view execution.

The `baseline-sys-schema-object-overview-view` slice adds the same SHOW
metadata surface for the supported synthetic `sys.schema_object_overview` view,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add physical sys views,
privilege checks, stored routine inventory, event inventory, or full
Performance Schema object summaries.

The `baseline-sys-schema-redundant-indexes-views` slice adds the same SHOW
metadata surface for the supported synthetic `sys.schema_redundant_indexes` and
`sys.x$schema_flattened_keys` views, including selected-schema access, empty
view index metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata.
It does not add physical sys views, temporary-table rows, built-in-schema rows,
FULLTEXT/SPATIAL/functional indexes, privilege checks, or broader sys view
execution.

The `baseline-sys-schema-table-lock-waits-views` slice adds the same SHOW
metadata surface for the supported empty synthetic
`sys.schema_table_lock_waits` and `sys.x$schema_table_lock_waits` views,
including selected-schema access, empty view index metadata, view status rows,
and MySQL-shaped `SHOW CREATE` metadata. It does not add live Performance
Schema metadata-lock waits, sys helper-function execution, privilege checks, or
broader sys view execution.

The `baseline-sys-schema-table-statistics-views` slice adds the same SHOW
metadata surface for the supported synthetic `sys.schema_table_statistics` and
`sys.x$schema_table_statistics` views, including selected-schema access, empty
view index metadata, view status rows, and MySQL-shaped `SHOW CREATE` metadata.
It does not add Performance Schema table wait collection, live counter
accumulation, privilege checks, or broader sys view execution.

The `baseline-sys-schema-table-statistics-with-buffer-views` slice adds the
same SHOW metadata surface for the supported synthetic
`sys.schema_table_statistics_with_buffer` and
`sys.x$schema_table_statistics_with_buffer` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema table wait
collection, InnoDB buffer-pool table accounting, live counter accumulation,
privilege checks, or broader sys view execution.

The `baseline-sys-schema-tables-with-full-table-scans-views` slice adds the
same SHOW metadata surface for the supported empty synthetic
`sys.schema_tables_with_full_table_scans` and
`sys.x$schema_tables_with_full_table_scans` views, including selected-schema
access, empty view index metadata, view status rows, and MySQL-shaped
`SHOW CREATE` metadata. It does not add Performance Schema table I/O wait
collection, live full-table-scan detection, privilege checks, or broader sys
view execution.

The `baseline-sys-schema-unused-indexes-view` slice adds the same SHOW metadata
surface for the supported synthetic `sys.schema_unused_indexes` view, including
selected-schema access, empty view index metadata, view status rows, and
MySQL-shaped `SHOW CREATE` metadata. It does not add Performance Schema
index-use collection, row removal after index reads, privilege checks, or
broader sys view execution.

The `baseline-sys-sys-config-triggers` slice extends `SHOW TRIGGERS` and
`SHOW FULL TRIGGERS` with metadata-only rows for the two built-in
`sys.sys_config` triggers. Trigger execution, trigger DDL, `SHOW CREATE
TRIGGER`, and `SHOW TRIGGERS ... WHERE` predicate evaluation remain unsupported.

[Back to compatibility overview](../../COMPATIBILITY.md)
