#ifndef MYLITE_EXECUTION_CATALOG_H
#define MYLITE_EXECUTION_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_execution_catalog_information_schema_table_kind {
    MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA = 0,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLES = 1,
    MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS = 2,
    MYLITE_EXECUTION_CATALOG_TABLE_CHARACTER_SETS = 3,
    MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS = 4,
    MYLITE_EXECUTION_CATALOG_TABLE_ENGINES = 5,
    MYLITE_EXECUTION_CATALOG_TABLE_EVENTS = 6,
    MYLITE_EXECUTION_CATALOG_TABLE_PARAMETERS = 7,
    MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST = 8,
    MYLITE_EXECUTION_CATALOG_TABLE_ROUTINES = 9,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS = 10,
    MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE = 11,
    MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS = 12,
    MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS = 13,
    MYLITE_EXECUTION_CATALOG_TABLE_TRIGGERS = 14,
    MYLITE_EXECUTION_CATALOG_TABLE_VIEWS = 15,
    MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS = 16,
    MYLITE_EXECUTION_CATALOG_TABLE_COLLATION_CHARACTER_SET_APPLICABILITY = 17,
    MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_PRIVILEGES = 18,
    MYLITE_EXECUTION_CATALOG_TABLE_SCHEMA_PRIVILEGES = 19,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLE_PRIVILEGES = 20,
    MYLITE_EXECUTION_CATALOG_TABLE_USER_PRIVILEGES = 21,
    MYLITE_EXECUTION_CATALOG_TABLE_KEYWORDS = 22,
    MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE = 23,
    MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS = 24,
    MYLITE_EXECUTION_CATALOG_TABLE_PLUGINS = 25,
    MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS = 26,
    MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS = 27,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS = 28,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS = 29,
    MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS = 30,
    MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_STATISTICS = 31,
    MYLITE_EXECUTION_CATALOG_TABLE_ROLE_COLUMN_GRANTS = 32,
    MYLITE_EXECUTION_CATALOG_TABLE_ROLE_ROUTINE_GRANTS = 33,
    MYLITE_EXECUTION_CATALOG_TABLE_ROLE_TABLE_GRANTS = 34,
    MYLITE_EXECUTION_CATALOG_TABLE_ADMINISTRABLE_ROLE_AUTHORIZATIONS = 35,
    MYLITE_EXECUTION_CATALOG_TABLE_APPLICABLE_ROLES = 36,
    MYLITE_EXECUTION_CATALOG_TABLE_ENABLED_ROLES = 37,
    MYLITE_EXECUTION_CATALOG_TABLE_USER_ATTRIBUTES = 38,
    MYLITE_EXECUTION_CATALOG_TABLE_VIEW_ROUTINE_USAGE = 39,
    MYLITE_EXECUTION_CATALOG_TABLE_OPTIMIZER_TRACE = 40,
    MYLITE_EXECUTION_CATALOG_TABLE_PROFILING = 41,
    MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS = 42,
    MYLITE_EXECUTION_CATALOG_TABLE_RESOURCE_GROUPS = 43,
    MYLITE_EXECUTION_CATALOG_TABLE_ST_UNITS_OF_MEASURE = 44,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_DATAFILES = 45,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES_BRIEF = 46,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TEMP_TABLE_INFO = 47,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DEFAULT_STOPWORD = 48,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_CONFIG = 49,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_BEING_DELETED = 50,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DELETED = 51,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_CACHE = 52,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_TABLE = 53,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX = 54,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX_RESET = 55,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP = 56,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_RESET = 57,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM = 58,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM_RESET = 59,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN = 60,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS = 61,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS = 62,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES = 63,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES = 64,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS = 65,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES = 66,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS = 67,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_SESSION_TEMP_TABLESPACES = 68,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL = 69,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TRX = 70,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE = 71,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE_LRU = 72,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_POOL_STATS = 73,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CACHED_INDEXES = 74,
    MYLITE_EXECUTION_CATALOG_TABLE_FILES = 75,
    MYLITE_EXECUTION_CATALOG_TABLE_INNODB_METRICS = 76,
    MYLITE_EXECUTION_CATALOG_TABLE_ST_SPATIAL_REFERENCE_SYSTEMS = 77,
    MYLITE_EXECUTION_CATALOG_TABLE_CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS = 78,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COMPONENT = 79,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_FUNC = 80,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVERS = 81,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GTID_EXECUTED = 82,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GENERAL_LOG = 83,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLOW_LOG = 84,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PLUGIN = 85,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ENGINE_COST = 86,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVER_COST = 87,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE = 88,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_LEAP_SECOND = 89,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_NAME = 90,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION = 91,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION_TYPE = 92,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_NDB_BINLOG_INDEX = 93,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_MASTER_INFO = 94,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_RELAY_LOG_INFO = 95,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_WORKER_INFO = 96,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_CATEGORY = 97,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_KEYWORD = 98,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_RELATION = 99,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_TOPIC = 100,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_USER = 101,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GLOBAL_GRANTS = 102,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DB = 103,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TABLES_PRIV = 104,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COLUMNS_PRIV = 105,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROCS_PRIV = 106,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROXIES_PRIV = 107,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DEFAULT_ROLES = 108,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ROLE_EDGES = 109,
    MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PASSWORD_HISTORY = 110,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG = 111,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION = 112,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS = 113,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_OBJECT_OVERVIEW = 114,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_INDEX_STATISTICS = 115,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS = 116,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_REDUNDANT_INDEXES = 117,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_FLATTENED_KEYS = 118,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_LOCK_WAITS = 119,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS = 120,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS = 121,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS = 122,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER = 123,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER = 124,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS = 125,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS = 126,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_UNUSED_INDEXES = 127,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION = 128,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_LOCK_WAITS = 129,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_LOCK_WAITS = 130,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_LATEST_FILE_IO = 131,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_LATEST_FILE_IO = 132,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO = 133,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_BYTES = 134,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES = 135,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_LATENCY = 136,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY = 137,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_BYTES = 138,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES = 139,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY = 140,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY = 141,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_BY_THREAD_BY_LATENCY = 142,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_BY_THREAD_BY_LATENCY = 143,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY = 144,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY = 145,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO = 146,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO = 147,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE = 148,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE = 149,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES = 150,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STAGES = 151,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY = 152,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY = 153,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE = 154,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE = 155,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_SCHEMA = 156,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA = 157,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_TABLE = 158,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_TABLE = 159,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES = 160,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES = 161,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES = 162,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES = 163,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_USER_BY_CURRENT_BYTES = 164,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES = 165,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES = 166,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES = 167,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_GLOBAL_TOTAL = 168,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_GLOBAL_TOTAL = 169,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_DIGEST_AVG_LATENCY_DISTRIBUTION = 170,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_DIGEST_95TH_PERCENTILE_BY_AVG_US = 171,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_METRICS = 172,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_PROCESSLIST = 173,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PROCESSLIST = 174,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SESSION = 175,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SESSION = 176,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_SESSION_SSL_STATUS = 177,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENT_ANALYSIS = 178,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENT_ANALYSIS = 179,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_ERRORS_OR_WARNINGS = 180,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_ERRORS_OR_WARNINGS = 181,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_FULL_TABLE_SCANS = 182,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_FULL_TABLE_SCANS = 183,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE = 184,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE = 185,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_SORTING = 186,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_SORTING = 187,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_TEMP_TABLES = 188,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_TEMP_TABLES = 189,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY = 190,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY = 191,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO = 192,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO = 193,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO_TYPE = 194,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO_TYPE = 195,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STAGES = 196,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STAGES = 197,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_LATENCY = 198,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_LATENCY = 199,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_TYPE = 200,
    MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_TYPE = 201,
};

