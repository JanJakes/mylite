# INFORMATION_SCHEMA tables

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

| Table | Status | Notes |
| --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | ❌ | Grantable users or roles for current user or role |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | ❌ | Applicable roles for current user |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | ❌ | Character set catalog |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | ❌ | Table and column CHECK constraints |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | ❌ | Charset for each collation |
| `INFORMATION_SCHEMA.COLLATIONS` | ❌ | Collation catalog |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | ❌ | Privileges on columns |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | ❌ | Histogram statistics for column values |
| `INFORMATION_SCHEMA.COLUMNS` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite descriptor columns, including current integer/exact `DECIMAL`/canonical `DATE`/`CHAR`/`VARCHAR`/baseline `TEXT` family metadata, nullability, integer, decimal, and canonical date-string defaults, visibility, primary-key plus supported unique/nonunique secondary-index `COLUMN_KEY` markers, auto-increment `EXTRA`, decimal `NUMERIC_PRECISION` / `NUMERIC_SCALE`, and `NULL` temporal precision for `DATE`; no privileges, generated columns, comments, string defaults outside canonical `DATE`, full charset/collation behavior, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS` | ❌ | Column attributes for primary and secondary storage engines |
| `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | ❌ | Failed login attempts per account |
| `INFORMATION_SCHEMA.ENABLED_ROLES` | ❌ | Roles enabled within current session |
| `INFORMATION_SCHEMA.ENGINES` | ❌ | Storage engine properties |
| `INFORMATION_SCHEMA.EVENTS` | ❌ | Event Manager events |
| `INFORMATION_SCHEMA.FILES` | ❌ | Files that store tablespace data |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE` | ❌ | Pages in InnoDB buffer pool |
| `INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU` | ❌ | LRU ordering of pages in InnoDB buffer pool |
| `INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS` | ❌ | InnoDB buffer pool statistics |
| `INFORMATION_SCHEMA.INNODB_CACHED_INDEXES` | ❌ | Number of index pages cached per index in InnoDB buffer pool |
| `INFORMATION_SCHEMA.INNODB_CMP` | ❌ | Status for operations related to compressed InnoDB tables |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_CMP_PER_INDEX_RESET` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_CMP_RESET` | ❌ | Status for operations related to compressed InnoDB tables |
| `INFORMATION_SCHEMA.INNODB_CMPMEM` | ❌ | Status for compressed pages within InnoDB buffer pool |
| `INFORMATION_SCHEMA.INNODB_CMPMEM_RESET` | ❌ | Status for compressed pages within InnoDB buffer pool |
| `INFORMATION_SCHEMA.INNODB_COLUMNS` | ❌ | Columns in each InnoDB table |
| `INFORMATION_SCHEMA.INNODB_DATAFILES` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_FIELDS` | ❌ | Key columns of InnoDB indexes |
| `INFORMATION_SCHEMA.INNODB_FOREIGN` | ❌ | InnoDB foreign-key metadata |
| `INFORMATION_SCHEMA.INNODB_FOREIGN_COLS` | ❌ | InnoDB foreign-key column status information |
| `INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED` | ❌ | Snapshot of INNODB_FT_DELETED table |
| `INFORMATION_SCHEMA.INNODB_FT_CONFIG` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD` | ❌ | Default list of stopwords for InnoDB FULLTEXT indexes |
| `INFORMATION_SCHEMA.INNODB_FT_DELETED` | ❌ | Rows deleted from InnoDB table FULLTEXT index |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_INDEXES` | ❌ | InnoDB index metadata |
| `INFORMATION_SCHEMA.INNODB_METRICS` | ❌ | InnoDB performance information |
| `INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES` | ❌ | Session temporary-tablespace metadata |
| `INFORMATION_SCHEMA.INNODB_TABLES` | ❌ | InnoDB table metadata |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES` | ❌ | InnoDB file-per-table, general, and undo tablespace metadata |
| `INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_TABLESTATS` | ❌ | InnoDB table low-level status information |
| `INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.INNODB_TRX` | ❌ | Active InnoDB transaction information |
| `INFORMATION_SCHEMA.INNODB_VIRTUAL` | ❌ | InnoDB virtual generated column metadata |
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key and supported unique-index columns on MyLite persistent base tables; includes current system-view column metadata, one row per single-column constraint, and NULL referenced-table columns; no foreign keys, composite keys, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.KEYWORDS` | ❌ | MySQL keywords |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS` | ❌ | Deprecated table shape |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST` | ❌ | Deprecated table shape |
| `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map` | ❌ | Conditional NDB transaction map |
| `INFORMATION_SCHEMA.OPTIMIZER_TRACE` | ❌ | Optimizer trace output |
| `INFORMATION_SCHEMA.PARAMETERS` | ❌ | Stored routine parameters and stored function return values |
| `INFORMATION_SCHEMA.PARTITIONS` | ❌ | Table partition information |
| `INFORMATION_SCHEMA.PLUGINS` | ❌ | Plugin information |
| `INFORMATION_SCHEMA.PROCESSLIST` | ❌ | Executing thread metadata |
| `INFORMATION_SCHEMA.PROFILING` | ❌ | Statement profiling information |
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | ❌ | Foreign key information |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | ❌ | Resource group information |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROUTINES` | ❌ | Stored routine information |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | ❌ | Privileges on schemas |
| `INFORMATION_SCHEMA.SCHEMATA` | 🟡 | Limited queryable synthetic rows for `information_schema` and MyLite catalog schemas with fixed charset/collation/default-encryption metadata; no `mysql`, `performance_schema`, `sys`, privileges, or schema options |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | ❌ | Schema options |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | ❌ | Columns in each table that store spatial data |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | ❌ | Available spatial reference systems |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | ❌ | Acceptable units for ST_Distance() |
| `INFORMATION_SCHEMA.STATISTICS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary plus supported unique and nonunique secondary indexes on MyLite persistent base tables, with fixed BTREE/statistics placeholders; no fulltext/spatial/functional/prefix/descending index metadata, views, temporary tables, privileges, storage-engine statistics, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key and supported unique-index constraints on MyLite persistent base tables, with fixed `ENFORCED='YES'`; no check constraints, foreign keys, composite keys, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | ❌ | Privileges on tables |
| `INFORMATION_SCHEMA.TABLES` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite base-table descriptors, with fixed InnoDB/table-status placeholders and descriptor-owned auto-increment metadata; no views, temporary tables, system schemas beyond the supported views, privileges, timestamps, or full storage-engine statistics |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | ❌ | Table attributes for primary and secondary storage engines |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | ❌ | Tablespace attributes for primary storage engines |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | ❌ | Thread pool thread group states |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | ❌ | Thread pool thread group statistics |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | ❌ | Thread pool thread information |
| `INFORMATION_SCHEMA.TRIGGERS` | ❌ | Trigger information |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | ❌ | User comments and attributes |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | ❌ | Privileges defined globally per user |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | ❌ | Stored functions used in views |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | ❌ | Tables and views used in views |
| `INFORMATION_SCHEMA.VIEWS` | ❌ | View information |

[Back to compatibility overview](../../COMPATIBILITY.md)
