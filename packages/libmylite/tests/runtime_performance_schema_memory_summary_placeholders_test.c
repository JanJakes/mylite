#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 12,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 5,
    information_schema_constraint_extensions_projection_count = 4,
    information_schema_constraint_extensions_row_count = 5,
    information_schema_key_usage_projection_count = 4,
    information_schema_key_usage_row_count = 10,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 10,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 5,
    show_columns_column_count = 6,
    show_account_columns_row_count = 3,
    show_index_account_row_count = 3,
    show_index_column_count = 15,
    show_index_thread_row_count = 2,
    show_table_status_column_count = 18,
    show_thread_columns_row_count = 3,
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

static int test_performance_schema_memory_summary_placeholders(void);
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
    return test_performance_schema_memory_summary_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_memory_summary_placeholders(void) {
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_account_columns_rows[] = {
        "USER",
        "char(32)",
        "YES",
        "MUL",
        NULL,
        "",
        "EVENT_NAME",
        "varchar(128)",
        "NO",
        "",
        NULL,
        "",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "bigint",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_thread_columns_rows[] = {
        "THREAD_ID",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "EVENT_NAME",
        "varchar(128)",
        "NO",
        "PRI",
        NULL,
        "",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "bigint",
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
        "memory_summary_by_account_by_event_name",
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
        "memory_summary_by_account_by_event_name",
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
        "memory_summary_by_account_by_event_name",
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
    static const char *const show_index_thread_rows[] = {
        "memory_summary_by_thread_by_event_name",
        "0",
        "PRIMARY",
        "1",
        "THREAD_ID",
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
        "memory_summary_by_thread_by_event_name",
        "0",
        "PRIMARY",
        "2",
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
        "memory_summary_by_account_by_event_name",
        "USER",
        "1",
        "char(32)",
        "MUL",
        "YES",
        "memory_summary_by_account_by_event_name",
        "HOST",
        "2",
        "char(255)",
        "",
        "YES",
        "memory_summary_by_account_by_event_name",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "12",
        "bigint",
        "",
        "NO",
        "memory_summary_by_host_by_event_name",
        "HOST",
        "1",
        "char(255)",
        "MUL",
        "YES",
        "memory_summary_by_host_by_event_name",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "11",
        "bigint",
        "",
        "NO",
        "memory_summary_by_thread_by_event_name",
        "THREAD_ID",
        "1",
        "bigint unsigned",
        "PRI",
        "NO",
        "memory_summary_by_thread_by_event_name",
        "EVENT_NAME",
        "2",
        "varchar(128)",
        "PRI",
        "NO",
        "memory_summary_by_thread_by_event_name",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "11",
        "bigint",
        "",
        "NO",
        "memory_summary_by_user_by_event_name",
        "USER",
        "1",
        "char(32)",
        "MUL",
        "YES",
        "memory_summary_by_user_by_event_name",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "11",
        "bigint",
        "",
        "NO",
        "memory_summary_global_by_event_name",
        "EVENT_NAME",
        "1",
        "varchar(128)",
        "PRI",
        "NO",
        "memory_summary_global_by_event_name",
        "CURRENT_NUMBER_OF_BYTES_USED",
        "10",
        "bigint",
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
        "memory_summary_by_account_by_event_name", "ACCOUNT", "0", "1", "USER",       "HASH",
        "memory_summary_by_account_by_event_name", "ACCOUNT", "0", "2", "HOST",       "HASH",
        "memory_summary_by_account_by_event_name", "ACCOUNT", "0", "3", "EVENT_NAME", "HASH",
        "memory_summary_by_host_by_event_name",    "HOST",    "0", "1", "HOST",       "HASH",
        "memory_summary_by_host_by_event_name",    "HOST",    "0", "2", "EVENT_NAME", "HASH",
        "memory_summary_by_thread_by_event_name",  "PRIMARY", "0", "1", "THREAD_ID",  "HASH",
        "memory_summary_by_thread_by_event_name",  "PRIMARY", "0", "2", "EVENT_NAME", "HASH",
        "memory_summary_by_user_by_event_name",    "USER",    "0", "1", "USER",       "HASH",
        "memory_summary_by_user_by_event_name",    "USER",    "0", "2", "EVENT_NAME", "HASH",
        "memory_summary_global_by_event_name",     "PRIMARY", "0", "1", "EVENT_NAME", "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "memory_summary_by_account_by_event_name", "ACCOUNT", "UNIQUE",      "YES",
        "memory_summary_by_host_by_event_name",    "HOST",    "UNIQUE",      "YES",
        "memory_summary_by_thread_by_event_name",  "PRIMARY", "PRIMARY KEY", "YES",
        "memory_summary_by_user_by_event_name",    "USER",    "UNIQUE",      "YES",
        "memory_summary_global_by_event_name",     "PRIMARY", "PRIMARY KEY", "YES",
    };
    static const char *const information_schema_key_usage_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
    };
    static const char *const information_schema_key_usage_rows[] = {
        "memory_summary_by_account_by_event_name", "ACCOUNT", "USER",       "1",
        "memory_summary_by_account_by_event_name", "ACCOUNT", "HOST",       "2",
        "memory_summary_by_account_by_event_name", "ACCOUNT", "EVENT_NAME", "3",
        "memory_summary_by_host_by_event_name",    "HOST",    "HOST",       "1",
        "memory_summary_by_host_by_event_name",    "HOST",    "EVENT_NAME", "2",
        "memory_summary_by_thread_by_event_name",  "PRIMARY", "THREAD_ID",  "1",
        "memory_summary_by_thread_by_event_name",  "PRIMARY", "EVENT_NAME", "2",
        "memory_summary_by_user_by_event_name",    "USER",    "USER",       "1",
        "memory_summary_by_user_by_event_name",    "USER",    "EVENT_NAME", "2",
        "memory_summary_global_by_event_name",     "PRIMARY", "EVENT_NAME", "1",
    };
    static const char *const information_schema_constraint_extensions_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "ENGINE_ATTRIBUTE",
        "SECONDARY_ENGINE_ATTRIBUTE",
    };
    static const char *const information_schema_constraint_extensions_rows[] = {
        "memory_summary_by_account_by_event_name", "ACCOUNT", NULL, NULL,
        "memory_summary_by_host_by_event_name",    "HOST",    NULL, NULL,
        "memory_summary_by_thread_by_event_name",  "PRIMARY", NULL, NULL,
        "memory_summary_by_user_by_event_name",    "USER",    NULL, NULL,
        "memory_summary_global_by_event_name",     "PRIMARY", NULL, NULL,
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "AUTO_INCREMENT",
    };
    static const char *const information_schema_tables_rows[] = {
        "memory_summary_by_account_by_event_name", "PERFORMANCE_SCHEMA", "Dynamic", "60160",  NULL,
        "memory_summary_by_host_by_event_name",    "PERFORMANCE_SCHEMA", "Dynamic", "60160",  NULL,
        "memory_summary_by_thread_by_event_name",  "PERFORMANCE_SCHEMA", "Dynamic", "120320", NULL,
        "memory_summary_by_user_by_event_name",    "PERFORMANCE_SCHEMA", "Dynamic", "60160",  NULL,
        "memory_summary_global_by_event_name",     "PERFORMANCE_SCHEMA", "Dynamic", "470",    NULL,
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
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_open_temporary(&database) != MYLITE_OK) {
        fprintf(stderr, "failed to open temporary database\n");
        return 1;
    }

    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.memory_summary_by_account_by_event_name",
        "memory_summary_by_account_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.memory_summary_by_host_by_event_name",
        "memory_summary_by_host_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.memory_summary_by_thread_by_event_name",
        "memory_summary_by_thread_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.memory_summary_by_user_by_event_name",
        "memory_summary_by_user_by_event_name count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.memory_summary_global_by_event_name",
        "memory_summary_global_by_event_name count"
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM memory_summary_global_by_event_name",
        "selected performance_schema memory summary count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.memory_summary_by_account_by_event_name "
                   "WHERE Field IN ('USER', 'EVENT_NAME', 'CURRENT_NUMBER_OF_BYTES_USED')",
            .column_names = show_columns_columns,
            .values = show_account_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_account_columns_row_count,
            .context = "show memory summary by account columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.memory_summary_by_thread_by_event_name "
                   "WHERE Field IN ('THREAD_ID', 'EVENT_NAME', 'CURRENT_NUMBER_OF_BYTES_USED')",
            .column_names = show_columns_columns,
            .values = show_thread_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = show_thread_columns_row_count,
            .context = "show memory summary by thread columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.memory_summary_by_account_by_event_name",
            .column_names = show_index_columns,
            .values = show_index_account_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_account_row_count,
            .context = "show memory summary by account index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.memory_summary_by_thread_by_event_name",
            .column_names = show_index_columns,
            .values = show_index_thread_rows,
            .column_count = show_index_column_count,
            .row_count = show_index_thread_row_count,
            .context = "show memory summary by thread index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'memory_summary_global_by_event_name' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status for memory summary",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_TYPE, "
                   "COLUMN_KEY, IS_NULLABLE "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'memory_summary_by_account_by_event_name' "
                   "AND COLUMN_NAME IN ('USER', 'HOST', 'CURRENT_NUMBER_OF_BYTES_USED')) "
                   "OR (TABLE_NAME = 'memory_summary_by_host_by_event_name' "
                   "AND COLUMN_NAME IN ('HOST', 'CURRENT_NUMBER_OF_BYTES_USED')) "
                   "OR (TABLE_NAME = 'memory_summary_by_thread_by_event_name' "
                   "AND COLUMN_NAME IN ("
                   "'THREAD_ID', 'EVENT_NAME', 'CURRENT_NUMBER_OF_BYTES_USED')) "
                   "OR (TABLE_NAME = 'memory_summary_by_user_by_event_name' "
                   "AND COLUMN_NAME IN ('USER', 'CURRENT_NUMBER_OF_BYTES_USED')) "
                   "OR (TABLE_NAME = 'memory_summary_global_by_event_name' "
                   "AND COLUMN_NAME IN ('EVENT_NAME', 'CURRENT_NUMBER_OF_BYTES_USED'))) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information schema memory summary columns",
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
                   "'memory_summary_by_account_by_event_name', "
                   "'memory_summary_by_host_by_event_name', "
                   "'memory_summary_by_thread_by_event_name', "
                   "'memory_summary_by_user_by_event_name', "
                   "'memory_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema memory summary statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'memory_summary_by_account_by_event_name', "
                   "'memory_summary_by_host_by_event_name', "
                   "'memory_summary_by_thread_by_event_name', "
                   "'memory_summary_by_user_by_event_name', "
                   "'memory_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information schema memory summary constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'memory_summary_by_account_by_event_name', "
                   "'memory_summary_by_host_by_event_name', "
                   "'memory_summary_by_thread_by_event_name', "
                   "'memory_summary_by_user_by_event_name', "
                   "'memory_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION",
            .column_names = information_schema_key_usage_columns,
            .values = information_schema_key_usage_rows,
            .column_count = information_schema_key_usage_projection_count,
            .row_count = information_schema_key_usage_row_count,
            .context = "information schema memory summary key usage",
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
                   "'memory_summary_by_account_by_event_name', "
                   "'memory_summary_by_host_by_event_name', "
                   "'memory_summary_by_thread_by_event_name', "
                   "'memory_summary_by_user_by_event_name', "
                   "'memory_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraint_extensions_columns,
            .values = information_schema_constraint_extensions_rows,
            .column_count = information_schema_constraint_extensions_projection_count,
            .row_count = information_schema_constraint_extensions_row_count,
            .context = "information schema memory summary constraint extensions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AUTO_INCREMENT "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ("
                   "'memory_summary_by_account_by_event_name', "
                   "'memory_summary_by_host_by_event_name', "
                   "'memory_summary_by_thread_by_event_name', "
                   "'memory_summary_by_user_by_event_name', "
                   "'memory_summary_global_by_event_name') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information schema memory summary tables",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.memory_summary_global_by_event_name "
        "(EVENT_NAME) VALUES ('memory/sql/test')",
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
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const zero_row[] = {"0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_names = count_column,
            .values = zero_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    static const char *const row_count_column[] = {"ROW_COUNT()"};
    static const char *const row_count_row[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = row_count_column,
            .values = row_count_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
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
        expected == NULL ? "<NULL>" : expected,
        actual == NULL ? "<NULL>" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "<NULL>" : actual,
        needle == NULL ? "<NULL>" : needle
    );
    return 1;
}