struct mylite_execution_catalog_character_set {
    const char *name;
    const char *default_collation;
    const char *description;
    const char *maxlen;
};

struct mylite_execution_catalog_collation {
    const char *name;
    const char *character_set;
    const char *id;
    const char *is_default;
    const char *compiled;
    const char *sortlen;
    const char *pad_attribute;
};

struct mylite_execution_catalog_scalar_collation {
    const char *collation;
    const char *charset;
    uint32_t id;
};

struct mylite_execution_catalog_keyword {
    const char *word;
    const char *reserved;
};

struct mylite_execution_catalog_column_definition {
    const char *name;
    const char *column_default;
    const char *is_nullable;
    const char *data_type;
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *numeric_precision;
    const char *numeric_scale;
    const char *datetime_precision;
    const char *character_set_name;
    const char *collation_name;
    const char *column_type;
};

struct mylite_execution_catalog_table_definition {
    enum mylite_execution_catalog_information_schema_table_kind kind;
    const char *name;
    const struct mylite_execution_catalog_column_definition *columns;
    size_t column_count;
};

struct mylite_execution_catalog_mysql_system_secondary_index {
    const char *name;
    size_t column_index;
    const char *cardinality;
    const char *non_unique;
    bool is_unique;
};

struct mylite_execution_catalog_mysql_system_table {
    const char *schema_name;
    struct mylite_execution_catalog_table_definition query_definition;
    const char *const *column_keys;
    const char *const *column_extras;
    const char *const *column_privileges;
    const char *const *column_comments;
    const size_t *primary_key_column_indexes;
    size_t primary_key_column_count;
    const char *const *column_generation_expressions;
    const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_indexes;
    size_t secondary_index_count;
};

