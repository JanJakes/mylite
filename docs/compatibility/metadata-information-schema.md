# INFORMATION_SCHEMA tables

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

| Table | Status | Notes |
| --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | ❌ | Grantable users or roles for current user or role |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | ❌ | Applicable roles for current user |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | 🟡 | Limited queryable synthetic one-row catalog for MyLite's fixed `utf8mb4` character set, with MySQL 8.4.9-shaped columns and system-view metadata; no alternate character sets, conversion, privileges, or complete MySQL catalog |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | ❌ | Table and column CHECK constraints |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | ❌ | Charset for each collation |
| `INFORMATION_SCHEMA.COLLATIONS` | 🟡 | Limited queryable synthetic catalog for `utf8mb4_0900_ai_ci`, `utf8mb4_general_ci`, `utf8mb4_bin`, `utf8mb4_unicode_ci`, and `utf8mb4_unicode_520_ci`, with MySQL 8.4.9-shaped columns and system-view metadata; no other collations, collation applicability table, comparison semantics, privileges, or complete MySQL catalog |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | ❌ | Privileges on columns |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | ❌ | Histogram statistics for column values |
| `INFORMATION_SCHEMA.COLUMNS` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite descriptor columns, including current integer/exact `DECIMAL`/approximate `FLOAT` and `DOUBLE`/canonical `YEAR`/canonical `DATE`/canonical `TIME`/canonical `DATETIME`/canonical `TIMESTAMP`/`CHAR`/`VARCHAR`/baseline `TEXT` family/binary string/`BIT`/limited `ENUM`/limited `SET`/limited `JSON` metadata, nullability, integer, decimal, approximate, literal `CHAR`/`VARCHAR` string, matching `ENUM` label string, matching `SET` member-list string, canonical year-string, canonical date-string, canonical time-string, canonical datetime-string, canonical timestamp-string, compatible `BIT` default, `JSON DEFAULT NULL`, and `CURRENT_TIMESTAMP` defaults, visibility, primary-key plus supported unique/nonunique secondary-index `COLUMN_KEY` markers, `DEFAULT_GENERATED`, `on update CURRENT_TIMESTAMP`, auto-increment `EXTRA`, decimal and approximate `NUMERIC_PRECISION` / `NUMERIC_SCALE` metadata, binary string byte-length metadata, `BIT` numeric precision metadata with SQL `NULL` character set and collation names, enum/set `DATA_TYPE`, `COLUMN_TYPE`, max-display character/octet lengths, JSON `DATA_TYPE` / `COLUMN_TYPE` with SQL `NULL` character/numeric/datetime metadata, and table character set/collation names, `NULL` numeric precision/scale and datetime precision for `YEAR`, `NULL` temporal precision for `DATE`, and `DATETIME_PRECISION = 0` for `TIME`, `DATETIME`, and `TIMESTAMP`; no privileges, generated columns, comments, binary string defaults, explicit `TEXT`-family string defaults, string/hex defaults for `BIT`, numeric or expression enum/set defaults, JSON expression defaults, empty-string set members, warning-producing overlength string-default truncation, full charset/collation behavior, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.COLUMNS_EXTENSIONS` | ❌ | Column attributes for primary and secondary storage engines |
| `INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS` | ❌ | Failed login attempts per account |
| `INFORMATION_SCHEMA.ENABLED_ROLES` | ❌ | Roles enabled within current session |
| `INFORMATION_SCHEMA.ENGINES` | 🟡 | Limited queryable synthetic one-row catalog for MyLite's fixed default `InnoDB` engine, with MySQL 8.4.9-shaped columns and system-view metadata; no alternate engines, plugins, privileges, or engine internals |
| `INFORMATION_SCHEMA.EVENTS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real Event Scheduler descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE EVENT`, `ALTER EVENT`, `DROP EVENT`, event execution, scheduling, stored definitions, definers, privileges, or Event Scheduler state |
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
| `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key, supported unique-index, and supported integer-family foreign-key columns on MyLite persistent base tables; includes current system-view column metadata, ordered rows for current composite primary-key and unique-index parts, and ordered referenced-table columns for supported one-column and composite FK descriptors; no non-integer foreign keys, views, temporary tables, privileges, or complete MySQL system catalogs |
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
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for supported descriptor-owned integer-family foreign keys, with referenced unique constraint name, `MATCH_OPTION = 'NONE'`, and default `UPDATE_RULE` / `DELETE_RULE = 'NO ACTION'`; no cascades, `SET NULL`, explicit `MATCH`, cross-schema references, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | ❌ | Resource group information |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real stored routine descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no stored routine DDL, `CALL`, routine execution, parameters, stored definitions, definers, privileges, or `INFORMATION_SCHEMA.PARAMETERS` rows |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | ❌ | Privileges on schemas |
| `INFORMATION_SCHEMA.SCHEMATA` | 🟡 | Limited queryable synthetic rows for `information_schema` and MyLite catalog schemas with fixed charset/collation/default-encryption metadata; no `mysql`, `performance_schema`, `sys`, privileges, or schema options |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | ❌ | Schema options |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | ❌ | Columns in each table that store spatial data |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | ❌ | Available spatial reference systems |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | ❌ | Acceptable units for ST_Distance() |
| `INFORMATION_SCHEMA.STATISTICS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary plus supported unique and nonunique secondary indexes on MyLite persistent base tables, including `SUB_PART` for current nonunique and one-part unique string prefix key parts, with fixed BTREE/statistics placeholders; no fulltext/spatial/functional/descending index metadata, views, temporary tables, privileges, storage-engine statistics, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key, supported unique-index, and supported integer-family foreign-key constraints on MyLite persistent base tables, including current composite primary-key, composite unique-index, and composite FK descriptors plus fixed `ENFORCED='YES'`; no check constraints, non-integer foreign keys, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | ❌ | Privileges on tables |
| `INFORMATION_SCHEMA.TABLES` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite base-table descriptors, with fixed InnoDB/table-status placeholders, descriptor-owned table collation, and descriptor-owned auto-increment metadata; no views, temporary tables, system schemas beyond the supported views, privileges, timestamps, or full storage-engine statistics |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | ❌ | Table attributes for primary and secondary storage engines |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | ❌ | Tablespace attributes for primary storage engines |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | ❌ | Thread pool thread group states |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | ❌ | Thread pool thread group statistics |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | ❌ | Thread pool thread information |
| `INFORMATION_SCHEMA.TRIGGERS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real trigger descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE TRIGGER`, `DROP TRIGGER`, trigger execution, stored trigger definitions, definers, privileges, or SQLite trigger reflection |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | ❌ | User comments and attributes |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | ❌ | Privileges defined globally per user |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | ❌ | Stored functions used in views |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | ❌ | Tables and views used in views |
| `INFORMATION_SCHEMA.VIEWS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real view descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE VIEW`, view execution, stored definitions, dependencies, privileges, or complete MySQL system catalogs |

[Back to compatibility overview](../../COMPATIBILITY.md)
