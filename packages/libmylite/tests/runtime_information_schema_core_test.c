#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown_table_in_schema = 1109,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_column_metadata {
    const char *label;
    const char *schema_name;
    const char *table_name;
    const char *origin_schema_name;
    const char *origin_table_name;
    const char *origin_column_name;
    enum mylite_result_column_type type;
    uint32_t flag_mask;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    int nullable;
};

static int test_information_schema_result_metadata(void);
static int test_information_schema_core_queries(void);
static int test_information_schema_wordpress_bridge_queries(void);
static int test_information_schema_doctrine_bridge_queries(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_columns(
    mylite_db *database,
    const char *sql,
    const char *const *columns,
    size_t column_count,
    const char *context
);
static int expect_query_result_metadata(
    mylite_db *database,
    const char *sql,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
);
static int expect_result_metadata(
    const mylite_result *result,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
);
static int expect_stmt_metadata(
    const mylite_stmt *stmt,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_information_schema_result_metadata();
    failures += test_information_schema_core_queries();
    failures += test_information_schema_wordpress_bridge_queries();
    failures += test_information_schema_doctrine_bridge_queries();
    return failures == 0 ? 0 : 1;
}

static int test_information_schema_result_metadata(void) {
    static const struct expected_column_metadata table_metadata[] = {
        {
            .label = "n",
            .schema_name = "information_schema",
            .table_name = "catalog_tables",
            .origin_schema_name = "information_schema",
            .origin_table_name = "TABLES",
            .origin_column_name = "TABLE_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                         MYLITE_RESULT_COLUMN_FLAG_BINARY |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 0,
        },
        {
            .label = "TABLE_ROWS",
            .schema_name = "information_schema",
            .table_name = "catalog_tables",
            .origin_schema_name = "information_schema",
            .origin_table_name = "TABLES",
            .origin_column_name = "TABLE_ROWS",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                         MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 20U,
            .nullable = 1,
        },
        {
            .label = "TABLE_TYPE",
            .schema_name = "information_schema",
            .table_name = "catalog_tables",
            .origin_schema_name = "information_schema",
            .origin_table_name = "TABLES",
            .origin_column_name = "TABLE_TYPE",
            .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                         MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_ENUM |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_ENUM |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 44U,
            .nullable = 0,
        },
    };
    static const struct expected_column_metadata count_metadata[] = {
        {
            .label = "c",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flag_mask = UINT32_MAX,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 21U,
            .nullable = 0,
        },
    };
    static const struct expected_column_metadata thread_metadata[] = {
        {
            .label = "THREAD_ID",
            .schema_name = "performance_schema",
            .table_name = "threads",
            .origin_schema_name = "performance_schema",
            .origin_table_name = "threads",
            .origin_column_name = "THREAD_ID",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                         MYLITE_RESULT_COLUMN_FLAG_PRI_KEY |
                         MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
                         MYLITE_RESULT_COLUMN_FLAG_PART_KEY |
                         MYLITE_RESULT_COLUMN_FLAG_NUM,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                     MYLITE_RESULT_COLUMN_FLAG_PRI_KEY |
                     MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
                     MYLITE_RESULT_COLUMN_FLAG_PART_KEY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 20U,
            .nullable = 0,
        },
    };
    static const struct expected_column_metadata sys_metadata[] = {
        {
            .label = "thd_id",
            .schema_name = "sys",
            .table_name = "processlist",
            .origin_schema_name = "sys",
            .origin_table_name = "processlist",
            .origin_column_name = "thd_id",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                         MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
                         MYLITE_RESULT_COLUMN_FLAG_NUM,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                     MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
                     MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 20U,
            .nullable = 0,
        },
    };
    static const char table_sql[] =
        "SELECT TABLE_NAME AS n, TABLE_ROWS, TABLE_TYPE "
        "FROM INFORMATION_SCHEMA.TABLES AS catalog_tables WHERE TABLE_SCHEMA = 'missing'";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    mylite_stmt *stmt = NULL;
    int failures = make_test_path(path, sizeof(path), "metadata");

    remove_related_files(path);
    if (failures == 0) {
        failures += expect_int(
            mylite_open(path, &database),
            MYLITE_OK,
            "open information schema metadata database"
        );
    }

    if (failures == 0) {
        failures += expect_int(
            mylite_execute(database, table_sql, strlen(table_sql), &result),
            MYLITE_OK,
            "execute information schema result metadata"
        );
    }
    if (result != NULL) {
        failures += expect_result_metadata(
            result,
            table_metadata,
            sizeof(table_metadata) / sizeof(table_metadata[0]),
            "information schema result metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    if (failures == 0) {
        failures += expect_int(
            mylite_prepare(database, table_sql, strlen(table_sql), &stmt),
            MYLITE_OK,
            "prepare information schema cursor metadata"
        );
    }
    if (stmt != NULL) {
        failures += expect_stmt_metadata(
            stmt,
            table_metadata,
            sizeof(table_metadata) / sizeof(table_metadata[0]),
            "information schema cursor metadata"
        );
        failures += expect_int(
            mylite_stmt_step(stmt),
            MYLITE_DONE,
            "step empty information schema metadata cursor"
        );
        failures += expect_int(
            mylite_stmt_finalize(stmt),
            MYLITE_OK,
            "finalize information schema metadata cursor"
        );
        stmt = NULL;
    }

    if (failures == 0) {
        static const char count_sql[] =
            "SELECT COUNT(*) AS c FROM INFORMATION_SCHEMA.TABLES LIMIT 0";

        failures += expect_int(
            mylite_execute(database, count_sql, strlen(count_sql), &result),
            MYLITE_OK,
            "execute information schema count metadata"
        );
    }
    if (result != NULL) {
        failures += expect_result_metadata(
            result,
            count_metadata,
            sizeof(count_metadata) / sizeof(count_metadata[0]),
            "information schema count metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    if (failures == 0) {
        static const char thread_sql[] =
            "SELECT THREAD_ID FROM performance_schema.threads LIMIT 0";

        failures += expect_int(
            mylite_execute(database, thread_sql, strlen(thread_sql), &result),
            MYLITE_OK,
            "execute performance schema result metadata"
        );
    }
    if (result != NULL) {
        failures += expect_result_metadata(
            result,
            thread_metadata,
            sizeof(thread_metadata) / sizeof(thread_metadata[0]),
            "performance schema result metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    if (failures == 0) {
        static const char sys_sql[] = "SELECT thd_id FROM sys.processlist LIMIT 0";

        failures += expect_int(
            mylite_execute(database, sys_sql, strlen(sys_sql), &result),
            MYLITE_OK,
            "execute sys result metadata"
        );
    }
    if (result != NULL) {
        failures += expect_result_metadata(
            result,
            sys_metadata,
            sizeof(sys_metadata) / sizeof(sys_metadata[0]),
            "sys result metadata"
        );
    }
    mylite_result_free(result);
    if (stmt != NULL) {
        (void)mylite_stmt_finalize(stmt);
    }
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_core_queries(void) {
    static const struct expected_column_metadata join_bridge_metadata[] = {
        {
            .label = "DATA_TYPE",
            .schema_name = "information_schema",
            .table_name = "cols",
            .origin_schema_name = "information_schema",
            .origin_table_name = "COLUMNS",
            .origin_column_name = "DATA_TYPE",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = UINT32_MAX,
            .nullable = 1,
        },
        {
            .label = "INDEX_NAME",
            .schema_name = "information_schema",
            .table_name = "stats",
            .origin_schema_name = "information_schema",
            .origin_table_name = "STATISTICS",
            .origin_column_name = "INDEX_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = 0U,
            .flags = 0U,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 1,
        },
        {
            .label = "COLUMN_NAME",
            .schema_name = "information_schema",
            .table_name = "stats",
            .origin_schema_name = "information_schema",
            .origin_table_name = "STATISTICS",
            .origin_column_name = "COLUMN_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = 0U,
            .flags = 0U,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 1,
        },
    };
    static const struct expected_column_metadata grouped_size_bridge_metadata[] = {
        {
            .label = "table",
            .schema_name = "information_schema",
            .table_name = "TABLES",
            .origin_schema_name = "information_schema",
            .origin_table_name = "TABLES",
            .origin_column_name = "TABLE_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 0,
        },
        {
            .label = "rows",
            .schema_name = "information_schema",
            .table_name = "TABLES",
            .origin_schema_name = "information_schema",
            .origin_table_name = "TABLES",
            .origin_column_name = "TABLE_ROWS",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                         MYLITE_RESULT_COLUMN_FLAG_NUM,
            .flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 20U,
            .nullable = 1,
        },
        {
            .label = "bytes",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .type = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
            .flag_mask = UINT32_MAX,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 45U,
            .nullable = 1,
        },
    };
    static const struct expected_column_metadata union_bridge_metadata[] = {
        {
            .label = "name",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = UINT32_MAX,
            .flags = 0U,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 292U,
            .nullable = 1,
        },
    };
    static const char join_bridge_sql[] =
        "SELECT cols.DATA_TYPE, stats.INDEX_NAME, stats.COLUMN_NAME "
        "FROM INFORMATION_SCHEMA.COLUMNS AS cols "
        "JOIN INFORMATION_SCHEMA.STATISTICS AS stats "
        "ON cols.TABLE_SCHEMA = stats.TABLE_SCHEMA "
        "AND cols.TABLE_NAME = stats.TABLE_NAME "
        "AND cols.COLUMN_NAME = stats.COLUMN_NAME "
        "WHERE cols.TABLE_SCHEMA = 'app' AND cols.TABLE_NAME = 'wp_users' "
        "ORDER BY INDEX_NAME ASC";
    static const char grouped_size_bridge_sql[] =
        "SELECT TABLE_NAME AS 'table', TABLE_ROWS AS 'rows', "
        "SUM(DATA_LENGTH + INDEX_LENGTH) AS 'bytes' "
        "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app' "
        "AND TABLE_NAME IN ('t', 'other', 'wp_users') "
        "GROUP BY TABLE_NAME ORDER BY TABLE_NAME";
    static const char union_bridge_sql[] =
        "WITH cols AS ("
        "SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users'), "
        "indexes AS ("
        "SELECT DISTINCT INDEX_NAME AS index_name FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users') "
        "SELECT CONCAT(column_name, ' (column)') AS name FROM cols "
        "UNION ALL "
        "SELECT CONCAT(index_name, ' (index)') AS name FROM indexes "
        "ORDER BY name";
    static const char *const schemata_columns[] = {
        "CATALOG_NAME",
        "SCHEMA_NAME",
        "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME",
        "SQL_PATH",
        "DEFAULT_ENCRYPTION",
    };
    static const char *const schemata_values[] = {
        "def",
        "app",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
    };
    static const char *const builtin_schemata_values[] = {
        "information_schema",
        "utf8mb3",
        "utf8mb3_general_ci",
        NULL,
        "NO",
        "mysql",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
        "performance_schema",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
        "sys",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
        "NO",
    };
    static const char *const schemata_limit_offset_columns[] = {"SCHEMA_NAME"};
    static const char *const schemata_limit_offset_values[] = {
        "information_schema",
        "performance_schema",
    };
    static const char *const table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const table_values[] = {
        "app", "other",    "BASE TABLE", "InnoDB", "10", "Dynamic", "1", "16384", NULL,
        "app", "t",        "BASE TABLE", "InnoDB", "10", "Dynamic", "1", "16384", "2",
        "app", "wp_users", "BASE TABLE", "InnoDB", "10", "Dynamic", "0", "16384", "1",
    };
    static const char *const auto_increment_predicate_columns[] = {"TABLE_NAME"};
    static const char *const auto_increment_predicate_values[] = {"t"};
    static const char *const table_computed_columns[] = {"name", "engine", "data"};
    static const char *const table_computed_values[] = {"t", "InnoDB", "0"};
    static const char *const table_name_values[] = {"other", "t", "wp_users"};
    static const char *const table_size_columns[] = {"table", "rows", "bytes"};
    static const char *const table_size_values[] = {
        "other",
        "1",
        "16384",
        "t",
        "1",
        "16384",
        "wp_users",
        "0",
        "32768",
    };
    static const char *const indexed_column_join_columns[] = {
        "DATA_TYPE",
        "INDEX_NAME",
        "COLUMN_NAME",
    };
    static const char *const indexed_column_join_values[] = {
        "bigint",
        "PRIMARY",
        "ID",
        "varchar",
        "user_email",
        "user_email",
        "varchar",
        "user_login_key",
        "user_login",
        "varchar",
        "user_nicename",
        "user_nicename",
    };
    static const char *const with_union_column[] = {"name"};
    static const char *const with_union_values[] = {
        "display_name (column)",
        "ID (column)",
        "PRIMARY (index)",
        "user_activation_key (column)",
        "user_email (column)",
        "user_email (index)",
        "user_login (column)",
        "user_login_key (index)",
        "user_nicename (column)",
        "user_nicename (index)",
        "user_pass (column)",
        "user_registered (column)",
        "user_status (column)",
        "user_url (column)",
    };
    static const char *const column_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "EXTRA",
    };
    static const char *const column_values[] = {
        "id",      "1",
        NULL,      "NO",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "PRI",     "auto_increment",
        "v",       "2",
        NULL,      "YES",
        "varchar", "3",
        "12",      NULL,
        NULL,      "varchar(3)",
        "",        "",
        "n",       "3",
        "7",       "NO",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "",        "",
        "u",       "4",
        NULL,      "YES",
        "tinyint", NULL,
        NULL,      "3",
        "0",       "tinyint unsigned",
        "",        "",
        "hidden",  "5",
        NULL,      "YES",
        "int",     NULL,
        NULL,      "10",
        "0",       "int",
        "",        "INVISIBLE",
    };
    static const char *const single_column[] = {"COLUMN_NAME"};
    static const char *const id_value[] = {"id"};
    static const char *const alias_limit_values[] = {"id", "v", "n"};
    static const char *const filtered_limit_offset_values[] = {"v", "n"};
    static const char *const desc_limit_values[] = {"hidden", "u"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const count_zero[] = {"0"};
    static const char *const one_column[] = {"1"};
    static const char *const expression_column[] = {"expression"};
    static const char *const builtin_schemata_columns[] = {
        "SCHEMA_NAME",
        "DEFAULT_CHARACTER_SET_NAME",
        "DEFAULT_COLLATION_NAME",
        "SQL_PATH",
        "DEFAULT_ENCRYPTION",
    };
    static const char *const schema_name_column[] = {"SCHEMA_NAME"};
    static const char *const collation_name_column[] = {"COLLATION_NAME"};
    static const char *const builtin_schemata_default_order_values[] = {
        "mysql",
        "information_schema",
        "performance_schema",
        "sys",
    };
    static const char *const mysql_collation_name_order_values[] = {
        "ucs2_romanian_ci",
        "ucs2_roman_ci",
    };
    static const char *const system_table_columns[] =
        {"TABLE_SCHEMA", "TABLE_NAME", "TABLE_TYPE", "ENGINE", "VERSION", "ROW_FORMAT", "TABLE_ROWS"
        };
    static const char *const system_table_values[] = {
        "information_schema", "COLUMNS",  "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "SCHEMATA", "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "TABLES",   "SYSTEM VIEW", NULL, "10", NULL, "0",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "core") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open information schema db");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = 'app'",
            .column_names = schemata_columns,
            .column_count = sizeof(schemata_columns) / sizeof(schemata_columns[0]),
            .values = schemata_values,
            .row_count = 1U,
            .context = "schemata wildcard app row",
        }
    );
    failures += expect_query_columns(
        database,
        "SELECT s.* FROM INFORMATION_SCHEMA.SCHEMATA AS s LEFT JOIN "
        "INFORMATION_SCHEMA.TABLES AS t ON t.TABLE_SCHEMA = s.SCHEMA_NAME "
        "ORDER BY s.SCHEMA_NAME",
        schemata_columns,
        sizeof(schemata_columns) / sizeof(schemata_columns[0]),
        "schemata tables left join bridge"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME, "
                   "SQL_PATH, DEFAULT_ENCRYPTION FROM INFORMATION_SCHEMA.SCHEMATA "
                   "WHERE SCHEMA_NAME IN ('information_schema', 'mysql', "
                   "'performance_schema', 'sys') ORDER BY SCHEMA_NAME",
            .column_names = builtin_schemata_columns,
            .column_count = sizeof(builtin_schemata_columns) / sizeof(builtin_schemata_columns[0]),
            .values = builtin_schemata_values,
            .row_count = sizeof(builtin_schemata_values) / sizeof(builtin_schemata_values[0]) /
                         (sizeof(builtin_schemata_columns) / sizeof(builtin_schemata_columns[0])),
            .context = "schemata built-in rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA "
                   "WHERE SCHEMA_NAME IN ('information_schema', 'mysql', "
                   "'performance_schema', 'sys')",
            .column_names = schema_name_column,
            .column_count = sizeof(schema_name_column) / sizeof(schema_name_column[0]),
            .values = builtin_schemata_default_order_values,
            .row_count = sizeof(builtin_schemata_default_order_values) /
                         sizeof(builtin_schemata_default_order_values[0]),
            .context = "schemata built-in default order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS "
                   "WHERE COLLATION_NAME IN ('ucs2_roman_ci', 'ucs2_romanian_ci') "
                   "ORDER BY COLLATION_NAME",
            .column_names = collation_name_column,
            .column_count = sizeof(collation_name_column) / sizeof(collation_name_column[0]),
            .values = mysql_collation_name_order_values,
            .row_count = sizeof(mysql_collation_name_order_values) /
                         sizeof(mysql_collation_name_order_values[0]),
            .context = "collations MySQL catalog name order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = 3U,
            .context = "tables base descriptor rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND AUTO_INCREMENT > 1 ORDER BY TABLE_NAME",
            .column_names = auto_increment_predicate_columns,
            .column_count = sizeof(auto_increment_predicate_columns) /
                            sizeof(auto_increment_predicate_columns[0]),
            .values = auto_increment_predicate_values,
            .row_count = 1U,
            .context = "tables implicit auto increment predicate next value",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME AS name, ENGINE AS engine, "
                   "CAST(DATA_LENGTH / 1024 / 1024 AS UNSIGNED) AS data "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't' ORDER BY name ASC",
            .column_names = table_computed_columns,
            .column_count = sizeof(table_computed_columns) / sizeof(table_computed_columns[0]),
            .values = table_computed_values,
            .row_count = 1U,
            .context = "tables computed data length projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME AS 'table', TABLE_ROWS AS 'rows', "
                   "SUM(DATA_LENGTH + INDEX_LENGTH) AS 'bytes' "
                   "FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME IN ('t', 'other', 'wp_users') "
                   "GROUP BY TABLE_NAME ORDER BY TABLE_NAME",
            .column_names = table_size_columns,
            .column_count = sizeof(table_size_columns) / sizeof(table_size_columns[0]),
            .values = table_size_values,
            .row_count = sizeof(table_size_values) / sizeof(table_size_values[0]) /
                         (sizeof(table_size_columns) / sizeof(table_size_columns[0])),
            .context = "tables grouped size metadata projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT cols.DATA_TYPE, stats.INDEX_NAME, stats.COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS AS cols "
                   "JOIN INFORMATION_SCHEMA.STATISTICS AS stats "
                   "ON cols.TABLE_SCHEMA = stats.TABLE_SCHEMA "
                   "AND cols.TABLE_NAME = stats.TABLE_NAME "
                   "AND cols.COLUMN_NAME = stats.COLUMN_NAME "
                   "WHERE cols.TABLE_SCHEMA = 'app' AND cols.TABLE_NAME = 'wp_users' "
                   "ORDER BY INDEX_NAME ASC",
            .column_names = indexed_column_join_columns,
            .column_count =
                sizeof(indexed_column_join_columns) / sizeof(indexed_column_join_columns[0]),
            .values = indexed_column_join_values,
            .row_count =
                sizeof(indexed_column_join_values) / sizeof(indexed_column_join_values[0]) /
                (sizeof(indexed_column_join_columns) / sizeof(indexed_column_join_columns[0])),
            .context = "columns statistics join bridge rows",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT cols.DATA_TYPE, stats.INDEX_NAME, stats.COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.COLUMNS AS cols "
                   "JOIN INFORMATION_SCHEMA.STATISTICS AS stats "
                   "ON cols.TABLE_SCHEMA = stats.TABLE_SCHEMA "
                   "AND cols.TABLE_NAME = stats.TABLE_NAME "
                   "AND cols.COLUMN_NAME = stats.COLUMN_NAME "
                   "WHERE cols.TABLE_SCHEMA = 'app' AND cols.TABLE_NAME = 'wp_users' "
                   "ORDER BY INDEX_NAME ASC LIMIT 1",
            .code = 1064,
            .sqlstate = "42000",
            .message_part = "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "WITH cols AS ("
                   "SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users'), "
                   "indexes AS ("
                   "SELECT DISTINCT INDEX_NAME AS index_name FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users') "
                   "SELECT CONCAT(column_name, ' (column)') AS name FROM cols "
                   "UNION ALL "
                   "SELECT CONCAT(index_name, ' (index)') AS name FROM indexes "
                   "ORDER BY name",
            .column_names = with_union_column,
            .column_count = 1U,
            .values = with_union_values,
            .row_count = sizeof(with_union_values) / sizeof(with_union_values[0]),
            .context = "information schema WITH union bridge rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "WITH `cols` AS ("
                   "SELECT `COLUMN_NAME` AS `column_name` FROM "
                   "`INFORMATION_SCHEMA`.`COLUMNS` WHERE `TABLE_SCHEMA` = 'app' "
                   "AND `TABLE_NAME` = 'wp_users'), "
                   "`indexes` AS (SELECT DISTINCT `INDEX_NAME` AS `index_name` FROM "
                   "`INFORMATION_SCHEMA`.`STATISTICS` WHERE `TABLE_SCHEMA` = 'app' "
                   "AND `TABLE_NAME` = 'wp_users') "
                   "SELECT CONCAT(`column_name`, ' (column)') AS `name` FROM `cols` "
                   "UNION ALL SELECT CONCAT(`index_name`, ' (index)') AS `name` "
                   "FROM `indexes` ORDER BY `name`",
            .column_names = with_union_column,
            .column_count = 1U,
            .values = with_union_values,
            .row_count = sizeof(with_union_values) / sizeof(with_union_values[0]),
            .context = "quoted information schema WITH union bridge rows",
        }
    );
    failures += expect_query_result_metadata(
        database,
        join_bridge_sql,
        join_bridge_metadata,
        sizeof(join_bridge_metadata) / sizeof(join_bridge_metadata[0]),
        "columns statistics join bridge metadata"
    );
    failures += expect_query_result_metadata(
        database,
        grouped_size_bridge_sql,
        grouped_size_bridge_metadata,
        sizeof(grouped_size_bridge_metadata) / sizeof(grouped_size_bridge_metadata[0]),
        "grouped table size bridge metadata"
    );
    failures += expect_query_result_metadata(
        database,
        union_bridge_sql,
        union_bridge_metadata,
        sizeof(union_bridge_metadata) / sizeof(union_bridge_metadata[0]),
        "information schema WITH union bridge metadata"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                   "/* cols.data_type stats.index_name stats.column_name "
                   "information_schema.columns information_schema.statistics "
                   "cols.table_schema = stats.table_schema "
                   "cols.table_name = stats.table_name "
                   "cols.column_name = stats.column_name order by index_name */ "
                   "WHERE TABLE_SCHEMA = 'app' ORDER BY TABLE_NAME",
            .column_names = auto_increment_predicate_columns,
            .column_count = 1U,
            .values = table_name_values,
            .row_count = sizeof(table_name_values) / sizeof(table_name_values[0]),
            .context = "bridge dispatch ignores comment text",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "WITH cols AS ("
                   "SELECT COLUMN_NAME AS column_name FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wp_users'), "
                   "indexes AS (SELECT DISTINCT INDEX_NAME AS index_name FROM "
                   "INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'wp_users') "
                   "SELECT CONCAT(column_name, ' wrong') AS name FROM cols UNION ALL "
                   "SELECT CONCAT(index_name, ' (index)') AS name FROM indexes ORDER BY name",
            .code = 1064,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA LIMIT 2 OFFSET 1",
            .column_names = schemata_limit_offset_columns,
            .column_count = 1U,
            .values = schemata_limit_offset_values,
            .row_count = 2U,
            .context = "metadata limit offset materialization",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA LIMIT 1 OFFSET 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "metadata count limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, COLUMN_TYPE, COLUMN_KEY, EXTRA "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' ORDER BY ORDINAL_POSITION",
            .column_names = column_columns,
            .column_count = sizeof(column_columns) / sizeof(column_columns[0]),
            .values = column_values,
            .row_count = sizeof(column_values) / sizeof(column_values[0]) /
                         (sizeof(column_columns) / sizeof(column_columns[0])),
            .context = "columns descriptor rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS AS c "
                   "WHERE c.TABLE_SCHEMA = 'app' AND c.TABLE_NAME = 't' "
                   "ORDER BY c.ORDINAL_POSITION LIMIT 3",
            .column_names = single_column,
            .column_count = 1U,
            .values = alias_limit_values,
            .row_count = 3U,
            .context = "alias qualified ordered limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' LIMIT 2 OFFSET 1",
            .column_names = single_column,
            .column_count = 1U,
            .values = filtered_limit_offset_values,
            .row_count = 2U,
            .context = "catalog metadata filtered limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND COLUMN_NAME = 'ID'",
            .column_names = single_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "metadata string predicate collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND ORDINAL_POSITION = '01'",
            .column_names = single_column,
            .column_count = 1U,
            .values = id_value,
            .row_count = 1U,
            .context = "numeric metadata string coercion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "ORDER BY ORDINAL_POSITION DESC LIMIT 2",
            .column_names = single_column,
            .column_count = 1U,
            .values = desc_limit_values,
            .row_count = 2U,
            .context = "descending ordered limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "count star database predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 AS expression FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't' "
                   "AND TABLE_TYPE = 'BASE TABLE'",
            .column_names = expression_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "tables existence predicate hit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'missing' "
                   "AND TABLE_TYPE = 'BASE TABLE'",
            .column_names = one_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "tables existence predicate miss",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'missing' "
                   "AND TABLE_TYPE = 'BASE TABLE'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "tables existence count miss",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() "
                   "AND TABLE_NAME = "
                   "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' "
                   "AND TABLE_TYPE = 'BASE TABLE'",
            .column_names = one_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "tables existence long table name predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't' "
                   "AND TABLE_TYPE = 'BASE TABLE' AND ENGINE = 'not-mylite'",
            .column_names = one_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "tables existence predicate extra condition",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'COLUMNS' OR TABLE_NAME = 'SCHEMATA' "
                   "OR TABLE_NAME = 'TABLES') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 3U,
            .context = "system view table rows",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLES.TABLE_NAME FROM INFORMATION_SCHEMA.TABLES AS t",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'TABLES.TABLE_NAME' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.NOPE",
            .code = mysql_error_unknown_table_in_schema,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'NOPE' in information_schema",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.TABLES",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen information schema db");
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "RENAME TABLE t TO renamed", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "renamed table metadata row",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed", -1);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "dropped table metadata rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(3), "
        "n INT NOT NULL DEFAULT 7, u TINYINT UNSIGNED, hidden INT)",
        -1
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE t ALTER COLUMN hidden SET INVISIBLE", -1);
    failures += expect_statement_ok(database, "CREATE TABLE other (x BIGINT UNSIGNED)", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_users ("
        "ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, "
        "user_login VARCHAR(60) NOT NULL DEFAULT '', "
        "user_pass VARCHAR(255) NOT NULL DEFAULT '', "
        "user_nicename VARCHAR(50) NOT NULL DEFAULT '', "
        "user_email VARCHAR(100) NOT NULL DEFAULT '', "
        "user_url VARCHAR(100) NOT NULL DEFAULT '', "
        "user_registered DATETIME NOT NULL, "
        "user_activation_key VARCHAR(255) NOT NULL DEFAULT '', "
        "user_status INT NOT NULL DEFAULT '0', "
        "display_name VARCHAR(250) NOT NULL DEFAULT '', "
        "PRIMARY KEY (ID), "
        "KEY user_login_key (user_login), "
        "KEY user_nicename (user_nicename), "
        "KEY user_email (user_email))",
        -1
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO t (v, u, hidden) VALUES ('abc', 2, 9)", 1);
    failures += expect_statement_ok(database, "INSERT INTO other VALUES (42)", 1);

    return failures;
}

static int test_information_schema_wordpress_bridge_queries(void) {
    static const struct expected_column_metadata dynamic_metadata[] = {
        {
            .label = "id",
            .schema_name = "app",
            .table_name = "sub",
            .origin_schema_name = "app",
            .origin_table_name = "t",
            .origin_column_name = "id",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = 63U,
            .collation_id = 63U,
            .display_length = 11U,
            .nullable = 1,
        },
        {
            .label = "TABLE_SCHEMA",
            .schema_name = "information_schema",
            .table_name = "sub",
            .origin_schema_name = "information_schema",
            .origin_table_name = "COLUMNS",
            .origin_column_name = "TABLE_SCHEMA",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 0,
        },
        {
            .label = "TABLE_NAME",
            .schema_name = "information_schema",
            .table_name = "sub",
            .origin_schema_name = "information_schema",
            .origin_table_name = "COLUMNS",
            .origin_column_name = "TABLE_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                         MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 0,
        },
        {
            .label = "COLUMN_NAME",
            .schema_name = "information_schema",
            .table_name = "sub",
            .origin_schema_name = "information_schema",
            .origin_table_name = "COLUMNS",
            .origin_column_name = "COLUMN_NAME",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flag_mask = 0U,
            .flags = 0U,
            .charset_id = 255U,
            .collation_id = 255U,
            .display_length = 256U,
            .nullable = 1,
        },
    };
    static const char dynamic_sql[] =
        "SELECT sub.id, sub.table_schema, sub.table_name, sub.column_name "
        "FROM ("
        "SELECT * FROM information_schema.columns c "
        "JOIN t ON t.db_name = CONCAT(COALESCE(c.table_schema, 'default'), '') "
        "JOIN information_schema.schemata s ON s.schema_name = c.table_schema "
        "WHERE c.table_name = 't'"
        ") sub "
        "ORDER BY ordinal_position";
    static const char *const dynamic_columns[] = {
        "id",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "COLUMN_NAME",
    };
    static const char *const dynamic_values[] = {
        "2",
        "app",
        "t",
        "id",
        "2",
        "app",
        "t",
        "db_name",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "wordpress_bridges") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open wordpress bridge db");
    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, db_name TEXT)", -1);
    failures += expect_statement_ok(
        database,
        "INSERT INTO t (id, db_name) VALUES (1, 'other'), (2, 'app')",
        2
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT sub.id, sub.table_schema, sub.table_name, sub.column_name "
                   "FROM ("
                   "SELECT * FROM information_schema.columns c "
                   "JOIN t ON t.db_name = CONCAT(COALESCE(c.table_schema, 'default'), '') "
                   "JOIN information_schema.schemata s ON s.schema_name = c.table_schema "
                   "WHERE c.table_name = 't'"
                   ") sub "
                   "ORDER BY ordinal_position",
            .column_names = dynamic_columns,
            .column_count = sizeof(dynamic_columns) / sizeof(dynamic_columns[0]),
            .values = dynamic_values,
            .row_count = sizeof(dynamic_values) / sizeof(dynamic_values[0]) /
                         (sizeof(dynamic_columns) / sizeof(dynamic_columns[0])),
            .context = "wordpress dynamic information schema columns bridge",
        }
    );
    failures += expect_query_result_metadata(
        database,
        dynamic_sql,
        dynamic_metadata,
        sizeof(dynamic_metadata) / sizeof(dynamic_metadata[0]),
        "wordpress dynamic information schema bridge metadata"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_doctrine_bridge_queries(void) {
    static const char *const column_names[] = {
        "TABLE_NAME",
        "field",
        "type",
        "COLUMN_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "null",
        "key",
        "default",
        "EXTRA",
        "comment",
        "characterset",
        "collation",
    };
    static const char *const index_column_names[] = {
        "TABLE_NAME",
        "Non_Unique",
        "Key_name",
        "Column_Name",
        "Sub_Part",
        "Index_Type",
    };
    static const char *const values[] = {
        "doctrine_users",
        "id",
        "int",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "",
        NULL,
        NULL,
        "doctrine_users",
        "email",
        "varchar",
        "varchar(191)",
        "191",
        "764",
        NULL,
        NULL,
        "NO",
        "UNI",
        NULL,
        "",
        "",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "doctrine_users",
        "name",
        "varchar",
        "varchar(191)",
        "191",
        "764",
        NULL,
        NULL,
        "NO",
        "",
        NULL,
        "",
        "",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "doctrine_users",
        "score",
        "int",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        "NO",
        "",
        "0",
        "",
        "",
        NULL,
        NULL,
    };
    static const char *const index_values[] = {
        "doctrine_users",
        "0",
        "PRIMARY",
        "id",
        NULL,
        "BTREE",
        "doctrine_users",
        "0",
        "doctrine_users_email_unique",
        "email",
        NULL,
        "BTREE",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "doctrine_bridge") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open doctrine bridge db");
    failures += expect_statement_ok(database, "CREATE DATABASE app", -1);
    failures += expect_statement_ok(database, "USE app", -1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE doctrine_users ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "email VARCHAR(191) NOT NULL, "
        "name VARCHAR(191) NOT NULL, "
        "score INT NOT NULL DEFAULT 0, "
        "UNIQUE KEY doctrine_users_email_unique (email))",
        -1
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.TABLE_NAME, c.COLUMN_NAME AS field, c.DATA_TYPE AS type, "
                   "c.COLUMN_TYPE, c.CHARACTER_MAXIMUM_LENGTH, "
                   "c.CHARACTER_OCTET_LENGTH, c.NUMERIC_PRECISION, c.NUMERIC_SCALE, "
                   "c.IS_NULLABLE AS `null`, c.COLUMN_KEY AS `key`, "
                   "c.COLUMN_DEFAULT AS `default`, c.EXTRA, c.COLUMN_COMMENT AS comment, "
                   "c.CHARACTER_SET_NAME AS characterset, c.COLLATION_NAME AS collation "
                   "FROM information_schema.COLUMNS c "
                   "INNER JOIN information_schema.TABLES t "
                   "ON t.TABLE_NAME = c.TABLE_NAME "
                   "WHERE c.TABLE_SCHEMA = 'app' AND t.TABLE_SCHEMA = 'app' "
                   "AND t.TABLE_NAME = 'doctrine_users' "
                   "AND t.TABLE_TYPE = 'BASE TABLE' "
                   "ORDER BY c.TABLE_NAME, c.ORDINAL_POSITION",
            .column_names = column_names,
            .column_count = sizeof(column_names) / sizeof(column_names[0]),
            .values = values,
            .row_count = sizeof(values) / sizeof(values[0]) /
                         (sizeof(column_names) / sizeof(column_names[0])),
            .context = "Doctrine COLUMNS/TABLES information_schema bridge",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, NON_UNIQUE AS Non_Unique, INDEX_NAME AS Key_name, "
                   "COLUMN_NAME AS Column_Name, SUB_PART AS Sub_Part, "
                   "INDEX_TYPE AS Index_Type "
                   "FROM information_schema.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'doctrine_users' "
                   "ORDER BY TABLE_NAME, SEQ_IN_INDEX",
            .column_names = index_column_names,
            .column_count = sizeof(index_column_names) / sizeof(index_column_names[0]),
            .values = index_values,
            .row_count = sizeof(index_values) / sizeof(index_values[0]) /
                         (sizeof(index_column_names) / sizeof(index_column_names[0])),
            .context = "Doctrine STATISTICS information_schema bridge",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    if (affected_rows >= 0) {
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    }
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_columns(
    mylite_db *database,
    const char *sql,
    const char *const *columns,
    size_t column_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), column_count, context);
    if (mylite_result_row_count(result) == 0U) {
        fprintf(stderr, "%s: expected at least one row\n", context);
        ++failures;
    }
    for (size_t column = 0U; column < column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            columns[column],
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_result_metadata(
    mylite_db *database,
    const char *sql,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_result_metadata(result, expected, column_count, context);
    mylite_result_free(result);
    return failures;
}

static int expect_result_metadata(
    const mylite_result *result,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
) {
    int failures = expect_size(mylite_result_column_count(result), column_count, context);

    for (size_t index = 0U; index < column_count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected[index].label,
            context
        );
        failures += expect_text_or_null(
            mylite_result_column_schema_name(result, index),
            expected[index].schema_name,
            context
        );
        failures += expect_text_or_null(
            mylite_result_column_table_name(result, index),
            expected[index].table_name,
            context
        );
        failures += expect_text_or_null(
            mylite_result_column_origin_schema_name(result, index),
            expected[index].origin_schema_name,
            context
        );
        failures += expect_text_or_null(
            mylite_result_column_origin_table_name(result, index),
            expected[index].origin_table_name,
            context
        );
        failures += expect_text_or_null(
            mylite_result_column_origin_name(result, index),
            expected[index].origin_column_name,
            context
        );
        failures += expect_int(
            (int)mylite_result_column_type(result, index),
            (int)expected[index].type,
            context
        );
        failures += expect_int64(
            (int64_t)(mylite_result_column_flags(result, index) & expected[index].flag_mask),
            (int64_t)expected[index].flags,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_charset_id(result, index),
            (int64_t)expected[index].charset_id,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_collation_id(result, index),
            (int64_t)expected[index].collation_id,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_display_length(result, index),
            (int64_t)expected[index].display_length,
            context
        );
        failures += expect_int(
            mylite_result_column_nullable(result, index),
            expected[index].nullable,
            context
        );
    }
    return failures;
}

static int expect_stmt_metadata(
    const mylite_stmt *stmt,
    const struct expected_column_metadata *expected,
    size_t column_count,
    const char *context
) {
    int failures = expect_size(mylite_stmt_column_count(stmt), column_count, context);

    for (size_t index = 0U; index < column_count; ++index) {
        failures +=
            expect_text_or_null(mylite_stmt_column_name(stmt, index), expected[index].label, context);
        failures += expect_text_or_null(
            mylite_stmt_column_schema_name(stmt, index),
            expected[index].schema_name,
            context
        );
        failures += expect_text_or_null(
            mylite_stmt_column_table_name(stmt, index),
            expected[index].table_name,
            context
        );
        failures += expect_text_or_null(
            mylite_stmt_column_origin_schema_name(stmt, index),
            expected[index].origin_schema_name,
            context
        );
        failures += expect_text_or_null(
            mylite_stmt_column_origin_table_name(stmt, index),
            expected[index].origin_table_name,
            context
        );
        failures += expect_text_or_null(
            mylite_stmt_column_origin_name(stmt, index),
            expected[index].origin_column_name,
            context
        );
        failures += expect_int(
            (int)mylite_stmt_column_type(stmt, index),
            (int)expected[index].type,
            context
        );
        failures += expect_int64(
            (int64_t)(mylite_stmt_column_flags(stmt, index) & expected[index].flag_mask),
            (int64_t)expected[index].flags,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_stmt_column_charset_id(stmt, index),
            (int64_t)expected[index].charset_id,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_stmt_column_collation_id(stmt, index),
            (int64_t)expected[index].collation_id,
            context
        );
        failures += expect_int64(
            (int64_t)mylite_stmt_column_display_length(stmt, index),
            (int64_t)expected[index].display_length,
            context
        );
        failures += expect_int(
            mylite_stmt_column_nullable(stmt, index),
            expected[index].nullable,
            context
        );
    }
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got %d\n", expected.sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }
    written = snprintf(
        path,
        path_size,
        "%s/mylite_information_schema_core_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
        return 1;
    }
    return 0;
}