struct mylite_execution_catalog_files_row {
    const char *file_id;
    const char *file_name;
    const char *file_type;
    const char *tablespace_name;
    const char *free_extents;
    const char *total_extents;
    const char *initial_size;
    const char *autoextend_size;
    const char *data_free;
};

struct mylite_execution_catalog_innodb_tablespace_row {
    const char *space;
    const char *name;
    const char *path;
    const char *flag;
    const char *space_type;
};

struct mylite_execution_catalog_innodb_tablespace_full_row {
    const char *space;
    const char *name;
    const char *flag;
    const char *row_format;
    const char *page_size;
    const char *zip_page_size;
    const char *space_type;
    const char *fs_block_size;
    const char *file_size;
    const char *allocated_size;
    const char *autoextend_size;
    const char *server_version;
    const char *space_version;
    const char *encryption;
    const char *state;
};

struct mylite_execution_catalog_innodb_session_temp_tablespace_row {
    const char *space;
    const char *path;
    const char *state;
    const char *purpose;
};

struct mylite_execution_catalog_st_unit_of_measure_row {
    const char *unit_name;
    const char *conversion_factor;
};

struct mylite_execution_catalog_builtin_schema {
    const char *name;
    const char *default_charset;
    const char *default_collation;
};

struct mylite_execution_catalog_builtin_schema_table_directory {
    const char *schema_name;
    const char *const *table_names;
    size_t table_count;
};

struct mylite_execution_catalog_builtin_sys_view {
    const char *name;
    const char *view_definition;
    const char *show_create_view_sql;
    const char *show_create_qualified_view_sql;
};

struct mylite_execution_catalog_sys_config_trigger {
    const char *name;
    const char *event;
};

const struct mylite_execution_catalog_character_set *mylite_execution_catalog_character_set_by_name(
    const char *name
);
const struct mylite_execution_catalog_collation *mylite_execution_catalog_collation_by_name(
    const char *name
);
const struct mylite_execution_catalog_collation *mylite_execution_catalog_utf8mb4_collation_by_name(
    const char *name
);
const struct mylite_execution_catalog_scalar_collation *mylite_execution_catalog_scalar_collation_info_by_name(
    const char *collation_name
);

