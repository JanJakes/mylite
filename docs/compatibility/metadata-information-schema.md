# INFORMATION_SCHEMA tables

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

Access semantics are limited but MySQL-shaped: `USE information_schema`
succeeds, the current metadata `SELECT` subset can resolve unqualified metadata
table names while that schema is selected, and currently supported mutating
schema, table, index, rename, truncate, and single-table DML statements that
target `information_schema` fail with `1044 / 42000` access denied diagnostics.
The same built-in write guard protects `mysql`, `performance_schema`, and `sys`
as metadata-only schemas; `performance_schema` uses the `1044 / 42000`
diagnostic and `mysql` / `sys` use `3552 / HY000` system-schema diagnostics.
MyLite does not implement a privilege engine, writable system views, or
physical system-schema tables.

The query surface is synthetic and limited. Supported `SELECT` forms can filter
one metadata table source with descriptor-defined metadata columns using
comparisons, `IS [NOT] NULL`, `LIKE` string patterns, literal-list `IN`,
`BETWEEN`, `NOT`, `AND`, `OR`, `XOR`, and parentheses. Predicate evaluation uses
MySQL-shaped true/false/unknown filtering over MyLite-built metadata rows and
honors the current `NO_BACKSLASH_ESCAPES` mode for admitted `LIKE` patterns.
There are no physical SQLite `information_schema` tables, predicate subqueries,
explicit `ESCAPE`, arbitrary expressions, joins, privilege filtering, or full
metadata-lock semantics.

The `baseline-sys-sys-config-table` slice extends the relevant
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `STATISTICS`, `TABLE_CONSTRAINTS`,
`KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` rows below with
MySQL-shaped metadata for the supported synthetic `sys.sys_config` table. Other
`sys` schema objects remain directory-only metadata rows unless their own
compatibility entries list broader support.

The `baseline-sys-sys-config-triggers` slice extends
`INFORMATION_SCHEMA.TRIGGERS` with metadata-only rows for the two built-in
`sys.sys_config` triggers. It does not add trigger execution, trigger DDL, or
user-created trigger descriptors.

The `baseline-sys-version-view` slice extends `INFORMATION_SCHEMA.COLUMNS`,
`TABLES`, `STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and
`TABLE_CONSTRAINTS_EXTENSIONS` with MySQL-shaped metadata for the supported
synthetic `sys.version` view. The `baseline-sys-version-view-definition` slice
adds the matching `INFORMATION_SCHEMA.VIEWS` row and keeps sys view dependency
metadata empty, matching the MySQL 8.4.9 target runtime. It does not add
persisted view descriptors, privilege filtering, or the broader sys view
catalog.

The `baseline-sys-ps-check-lost-instrumentation-view` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.ps_check_lost_instrumentation` view. It does not add Performance Schema
lost-instrumentation counters, positive status rows, physical sys views,
privilege filtering, or broader sys view execution.

The `baseline-sys-host-summary-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic `sys.host_summary` and
`sys.x$host_summary` views, while preserving MySQL-observed empty
`VIEW_ROUTINE_USAGE` results for those views. It does not add Performance
Schema account, statement, file-I/O, or memory summary collection, live host
rows, raw-view warning production, sys helper-function execution, privilege
filtering, or broader sys view execution.

The `baseline-sys-host-summary-by-file-io-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.host_summary_by_file_io` and `sys.x$host_summary_by_file_io` views, while
preserving MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views.
It does not add Performance Schema host wait-summary collection, live file-I/O
rows, sys helper-function execution, privilege filtering, or broader sys view
execution.

The `baseline-sys-host-summary-by-file-io-type-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.host_summary_by_file_io_type` and
`sys.x$host_summary_by_file_io_type` views, while preserving MySQL-observed
empty `VIEW_ROUTINE_USAGE` results for those views. It does not add
Performance Schema host/event wait-summary collection, live file-I/O type
rows, sys helper-function execution, privilege filtering, or broader sys view
execution.

The `baseline-sys-host-summary-by-stages-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.host_summary_by_stages` and `sys.x$host_summary_by_stages` views, while
preserving MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views.
It does not add Performance Schema host/stage summary collection, live stage
rows, sys helper-function execution, privilege filtering, or broader sys view
execution.

The `baseline-sys-host-summary-by-statement-latency-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.host_summary_by_statement_latency` and
`sys.x$host_summary_by_statement_latency` views, while preserving
MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views. It does not
add Performance Schema host statement-summary collection, live statement rows,
latency or row counters, sys helper-function execution, privilege filtering,
or broader sys view execution.

The `baseline-sys-host-summary-by-statement-type-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.host_summary_by_statement_type` and
`sys.x$host_summary_by_statement_type` views, while preserving
MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views. It does not
add Performance Schema host statement-summary collection, live statement-type
rows, latency or row counters, sys helper-function execution, privilege
filtering, or broader sys view execution.

The `baseline-sys-innodb-buffer-stats-by-schema-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.innodb_buffer_stats_by_schema` and
`sys.x$innodb_buffer_stats_by_schema` views, while preserving
MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views. It does not
add live InnoDB buffer-pool page inventory, schema page accounting,
sys helper-function execution, privilege filtering, or broader sys view
execution.

The `baseline-sys-innodb-buffer-stats-by-table-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.innodb_buffer_stats_by_table` and
`sys.x$innodb_buffer_stats_by_table` views, while preserving MySQL-observed
empty `VIEW_ROUTINE_USAGE` results for those views. It does not add live
InnoDB buffer-pool page inventory, table page accounting, sys helper-function
execution, privilege filtering, or broader sys view execution.

The `baseline-sys-innodb-lock-waits-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported empty
synthetic `sys.innodb_lock_waits` and `sys.x$innodb_lock_waits` views. It does
not add live InnoDB lock-wait collection, Performance Schema data-lock rows,
sys helper-function execution, privilege filtering, or broader sys view
execution.

