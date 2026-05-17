# INFORMATION_SCHEMA tables

Metadata rows include base MySQL objects plus optional plugin, Enterprise, NDB Cluster, and debug/development objects documented or shipped with MySQL 8.4.9. Each implementation should match the target build availability.

Access semantics are limited but MySQL-shaped: `USE information_schema`
succeeds, the current metadata `SELECT` subset can resolve unqualified metadata
table names while that schema is selected, and currently supported mutating
schema, table, index, rename, truncate, and single-table DML statements that
target `information_schema` fail with `1044 / 42000` access denied diagnostics.
MyLite does not implement a privilege engine or writable system views.

| Table | Status | Notes |
| --- | --- | --- |
| `INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS` | ❌ | Grantable users or roles for current user or role |
| `INFORMATION_SCHEMA.APPLICABLE_ROLES` | ❌ | Applicable roles for current user |
| `INFORMATION_SCHEMA.CHARACTER_SETS` | 🟡 | Limited queryable synthetic catalog for MyLite's fixed `binary`, `ascii`, and `utf8mb4` character-set rows, with MySQL 8.4.9-shaped columns and system-view metadata; no alternate character sets, conversion, privileges, or complete MySQL catalog |
| `INFORMATION_SCHEMA.CHECK_CONSTRAINTS` | 🟡 | Queryable synthetic check-constraint catalog with MySQL 8.4.9-shaped columns, matching system metadata, and descriptor rows for the current limited persistent-table `CHECK` subset including supported `ALTER TABLE ... ADD/DROP/ALTER CHECK`; no complete expression coverage, temporary-table checks, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` | 🟡 | Limited queryable synthetic catalog mapping the currently admitted MyLite `binary` collation to `binary`, admitted `ascii` collations to `ascii`, and admitted `utf8mb4` collations to `utf8mb4`, with MySQL 8.4.9-shaped columns and system-view metadata; no other character sets, other collations, comparison semantics, privileges, or complete MySQL catalog |
| `INFORMATION_SCHEMA.COLLATIONS` | 🟡 | Limited queryable synthetic catalog for `binary`, `ascii_general_ci`, `ascii_bin`, `utf8mb4_0900_ai_ci`, `utf8mb4_0900_bin`, `utf8mb4_general_ci`, `utf8mb4_bin`, `utf8mb4_unicode_ci`, and `utf8mb4_unicode_520_ci`, with MySQL 8.4.9-shaped columns and system-view metadata; no other collations, comparison semantics, privileges, or complete MySQL catalog |
| `INFORMATION_SCHEMA.COLUMN_PRIVILEGES` | 🟡 | Queryable empty synthetic column-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no column grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.COLUMN_STATISTICS` | ❌ | Histogram statistics for column values |
| `INFORMATION_SCHEMA.COLUMNS` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite descriptor columns, including current integer/exact `DECIMAL`/approximate `FLOAT` and `DOUBLE`/canonical `YEAR`/canonical `DATE`/canonical `TIME`/canonical `DATETIME`/canonical `TIMESTAMP`/`CHAR`/`VARCHAR`/baseline `TEXT` family/binary string/`BIT`/limited `ENUM`/limited `SET`/limited `JSON` metadata, nullability, integer, decimal, approximate, literal `CHAR`/`VARCHAR` string, matching `ENUM` label string, matching `SET` member-list string, canonical year-string, canonical date-string, canonical time-string, canonical datetime-string, canonical timestamp-string, compatible `BIT` default, `JSON DEFAULT NULL`, and generated `curdate()`, `curtime()`, and `CURRENT_TIMESTAMP` defaults, visibility, primary-key plus supported unique/nonunique secondary-index and metadata-only first-part `FULLTEXT` `COLUMN_KEY` markers, `DEFAULT_GENERATED`, `on update CURRENT_TIMESTAMP`, auto-increment `EXTRA`, decimal and approximate `NUMERIC_PRECISION` / `NUMERIC_SCALE` metadata, binary string byte-length metadata, `BIT` numeric precision metadata with SQL `NULL` character set and collation names, enum/set `DATA_TYPE`, `COLUMN_TYPE`, max-display character/octet lengths, JSON `DATA_TYPE` / `COLUMN_TYPE` with SQL `NULL` character/numeric/datetime metadata, effective table/column character set and collation names for admitted character descriptors, `NULL` numeric precision/scale and datetime precision for `YEAR`, `NULL` temporal precision for `DATE`, and `DATETIME_PRECISION = 0` for `TIME`, `DATETIME`, and `TIMESTAMP`; no privileges, generated columns, comments, binary string defaults, explicit `TEXT`-family string defaults, string/hex defaults for `BIT`, numeric or expression enum/set defaults, JSON expression defaults, empty-string set members, warning-producing overlength string-default truncation, charset conversion or comparison semantics, or complete MySQL system catalogs |
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
| `INFORMATION_SCHEMA.KEYWORDS` | 🟡 | Queryable synthetic MySQL 8.4.9 keyword catalog with `WORD` and integer `RESERVED` columns, matching system metadata, case-insensitive `WORD` predicates, numeric `RESERVED` predicates, and the current limited information-schema query surface; no parser behavior driven from the table, mutable keyword catalog, bare truth predicates, privilege filtering, or full information-schema query support |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_USERS` | ❌ | Deprecated table shape |
| `INFORMATION_SCHEMA.MYSQL_FIREWALL_WHITELIST` | ❌ | Deprecated table shape |
| `INFORMATION_SCHEMA.ndb_transid_mysql_connection_map` | ❌ | Conditional NDB transaction map |
| `INFORMATION_SCHEMA.OPTIMIZER_TRACE` | ❌ | Optimizer trace output |
| `INFORMATION_SCHEMA.PARAMETERS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real stored routine and parameter descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no stored routine DDL, `CALL`, routine execution, parameter descriptors, function return rows, stored definitions, definers, or privileges |
| `INFORMATION_SCHEMA.PARTITIONS` | ❌ | Table partition information |
| `INFORMATION_SCHEMA.PLUGINS` | ❌ | Plugin information |
| `INFORMATION_SCHEMA.PROCESSLIST` | 🟡 | Limited queryable synthetic process-list system view with one current embedded-handle row, MySQL 8.4.9-shaped columns, selected-schema `DB`, untruncated current-statement `INFO`, a deprecation warning when a row is read, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no server-wide threads, sleeping/background rows, privileges, Performance Schema, sys-schema views, deprecated access counters, or `KILL` |
| `INFORMATION_SCHEMA.PROFILING` | ❌ | Statement profiling information |
| `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for supported descriptor-owned integer-family foreign keys, with referenced unique constraint name, `MATCH_OPTION = 'NONE'`, and stored `UPDATE_RULE` / `DELETE_RULE` values for the admitted `CASCADE`, `RESTRICT`, `NO ACTION`, and `SET NULL` subset; no `SET DEFAULT`, explicit `MATCH`, recursive action metadata, cross-schema references, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.RESOURCE_GROUPS` | ❌ | Resource group information |
| `INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROLE_TABLE_GRANTS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.ROUTINES` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real stored routine descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no stored routine DDL, `CALL`, routine execution, parameter descriptors, stored definitions, definers, or privileges |
| `INFORMATION_SCHEMA.SCHEMA_PRIVILEGES` | 🟡 | Queryable empty synthetic schema-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no schema grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.SCHEMATA` | 🟡 | Limited queryable synthetic rows for `information_schema` and MyLite catalog schemas with descriptor-owned default charset/collation metadata and fixed default-encryption metadata; no `mysql`, `performance_schema`, `sys`, privileges, encryption mutation, or full schema option catalog |
| `INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS` | ❌ | Schema options |
| `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS` | ❌ | Columns in each table that store spatial data |
| `INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS` | ❌ | Available spatial reference systems |
| `INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE` | ❌ | Acceptable units for ST_Distance() |
| `INFORMATION_SCHEMA.STATISTICS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary plus supported unique, nonunique secondary, and metadata-only `FULLTEXT` indexes on MyLite persistent base tables, including `SUB_PART` for current supported string prefix key parts, `COLLATION` values `A` / `D` for stored ascending / descending key-part direction, and `FULLTEXT` rows with `COLLATION` / `SUB_PART` as SQL `NULL`, with fixed statistics placeholders; no spatial/functional index metadata, views, temporary tables, privileges, storage-engine statistics, full-text search behavior, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` | 🟡 | Limited queryable synthetic rows for descriptor-owned primary-key, supported unique-index, supported integer-family foreign-key, and supported check constraints on MyLite persistent base tables, including current composite primary-key, composite unique-index, composite FK descriptors, and `CHECK` `ENFORCED='YES'` / `'NO'`; no non-integer foreign keys, views, temporary tables, privileges, or complete MySQL system catalogs |
| `INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS` | ❌ | Table shape and diagnostics |
| `INFORMATION_SCHEMA.TABLE_PRIVILEGES` | 🟡 | Queryable empty synthetic table-privilege metadata view with MySQL 8.4.9-shaped columns and matching system metadata; no table grant descriptors, accounts, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.TABLES` | 🟡 | Limited queryable synthetic rows for supported `information_schema` system views and MyLite base-table descriptors, with fixed InnoDB/table-status placeholders, descriptor-owned table collation, descriptor-owned auto-increment metadata, descriptor-owned creation/update timestamps rendered in the session time zone, and descriptor-derived non-primary-index `INDEX_LENGTH` placeholders; includes system-view rows for current metadata views such as `CHECK_CONSTRAINTS`, `EVENTS`, `KEYWORDS`, `PARAMETERS`, `PROCESSLIST`, `ROUTINES`, `TRIGGERS`, `VIEWS`, and `VIEW_TABLE_USAGE`, but no server-wide process rows, stored event/routine/trigger/view rows, view dependency rows, temporary tables, system schemas beyond the supported views, privileges, comments, checksums, or full storage-engine statistics |
| `INFORMATION_SCHEMA.TABLES_EXTENSIONS` | ❌ | Table attributes for primary and secondary storage engines |
| `INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS` | ❌ | Tablespace attributes for primary storage engines |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATE` | ❌ | Thread pool thread group states |
| `INFORMATION_SCHEMA.TP_THREAD_GROUP_STATS` | ❌ | Thread pool thread group statistics |
| `INFORMATION_SCHEMA.TP_THREAD_STATE` | ❌ | Thread pool thread information |
| `INFORMATION_SCHEMA.TRIGGERS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real trigger descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE TRIGGER`, `DROP TRIGGER`, trigger execution, stored trigger definitions, definers, privileges, or SQLite trigger reflection |
| `INFORMATION_SCHEMA.USER_ATTRIBUTES` | ❌ | User comments and attributes |
| `INFORMATION_SCHEMA.USER_PRIVILEGES` | 🟡 | Limited synthetic global privilege rows for MyLite's embedded `root@%` identity with MySQL 8.4.9-shaped columns and matching system metadata; no account storage, roles, grants, revokes, privilege filtering, or enforcement |
| `INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE` | ❌ | Stored functions used in views |
| `INFORMATION_SCHEMA.VIEW_TABLE_USAGE` | 🟡 | Queryable empty synthetic view-dependency catalog with MySQL 8.4.9-shaped columns and matching system metadata; no stored view descriptors, dependency rows, privilege filtering, or view execution |
| `INFORMATION_SCHEMA.VIEWS` | 🟡 | Queryable synthetic system view with MySQL 8.4.9-shaped columns, empty user rows until MyLite implements real view descriptors, and matching `INFORMATION_SCHEMA.TABLES` / `INFORMATION_SCHEMA.COLUMNS` metadata; no `CREATE VIEW`, view execution, stored definitions, dependencies, privileges, or complete MySQL system catalogs |

[Back to compatibility overview](../../COMPATIBILITY.md)
