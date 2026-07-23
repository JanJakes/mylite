#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    count_projection_count = 1,
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 11,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 1,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 2,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 3,
    show_columns_column_count = 6,
    show_host_cache_row_count = 29,
    show_index_column_count = 15,
    show_index_host_cache_row_count = 2,
    show_keyring_keys_row_count = 3,
    show_table_status_column_count = 18,
    mysql_error_access_denied = 1044,
};

static const char host_cache_validated_type[] = "enum('YES','NO')";

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

static int test_performance_schema_host_keyring_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);

int main(void) {
    return test_performance_schema_host_keyring_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_host_keyring_placeholders(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const count_2_rows[] = {"2"};
    static const char *const count_3_rows[] = {"3"};
    static const char *const count_29_rows[] = {"29"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const describe_keyring_keys_values[] = {
        "KEY_ID",
        "varchar(255)",
        "NO",
        "",
        NULL,
        "",
        "KEY_OWNER",
        "varchar(255)",
        "YES",
        "",
        NULL,
        "",
        "BACKEND_KEY_ID",
        "varchar(255)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const information_schema_columns_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "COLUMN_TYPE",
        "COLUMN_KEY",
        "IS_NULLABLE",
        "COLLATION_NAME",
    };
    static const char *const information_schema_columns_rows[] = {
        "host_cache",
        "IP",
        "varchar(64)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "host_cache",
        "HOST",
        "varchar(255)",
        "MUL",
        "YES",
        "ascii_general_ci",
        "host_cache",
        "HOST_VALIDATED",
        host_cache_validated_type,
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "host_cache",
        "SUM_CONNECT_ERRORS",
        "bigint",
        "",
        "NO",
        NULL,
        "host_cache",
        "FIRST_SEEN",
        "timestamp",
        "",
        "NO",
        NULL,
        "host_cache",
        "LAST_ERROR_SEEN",
        "timestamp",
        "",
        "YES",
        NULL,
        "keyring_component_status",
        "STATUS_KEY",
        "varchar(256)",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "keyring_component_status",
        "STATUS_VALUE",
        "varchar(1024)",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "keyring_keys",
        "KEY_ID",
        "varchar(255)",
        "",
        "NO",
        "utf8mb4_bin",
        "keyring_keys",
        "KEY_OWNER",
        "varchar(255)",
        "",
        "YES",
        "utf8mb4_bin",
        "keyring_keys",
        "BACKEND_KEY_ID",
        "varchar(255)",
        "",
        "YES",
        "utf8mb4_bin",
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
        "host_cache",
        "HOST",
        "1",
        "1",
        "HOST",
        "HASH",
        "host_cache",
        "PRIMARY",
        "0",
        "1",
        "IP",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "host_cache",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "host_cache",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "keyring_component_status",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "keyring_keys",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_bin",
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
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.host_cache",
        "host_cache row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.keyring_component_status",
        "keyring_component_status row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.keyring_keys",
        "keyring_keys row count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.host_cache",
            .column_names = show_columns_columns,
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = show_host_cache_row_count,
            .context = "host_cache show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC performance_schema.keyring_keys",
            .column_names = show_columns_columns,
            .values = describe_keyring_keys_values,
            .column_count = show_columns_column_count,
            .row_count = show_keyring_keys_row_count,
            .context = "keyring_keys describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' AND TABLE_NAME = 'host_cache'",
            .column_names = count_columns,
            .values = count_29_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "host_cache column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'keyring_component_status'",
            .column_names = count_columns,
            .values = count_2_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "keyring_component_status column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' AND TABLE_NAME = 'keyring_keys'",
            .column_names = count_columns,
            .values = count_3_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "keyring_keys column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY, "
                   "IS_NULLABLE, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('host_cache', 'keyring_component_status', "
                   "'keyring_keys') "
                   "AND COLUMN_NAME IN ('IP', 'HOST', 'HOST_VALIDATED', "
                   "'SUM_CONNECT_ERRORS', 'FIRST_SEEN', 'LAST_ERROR_SEEN', "
                   "'STATUS_KEY', 'STATUS_VALUE', 'KEY_ID', 'KEY_OWNER', "
                   "'BACKEND_KEY_ID') "
                   "ORDER BY TABLE_NAME, ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns_rows,
            .column_count = information_schema_columns_projection_count,
            .row_count = information_schema_columns_row_count,
            .context = "information_schema selected columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('host_cache', 'keyring_component_status', "
                   "'keyring_keys') "
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
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('host_cache', 'keyring_component_status', "
                   "'keyring_keys') "
                   "ORDER BY TABLE_NAME, CONSTRAINT_NAME",
            .column_names = information_schema_constraints_columns,
            .values = information_schema_constraints_rows,
            .column_count = information_schema_constraints_projection_count,
            .row_count = information_schema_constraints_row_count,
            .context = "information_schema constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('host_cache', 'keyring_component_status', "
                   "'keyring_keys') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables_rows,
            .column_count = information_schema_tables_projection_count,
            .row_count = information_schema_tables_row_count,
            .context = "information_schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.host_cache",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = show_index_host_cache_row_count,
            .context = "host_cache show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'keyring_keys' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_bin'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status keyring_keys",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM keyring_component_status",
            .column_names = count_columns,
            .values = zero_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "selected performance_schema keyring_component_status row count",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.keyring_keys VALUES ('key', NULL, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after host/keyring placeholder error");

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

static int expect_empty_count(mylite_db *database, const char *sql, const char *context) {
    static const char *const columns[] = {"COUNT(*)"};
    static const char *const values[] = {"0"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = sql,
            .column_names = columns,
            .values = values,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = context,
        }
    );
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