The `baseline-sys-latest-file-io-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported empty
synthetic `sys.latest_file_io` and `sys.x$latest_file_io` views. It does not
add Performance Schema file-I/O wait collection, sys helper-function execution,
privilege filtering, or broader sys view execution.

The `baseline-sys-memory-by-host-by-current-bytes-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.memory_by_host_by_current_bytes` and
`sys.x$memory_by_host_by_current_bytes` views, while preserving MySQL-observed
empty `VIEW_ROUTINE_USAGE` results for those views. It does not add
Performance Schema memory-summary collection, live host memory rows, sys
helper-function execution, privilege filtering, or broader sys view execution.

The `baseline-sys-io-by-thread-by-latency-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.io_by_thread_by_latency` and `sys.x$io_by_thread_by_latency` views, while
preserving MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those views.
It does not add Performance Schema wait-summary collection, live thread/file
I/O latency rows, sys helper-function execution, privilege filtering, or
broader sys view execution.

The `baseline-sys-io-global-by-file-by-bytes-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported empty
synthetic `sys.io_global_by_file_by_bytes` and
`sys.x$io_global_by_file_by_bytes` views. It does not add Performance Schema
file-summary collection, live byte/average/write percentage rows, sys
helper-function execution, privilege filtering, or broader sys view execution.

The `baseline-sys-io-global-by-file-by-latency-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported empty
synthetic `sys.io_global_by_file_by_latency` and
`sys.x$io_global_by_file_by_latency` views. It does not add Performance Schema
file-summary collection, live event/latency rows, sys helper-function
execution, privilege filtering, or broader sys view execution.

The `baseline-sys-io-global-by-wait-by-bytes-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.io_global_by_wait_by_bytes` and `sys.x$io_global_by_wait_by_bytes` views,
while preserving MySQL-observed empty `VIEW_ROUTINE_USAGE` results for those
views. It does not add Performance Schema file-summary collection, live
event/byte/latency rows, sys helper-function execution, privilege filtering, or
broader sys view execution.

The `baseline-sys-io-global-by-wait-by-latency-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.io_global_by_wait_by_latency` and
`sys.x$io_global_by_wait_by_latency` views, while preserving MySQL-observed
empty `VIEW_ROUTINE_USAGE` results for those views. It does not add
Performance Schema file-summary collection, live event/latency/byte rows, sys
helper-function execution, privilege filtering, or broader sys view execution.

The `baseline-sys-x-ps-schema-table-statistics-io-view` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported synthetic
`sys.x$ps_schema_table_statistics_io` helper view. It does not add Performance
Schema file-summary collection, sys helper-function execution, privilege
filtering, or broader sys view execution.

The `baseline-sys-schema-auto-increment-columns-view` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_auto_increment_columns` view. It does not add physical sys views,
privilege filtering, temporary-table rows, or broader sys view execution.

The `baseline-sys-schema-object-overview-view` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_object_overview` view. It does not add physical sys views,
privilege filtering, stored routine inventory, event inventory, or full
Performance Schema object summaries.

The `baseline-sys-schema-index-statistics-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_index_statistics` and `sys.x$schema_index_statistics` views. It
does not add Performance Schema wait collection, physical sys views, privilege
filtering, or broader sys view execution.

The `baseline-sys-schema-redundant-indexes-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_redundant_indexes` and `sys.x$schema_flattened_keys` views. It
does not add physical sys view execution, temporary-table rows,
built-in-schema rows, FULLTEXT/SPATIAL/functional indexes, privilege filtering,
or broader sys view execution.

The `baseline-sys-schema-table-lock-waits-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, `VIEW_TABLE_USAGE`, and
`VIEW_ROUTINE_USAGE` with MySQL-shaped metadata for the supported empty
synthetic `sys.schema_table_lock_waits` and
`sys.x$schema_table_lock_waits` views. It does not add live Performance Schema
metadata-lock wait collection, sys helper-function execution, privilege
filtering, or broader sys view execution.

The `baseline-sys-schema-table-statistics-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_table_statistics` and `sys.x$schema_table_statistics` views. It
does not add Performance Schema table wait collection, live counter
accumulation, physical sys views, privilege filtering, or broader sys view
execution.

The `baseline-sys-schema-table-statistics-with-buffer-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_table_statistics_with_buffer` and
`sys.x$schema_table_statistics_with_buffer` views. It does not add Performance
Schema table wait collection, InnoDB buffer-pool table accounting, live
counter accumulation, physical sys views, privilege filtering, or broader sys
view execution.

The `baseline-sys-schema-tables-with-full-table-scans-views` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported empty synthetic
`sys.schema_tables_with_full_table_scans` and
`sys.x$schema_tables_with_full_table_scans` views. It does not add Performance
Schema table I/O wait collection, live full-table-scan detection, physical sys
views, privilege filtering, or broader sys view execution.

The `baseline-sys-schema-unused-indexes-view` slice extends
`INFORMATION_SCHEMA.COLUMNS`, `TABLES`, `VIEWS`, and `VIEW_TABLE_USAGE` with
MySQL-shaped metadata for the supported synthetic
`sys.schema_unused_indexes` view. It does not add Performance Schema index-use
collection, removal of rows after index reads, physical sys views, privilege
filtering, or broader sys view execution.

