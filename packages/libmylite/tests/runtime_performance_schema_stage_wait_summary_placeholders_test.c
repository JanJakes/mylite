#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 6,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 11,
    information_schema_constraint_extensions_projection_count = 4,
    information_schema_constraint_extensions_row_count = 12,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 21,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 22,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 11,
    show_columns_column_count = 6,
    show_account_columns_row_count = 8,
    show_instance_columns_row_count = 7,
    show_index_account_row_count = 3,
    show_index_column_count = 15,
    show_index_instance_row_count = 2,
    show_table_status_column_count = 18,
    mysql_error_access_denied = 1044,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_performance_schema_stage_wait_summary_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_stage_wait_summary_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_stage_wait_summary_placeholders(void) {
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_account_columns_rows[] = {
        "USER",           "char(32)",        "YES", "MUL", NULL, "",
        "HOST",           "char(255)",       "YES", "",    NULL, "",
        "EVENT_NAME",     "varchar(128)",    "NO",  "",    NULL, "",
        "COUNT_STAR",     "bigint unsigned", "NO",  "",    NULL, "",
        "SUM_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "MIN_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "AVG_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
        "MAX_TIMER_WAIT", "bigint unsigned", "NO",  "",    NULL, "",
    };
    static const char *const show_instance_columns_rows[] = {
        "EVENT_NAME",
        "varchar(128)",
        "NO",
        "MUL",
        NULL,
        "",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "COUNT_STAR",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "SUM_TIMER_WAIT",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "MIN_TIMER_WAIT",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "AVG_TIMER_WAIT",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "MAX_TIMER_WAIT",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_index_columns[] = {
        "Table",
        "Non_unique",
        "Key_name",
        "Seq_in_index",
        "Column_name",
        "Collation",
        "Cardinality",
        "Sub_part",
        "Packed",
        "Null",
        "Index_type",
        "Comment",
        "Index_comment",
        "Visible",
        "Expression",
    };
    static const char *const show_index_account_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "0",
        "ACCOUNT",
        "1",
        "USER",
        NULL,
        NULL,
        NULL,
        NULL,
        "YES",
        "HASH",
        "",
        "",
        "YES",
        NULL,
        "events_stages_summary_by_account_by_event_name",
        "0",
        "ACCOUNT",
        "2",
        "HOST",
        NULL,
        NULL,
        NULL,
        NULL,
        "YES",
        "HASH",
        "",
        "",
        "YES",
        NULL,
        "events_stages_summary_by_account_by_event_name",
        "0",
        "ACCOUNT",
        "3",
        "EVENT_NAME",
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "HASH",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const show_index_instance_rows[] = {
        "events_waits_summary_by_instance",
        "0",
        "PRIMARY",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "HASH",
        "",
        "",
        "YES",
        NULL,
        "events_waits_summary_by_instance",
        "1",
        "EVENT_NAME",
        "1",
        "EVENT_NAME",
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "HASH",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
    };
    static const char *const information_schema_columns_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "USER",
        "1",
        "char(32)",
        "MUL",
        "YES",
        "events_stages_summary_by_account_by_event_name",
        "EVENT_NAME",
        "3",
        "varchar(128)",
        "",
        "NO",
        "events_stages_summary_global_by_event_name",
        "EVENT_NAME",
        "1",
        "varchar(128)",
        "PRI",
        "NO",
        "events_waits_summary_by_instance",
        "EVENT_NAME",
        "1",
        "varchar(128)",
        "MUL",
        "NO",
        "events_waits_summary_by_instance",
        "OBJECT_INSTANCE_BEGIN",
        "2",
        "bigint unsigned",
        "PRI",
        "NO",
        "events_waits_summary_global_by_event_name",
        "MAX_TIMER_WAIT",
        "6",
        "bigint unsigned",
        "",
        "NO",
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "INDEX_TYPE",
    };
    static const char *const information_schema_statistics_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "1",
        "USER",
        "HASH",
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "2",
        "HOST",
        "HASH",
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "3",
        "EVENT_NAME",
        "HASH",
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        "0",
        "1",
        "HOST",
        "HASH",
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_stages_summary_by_user_by_event_name",
        "USER",
        "0",
        "1",
        "USER",
        "HASH",
        "events_stages_summary_by_user_by_event_name",
        "USER",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_stages_summary_global_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "1",
        "USER",
        "HASH",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "2",
        "HOST",
        "HASH",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "0",
        "3",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        "0",
        "1",
        "HOST",
        "HASH",
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_by_instance",
        "EVENT_NAME",
        "1",
        "1",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_by_instance",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_by_user_by_event_name",
        "USER",
        "0",
        "1",
        "USER",
        "HASH",
        "events_waits_summary_by_user_by_event_name",
        "USER",
        "0",
        "2",
        "EVENT_NAME",
        "HASH",
        "events_waits_summary_global_by_event_name",
        "PRIMARY",
        "0",
        "1",
        "EVENT_NAME",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "UNIQUE",
        "YES",
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        "UNIQUE",
        "YES",
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_stages_summary_by_user_by_event_name",
        "USER",
        "UNIQUE",
        "YES",
        "events_stages_summary_global_by_event_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "UNIQUE",
        "YES",
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        "UNIQUE",
        "YES",
        "events_waits_summary_by_instance",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "events_waits_summary_by_user_by_event_name",
        "USER",
        "UNIQUE",
        "YES",
        "events_waits_summary_global_by_event_name",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "USER",
        "1",
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "HOST",
        "2",
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        "EVENT_NAME",
        "3",
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        "HOST",
        "1",
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        "EVENT_NAME",
        "2",
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        "THREAD_ID",
        "1",
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "2",
        "events_stages_summary_by_user_by_event_name",
        "USER",
        "USER",
        "1",
        "events_stages_summary_by_user_by_event_name",
        "USER",
        "EVENT_NAME",
        "2",
        "events_stages_summary_global_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "1",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "USER",
        "1",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "HOST",
        "2",
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        "EVENT_NAME",
        "3",
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        "HOST",
        "1",
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        "EVENT_NAME",
        "2",
        "events_waits_summary_by_instance",
        "PRIMARY",
        "OBJECT_INSTANCE_BEGIN",
        "1",
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        "THREAD_ID",
        "1",
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "2",
        "events_waits_summary_by_user_by_event_name",
        "USER",
        "USER",
        "1",
        "events_waits_summary_by_user_by_event_name",
        "USER",
        "EVENT_NAME",
        "2",
        "events_waits_summary_global_by_event_name",
        "PRIMARY",
        "EVENT_NAME",
        "1",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const information_schema_constraint_extensions_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "ACCOUNT",
        NULL,
        NULL,
        "events_stages_summary_by_host_by_event_name",
        "HOST",
        NULL,
        NULL,
        "events_stages_summary_by_thread_by_event_name",
        "PRIMARY",
        NULL,
        NULL,
        "events_stages_summary_by_user_by_event_name",
        "USER",
        NULL,
        NULL,
        "events_stages_summary_global_by_event_name",
        "PRIMARY",
        NULL,
        NULL,
        "events_waits_summary_by_account_by_event_name",
        "ACCOUNT",
        NULL,
        NULL,
        "events_waits_summary_by_host_by_event_name",
        "HOST",
        NULL,
        NULL,
        "events_waits_summary_by_instance",
        "EVENT_NAME",
        NULL,
        NULL,
        "events_waits_summary_by_instance",
        "PRIMARY",
        NULL,
        NULL,
        "events_waits_summary_by_thread_by_event_name",
        "PRIMARY",
        NULL,
        NULL,
        "events_waits_summary_by_user_by_event_name",
        "USER",
        NULL,
        NULL,
        "events_waits_summary_global_by_event_name",
        "PRIMARY",
        NULL,
        NULL,
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "events_stages_summary_by_account_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "22400",
        "utf8mb4_0900_ai_ci",
        "events_stages_summary_by_host_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "22400",
        "utf8mb4_0900_ai_ci",
        "events_stages_summary_by_thread_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "44800",
        "utf8mb4_0900_ai_ci",
        "events_stages_summary_by_user_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "22400",
        "utf8mb4_0900_ai_ci",
        "events_stages_summary_global_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "175",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_by_account_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "88832",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_by_host_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "88832",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_by_instance",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "10752",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_by_thread_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "177664",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_by_user_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "88832",
        "utf8mb4_0900_ai_ci",
        "events_waits_summary_global_by_event_name",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        "694",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const show_table_status_columns[] = {
        "Name",
        "Engine",
        "Version",
        "Row_format",
        "Rows",
        "Avg_row_length",
        "Data_length",
        "Max_data_length",
        "Index_length",
        "Data_free",
        "Auto_increment",
        "Create_time",
        "Update_time",
        "Check_time",
        "Collation",
        "Checksum",
        "Create_options",
        "Comment",
    };
    static const char *const one_count_column[] = {"COUNT(*)"};
    static const char *const zero_count[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (database == NULL) {
        return failures + 1;
    }

    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_summary_by_account_by_event_name",
        "events_stages_summary_by_account_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_summary_by_host_by_event_name",
        "events_stages_summary_by_host_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_summary_by_thread_by_event_name",
        "events_stages_summary_by_thread_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_summary_by_user_by_event_name",
        "events_stages_summary_by_user_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_stages_summary_global_by_event_name",
        "events_stages_summary_global_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_by_account_by_event_name",
        "events_waits_summary_by_account_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_by_host_by_event_name",
        "events_waits_summary_by_host_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_by_instance",
        "events_waits_summary_by_instance count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_by_thread_by_event_name",
        "events_waits_summary_by_thread_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_by_user_by_event_name",
        "events_waits_summary_by_user_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.events_waits_summary_global_by_event_name",
        "events_waits_summary_global_by_event_name count"
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM events_waits_summary_global_by_event_name",
            .column_names = one_count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected schema wait summary read",
        }
    );
    failures += execute_ok(database, "USE mysql");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM "
                   "performance_schema.events_stages_summary_by_account_by_event_name",
            .column_names = show_columns_columns,
            .values = show_account_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_account_columns_row_count,
            .context = "show account stage summary columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE performance_schema.events_waits_summary_by_instance",
            .column_names = show_columns_columns,
            .values = show_instance_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_instance_columns_row_count,
            .context = "describe wait summary instance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM "
                   "performance_schema.events_stages_summary_by_account_by_event_name",
            .column_names = show_index_columns,
            .values = show_index_account_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_account_row_count,
            .context = "show account stage summary index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.events_waits_summary_by_instance",
            .column_names = show_index_columns,
            .values = show_index_instance_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_instance_row_count,
            .context = "show wait summary instance indexes",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'events_waits_summary_by_instance' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status for wait summary instance",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'events_stages_summary_by_account_by_event_name' "
                   "AND COLUMN_NAME IN ('USER', 'EVENT_NAME')) "
                   "OR (TABLE_NAME = 'events_stages_summary_global_by_event_name' "
                   "AND COLUMN_NAME = 'EVENT_NAME') "
                   "OR (TABLE_NAME = 'events_waits_summary_by_instance' "
                   "AND COLUMN_NAME IN ('EVENT_NAME', 'OBJECT_INSTANCE_BEGIN')) "
                   "OR (TABLE_NAME = 'events_waits_summary_global_by_event_name' "
                   "AND COLUMN_NAME = 'MAX_TIMER_WAIT')) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema stage/wait summary columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_stages_summary_by_account_by_event_name', "
                   "'events_stages_summary_by_host_by_event_name', "
                   "'events_stages_summary_by_thread_by_event_name', "
                   "'events_stages_summary_by_user_by_event_name', "
                   "'events_stages_summary_global_by_event_name', "
                   "'events_waits_summary_by_account_by_event_name', "
                   "'events_waits_summary_by_host_by_event_name', "
                   "'events_waits_summary_by_instance', "
                   "'events_waits_summary_by_thread_by_event_name', "
                   "'events_waits_summary_by_user_by_event_name', "
                   "'events_waits_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema stage/wait summary statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_stages_summary_by_account_by_event_name', "
                   "'events_stages_summary_by_host_by_event_name', "
                   "'events_stages_summary_by_thread_by_event_name', "
                   "'events_stages_summary_by_user_by_event_name', "
                   "'events_stages_summary_global_by_event_name', "
                   "'events_waits_summary_by_account_by_event_name', "
                   "'events_waits_summary_by_host_by_event_name', "
                   "'events_waits_summary_by_instance', "
                   "'events_waits_summary_by_thread_by_event_name', "
                   "'events_waits_summary_by_user_by_event_name', "
                   "'events_waits_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information schema stage/wait summary constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_stages_summary_by_account_by_event_name', "
                   "'events_stages_summary_by_host_by_event_name', "
                   "'events_stages_summary_by_thread_by_event_name', "
                   "'events_stages_summary_by_user_by_event_name', "
                   "'events_stages_summary_global_by_event_name', "
                   "'events_waits_summary_by_account_by_event_name', "
                   "'events_waits_summary_by_host_by_event_name', "
                   "'events_waits_summary_by_instance', "
                   "'events_waits_summary_by_thread_by_event_name', "
                   "'events_waits_summary_by_user_by_event_name', "
                   "'events_waits_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information schema stage/wait summary key usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, ENGINE_ATTRIBUTE, "
                   "SECONDARY_ENGINE_ATTRIBUTE "
                   "FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_stages_summary_by_account_by_event_name', "
                   "'events_stages_summary_by_host_by_event_name', "
                   "'events_stages_summary_by_thread_by_event_name', "
                   "'events_stages_summary_by_user_by_event_name', "
                   "'events_stages_summary_global_by_event_name', "
                   "'events_waits_summary_by_account_by_event_name', "
                   "'events_waits_summary_by_host_by_event_name', "
                   "'events_waits_summary_by_instance', "
                   "'events_waits_summary_by_thread_by_event_name', "
                   "'events_waits_summary_by_user_by_event_name', "
                   "'events_waits_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions_rows,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = information_schema_constraint_extensions_row_count,
            .context = "information schema stage/wait summary constraint extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'events_stages_summary_by_account_by_event_name', "
                   "'events_stages_summary_by_host_by_event_name', "
                   "'events_stages_summary_by_thread_by_event_name', "
                   "'events_stages_summary_by_user_by_event_name', "
                   "'events_stages_summary_global_by_event_name', "
                   "'events_waits_summary_by_account_by_event_name', "
                   "'events_waits_summary_by_host_by_event_name', "
                   "'events_waits_summary_by_instance', "
                   "'events_waits_summary_by_thread_by_event_name', "
                   "'events_waits_summary_by_user_by_event_name', "
                   "'events_waits_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information schema stage/wait summary tables",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.events_waits_summary_by_instance "
        "(EVENT_NAME, OBJECT_INSTANCE_BEGIN) VALUES ('wait/io/file/test', 1)",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after performance schema write error");

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s / %s\n", sql, mylite_sqlstate(database), mylite_errmsg(database));
    }
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: %s / %s\n",
            query.context,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_text(
                mylite_result_column_name(result, column),
                query.column_names[column],
                query.context
            );
        }
        if (query.values == NULL) {
            mylite_result_free(result);
            return failures;
        }
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;
                failures += expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[value_index],
                    query.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_empty_count(mylite_db *database, const char *sql, const char *context) {
    static const char *const columns[] = {"COUNT(*)"};
    static const char *const values[] = {"0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_names = columns,
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, "SELECT ROW_COUNT()", strlen("SELECT ROW_COUNT()"), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 1U, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), "-1", context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle
    );
    return 1;
}
