# mysql system schema and data dictionary

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

The `mysql` schema name is exposed in the limited built-in schema catalog and
can be selected with `USE mysql`. The MySQL 8.4.9 target runtime's built-in
`mysql` table names are exposed as metadata-only rows through
`INFORMATION_SCHEMA.TABLES`, `SHOW TABLES`, `SHOW FULL TABLES`, and
`SHOW TABLE STATUS`. The tables remain non-queryable and unsupported unless
listed otherwise below; the current exceptions are limited read-only
`SELECT` access to `mysql.innodb_table_stats` and
`mysql.innodb_index_stats`, plus `SHOW COLUMNS` / `SHOW FULL COLUMNS` /
`DESCRIBE` and `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` shape metadata for
those two tables. MyLite rejects schema, table, index, rename,
truncate, and single-table DML writes targeting `mysql` with
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
| `mysql.component` | ❌ | Registry: server components installed with INSTALL COMPONENT |
| `mysql.func` | ❌ | Registry: loadable functions installed with CREATE FUNCTION |
| `mysql.plugin` | ❌ | Registry: server-side plugins installed with INSTALL PLUGIN |
| `mysql.general_log` | ❌ | CSV log table for the general query log; limited `@@sql_log_off` scalar reads do not create or write this table |
| `mysql.slow_log` | ❌ | CSV log table for the slow query log |
| `mysql.help_category` | ❌ | HELP category table |
| `mysql.help_keyword` | ❌ | HELP keyword table |
| `mysql.help_relation` | ❌ | HELP relation table |
| `mysql.help_topic` | ❌ | HELP topic table |
| `mysql.time_zone` | ❌ | Time zone ID and leap-second usage table |
| `mysql.time_zone_leap_second` | ❌ | Leap-second transition table |
| `mysql.time_zone_name` | ❌ | Time zone name mapping table |
| `mysql.time_zone_transition` | ❌ | Time zone transition table |
| `mysql.time_zone_transition_type` | ❌ | Time zone transition type table |
| `mysql.gtid_executed` | ❌ | Replication table storing GTID values |
| `mysql.ndb_binlog_index` | ❌ | NDB Cluster replication binary log information table |
| `mysql.slave_master_info` | ❌ | Table shape and diagnostics |
| `mysql.slave_relay_log_info` | ❌ | Replication metadata repository table for relay log metadata |
| `mysql.slave_worker_info` | ❌ | Replication metadata repository table for worker metadata |
| `mysql.innodb_index_stats` | 🟡 | Limited read-only synthetic optimizer table for InnoDB persistent index statistics: direct `SELECT` and unqualified reads after `USE mysql`, stable built-in rows for `mysql.component.PRIMARY` and `sys.sys_config.PRIMARY`, descriptor-backed rows for persistent base-table primary, unique secondary, nonunique secondary, prefix-key-part, and generated clustered indexes, exact MyLite distinct indexed-prefix counts, deterministic page-count placeholders, matching `INFORMATION_SCHEMA.COLUMNS` metadata, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE` metadata, and MySQL-shaped `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` primary-key metadata; no writable persistent statistics, `ANALYZE TABLE` side effects, statistics reload, histograms, full-text/spatial auxiliary statistics rows, optimizer behavior, physical page counts, privilege filtering, or complete data-dictionary tables |
| `mysql.innodb_table_stats` | 🟡 | Limited read-only synthetic optimizer table for InnoDB persistent table statistics: direct `SELECT` and unqualified reads after `USE mysql`, stable built-in rows for `mysql.component` and `sys.sys_config`, descriptor-backed rows for persistent base tables, exact row counts, deterministic clustered-index placeholder size, non-primary descriptor index counts, matching `INFORMATION_SCHEMA.COLUMNS` metadata, MySQL-shaped `SHOW COLUMNS` / `SHOW FULL COLUMNS` / `DESCRIBE` metadata, and MySQL-shaped `SHOW INDEX` / `SHOW INDEXES` / `SHOW KEYS` primary-key metadata; no writable persistent statistics, `ANALYZE TABLE` side effects, statistics reload, histograms, optimizer behavior, physical page counts, privilege filtering, or complete data-dictionary tables |
| `mysql.server_cost` | ❌ | Table shape and diagnostics |
| `mysql.engine_cost` | ❌ | Table shape and diagnostics |
| `mysql.audit_log_filter` | ❌ | Table shape and diagnostics |
| `mysql.audit_log_user` | ❌ | Enterprise Audit table for persistent audit user mappings |
| `mysql.firewall_group_allowlist` | ❌ | Enterprise Firewall table for group profile allowlists |
| `mysql.firewall_groups` | ❌ | Enterprise Firewall table for group profiles |
| `mysql.firewall_membership` | ❌ | Enterprise Firewall table for group profile memberships |
| `mysql.firewall_users` | ❌ | Enterprise Firewall table for account profiles |
| `mysql.firewall_whitelist` | ❌ | Deprecated Enterprise Firewall allowlist table |
| `mysql.servers` | ❌ | FEDERATED storage engine server definition table |
| `mysql.innodb_dynamic_metadata` | ❌ | Table shape and diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