size_t mylite_execution_catalog_mysql_character_set_count(void);
const struct mylite_execution_catalog_character_set *mylite_execution_catalog_mysql_character_set_at(
    size_t index
);
size_t mylite_execution_catalog_mysql_collation_count(void);
const struct mylite_execution_catalog_collation *mylite_execution_catalog_mysql_collation_at(
    size_t index
);
bool mylite_execution_catalog_mysql_collation_index_by_name(const char *name, size_t *out_index);
size_t mylite_execution_catalog_keyword_count(void);
const struct mylite_execution_catalog_keyword *mylite_execution_catalog_keyword_at(size_t index);

size_t mylite_execution_catalog_information_schema_table_definition_count(void);
const struct mylite_execution_catalog_table_definition *mylite_execution_catalog_information_schema_table_definition_at(
    size_t index
);
const struct mylite_execution_catalog_table_definition *mylite_execution_catalog_information_schema_table_definition_by_name(
    const char *table_name
);

size_t mylite_execution_catalog_mysql_system_table_definition_count(void);
const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_at(
    size_t index
);
const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_by_name(
    const char *schema_name,
    const char *table_name
);

size_t mylite_execution_catalog_builtin_schema_count(void);
const struct mylite_execution_catalog_builtin_schema *mylite_execution_catalog_builtin_schema_at(
    size_t index
);
const struct mylite_execution_catalog_builtin_schema *mylite_execution_catalog_builtin_schema_by_name(
    const char *schema_name
);

size_t mylite_execution_catalog_builtin_schema_table_directory_count(void);
const struct mylite_execution_catalog_builtin_schema_table_directory *mylite_execution_catalog_builtin_schema_table_directory_at(
    size_t index
);
const struct mylite_execution_catalog_builtin_schema_table_directory *mylite_execution_catalog_builtin_schema_table_directory_by_name(
    const char *schema_name
);

size_t mylite_execution_catalog_information_schema_files_row_count(void);
const struct mylite_execution_catalog_files_row *mylite_execution_catalog_information_schema_files_row_at(
    size_t index
);
size_t mylite_execution_catalog_innodb_tablespace_row_count(void);
const struct mylite_execution_catalog_innodb_tablespace_row *mylite_execution_catalog_innodb_tablespace_row_at(
    size_t index
);
size_t mylite_execution_catalog_innodb_tablespace_full_row_count(void);
const struct mylite_execution_catalog_innodb_tablespace_full_row *mylite_execution_catalog_innodb_tablespace_full_row_at(
    size_t index
);
size_t mylite_execution_catalog_innodb_session_temp_tablespace_row_count(void);
const struct mylite_execution_catalog_innodb_session_temp_tablespace_row *mylite_execution_catalog_innodb_session_temp_tablespace_row_at(
    size_t index
);
size_t mylite_execution_catalog_st_unit_of_measure_row_count(void);
const struct mylite_execution_catalog_st_unit_of_measure_row *mylite_execution_catalog_st_unit_of_measure_row_at(
    size_t index
);

const char *mylite_execution_catalog_sys_sys_config_trigger_action_statement(void);
const char *mylite_execution_catalog_sys_sys_config_trigger_sql_mode(void);
size_t mylite_execution_catalog_sys_sys_config_trigger_count(void);
const struct mylite_execution_catalog_sys_config_trigger *mylite_execution_catalog_sys_sys_config_trigger_at(
    size_t index
);

size_t mylite_execution_catalog_builtin_sys_view_definition_count(void);
const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_at(
    size_t index
);
const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_by_name(
    const char *view_name
);

size_t mylite_execution_catalog_builtin_tablespace_extension_name_count(void);
const char *mylite_execution_catalog_builtin_tablespace_extension_name_at(size_t index);
size_t mylite_execution_catalog_innodb_compressed_page_size_count(void);
const char *mylite_execution_catalog_innodb_compressed_page_size_at(size_t index);
size_t mylite_execution_catalog_innodb_ft_default_stopword_count(void);
const char *mylite_execution_catalog_innodb_ft_default_stopword_at(size_t index);

#endif
