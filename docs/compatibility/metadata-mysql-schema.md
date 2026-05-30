# mysql system schema and data dictionary

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

The `mysql` schema name is exposed in the limited built-in schema catalog and
can be selected with `USE mysql`. The MySQL 8.4.9 target runtime's built-in
`mysql` table names are exposed as metadata-only rows through
`INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`, and
`SHOW TABLE STATUS`. The tables remain non-queryable and unsupported unless
listed otherwise below; the current exceptions are limited read-only
`SELECT` access to empty `mysql.component` and `mysql.func`, the limited
connection-control registry rows in `mysql.plugin`, default optimizer cost
rows in `mysql.server_cost` and `mysql.engine_cost`, empty `mysql.servers`,
`mysql.gtid_executed`, `mysql.general_log`, `mysql.slow_log`, and empty
placeholder `mysql.time_zone*` reads plus
`mysql.innodb_table_stats` and `mysql.innodb_index_stats`, along with
`SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` /
`SHOW INDEXES` / `SHOW KEYS`, MySQL-observed `INFORMATION_SCHEMA.TABLES` /
`SHOW TABLE STATUS` status fields, and `INFORMATION_SCHEMA.STATISTICS` /
`TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` /
`TABLE_CONSTRAINTS_EXTENSIONS` primary-key shape metadata or zero-row no-index
metadata for those supported tables. MyLite rejects schema, table, index,
rename, truncate, and
single-table DML writes targeting `mysql` with
`3552 / HY000` system-schema diagnostics as a stricter embedded-design
decision; MySQL 8.4.9 permits some `root` temporary-table writes in `mysql`.