| Table | Status | Notes |
| --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | 🟡 | Queryable empty synthetic administrable-role metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, active-role state, grant descriptors, default or mandatory roles, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | 🟡 | Queryable empty synthetic applicable-role metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, active-role state, grant descriptors, default or mandatory roles, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | 🟡 | Queryable synthetic metadata-only catalog for the 41 MySQL 8.4.9 character-set rows, with MySQL-shaped columns and system-view metadata; catalog visibility does not imply DDL admission, conversion, comparison semantics, privileges, or `mysql` data-dictionary base tables |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | 🟡 | Queryable synthetic check-constraint catalog with MySQL 8.4.9-shaped columns, matching system metadata, and descriptor rows for the current limited persistent-table `CHECK` subset including supported `ALTER TABLE ... ADD/DROP/ALTER CHECK`; no complete expression coverage, temporary-table checks, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | 🟡 | Queryable synthetic metadata-only mapping for the 286 MySQL 8.4.9 collation-to-character-set rows, with MySQL-shaped columns and system-view metadata; catalog visibility does not imply DDL admission, conversion, comparison semantics, privileges, or `mysql` data-dictionary base tables |
| `INFORMATION_SCHEMA.COLLATIONS` | 🟡 | Queryable synthetic metadata-only catalog for the 286 MySQL 8.4.9 collation rows, with MySQL-shaped columns and system-view metadata; catalog visibility does not imply DDL admission, conversion, comparison semantics, privileges, or `mysql` data-dictionary base tables |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | 🟡 | Queryable empty synthetic column-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no column grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | 🟡 | Queryable empty synthetic histogram-statistics catalog with MySQL 8.4.9-shaped columns and matching system metadata; no histogram descriptors, `ANALYZE TABLE ... UPDATE HISTOGRAM`, optimizer statistics, privilege filtering, or physical data dictionary tables |
| `INFORMATION_SCHEMA.COLUMNS` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views, supported `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats` system tables, supported `sys.sys_config`, `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` objects, and MyLite descriptor columns, including current integer/exact `DECIMAL`/approximate `FLOAT` and `DOUBLE`/canonical `YEAR`/canonical `DATE`/canonical `TIME`/canonical `DATETIME`/canonical `TIMESTAMP`/`CHAR`/`VARCHAR`/baseline `TEXT` family/binary string/`BIT`/limited `ENUM`/limited `SET`/limited `JSON`/limited spatial `GEOMETRY` family metadata, nullability, integer, decimal, approximate, literal and limited generated `CHAR`/`VARCHAR` string/`NULL`, matching `ENUM` label string, matching `SET` member-list string, canonical year-string, canonical date-string, canonical time-string, canonical datetime-string, canonical timestamp-string, compatible `BIT` default, `BINARY`/`VARBINARY` string or hex defaults, limited BLOB-family hex/`NULL` generated defaults, `JSON DEFAULT NULL`, spatial `DEFAULT NULL`, and generated `curdate()`, `curtime()`, and `CURRENT_TIMESTAMP` defaults, visibility, MySQL-shaped `COLUMN_KEY` markers for primary-key descriptors, the first suitable `NOT NULL` unique descriptor when no primary exists, supported single-column unique descriptors, leftmost composite unique/nonunique secondary descriptors, and metadata-only first-part `FULLTEXT` / `SPATIAL` descriptors, `DEFAULT_GENERATED`, generated-column `EXTRA` and `GENERATION_EXPRESSION`, `on update CURRENT_TIMESTAMP`, auto-increment `EXTRA`, descriptor-owned `COLUMN_COMMENT` values, decimal and approximate `NUMERIC_PRECISION` / `NUMERIC_SCALE` metadata, binary string byte-length metadata, `BIT` numeric precision metadata with SQL `NULL` character set and collation names, enum/set `DATA_TYPE`, `COLUMN_TYPE`, max-display character/octet lengths, JSON and spatial `DATA_TYPE` / `COLUMN_TYPE` with SQL `NULL` character/numeric/datetime metadata, effective table/column character set and collation names for admitted character descriptors, `NULL` numeric precision/scale and datetime precision for `YEAR`, `NULL` temporal precision for `DATE`, and `DATETIME_PRECISION = 0` for `TIME`, `DATETIME`, and `TIMESTAMP`; no privileges, generated columns outside the limited integer expression descriptor subset and supported mysql cost-table default columns, BLOB-family defaults beyond the limited parenthesized hex/`NULL` generated subset, explicit `TEXT`-family string defaults, string/hex defaults for `BIT`, numeric or expression enum/set defaults, JSON expression defaults, non-`NULL` spatial defaults, SRID attributes, empty-string set members, warning-producing overlength string-default truncation, charset conversion or comparison semantics, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS` | 🟡 | Limited queryable extension-attribute rows for supported `information_schema` synthetic table definitions plus MyLite base-table and view columns, with MySQL 8.4.9-shaped columns, matching system metadata, and `NULL` engine attributes; no complete built-in-schema column catalogs, storage-engine attributes, secondary engines, privileges, or physical data dictionary tables |
| `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | 🟡 | Queryable empty synthetic connection-control failed-login-attempts view with MySQL 8.4.9 activated-plugin-shaped columns and matching system metadata; no connection-control plugin loading, plugin inventory rows, account descriptors, failed-login counters, delayed login behavior, connection-control system variables, privileges, or authentication subsystem |
| `INFORMATION_SCHEMA.ENABLED_ROLES` | 🟡 | Queryable empty synthetic enabled-role metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, active-role state, default or mandatory roles, `SET ROLE`, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.ENGINES` | 🟡 | Limited queryable synthetic one-row catalog for MyLite's fixed default `InnoDB` engine, with MySQL 8.4.9-shaped columns and system-view metadata; no alternate engines, plugins, privileges, or engine internals |
| `INFORMATION_SCHEMA.EVENTS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real Event Scheduler descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE EVENT`, `ALTER EVENT`, `DROP EVENT`, event execution, scheduling, stored definitions, definers, privileges, or Event Scheduler state |
| `INFORMATION_SCHEMA.FILES` | 🟡 | Queryable synthetic tablespace-file catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the six observed fresh-runtime InnoDB file rows; no descriptor-owned user table file rows, physical InnoDB `.ibd` / `ibdata` / undo / temporary files, NDB Disk Data files, privileges, or mutable storage-engine accounting |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` | 🟡 | Queryable empty synthetic InnoDB buffer-pool page view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic buffer-pool page rows, page states, LRU residency, page ownership, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` | 🟡 | Queryable empty synthetic InnoDB buffer-pool LRU page view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic LRU page rows, LRU positions, compressed-page state, page ownership, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` | 🟡 | Queryable synthetic InnoDB buffer-pool statistics view with MySQL 8.4.9-shaped columns, matching system metadata, and one deterministic zero-valued `POOL_ID = 0` row; no dynamic InnoDB buffer-pool sizing, page residency, hit-rate or I/O accounting, multiple pool instances, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` | 🟡 | Queryable empty synthetic InnoDB cached-index-page view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic rows for cached MyLite index pages, InnoDB buffer-pool page residency, exact physical space/index ids, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_CMP` | 🟡 | Queryable synthetic InnoDB compression-statistics view with MySQL 8.4.9-shaped lower-case page-size/counter column metadata, matching system metadata, and five zero-counter page-size rows; no physical InnoDB compression, dynamic compressed-page statistics, `_RESET` counter side effects, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` | 🟡 | Queryable empty synthetic InnoDB per-index compression-statistics view with MySQL 8.4.9-shaped lower-case table/index/counter column metadata and matching system metadata; no `innodb_cmp_per_index_enabled` collection path, compressed-page statistics, `_RESET` counter side effects, privileges, or physical InnoDB compression behavior |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` | 🟡 | Queryable empty synthetic InnoDB per-index compression-statistics reset view with MySQL 8.4.9-shaped lower-case table/index/counter column metadata and matching system metadata; no `innodb_cmp_per_index_enabled` collection path, compressed-page statistics, `_RESET` counter side effects, privileges, or physical InnoDB compression behavior |
| `INFORMATION_SCHEMA.INNODB_CMP_RESET` | 🟡 | Queryable synthetic InnoDB compression-statistics reset view with MySQL 8.4.9-shaped lower-case page-size/counter column metadata, matching system metadata, and five zero-counter page-size rows; no physical InnoDB compression, dynamic compressed-page statistics, `_RESET` counter side effects, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_CMPMEM` | 🟡 | Queryable synthetic InnoDB compressed-page buffer-pool view with MySQL 8.4.9-shaped lower-case page-size, buffer-pool instance, page-use, and relocation column metadata, matching system metadata, and five zero-counter page-size rows; no physical InnoDB compression, dynamic buffer-pool memory statistics, `_RESET` counter side effects, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` | 🟡 | Queryable synthetic InnoDB compressed-page buffer-pool reset view with MySQL 8.4.9-shaped lower-case page-size, buffer-pool instance, page-use, and relocation column metadata, matching system metadata, and five zero-counter page-size rows; no physical InnoDB compression, dynamic buffer-pool memory statistics, `_RESET` counter side effects, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_COLUMNS` | 🟡 | Queryable descriptor-backed InnoDB column dictionary view for supported persistent base-table columns, with MySQL 8.4.9-shaped columns, matching system metadata, zero-based `POS`, observed `MTYPE` / `PRTYPE` / `LEN` mappings for current MyLite descriptor families, and `HAS_DEFAULT = 0` / `DEFAULT_VALUE = NULL`; no hidden InnoDB system columns, virtual-column `POS` bit encoding, instant-DDL default history, temporary-table rows, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_DATAFILES` | 🟡 | Queryable static default InnoDB datafile catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the observed four fresh-runtime rows for `ibdata1`, `sys/sys_config`, and the two undo files; no descriptor-owned user table files, physical `.ibd` storage, mutable tablespaces, privileges, or complete InnoDB data dictionary |
| `INFORMATION_SCHEMA.INNODB_FIELDS` | 🟡 | Queryable descriptor-backed InnoDB index-field dictionary view for supported persistent base-table index descriptors, with MySQL 8.4.9-shaped columns, matching system metadata, explicit key column names, and zero-based `POS`; no hidden clustered fields, physical InnoDB auxiliary fields, temporary-table rows, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_FOREIGN` | 🟡 | Queryable descriptor-backed InnoDB foreign-key dictionary view for supported persistent base-table foreign keys, with MySQL 8.4.9-shaped columns, matching system metadata, `schema/object` IDs and table names, `N_COLS`, and observed action-bit `TYPE` values for `CASCADE`, `SET NULL`, `NO ACTION`, and `RESTRICT`; no cross-schema foreign keys, temporary-table rows, physical InnoDB dictionary state, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` | 🟡 | Queryable descriptor-backed InnoDB foreign-key column dictionary view for supported persistent base-table foreign keys, with MySQL 8.4.9-shaped columns, matching system metadata, child and referenced column names, and observed `POS` ordering; no cross-schema foreign keys, temporary-table rows, physical InnoDB dictionary state, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED` | 🟡 | Queryable empty synthetic InnoDB full-text being-deleted document-id view with MySQL 8.4.9-shaped `DOC_ID bigint unsigned` metadata and matching system metadata; no `innodb_ft_aux_table`-driven rows, physical InnoDB full-text auxiliary tables, optimize/delete queues, privileges, or full-text search behavior |
| `INFORMATION_SCHEMA.INNODB_FT_CONFIG` | 🟡 | Queryable empty synthetic InnoDB full-text configuration view with MySQL 8.4.9-shaped `KEY` / `VALUE` column metadata and matching system metadata; no `innodb_ft_aux_table`-driven rows, physical InnoDB full-text auxiliary tables, parser plugins, privileges, or full-text search behavior |
| `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` | 🟡 | Queryable static InnoDB default full-text stopword catalog with MySQL 8.4.9-shaped lower-case `value` column metadata and the observed 36 default rows including duplicate `the`; catalog visibility does not imply full-text tokenization, `MATCH ... AGAINST`, parser plugins, custom stopword variables, privileges, or physical InnoDB full-text tables |
| `INFORMATION_SCHEMA.INNODB_FT_DELETED` | 🟡 | Queryable empty synthetic InnoDB full-text deleted document-id view with MySQL 8.4.9-shaped `DOC_ID bigint unsigned` metadata and matching system metadata; no `innodb_ft_aux_table`-driven rows, physical InnoDB full-text auxiliary tables, optimize/delete queues, privileges, or full-text search behavior |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` | 🟡 | Queryable empty synthetic InnoDB full-text index-cache view with MySQL 8.4.9-shaped `WORD`, document-id, count, and position column metadata and matching system metadata; no `innodb_ft_aux_table`-driven rows, physical InnoDB full-text auxiliary tables, token-position rows, privileges, or full-text search behavior |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` | 🟡 | Queryable empty synthetic InnoDB full-text index-table view with MySQL 8.4.9-shaped `WORD`, document-id, count, and position column metadata and matching system metadata; no `innodb_ft_aux_table`-driven rows, physical InnoDB full-text auxiliary tables, token-position rows, privileges, or full-text search behavior |
| `INFORMATION_SCHEMA.INNODB_INDEXES` | 🟡 | Queryable descriptor-backed InnoDB index dictionary view for supported persistent base-table primary, unique, nonunique, full-text, and spatial index descriptors, with MySQL 8.4.9-shaped columns, matching system metadata, MyLite descriptor ids, observed index `TYPE` code points, clustered unique-key fallback, synthetic `GEN_CLUST_INDEX` rows, and synthetic physical metadata values; no hidden full-text auxiliary indexes, exact clustered-record field counts, physical pages, tablespaces, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_METRICS` | 🟡 | Queryable empty synthetic InnoDB monitor-counter view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic monitor rows, counter collection, enable/disable/reset state, Performance Schema integration, privileges, or storage-engine instrumentation |
| `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` | 🟡 | Queryable synthetic session temporary-tablespace view with MySQL 8.4.9-shaped columns, matching system metadata, and the observed ten-row baseline pool including one current-session `ACTIVE` / `INTRINSIC` row keyed to MyLite's connection id; no dynamic `ACTIVE` / `USER` rows for user-created temporary tables, replica temporary tablespaces, physical `#innodb_temp` files, privilege filtering, or complete InnoDB temporary tablespace state |
| `INFORMATION_SCHEMA.INNODB_TABLES` | 🟡 | Queryable descriptor-backed InnoDB table dictionary view for supported persistent base-table descriptors, with MySQL 8.4.9-shaped columns, matching system metadata, `schema/table` names, MyLite table ids, observed row-format flag code points, synthetic `N_COLS`, `SPACE`, zip-page, space-type, instant-column, and row-version values; no physical InnoDB dictionary, tablespaces, temporary-table rows, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES` | 🟡 | Queryable synthetic full InnoDB tablespace catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the observed baseline rows for `sys/sys_config`, the two undo tablespaces, the temporary tablespace, and `mysql`; no descriptor-owned user table rows, physical `.ibd` storage, mutable tablespaces, privileges, or complete InnoDB data dictionary |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` | 🟡 | Queryable static default InnoDB tablespace summary catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the observed four fresh-runtime rows for system, sys config, and undo tablespaces; no descriptor-owned user table rows, physical tablespaces, temporary tablespaces, privileges, or complete InnoDB data dictionary |
| `INFORMATION_SCHEMA.INNODB_TABLESTATS` | 🟡 | Queryable descriptor-backed InnoDB table statistics view for supported persistent base-table descriptors, with MySQL 8.4.9-shaped columns, matching system metadata, `schema/table` names, MyLite table ids, exact current row counts, deterministic synthetic clustered-index, secondary-index, modified-counter, auto-increment, and ref-count fields; no physical InnoDB statistics cache, built-in `mysql` system-table rows, temporary-table rows, `ANALYZE TABLE` invalidation, privileges, or complete storage-engine metadata |
| `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` | 🟡 | Queryable empty synthetic InnoDB temporary-table metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic rows for user-created temporary tables, InnoDB table ids, generated InnoDB temporary names, temporary tablespace ids, privileges, or physical InnoDB temporary tablespaces |
| `INFORMATION_SCHEMA.INNODB_TRX` | 🟡 | Queryable empty synthetic InnoDB transaction-monitoring view with MySQL 8.4.9-shaped columns and matching system metadata; no dynamic rows for active MyLite transactions, lock waits, transaction weights, row-lock counters, current SQL text, Performance Schema lock integration, privileges, or physical InnoDB transaction state |
| `INFORMATION_SCHEMA.INNODB_VIRTUAL` | 🟡 | Limited descriptor-backed InnoDB virtual generated-column dependency view for supported persistent base-table descriptors, with MySQL 8.4.9-shaped columns, matching system metadata, MySQL-observed `POS` / `BASE_POS` encoding, duplicate base-reference collapsing, constant virtual-column omission, and close/reopen persistence; no temporary-table rows, privilege filtering, generated-expression dependencies beyond MyLite's current canonical integer generated-column subset, or physical InnoDB dictionary state |
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key, supported unique-index, and supported integer-family foreign-key columns on MyLite persistent base tables, plus ordered primary-key rows for supported synthetic `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats` tables, with supported no-key `mysql.general_log` and `mysql.slow_log` returning zero rows; includes current system-view column metadata, ordered rows for current composite primary-key and unique-index parts, and ordered referenced-table columns for supported one-column and composite FK descriptors; no non-integer foreign keys, views, temporary tables, privileges, unsupported mysql system-table rows, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.KEYWORDS` | 🟡 | Queryable synthetic MySQL 8.4.9 keyword catalog with `WORD` and integer `RESERVED` columns, matching system metadata, case-insensitive `WORD` predicates, numeric `RESERVED` predicates, and the current limited information-schema query surface; no parser behavior driven from the table, mutable keyword catalog, bare truth predicates, privilege filtering, or full information-schema query support |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS` | 🟡 | Target-build absent Enterprise Firewall table: standard MySQL 8.4.9 does not expose the plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from `INFORMATION_SCHEMA.TABLES`, `INFORMATION_SCHEMA.COLUMNS`, and `SHOW FULL TABLES`; no Enterprise Firewall plugin, account profile cache, or placeholder view |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST` | 🟡 | Target-build absent Enterprise Firewall table: standard MySQL 8.4.9 does not expose the deprecated plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from directory metadata; no firewall allowlist cache or placeholder view |
| `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map` | 🟡 | Target-build absent NDB Cluster table: standard MySQL 8.4.9 does not expose the NDB-only plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from directory metadata; no NDB Cluster integration or placeholder view |
| `INFORMATION_SCHEMA.OPTIMIZER_TRACE` | 🟡 | Queryable empty synthetic optimizer-trace catalog with MySQL 8.4.9-shaped columns and matching system metadata; no `optimizer_trace` variables, trace collection, JSON trace rows, memory-limit accounting, privilege filtering, or optimizer instrumentation |
| `INFORMATION_SCHEMA.PARAMETERS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real stored routine and parameter descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no stored routine DDL, `CALL`, routine execution, parameter descriptors, function return rows, stored definitions, definers, or privileges |
| `INFORMATION_SCHEMA.PARTITIONS` | 🟡 | Limited queryable synthetic partition metadata with one nonpartitioned row per supported persistent base table, one system-view row per supported `information_schema` view, MySQL 8.4.9-shaped columns, fixed nonpartitioned partition fields, descriptor-derived row-count and index-length statistics, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no partition DDL, partition descriptors, subpartition rows, partition pruning, partition maintenance statements, temporary-table rows, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.PLUGINS` | 🟡 | Limited queryable synthetic one-row catalog for MyLite's embedded active `InnoDB` storage-engine plugin surface, with MySQL 8.4.9-shaped columns and matching system metadata; no plugin loading, disabled plugin rows, alternate engines, authentication/audit/parser plugins, privileges, or complete MySQL plugin inventory |
| `INFORMATION_SCHEMA.PROCESSLIST` | 🟡 | Limited queryable synthetic process-list system view with one current embedded-handle row, MySQL 8.4.9-shaped columns, selected-schema `DB`, untruncated current-statement `INFO`, a deprecation warning when a row is read, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no server-wide threads, sleeping/background rows, privileges, Performance Schema, sys-schema views, deprecated access counters, or `KILL` |
| `INFORMATION_SCHEMA.PROFILING` | 🟡 | Queryable empty synthetic deprecated statement-profiling catalog with MySQL 8.4.9-shaped columns, matching system metadata, and direct-read deprecation warnings; no `profiling` variables, `SHOW PROFILE`, profiler collection, dynamic profiling rows, Performance Schema instrumentation, or privilege filtering |
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for supported descriptor-owned integer-family foreign keys, with referenced unique constraint name, `MATCH_OPTION = 'NONE'`, and stored `UPDATE_RULE` / `DELETE_RULE` values for the admitted `CASCADE`, `RESTRICT`, `NO ACTION`, and `SET NULL` subset; no `SET DEFAULT`, explicit `MATCH`, recursive action metadata, cross-schema references, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | 🟡 | Queryable synthetic default resource-group catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the built-in `USR_default` and `SYS_default` rows using an all-online-CPU `VCPU_IDS` range; no resource-group DDL, mutable groups, thread assignment, scheduler integration, optimizer hints, privilege filtering, or persisted resource-group descriptors |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | 🟡 | Queryable empty synthetic role column-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, grant descriptors, column privilege rows, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | 🟡 | Queryable empty synthetic role routine-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, grant descriptors, routine descriptors, routine privilege rows, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | 🟡 | Queryable empty synthetic role table-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no role graph, grant descriptors, table privilege rows, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real stored routine descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no stored routine DDL, `CALL`, routine execution, parameter descriptors, stored definitions, definers, or privileges |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | 🟡 | Queryable empty synthetic schema-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no schema grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.SCHEMATA` | 🟡 | Limited queryable synthetic rows for `information_schema`, `mysql`, `performance_schema`, `sys`, and MyLite catalog schemas with descriptor-owned user-schema default charset/collation metadata and fixed default-encryption metadata; no privileges, encryption mutation, schema extension rows, or full schema option catalog |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | 🟡 | Limited queryable synthetic schema-options rows for built-in and MyLite catalog schemas, with MySQL 8.4.9-shaped columns, matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata, and empty `OPTIONS` values; no `ALTER SCHEMA ... READ ONLY`, mutable read-only state, privileges, or physical data dictionary tables |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | 🟡 | Limited queryable synthetic spatial-column catalog with MySQL 8.4.9-shaped columns, matching system metadata, and descriptor rows for supported persistent base-table `GEOMETRY` family columns; `SRS_NAME` and `SRS_ID` remain SQL `NULL` until MyLite implements SRID attributes and SRS catalogs. No temporary-table rows, view rows, privilege filtering, non-`NULL` spatial values, spatial functions, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | 🟡 | Queryable empty synthetic spatial reference system catalog with MySQL 8.4.9-shaped columns and matching system metadata; no EPSG/SRID rows, SRID attributes, spatial value parsing, spatial function SRID validation, WKT parsing, SRS DDL, `mysql.st_spatial_reference_systems` data dictionary rows, or privilege filtering |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | 🟡 | Queryable static spatial unit catalog with MySQL 8.4.9-shaped columns, matching system metadata, and the observed 47 linear unit rows with empty descriptions; no `ST_Distance()` unit validation, spatial calculations, mutable units, SRS integration, or privilege filtering |
| `INFORMATION_SCHEMA.STATISTICS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary plus supported unique, nonunique secondary, metadata-only `FULLTEXT`, and metadata-only `SPATIAL` indexes on MyLite persistent base tables, plus primary-key and supported secondary-index rows for supported synthetic `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats` tables, with supported no-index `mysql.general_log` and `mysql.slow_log` returning zero rows. Persistent rows include `SUB_PART` for current supported string and binary prefix key parts, `COLLATION` values `A` / `D` for stored ascending / descending key-part direction, `INDEX_TYPE` values `BTREE` / `FULLTEXT` / `SPATIAL`, `INDEX_COMMENT` from descriptor index comments, empty `COMMENT` storage-engine placeholders, `IS_VISIBLE` from descriptor visibility metadata, `FULLTEXT` rows with `COLLATION` / `SUB_PART` as SQL `NULL`, and `SPATIAL` rows with `COLLATION = 'A'` and `SUB_PART = 32`, with fixed statistics placeholders; no functional index metadata, views, temporary tables, privileges, storage-engine statistics, full-text search behavior, spatial search behavior, unsupported mysql system-table rows, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key, supported unique-index, supported integer-family foreign-key, and supported check constraints on MyLite persistent base tables, plus primary-key rows for supported synthetic `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats` tables, with supported no-constraint `mysql.general_log` and `mysql.slow_log` returning zero rows, including current composite primary-key, composite unique-index, composite FK descriptors, and `CHECK` `ENFORCED='YES'` / `'NO'`; no non-integer foreign keys, views, temporary tables, privileges, unsupported mysql system-table rows, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | 🟡 | Limited queryable extension-attribute rows for descriptor-owned primary-key, unique-index, and foreign-key constraints on MyLite persistent base tables, plus `NULL`-attribute primary-key and supported secondary-index rows for supported synthetic `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, and `mysql.innodb_index_stats` tables, with supported no-constraint `mysql.general_log` and `mysql.slow_log` returning zero rows, with MySQL 8.4.9-shaped columns, matching system metadata, and `NULL` engine attributes; check constraints are omitted to match observed MySQL 8.4.9 behavior. No unsupported built-in-schema constraint catalogs, storage-engine attributes, secondary engines, temporary-table rows, privileges, or physical data dictionary tables |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | 🟡 | Queryable empty synthetic table-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no table grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.TABLES` | 🟡 | Limited queryable synthetic rows for MySQL 8.4.9 built-in schema table-directory names/types in `information_schema`, `mysql`, `performance_schema`, and `sys`, plus MyLite base-table descriptors and baseline view descriptors. Built-in rows are metadata-only directory rows and do not make unsupported system tables queryable; supported `mysql.user`, `mysql.global_grants`, `mysql.db`, `mysql.tables_priv`, `mysql.columns_priv`, `mysql.procs_priv`, `mysql.proxies_priv`, `mysql.default_roles`, `mysql.role_edges`, `mysql.password_history`, `mysql.component`, `mysql.func`, `mysql.plugin`, `mysql.server_cost`, `mysql.engine_cost`, `mysql.servers`, `mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, the `mysql.time_zone*` table family, `mysql.ndb_binlog_index`, the `mysql.slave_*` replication metadata table family, `mysql.innodb_table_stats`, `mysql.innodb_index_stats`, `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` rows include MySQL-observed table/view-status fields, while other built-in rows keep documented placeholders. User base/view rows remain descriptor-driven with current table-status, collation, comment, option, auto-increment, timestamp, and `TABLE_TYPE` metadata. No temporary tables, privileges, checksums, live system-table data, full storage-engine statistics, or complete system-table column catalogs |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | 🟡 | Limited queryable extension-attribute rows for built-in table-directory entries, MyLite base tables, and MyLite views, with MySQL 8.4.9-shaped columns, matching system metadata, and `NULL` engine attributes; no storage-engine attributes, secondary engines, privilege filtering, temporary-table rows, or physical data dictionary tables |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | 🟡 | Limited queryable tablespace extension-attribute rows for fixed MySQL 8.4.9 baseline InnoDB tablespace names plus descriptor-owned MyLite persistent base tables formatted as `schema/table`, with MySQL-shaped columns, matching system metadata, and `NULL` engine attributes; no physical tablespaces, `FILES`, `INNODB_TABLESPACES`, views, temporary-table rows, storage-engine attributes, privileges, or tablespace DDL |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | 🟡 | Target-build absent deprecated Enterprise Thread Pool table: standard MySQL 8.4.9 does not expose the plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from directory metadata; no thread-pool plugin state or placeholder view |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | 🟡 | Target-build absent deprecated Enterprise Thread Pool table: standard MySQL 8.4.9 does not expose the plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from directory metadata; no thread-pool statistics or placeholder view |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | 🟡 | Target-build absent deprecated Enterprise Thread Pool table: standard MySQL 8.4.9 does not expose the plugin view, so direct reads and `SHOW COLUMNS` reject with `1109 / 42S02` and the name is absent from directory metadata; no thread-pool thread-state snapshots or placeholder view |
| `INFORMATION_SCHEMA.TRIGGERS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, metadata-only rows for the two built-in `sys.sys_config` triggers, empty user rows until MyLite implements real trigger descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE TRIGGER`, `DROP TRIGGER`, trigger execution, stored trigger definitions beyond the sys metadata placeholders, privileges, or SQLite trigger reflection |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | 🟡 | Limited synthetic user-attribute row for MyLite's embedded `root@%` identity with `ATTRIBUTE = NULL`, MySQL 8.4.9-shaped columns, and matching system metadata; no account storage, comments, arbitrary attributes, MySQL system-account rows, privilege filtering, or `mysql.user` table |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | 🟡 | Limited synthetic global privilege rows for MyLite's embedded `root@%` identity with MySQL 8.4.9-shaped columns and matching system metadata; no account storage, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | 🟡 | Queryable synthetic view-to-stored-function dependency catalog with MySQL 8.4.9-shaped columns, matching system metadata, built-in dependency rows from `sys.innodb_lock_waits` to `sys.format_statement` and `sys.quote_identifier`, from `sys.x$innodb_lock_waits` to `sys.quote_identifier`, from `sys.latest_file_io` to `sys.format_path`, from `sys.io_global_by_file_by_bytes` to `sys.format_path`, from `sys.io_global_by_file_by_latency` to `sys.format_path`, from `sys.x$ps_schema_table_statistics_io` to `sys.extract_schema_from_file_name` and `sys.extract_table_from_file_name`, from `sys.schema_table_lock_waits` to `sys.format_statement` and `sys.ps_thread_account`, and from `sys.x$schema_table_lock_waits` to `sys.ps_thread_account`; no stored routine descriptors, broader view-to-routine dependency analysis, native/loadable function rows, sys helper-function execution, privilege filtering, or view execution |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | 🟡 | Queryable synthetic view-dependency catalog with MySQL 8.4.9-shaped columns, one source-table dependency row for each baseline direct single-base-table view, one dependency from `sys.ps_check_lost_instrumentation` to `performance_schema.global_status`, dependencies from both `sys.host_summary` views to `performance_schema.accounts`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_statement_latency`, and `sys.x$memory_by_host_by_current_bytes`, dependencies from both `sys.host_summary_by_file_io` views to `performance_schema.events_waits_summary_by_host_by_event_name`, dependencies from both `sys.host_summary_by_file_io_type` views to `performance_schema.events_waits_summary_by_host_by_event_name`, dependencies from both `sys.host_summary_by_stages` views to `performance_schema.events_stages_summary_by_host_by_event_name`, dependencies from both `sys.host_summary_by_statement_latency` views to `performance_schema.events_statements_summary_by_host_by_event_name`, dependencies from both `sys.host_summary_by_statement_type` views to `performance_schema.events_statements_summary_by_host_by_event_name`, dependencies from both `sys.innodb_buffer_stats_by_schema` views and both `sys.innodb_buffer_stats_by_table` views to `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE`, dependencies from both `sys.memory_by_host_by_current_bytes` views to `performance_schema.memory_summary_by_host_by_event_name`, dependencies from both `sys.innodb_lock_waits` views to `INFORMATION_SCHEMA.INNODB_TRX`, `performance_schema.data_lock_waits`, and `performance_schema.data_locks`, one dependency from `sys.innodb_lock_waits` to `sys.sys_config`, dependencies from both `sys.latest_file_io` views to `INFORMATION_SCHEMA.PROCESSLIST`, `performance_schema.events_waits_history_long`, and `performance_schema.threads`, one dependency from `sys.latest_file_io` to `performance_schema.global_variables`, dependencies from both `sys.io_by_thread_by_latency` views to `performance_schema.events_waits_summary_by_thread_by_event_name` and `performance_schema.threads`, dependencies from both `sys.io_global_by_file_by_bytes` views to `performance_schema.file_summary_by_instance`, one dependency from `sys.io_global_by_file_by_bytes` to `performance_schema.global_variables`, dependencies from both `sys.io_global_by_file_by_latency` views to `performance_schema.file_summary_by_instance`, one dependency from `sys.io_global_by_file_by_latency` to `performance_schema.global_variables`, dependencies from both `sys.io_global_by_wait_by_bytes` views to `performance_schema.file_summary_by_event_name`, dependencies from both `sys.io_global_by_wait_by_latency` views to `performance_schema.file_summary_by_event_name`, one dependency from `sys.x$ps_schema_table_statistics_io` to `performance_schema.file_summary_by_instance`, built-in dependency rows from `sys.schema_auto_increment_columns` to `INFORMATION_SCHEMA.COLUMNS` and `INFORMATION_SCHEMA.TABLES`, built-in dependency rows from `sys.schema_index_statistics` and `sys.x$schema_index_statistics` to `performance_schema.table_io_waits_summary_by_index_usage`, built-in dependency rows from `sys.schema_object_overview` to `INFORMATION_SCHEMA.EVENTS`, `ROUTINES`, `STATISTICS`, `TABLES`, and `TRIGGERS`, one dependency from `sys.schema_redundant_indexes` to `sys.x$schema_flattened_keys`, one dependency from `sys.x$schema_flattened_keys` to `INFORMATION_SCHEMA.STATISTICS`, dependencies from both `sys.schema_table_lock_waits` views to `performance_schema.events_statements_current`, `performance_schema.metadata_locks`, and `performance_schema.threads`, one dependency from `sys.schema_table_lock_waits` to `sys.sys_config`, dependencies from both `sys.schema_table_statistics` views to `performance_schema.table_io_waits_summary_by_table` and `sys.x$ps_schema_table_statistics_io`, dependencies from both `sys.schema_table_statistics_with_buffer` views to `performance_schema.table_io_waits_summary_by_table`, `sys.x$ps_schema_table_statistics_io`, and `sys.x$innodb_buffer_stats_by_table`, dependencies from both `sys.schema_tables_with_full_table_scans` views to `performance_schema.table_io_waits_summary_by_index_usage`, and dependencies from `sys.schema_unused_indexes` to `INFORMATION_SCHEMA.STATISTICS` and `performance_schema.table_io_waits_summary_by_index_usage`; no dependencies on routines, joins, subqueries outside supported built-in definitions, privilege filtering, or view execution |
| `INFORMATION_SCHEMA.VIEWS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, rows for baseline view descriptors and the supported built-in `sys.version`, `sys.host_summary`, `sys.host_summary_by_file_io`, `sys.host_summary_by_file_io_type`, `sys.host_summary_by_stages`, `sys.host_summary_by_statement_latency`, `sys.host_summary_by_statement_type`, `sys.innodb_buffer_stats_by_schema`, `sys.innodb_buffer_stats_by_table`, `sys.ps_check_lost_instrumentation`, `sys.innodb_lock_waits`, `sys.io_by_thread_by_latency`, `sys.io_global_by_file_by_bytes`, `sys.io_global_by_file_by_latency`, `sys.io_global_by_wait_by_bytes`, `sys.io_global_by_wait_by_latency`, `sys.latest_file_io`, `sys.memory_by_host_by_current_bytes`, `sys.schema_auto_increment_columns`, `sys.schema_index_statistics`, `sys.schema_object_overview`, `sys.schema_redundant_indexes`, `sys.schema_table_lock_waits`, `sys.schema_table_statistics`, `sys.schema_table_statistics_with_buffer`, `sys.schema_tables_with_full_table_scans`, `sys.schema_unused_indexes`, `sys.x$host_summary`, `sys.x$host_summary_by_file_io`, `sys.x$host_summary_by_file_io_type`, `sys.x$host_summary_by_stages`, `sys.x$host_summary_by_statement_latency`, `sys.x$host_summary_by_statement_type`, `sys.x$innodb_buffer_stats_by_schema`, `sys.x$innodb_buffer_stats_by_table`, `sys.x$innodb_lock_waits`, `sys.x$io_by_thread_by_latency`, `sys.x$io_global_by_file_by_bytes`, `sys.x$io_global_by_file_by_latency`, `sys.x$io_global_by_wait_by_bytes`, `sys.x$io_global_by_wait_by_latency`, `sys.x$latest_file_io`, `sys.x$memory_by_host_by_current_bytes`, `sys.x$ps_schema_table_statistics_io`, `sys.x$schema_flattened_keys`, `sys.x$schema_index_statistics`, `sys.x$schema_table_lock_waits`, `sys.x$schema_table_statistics`, `sys.x$schema_table_statistics_with_buffer`, and `sys.x$schema_tables_with_full_table_scans` views, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; definitions, fixed definer/security/check metadata, charset/collation, and deliberately non-executable status are stored or synthesized by MyLite, but there is no privilege filtering, check-option enforcement, view execution, or complete MySQL system catalogs |

[Back to compatibility overview](../../COMPATIBILITY.md)
