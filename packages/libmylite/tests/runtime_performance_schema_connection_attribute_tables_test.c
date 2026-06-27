#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    attribute_projection_count = 3,
    attribute_row_count = 3,
    information_schema_columns_projection_count = 4,
    information_schema_columns_row_count = 8,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 4,
    information_schema_tables_projection_count = 5,
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

static int test_performance_schema_connection_attribute_tables(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_connection_attribute_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_connection_attribute_tables(void) {
    static const char *const attribute_columns[] = {
        "ATTR_NAME",
        "ATTR_VALUE",
        "ORDINAL_POSITION",
    };
    static const char *const count_columns[] = {
        "COUNT(*)",
    };
    static const char *const count_rows[] = {
        "3",
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
    };
    static const char *const information_schema_columns_rows[] = {
        "session_account_connect_attrs",
        "PROCESSLIST_ID",
        "bigint unsigned",
        "PRI",
        "session_account_connect_attrs",
        "ATTR_NAME",
        "varchar(32)",
        "PRI",
        "session_account_connect_attrs",
        "ATTR_VALUE",
        "varchar(1024)",
        "",
        "session_account_connect_attrs",
        "ORDINAL_POSITION",
        "int",
        "",
        "session_connect_attrs",
        "PROCESSLIST_ID",
        "bigint unsigned",
        "PRI",
        "session_connect_attrs",
        "ATTR_NAME",
        "varchar(32)",
        "PRI",
        "session_connect_attrs",
        "ATTR_VALUE",
        "varchar(1024)",
        "",
        "session_connect_attrs",
        "ORDINAL_POSITION",
        "int",
        "",
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
        "session_account_connect_attrs", "PRIMARY", "0", "1", "PROCESSLIST_ID", "HASH",
        "session_account_connect_attrs", "PRIMARY", "0", "2", "ATTR_NAME",      "HASH",
        "session_connect_attrs",         "PRIMARY", "0", "1", "PROCESSLIST_ID", "HASH",
        "session_connect_attrs",         "PRIMARY", "0", "2", "ATTR_NAME",      "HASH",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "session_account_connect_attrs",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_bin",
        "session_connect_attrs",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_bin",
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
    const char *const attribute_rows[] = {
        "_client_name",
        "mylite",
        "0",
        "_client_version",
        mylite_version(),
        "1",
        "program_name",
        "mylite",
        "2",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ATTR_NAME, ATTR_VALUE, ORDINAL_POSITION "
                   "FROM performance_schema.session_connect_attrs "
                   "ORDER BY PROCESSLIST_ID, ORDINAL_POSITION",
            .column_names = attribute_columns,
            .values = attribute_rows,
            .column_count = attribute_projection_count,
            .row_count = attribute_row_count,
            .context = "session_connect_attrs rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ATTR_NAME, ATTR_VALUE, ORDINAL_POSITION "
                   "FROM performance_schema.session_account_connect_attrs "
                   "ORDER BY PROCESSLIST_ID, ORDINAL_POSITION",
            .column_names = attribute_columns,
            .values = attribute_rows,
            .column_count = attribute_projection_count,
            .row_count = attribute_row_count,
            .context = "session_account_connect_attrs rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.session_connect_attrs",
            .column_names = count_columns,
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "session_connect_attrs row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.session_account_connect_attrs",
            .column_names = count_columns,
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "session_account_connect_attrs row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('session_account_connect_attrs', "
                   "'session_connect_attrs') "
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
                   "AND TABLE_NAME IN ('session_account_connect_attrs', "
                   "'session_connect_attrs') "
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
                   "AND TABLE_NAME IN ('session_account_connect_attrs', "
                   "'session_connect_attrs') ORDER BY TABLE_NAME",
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
                   "WHERE Name = 'session_connect_attrs' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' AND Row_format = 'Dynamic' "
                   "AND Auto_increment IS NULL AND Collation = 'utf8mb4_bin'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status session_connect_attrs",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ATTR_NAME, ATTR_VALUE, ORDINAL_POSITION FROM session_connect_attrs "
                   "ORDER BY PROCESSLIST_ID, ORDINAL_POSITION",
            .column_names = attribute_columns,
            .values = attribute_rows,
            .column_count = attribute_projection_count,
            .row_count = attribute_row_count,
            .context = "selected performance_schema session_connect_attrs rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.session_connect_attrs "
        "(PROCESSLIST_ID, ATTR_NAME) VALUES (CONNECTION_ID(), 'x')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after connection attribute error");

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", query.context, mylite_errmsg(database));
        mylite_result_free(result);
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (mylite_result_column_count(result) == query.column_count) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_text(
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
                failures += expect_text(
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

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