| Table | Status | Notes |
| --- | --- | --- |
| `mysql.catalogs` | ❌ | Dictionary table: catalog metadata |
| `mysql.character_sets` | ❌ | Dictionary table: character set metadata |
| `mysql.check_constraints` | ❌ | Dictionary table: CHECK constraint metadata |
| `mysql.collations` | ❌ | Dictionary table: collation metadata |
| `mysql.column_statistics` | ❌ | Dictionary table: histogram statistics |
| `mysql.column_type_elements` | ❌ | Dictionary table: ENUM/SET and other column type elements |
| `mysql.columns` | ❌ | Dictionary table: table column metadata |
| `mysql.dd_properties` | ❌ | Dictionary table: dictionary version and upgrade metadata |
| `mysql.events` | ❌ | Dictionary table: Event Scheduler metadata |
| `mysql.foreign_keys` | ❌ | Dictionary table: foreign key metadata |
| `mysql.foreign_key_column_usage` | ❌ | Dictionary table: foreign key column mappings |
| `mysql.index_column_usage` | ❌ | Dictionary table: index column usage |
| `mysql.index_partitions` | ❌ | Dictionary table: index partition metadata |
| `mysql.index_stats` | ❌ | Dictionary table: dynamic index statistics |
| `mysql.indexes` | ❌ | Dictionary table: index metadata |
| `mysql.innodb_ddl_log` | ❌ | Dictionary table: crash-safe DDL logs |
| `mysql.parameter_type_elements` | ❌ | Dictionary table: routine parameter and return type elements |
| `mysql.parameters` | ❌ | Table shape and diagnostics |
| `mysql.resource_groups` | ❌ | Dictionary table: resource group metadata |
| `mysql.routines` | ❌ | Dictionary table: stored procedure and function metadata |
| `mysql.schemata` | ❌ | Dictionary table: schema metadata |
| `mysql.st_spatial_reference_systems` | ❌ | Dictionary table: spatial reference systems |
| `mysql.table_partition_values` | ❌ | Dictionary table: partition values |
| `mysql.table_partitions` | ❌ | Dictionary table: table partition metadata |
| `mysql.table_stats` | ❌ | Dictionary table: dynamic table statistics |
| `mysql.tables` | ❌ | Dictionary table: table metadata |
| `mysql.tablespace_files` | ❌ | Dictionary table: tablespace files |
| `mysql.tablespaces` | ❌ | Dictionary table: active tablespaces |
| `mysql.triggers` | ❌ | Dictionary table: trigger metadata |
| `mysql.view_routine_usage` | ❌ | Dictionary table: view-to-routine dependencies |
| `mysql.view_table_usage` | ❌ | Dictionary table: view-to-table dependencies |
| `mysql.user` | ❌ | Table shape and diagnostics |
| `mysql.global_grants` | ❌ | Grant table: dynamic global privilege assignments |
| `mysql.db` | ❌ | Grant table: database-level privileges |
| `mysql.tables_priv` | ❌ | Grant table: table-level privileges |
| `mysql.columns_priv` | ❌ | Grant table: column-level privileges |
| `mysql.procs_priv` | ❌ | Grant table: routine privileges |
| `mysql.proxies_priv` | ❌ | Grant table: proxy-user privileges |
| `mysql.default_roles` | ❌ | Grant table: default role activation |
| `mysql.role_edges` | ❌ | Grant table: role graph edges |
| `mysql.password_history` | ❌ | Grant table: password history |
| `mysql.component` | 🟡 | Limited read-only empty component registry table with MySQL 8.4.9-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no installed component rows, component DDL, component loading, persisted component services, privilege filtering, or writable system table |
| `mysql.func` | 🟡 | Limited read-only empty loadable-function registry table with MySQL 8.4.9-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loadable-function rows, loadable-function DDL, function loading, persisted `mysql.func` rows, privilege filtering, or writable system table |
| `mysql.plugin` | 🟡 | Limited read-only plugin registry table with the target MySQL 8.4.9 connection-control plugin rows, MySQL-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no plugin loading, plugin lifecycle, mutable plugin registry, expanded `SHOW PLUGINS` / `INFORMATION_SCHEMA.PLUGINS` inventory, privilege filtering, or writable system table |
| `mysql.general_log` | 🟡 | Limited read-only empty CSV general-query-log table with MySQL 8.4.9-shaped columns, no index/constraint metadata rows, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` empty results, and `SHOW TABLE STATUS`; no log-row storage, logging side effects, log output routing, log rotation, DDL on log tables, privilege filtering, or writable system table |
| `mysql.slow_log` | 🟡 | Limited read-only empty CSV slow-query-log table with MySQL 8.4.9-shaped columns, no index/constraint metadata rows, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` empty results, and `SHOW TABLE STATUS`; no slow-query logging, log-row storage, log output routing, log rotation, DDL on log tables, privilege filtering, or writable system table |
| `mysql.help_category` | ❌ | HELP category table |
| `mysql.help_keyword` | ❌ | HELP keyword table |
| `mysql.help_relation` | ❌ | HELP relation table |
| `mysql.help_topic` | ❌ | HELP topic table |
| `mysql.time_zone` | 🟡 | Limited read-only empty placeholder time-zone ID and leap-second usage table with MySQL 8.4.9-shaped columns, auto-increment primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loaded time-zone rows, `mysql_tzinfo_to_sql` import, named-zone conversion, leap-second behavior, privilege filtering, or writable system table |
| `mysql.time_zone_leap_second` | 🟡 | Limited read-only empty placeholder leap-second transition table with MySQL 8.4.9-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loaded leap-second rows, leap-second adjustment, privilege filtering, or writable system table |
| `mysql.time_zone_name` | 🟡 | Limited read-only empty placeholder time-zone name mapping table with MySQL 8.4.9-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loaded named-zone rows, named time-zone lookup beyond current fixed support, privilege filtering, or writable system table |
| `mysql.time_zone_transition` | 🟡 | Limited read-only empty placeholder time-zone transition table with MySQL 8.4.9-shaped columns, composite primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loaded transition rows, daylight-saving conversion, privilege filtering, or writable system table |
| `mysql.time_zone_transition_type` | 🟡 | Limited read-only empty placeholder time-zone transition type table with MySQL 8.4.9-shaped columns, composite primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no loaded transition-type rows, named-zone offset/abbreviation resolution, privilege filtering, or writable system table |
| `mysql.gtid_executed` | 🟡 | Limited read-only empty GTID persistence table with MySQL 8.4.9-shaped columns, column comments, composite primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no GTID rows, binary logging, GTID mode or persistence, GTID table compression, replication state, reset-side effects, privilege filtering, or writable system table |
| `mysql.ndb_binlog_index` | ❌ | NDB Cluster replication binary log information table |
| `mysql.slave_master_info` | ❌ | Table shape and diagnostics |
| `mysql.slave_relay_log_info` | ❌ | Replication metadata repository table for relay log metadata |
| `mysql.slave_worker_info` | ❌ | Replication metadata repository table for worker metadata |
| `mysql.innodb_index_stats` | 🟡 | Limited read-only synthetic optimizer table for InnoDB persistent index statistics: direct `SELECT` and unqualified reads after `USE mysql`, stable built-in rows for `mysql.component.PRIMARY` and `sys.sys_config.PRIMARY`, descriptor-backed rows for persistent base-table primary, unique secondary, nonunique secondary, prefix-key-part, and generated clustered indexes, exact MyLite distinct indexed-prefix counts, deterministic page-count placeholders, matching `INFORMATION_SCHEMA.COLUMNS` metadata, MySQL-shaped `INFORMATION_SCHEMA.TABLES` / `SHOW TABLE STATUS` status metadata, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE` metadata, and MySQL-shaped `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` plus `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` primary-key metadata; no writable persistent statistics, `ANALYZE TABLE` side effects, statistics reload, histograms, full-text/spatial auxiliary statistics rows, optimizer behavior, physical page counts, privilege filtering, or complete data-dictionary tables |
| `mysql.innodb_table_stats` | 🟡 | Limited read-only synthetic optimizer table for InnoDB persistent table statistics: direct `SELECT` and unqualified reads after `USE mysql`, stable built-in rows for `mysql.component` and `sys.sys_config`, descriptor-backed rows for persistent base tables, exact row counts, deterministic clustered-index placeholder size, non-primary descriptor index counts, matching `INFORMATION_SCHEMA.COLUMNS` metadata, MySQL-shaped `INFORMATION_SCHEMA.TABLES` / `SHOW TABLE STATUS` status metadata, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE` metadata, and MySQL-shaped `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` plus `INFORMATION_SCHEMA.STATISTICS`, `TABLE_CONSTRAINTS`, `KEY_COLUMN_USAGE`, and `TABLE_CONSTRAINTS_EXTENSIONS` primary-key metadata; no writable persistent statistics, `ANALYZE TABLE` side effects, statistics reload, histograms, optimizer behavior, physical page counts, privilege filtering, or complete data-dictionary tables |
| `mysql.server_cost` | 🟡 | Limited read-only optimizer cost table with the target MySQL 8.4.9 default cost rows, MySQL-shaped columns including the generated `default_value` expression metadata, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no mutable cost overrides, optimizer cost reload, `FLUSH OPTIMIZER_COSTS` behavior, privilege filtering, or writable system table |
| `mysql.engine_cost` | 🟡 | Limited read-only optimizer engine cost table with the target MySQL 8.4.9 default engine/device cost rows, MySQL-shaped columns including the generated `default_value` expression metadata, composite primary-key metadata in MySQL-observed key order, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no engine-specific cost overrides, optimizer cost reload, `FLUSH OPTIMIZER_COSTS` behavior, privilege filtering, or writable system table |
| `mysql.audit_log_filter` | ❌ | Table shape and diagnostics |
| `mysql.audit_log_user` | ❌ | Enterprise Audit table for persistent audit user mappings |
| `mysql.firewall_group_allowlist` | ❌ | Enterprise Firewall table for group profile allowlists |
| `mysql.firewall_groups` | ❌ | Enterprise Firewall table for group profiles |
| `mysql.firewall_membership` | ❌ | Enterprise Firewall table for group profile memberships |
| `mysql.firewall_users` | ❌ | Enterprise Firewall table for account profiles |
| `mysql.firewall_whitelist` | ❌ | Deprecated Enterprise Firewall allowlist table |
| `mysql.servers` | 🟡 | Limited read-only empty FEDERATED server-definition table with MySQL 8.4.9-shaped columns, primary-key metadata, `INFORMATION_SCHEMA.COLUMNS` / `TABLES` / `STATISTICS` / `TABLE_CONSTRAINTS` / `KEY_COLUMN_USAGE` / `TABLE_CONSTRAINTS_EXTENSIONS` rows, `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE`, `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS`, and `SHOW TABLE STATUS`; no server-definition rows, `CREATE SERVER` / `ALTER SERVER` / `DROP SERVER`, FEDERATED storage-engine connections, persisted server definitions, privilege filtering, or writable system table |
| `mysql.innodb_dynamic_metadata` | ❌ | Table shape and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
