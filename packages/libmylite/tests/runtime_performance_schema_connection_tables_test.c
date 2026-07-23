#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    account_column_count = 6,
    host_column_count = 5,
    user_column_count = 5,
    processlist_projection_count = 8,
    thread_projection_count = 17,
    mysql_error_access_denied = 1044,
    information_schema_columns_projection_count = 4,
    information_schema_columns_row_count = 7,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 10,
    information_schema_tables_projection_count = 5,
    show_table_status_column_count = 18,
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

static int test_performance_schema_connection_tables(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_connection_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_connection_tables(void) {
    static const char *const account_columns[] = {
        "USER",
        "HOST",
        "CURRENT_CONNECTIONS",
        "TOTAL_CONNECTIONS",
        "MAX_SESSION_CONTROLLED_MEMORY",
        "MAX_SESSION_TOTAL_MEMORY",
    };
    static const char *const host_columns[] = {
        "HOST",
        "CURRENT_CONNECTIONS",
        "TOTAL_CONNECTIONS",
        "MAX_SESSION_CONTROLLED_MEMORY",
        "MAX_SESSION_TOTAL_MEMORY",
    };
    static const char *const user_columns[] = {
        "USER",
        "CURRENT_CONNECTIONS",
        "TOTAL_CONNECTIONS",
        "MAX_SESSION_CONTROLLED_MEMORY",
        "MAX_SESSION_TOTAL_MEMORY",
    };
    static const char *const processlist_columns[] = {
        "USER",
        "HOST",
        "DB",
        "COMMAND",
        "TIME",
        "STATE",
        "INFO",
        "EXECUTION_ENGINE",
    };
    static const char *const thread_columns[] = {
        "NAME",
        "TYPE",
        "PROCESSLIST_USER",
        "PROCESSLIST_HOST",
        "PROCESSLIST_DB",
        "PROCESSLIST_COMMAND",
        "PROCESSLIST_TIME",
        "PROCESSLIST_STATE",
        "PROCESSLIST_INFO",
        "INSTRUMENTED",
        "HISTORY",
        "EXECUTION_ENGINE",
        "CONTROLLED_MEMORY",
        "MAX_CONTROLLED_MEMORY",
        "TOTAL_MEMORY",
        "MAX_TOTAL_MEMORY",
        "TELEMETRY_ACTIVE",
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "NON_UNIQUE",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "INDEX_TYPE",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
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
    static const char *const selected_schema_columns[] = {"USER"};
    static const char *const account_rows[] = {
        "root",
        "%",
        "1",
        "1",
        "0",
        "0",
    };
    static const char *const host_rows[] = {
        "%",
        "1",
        "1",
        "0",
        "0",
    };
    static const char *const user_rows[] = {
        "root",
        "1",
        "1",
        "0",
        "0",
    };
    static const char *const processlist_sql =
        "SELECT USER, HOST, DB, COMMAND, TIME, STATE, INFO, EXECUTION_ENGINE "
        "FROM performance_schema.processlist ORDER BY ID";
    static const char *const processlist_rows[] = {
        "root",
        "%",
        "app",
        "Query",
        "0",
        "executing",
        processlist_sql,
        "PRIMARY",
    };
    static const char *const threads_sql =
        "SELECT NAME, TYPE, PROCESSLIST_USER, PROCESSLIST_HOST, PROCESSLIST_DB, "
        "PROCESSLIST_COMMAND, PROCESSLIST_TIME, PROCESSLIST_STATE, PROCESSLIST_INFO, "
        "INSTRUMENTED, HISTORY, EXECUTION_ENGINE, CONTROLLED_MEMORY, MAX_CONTROLLED_MEMORY, "
        "TOTAL_MEMORY, MAX_TOTAL_MEMORY, TELEMETRY_ACTIVE FROM performance_schema.threads "
        "ORDER BY THREAD_ID";
    static const char *const thread_rows[] = {
        "thread/sql/one_connection",
        "FOREGROUND",
        "root",
        "%",
        "app",
        "Query",
        "0",
        "executing",
        threads_sql,
        "YES",
        "YES",
        "PRIMARY",
        "0",
        "0",
        "0",
        "0",
        "NO",
    };
    static const char *const information_schema_columns_rows[] = {
        "accounts",
        "USER",
        "char(32)",
        "MUL",
        "accounts",
        "CURRENT_CONNECTIONS",
        "bigint",
        "",
        "processlist",
        "ID",
        "bigint unsigned",
        "PRI",
        "processlist",
        "EXECUTION_ENGINE",
        "enum('PRIMARY','SECONDARY')",
        "",
        "threads",
        "THREAD_ID",
        "bigint unsigned",
        "PRI",
        "threads",
        "PROCESSLIST_USER",
        "varchar(32)",
        "MUL",
        "threads",
        "TELEMETRY_ACTIVE",
        "enum('YES','NO')",
        "",
    };
    static const char *const information_schema_statistics_rows[] = {
        "accounts",
        "ACCOUNT",
        "0",
        "1",
        "USER",
        "HASH",
        "accounts",
        "ACCOUNT",
        "0",
        "2",
        "HOST",
        "HASH",
        "threads",
        "NAME",
        "1",
        "1",
        "NAME",
        "HASH",
        "threads",
        "PRIMARY",
        "0",
        "1",
        "THREAD_ID",
        "HASH",
        "threads",
        "PROCESSLIST_ACCOUNT",
        "1",
        "1",
        "PROCESSLIST_USER",
        "HASH",
        "threads",
        "PROCESSLIST_ACCOUNT",
        "1",
        "2",
        "PROCESSLIST_HOST",
        "HASH",
        "threads",
        "PROCESSLIST_HOST",
        "1",
        "1",
        "PROCESSLIST_HOST",
        "HASH",
        "threads",
        "PROCESSLIST_ID",
        "1",
        "1",
        "PROCESSLIST_ID",
        "HASH",
        "threads",
        "RESOURCE_GROUP",
        "1",
        "1",
        "RESOURCE_GROUP",
        "HASH",
        "threads",
        "THREAD_OS_ID",
        "1",
        "1",
        "THREAD_OS_ID",
        "HASH",
    };
    static const char *const information_schema_tables_rows[] = {
        "accounts",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        NULL,
        "utf8mb4_0900_ai_ci",
        "processlist",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const char *const selected_schema_rows[] = {"root"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER, HOST, CURRENT_CONNECTIONS, TOTAL_CONNECTIONS, "
                   "MAX_SESSION_CONTROLLED_MEMORY, MAX_SESSION_TOTAL_MEMORY "
                   "FROM performance_schema.accounts ORDER BY USER, HOST",
            .column_names = account_columns,
            .values = account_rows,
            .column_count = account_column_count,
            .row_count = 1U,
            .context = "accounts rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HOST, CURRENT_CONNECTIONS, TOTAL_CONNECTIONS, "
                   "MAX_SESSION_CONTROLLED_MEMORY, MAX_SESSION_TOTAL_MEMORY "
                   "FROM performance_schema.hosts ORDER BY HOST",
            .column_names = host_columns,
            .values = host_rows,
            .column_count = host_column_count,
            .row_count = 1U,
            .context = "hosts rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER, CURRENT_CONNECTIONS, TOTAL_CONNECTIONS, "
                   "MAX_SESSION_CONTROLLED_MEMORY, MAX_SESSION_TOTAL_MEMORY "
                   "FROM performance_schema.users ORDER BY USER",
            .column_names = user_columns,
            .values = user_rows,
            .column_count = user_column_count,
            .row_count = 1U,
            .context = "users rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = processlist_sql,
            .column_names = processlist_columns,
            .values = processlist_rows,
            .column_count = processlist_projection_count,
            .row_count = 1U,
            .context = "processlist rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = threads_sql,
            .column_names = thread_columns,
            .values = thread_rows,
            .column_count = thread_projection_count,
            .row_count = 1U,
            .context = "threads rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND ((TABLE_NAME = 'accounts' AND COLUMN_NAME IN "
                   "('USER', 'CURRENT_CONNECTIONS')) "
                   "OR (TABLE_NAME = 'processlist' AND COLUMN_NAME IN "
                   "('ID', 'EXECUTION_ENGINE')) "
                   "OR (TABLE_NAME = 'threads' AND COLUMN_NAME IN "
                   "('THREAD_ID', 'PROCESSLIST_USER', 'TELEMETRY_ACTIVE'))) "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information_schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, "
                   "INDEX_TYPE FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('accounts', 'threads') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information_schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('accounts', 'processlist') ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = 2U,
            .context = "information_schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'accounts' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Fixed' AND Auto_increment IS NULL",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status accounts",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'processlist' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' AND Auto_increment IS NULL",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status processlist",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER FROM users",
            .column_names = selected_schema_columns,
            .values = selected_schema_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema users",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.threads SET INSTRUMENTED = 'NO'",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after connection table error");

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", query.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (mylite_result_column_count(result) == query.column_count) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                query.column_names[column],
                query.context
            );
        }
    }
    if (query.values != NULL && mylite_result_column_count(result) == query.column_count &&
        mylite_result_row_count(result) == query.row_count) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t index = (row * query.column_count) + column;
                failures += mylite_test_expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[index],
                    query.context
                );
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_state(mylite_db *database, const char *context) {
    static const char *const columns[] = {"ROW_COUNT()"};
    static const char *const values[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = columns,
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}
