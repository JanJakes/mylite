#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    count_projection_count = 1,
    information_schema_columns_projection_count = 5,
    information_schema_columns_row_count = 10,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 1,
    information_schema_tables_projection_count = 5,
    show_index_column_count = 15,
    show_table_status_column_count = 18,
    variables_info_projection_count = 8,
    variables_info_row_count = 3,
    mysql_error_access_denied = 1044,
};

static const char variables_info_source_column_type[] =
    "enum('COMPILED','GLOBAL','SERVER','EXPLICIT','EXTRA','USER','LOGIN','COMMAND_LINE','PERSISTED'"
    ",'DYNAMIC')";

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

static int test_performance_schema_variable_info_tables(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_variable_info_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_variable_info_tables(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const persisted_count_rows[] = {"0"};
    static const char *const variables_info_count_rows[] = {"643"};
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "COLLATION_NAME",
    };
    static const char *const information_schema_columns_rows[] = {
        "persisted_variables",
        "VARIABLE_NAME",
        "varchar(64)",
        "PRI",
        "utf8mb4_0900_ai_ci",
        "persisted_variables",
        "VARIABLE_VALUE",
        "varchar(1024)",
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "VARIABLE_NAME",
        "varchar(64)",
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "VARIABLE_SOURCE",
        variables_info_source_column_type,
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "VARIABLE_PATH",
        "varchar(1024)",
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "MIN_VALUE",
        "varchar(64)",
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "MAX_VALUE",
        "varchar(64)",
        "",
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "SET_TIME",
        "timestamp(6)",
        "",
        NULL,
        "variables_info",
        "SET_USER",
        "char(32)",
        "",
        "utf8mb4_bin",
        "variables_info",
        "SET_HOST",
        "char(255)",
        "",
        "ascii_general_ci",
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
        "persisted_variables",
        "PRIMARY",
        "0",
        "1",
        "VARIABLE_NAME",
        "HASH",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "persisted_variables",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "variables_info",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
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
    static const char *const variables_info_columns[] = {
        "VARIABLE_NAME",
        "VARIABLE_SOURCE",
        "VARIABLE_PATH",
        "MIN_VALUE",
        "MAX_VALUE",
        "SET_TIME",
        "SET_USER",
        "SET_HOST",
    };
    static const char *const variables_info_rows[] = {
        "autocommit", "COMPILED", "", "0", "0", NULL, "", "",
        "time_zone",  "COMPILED", "", "0", "0", NULL, "", "",
        "version",    "COMPILED", "", "0", "0", NULL, "", "",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.persisted_variables",
            .column_names = count_columns,
            .values = persisted_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "persisted_variables row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.variables_info",
            .column_names = count_columns,
            .values = variables_info_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "variables_info row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIABLE_NAME, VARIABLE_SOURCE, VARIABLE_PATH, MIN_VALUE, "
                   "MAX_VALUE, SET_TIME, SET_USER, SET_HOST "
                   "FROM performance_schema.variables_info "
                   "WHERE VARIABLE_NAME IN ('autocommit', 'time_zone', 'version') "
                   "ORDER BY VARIABLE_NAME",
            .column_names = variables_info_columns,
            .values = variables_info_rows,
            .column_count = variables_info_projection_count,
            .row_count = variables_info_row_count,
            .context = "variables_info representative rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('persisted_variables', 'variables_info') "
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
                   "AND TABLE_NAME IN ('persisted_variables', 'variables_info') "
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
                   "AND TABLE_NAME IN ('persisted_variables', 'variables_info') "
                   "ORDER BY TABLE_NAME",
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
            .sql = "SHOW INDEX FROM performance_schema.variables_info",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "variables_info empty index metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'variables_info' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status variables_info",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VARIABLE_SOURCE FROM variables_info "
                   "WHERE VARIABLE_NAME = 'autocommit'",
            .column_names = (const char *const[]){"VARIABLE_SOURCE"},
            .values = (const char *const[]){"COMPILED"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema variables_info row",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.persisted_variables VALUES ('autocommit', 'ON')",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after variable info error");

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
