#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    count_projection_count = 1,
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 14,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 4,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 14,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 4,
    show_columns_column_count = 6,
    show_data_lock_waits_row_count = 11,
    show_index_column_count = 15,
    show_index_data_lock_waits_row_count = 15,
    show_prepared_columns_row_count = 40,
    show_table_status_column_count = 18,
    mysql_error_access_denied = 1044,
};

static const char prepared_owner_object_type[] =
    "enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')";

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

static int test_performance_schema_instrumentation_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_empty_count(mylite_db *database, const char *sql, const char *context);
static int expect_row_count_state(mylite_db *database, const char *context);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_performance_schema_instrumentation_placeholders() == 0 ? 0 : 1;
}

static int test_performance_schema_instrumentation_placeholders(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const count_11_rows[] = {"11"};
    static const char *const count_14_rows[] = {"14"};
    static const char *const count_15_rows[] = {"15"};
    static const char *const count_40_rows[] = {"40"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const describe_data_lock_waits_values[] = {
        "ENGINE",
        "varchar(32)",
        "NO",
        "PRI",
        NULL,
        "",
        "REQUESTING_ENGINE_LOCK_ID",
        "varchar(128)",
        "NO",
        "PRI",
        NULL,
        "",
        "REQUESTING_ENGINE_TRANSACTION_ID",
        "bigint unsigned",
        "YES",
        "MUL",
        NULL,
        "",
        "REQUESTING_THREAD_ID",
        "bigint unsigned",
        "YES",
        "MUL",
        NULL,
        "",
        "REQUESTING_EVENT_ID",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "REQUESTING_OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "NO",
        "",
        NULL,
        "",
        "BLOCKING_ENGINE_LOCK_ID",
        "varchar(128)",
        "NO",
        "PRI",
        NULL,
        "",
        "BLOCKING_ENGINE_TRANSACTION_ID",
        "bigint unsigned",
        "YES",
        "MUL",
        NULL,
        "",
        "BLOCKING_THREAD_ID",
        "bigint unsigned",
        "YES",
        "MUL",
        NULL,
        "",
        "BLOCKING_EVENT_ID",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "BLOCKING_OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "NO",
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
        "binary_log_transaction_compression_stats",
        "LOG_TYPE",
        "enum('BINARY','RELAY')",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "binary_log_transaction_compression_stats",
        "COMPRESSION_TYPE",
        "varchar(64)",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "binary_log_transaction_compression_stats",
        "LAST_TRANSACTION_TIMESTAMP",
        "timestamp(6)",
        "",
        "YES",
        NULL,
        "data_lock_waits",
        "ENGINE",
        "varchar(32)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "data_lock_waits",
        "REQUESTING_ENGINE_LOCK_ID",
        "varchar(128)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "data_lock_waits",
        "BLOCKING_ENGINE_LOCK_ID",
        "varchar(128)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "data_locks",
        "ENGINE",
        "varchar(32)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "data_locks",
        "ENGINE_LOCK_ID",
        "varchar(128)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "data_locks",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "",
        "NO",
        NULL,
        "data_locks",
        "LOCK_DATA",
        "varchar(8192)",
        "",
        "YES",
        "utf8mb4_0900_ai_ci",
        "prepared_statements_instances",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        "prepared_statements_instances",
        "OWNER_THREAD_ID",
        "bigint unsigned",
        "MUL",
        "NO",
        NULL,
        "prepared_statements_instances",
        "OWNER_OBJECT_TYPE",
        prepared_owner_object_type,
        "MUL",
        "YES",
        "utf8mb4_0900_ai_ci",
        "prepared_statements_instances",
        "COUNT_SECONDARY",
        "bigint unsigned",
        "",
        "NO",
        NULL,
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
        "data_lock_waits",
        "PRIMARY",
        "0",
        "1",
        "REQUESTING_ENGINE_LOCK_ID",
        "HASH",
        "data_lock_waits",
        "PRIMARY",
        "0",
        "2",
        "BLOCKING_ENGINE_LOCK_ID",
        "HASH",
        "data_lock_waits",
        "PRIMARY",
        "0",
        "3",
        "ENGINE",
        "HASH",
        "data_lock_waits",
        "REQUESTING_THREAD_ID",
        "1",
        "1",
        "REQUESTING_THREAD_ID",
        "HASH",
        "data_lock_waits",
        "REQUESTING_THREAD_ID",
        "1",
        "2",
        "REQUESTING_EVENT_ID",
        "HASH",
        "data_locks",
        "OBJECT_SCHEMA",
        "1",
        "1",
        "OBJECT_SCHEMA",
        "HASH",
        "data_locks",
        "OBJECT_SCHEMA",
        "1",
        "2",
        "OBJECT_NAME",
        "HASH",
        "data_locks",
        "OBJECT_SCHEMA",
        "1",
        "3",
        "PARTITION_NAME",
        "HASH",
        "data_locks",
        "OBJECT_SCHEMA",
        "1",
        "4",
        "SUBPARTITION_NAME",
        "HASH",
        "data_locks",
        "PRIMARY",
        "0",
        "1",
        "ENGINE_LOCK_ID",
        "HASH",
        "data_locks",
        "PRIMARY",
        "0",
        "2",
        "ENGINE",
        "HASH",
        "prepared_statements_instances",
        "OWNER_THREAD_ID",
        "0",
        "1",
        "OWNER_THREAD_ID",
        "HASH",
        "prepared_statements_instances",
        "OWNER_THREAD_ID",
        "0",
        "2",
        "OWNER_EVENT_ID",
        "HASH",
        "prepared_statements_instances",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "data_lock_waits",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "data_locks",
        "PRIMARY",
        "PRIMARY KEY",
        "YES",
        "prepared_statements_instances",
        "OWNER_THREAD_ID",
        "UNIQUE",
        "YES",
        "prepared_statements_instances",
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
        "binary_log_transaction_compression_stats",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "data_lock_waits",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "data_locks",
        "PERFORMANCE_SCHEMA",
        "Dynamic",
        NULL,
        "utf8mb4_0900_ai_ci",
        "prepared_statements_instances",
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
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.binary_log_transaction_compression_stats",
        "binary_log_transaction_compression_stats row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.data_locks",
        "data_locks row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.data_lock_waits",
        "data_lock_waits row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.prepared_statements_instances",
        "prepared_statements_instances row count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC performance_schema.data_lock_waits",
            .column_names = show_columns_columns,
            .values = describe_data_lock_waits_values,
            .column_count = show_columns_column_count,
            .row_count = show_data_lock_waits_row_count,
            .context = "data_lock_waits describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.prepared_statements_instances",
            .column_names = show_columns_columns,
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = show_prepared_columns_row_count,
            .context = "prepared_statements_instances show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'binary_log_transaction_compression_stats'",
            .column_names = count_columns,
            .values = count_14_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "binary_log_transaction_compression_stats column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' AND TABLE_NAME = 'data_locks'",
            .column_names = count_columns,
            .values = count_15_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "data_locks column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' AND TABLE_NAME = 'data_lock_waits'",
            .column_names = count_columns,
            .values = count_11_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "data_lock_waits column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'prepared_statements_instances'",
            .column_names = count_columns,
            .values = count_40_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "prepared_statements_instances column count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY, "
                   "IS_NULLABLE, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('binary_log_transaction_compression_stats', "
                   "'data_locks', 'data_lock_waits', 'prepared_statements_instances') "
                   "AND COLUMN_NAME IN ('LOG_TYPE', 'COMPRESSION_TYPE', "
                   "'LAST_TRANSACTION_TIMESTAMP', 'ENGINE', 'ENGINE_LOCK_ID', "
                   "'LOCK_DATA', 'REQUESTING_ENGINE_LOCK_ID', "
                   "'BLOCKING_ENGINE_LOCK_ID', 'OBJECT_INSTANCE_BEGIN', "
                   "'OWNER_THREAD_ID', 'OWNER_OBJECT_TYPE', 'COUNT_SECONDARY') "
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
                   "AND TABLE_NAME IN ('data_locks', 'data_lock_waits', "
                   "'prepared_statements_instances') "
                   "AND INDEX_NAME IN ('PRIMARY', 'OBJECT_SCHEMA', "
                   "'REQUESTING_THREAD_ID', 'OWNER_THREAD_ID') "
                   "ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX",
            .column_names = information_schema_statistics_columns,
            .values = information_schema_statistics_rows,
            .column_count = information_schema_statistics_projection_count,
            .row_count = information_schema_statistics_row_count,
            .context = "information_schema selected statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('binary_log_transaction_compression_stats', "
                   "'data_locks', 'data_lock_waits', 'prepared_statements_instances') "
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
                   "AND TABLE_NAME IN ('binary_log_transaction_compression_stats', "
                   "'data_locks', 'data_lock_waits', 'prepared_statements_instances') "
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
            .sql = "SHOW INDEX FROM performance_schema.data_lock_waits",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = show_index_data_lock_waits_row_count,
            .context = "data_lock_waits show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'data_locks' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status data_locks",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM prepared_statements_instances",
            .column_names = count_columns,
            .values = zero_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "selected performance_schema prepared_statements_instances row count",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.data_locks VALUES "
        "('INNODB', 'lock-id', 1, 1, 1, 'app', 't', NULL, NULL, NULL, 1, "
        "'TABLE', 'IX', 'GRANTED', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures +=
        expect_row_count_state(database, "row count after instrumentation placeholder error");

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
