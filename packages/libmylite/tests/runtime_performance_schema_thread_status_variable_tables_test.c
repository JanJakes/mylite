#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    show_table_status_column_count = 18,
    information_schema_columns_projection_count = 6,
    information_schema_statistics_projection_count = 7,
    information_schema_statistics_row_count = 11,
    information_schema_tables_projection_count = 6,
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

static int test_performance_schema_thread_status_variable_tables(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_thread_status_variable_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_thread_status_variable_tables(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const variable_columns[] = {"VARIABLE_NAME", "VARIABLE_VALUE"};
    static const char *const account_status_columns[] = {
        "USER",
        "HOST",
        "VARIABLE_NAME",
        "VARIABLE_VALUE",
    };
    static const char *const host_status_columns[] = {
        "HOST",
        "VARIABLE_NAME",
        "VARIABLE_VALUE",
    };
    static const char *const user_status_columns[] = {
        "USER",
        "VARIABLE_NAME",
        "VARIABLE_VALUE",
    };
    static const char *const show_full_columns_columns[] = {
        "Field",
        "Type",
        "Collation",
        "Null",
        "Key",
        "Default",
        "Extra",
        "Privileges",
        "Comment",
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
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
    };
    static const char *const information_schema_statistics_columns[] = {
        "TABLE_NAME",
        "INDEX_NAME",
        "SEQ_IN_INDEX",
        "COLUMN_NAME",
        "COLLATION",
        "CARDINALITY",
        "INDEX_TYPE",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const variables_by_thread_count[] = {"643"};
    static const char *const status_count[] = {"338"};
    static const char *const variable_rows[] = {
        "autocommit",
        "OFF",
        "time_zone",
        "+02:00",
    };
    static const char *const status_rows[] = {
        "Com_stmt_reprepare",
        "0",
        "Compression",
        "OFF",
    };
    static const char *const account_status_rows[] = {
        "root",
        "%",
        "Com_stmt_reprepare",
        "0",
        "root",
        "%",
        "Compression",
        "OFF",
    };
    static const char *const host_status_rows[] = {
        "%",
        "Com_stmt_reprepare",
        "0",
        "%",
        "Compression",
        "OFF",
    };
    static const char *const user_status_rows[] = {
        "root",
        "Com_stmt_reprepare",
        "0",
        "root",
        "Compression",
        "OFF",
    };
    static const char *const selected_schema_value[] = {"OFF"};
    static const char *const variables_full_columns[] = {
        "THREAD_ID",
        "bigint unsigned",
        NULL,
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "VARIABLE_NAME",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "PRI",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "VARIABLE_VALUE",
        "varchar(1024)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const account_full_columns[] = {
        "USER",
        "char(32)",
        "utf8mb4_bin",
        "YES",
        "MUL",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "HOST",
        "char(255)",
        "ascii_general_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "VARIABLE_NAME",
        "varchar(64)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "VARIABLE_VALUE",
        "varchar(1024)",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const variables_show_index[] = {
        "variables_by_thread",
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
        "variables_by_thread",
        "0",
        "PRIMARY",
        "2",
        "VARIABLE_NAME",
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
    static const char *const account_show_index[] = {
        "status_by_account",
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
        "status_by_account",
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
        "status_by_account",
        "0",
        "ACCOUNT",
        "3",
        "VARIABLE_NAME",
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
    static const char *const information_schema_columns[] = {
        "variables_by_thread",
        "THREAD_ID",
        "1",
        "NO",
        "bigint unsigned",
        "PRI",
        "variables_by_thread",
        "VARIABLE_NAME",
        "2",
        "NO",
        "varchar(64)",
        "PRI",
        "variables_by_thread",
        "VARIABLE_VALUE",
        "3",
        "YES",
        "varchar(1024)",
        "",
    };
    static const char *const information_schema_statistics[] = {
        "status_by_account",   "ACCOUNT", "1", "USER",          NULL, NULL, "HASH",
        "status_by_account",   "ACCOUNT", "2", "HOST",          NULL, NULL, "HASH",
        "status_by_account",   "ACCOUNT", "3", "VARIABLE_NAME", NULL, NULL, "HASH",
        "status_by_host",      "HOST",    "1", "HOST",          NULL, NULL, "HASH",
        "status_by_host",      "HOST",    "2", "VARIABLE_NAME", NULL, NULL, "HASH",
        "status_by_thread",    "PRIMARY", "1", "THREAD_ID",     NULL, NULL, "HASH",
        "status_by_thread",    "PRIMARY", "2", "VARIABLE_NAME", NULL, NULL, "HASH",
        "status_by_user",      "USER",    "1", "USER",          NULL, NULL, "HASH",
        "status_by_user",      "USER",    "2", "VARIABLE_NAME", NULL, NULL, "HASH",
        "variables_by_thread", "PRIMARY", "1", "THREAD_ID",     NULL, NULL, "HASH",
        "variables_by_thread", "PRIMARY", "2", "VARIABLE_NAME", NULL, NULL, "HASH",
    };
    static const char *const information_schema_tables[] = {
        "variables_by_thread",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
    };
    static const struct expected_sql_error access_denied = {
        .code = mysql_error_access_denied,
        .sqlstate = "42000",
        .message_part = "Access denied",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "SET autocommit = 0");
    failures += execute_ok(database, "SET time_zone = '+02:00'");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.variables_by_thread",
            .column_names = count_column,
            .values = variables_by_thread_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "variables_by_thread row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.status_by_thread",
            .column_names = count_column,
            .values = status_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "status_by_thread row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.status_by_account",
            .column_names = count_column,
            .values = status_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "status_by_account row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.status_by_host",
            .column_names = count_column,
            .values = status_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "status_by_host row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.status_by_user",
            .column_names = count_column,
            .values = status_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "status_by_user row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIABLE_NAME, VARIABLE_VALUE "
                   "FROM performance_schema.variables_by_thread "
                   "WHERE VARIABLE_NAME IN ('autocommit', 'time_zone') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = variable_columns,
            .values = variable_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "variables_by_thread rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIABLE_NAME, VARIABLE_VALUE "
                   "FROM performance_schema.status_by_thread "
                   "WHERE VARIABLE_NAME IN ('Com_stmt_reprepare', 'Compression') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = variable_columns,
            .values = status_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "status_by_thread rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER, HOST, VARIABLE_NAME, VARIABLE_VALUE "
                   "FROM performance_schema.status_by_account "
                   "WHERE VARIABLE_NAME IN ('Com_stmt_reprepare', 'Compression') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = account_status_columns,
            .values = account_status_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "status_by_account rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HOST, VARIABLE_NAME, VARIABLE_VALUE "
                   "FROM performance_schema.status_by_host "
                   "WHERE VARIABLE_NAME IN ('Com_stmt_reprepare', 'Compression') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = host_status_columns,
            .values = host_status_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "status_by_host rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER, VARIABLE_NAME, VARIABLE_VALUE "
                   "FROM performance_schema.status_by_user "
                   "WHERE VARIABLE_NAME IN ('Com_stmt_reprepare', 'Compression') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = user_status_columns,
            .values = user_status_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "status_by_user rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIABLE_VALUE FROM variables_by_thread "
                   "WHERE VARIABLE_NAME = 'autocommit'",
            .column_names = (const char *const[]){"VARIABLE_VALUE"},
            .values = selected_schema_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.variables_by_thread",
            .column_names = show_full_columns_columns,
            .values = variables_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = 3U,
            .context = "variables_by_thread show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.status_by_account",
            .column_names = show_full_columns_columns,
            .values = account_full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = 4U,
            .context = "status_by_account show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.variables_by_thread",
            .column_names = show_index_columns,
            .values = variables_show_index,
            .column_count = show_index_column_count,
            .row_count = 2U,
            .context = "variables_by_thread show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.status_by_account",
            .column_names = show_index_columns,
            .values = account_show_index,
            .column_count = show_index_column_count,
            .row_count = 3U,
            .context = "status_by_account show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'variables_by_thread' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "variables_by_thread show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, "
                   "COLUMN_TYPE, COLUMN_KEY "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'variables_by_thread' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = 3U,
            .context = "information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, "
                   "COLLATION, CARDINALITY, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('variables_by_thread', 'status_by_thread', "
                   "'status_by_account', 'status_by_host', 'status_by_user') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'variables_by_thread'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "information schema tables",
        }
    );
    failures += execute_error(
        database,
        "UPDATE performance_schema.status_by_thread SET VARIABLE_VALUE = '1'",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after denied update");

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
        if (query.values != NULL) {
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
