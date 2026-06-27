#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    count_projection_count = 1,
    information_schema_columns_projection_count = 6,
    information_schema_columns_row_count = 19,
    information_schema_constraints_projection_count = 4,
    information_schema_constraints_row_count = 5,
    information_schema_statistics_projection_count = 6,
    information_schema_statistics_row_count = 15,
    information_schema_tables_projection_count = 5,
    information_schema_tables_row_count = 5,
    show_columns_column_count = 6,
    show_index_column_count = 15,
    show_index_socket_row_count = 5,
    show_table_status_column_count = 18,
    mysql_error_access_denied = 1044,
};

static const char socket_state_column_type[] = "enum('IDLE','ACTIVE')";

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

static int test_performance_schema_instance_tables(void);
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
    return test_performance_schema_instance_tables() == 0 ? 0 : 1;
}

static int test_performance_schema_instance_tables(void) {
    static const char *const count_columns[] = {"COUNT(*)"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const show_columns_columns[] = {
        "Field",
        "Type",
        "Null",
        "Key",
        "Default",
        "Extra",
    };
    static const char *const show_columns_cond_values[] = {
        "NAME",
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
    };
    static const char *const describe_file_values[] = {
        "FILE_NAME",
        "varchar(512)",
        "NO",
        "PRI",
        NULL,
        "",
        "EVENT_NAME",
        "varchar(128)",
        "NO",
        "MUL",
        NULL,
        "",
        "OPEN_COUNT",
        "int unsigned",
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
        "cond_instances",
        "NAME",
        "varchar(128)",
        "MUL",
        "NO",
        "utf8mb4_0900_ai_ci",
        "cond_instances",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        "file_instances",
        "FILE_NAME",
        "varchar(512)",
        "PRI",
        "NO",
        "utf8mb4_0900_ai_ci",
        "file_instances",
        "EVENT_NAME",
        "varchar(128)",
        "MUL",
        "NO",
        "utf8mb4_0900_ai_ci",
        "file_instances",
        "OPEN_COUNT",
        "int unsigned",
        "",
        "NO",
        NULL,
        "mutex_instances",
        "NAME",
        "varchar(128)",
        "MUL",
        "NO",
        "utf8mb4_0900_ai_ci",
        "mutex_instances",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        "mutex_instances",
        "LOCKED_BY_THREAD_ID",
        "bigint unsigned",
        "MUL",
        "YES",
        NULL,
        "rwlock_instances",
        "NAME",
        "varchar(128)",
        "MUL",
        "NO",
        "utf8mb4_0900_ai_ci",
        "rwlock_instances",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        "rwlock_instances",
        "WRITE_LOCKED_BY_THREAD_ID",
        "bigint unsigned",
        "MUL",
        "YES",
        NULL,
        "rwlock_instances",
        "READ_LOCKED_BY_COUNT",
        "int unsigned",
        "",
        "NO",
        NULL,
        "socket_instances",
        "EVENT_NAME",
        "varchar(128)",
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
        "socket_instances",
        "OBJECT_INSTANCE_BEGIN",
        "bigint unsigned",
        "PRI",
        "NO",
        NULL,
        "socket_instances",
        "THREAD_ID",
        "bigint unsigned",
        "MUL",
        "YES",
        NULL,
        "socket_instances",
        "SOCKET_ID",
        "int",
        "MUL",
        "NO",
        NULL,
        "socket_instances",
        "IP",
        "varchar(64)",
        "MUL",
        "NO",
        "utf8mb4_0900_ai_ci",
        "socket_instances",
        "PORT",
        "int",
        "",
        "NO",
        NULL,
        "socket_instances",
        "STATE",
        socket_state_column_type,
        "",
        "NO",
        "utf8mb4_0900_ai_ci",
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
        "cond_instances",
        "NAME",
        "1",
        "1",
        "NAME",
        "HASH",
        "cond_instances",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
        "file_instances",
        "EVENT_NAME",
        "1",
        "1",
        "EVENT_NAME",
        "HASH",
        "file_instances",
        "PRIMARY",
        "0",
        "1",
        "FILE_NAME",
        "HASH",
        "mutex_instances",
        "LOCKED_BY_THREAD_ID",
        "1",
        "1",
        "LOCKED_BY_THREAD_ID",
        "HASH",
        "mutex_instances",
        "NAME",
        "1",
        "1",
        "NAME",
        "HASH",
        "mutex_instances",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
        "rwlock_instances",
        "NAME",
        "1",
        "1",
        "NAME",
        "HASH",
        "rwlock_instances",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
        "rwlock_instances",
        "WRITE_LOCKED_BY_THREAD_ID",
        "1",
        "1",
        "WRITE_LOCKED_BY_THREAD_ID",
        "HASH",
        "socket_instances",
        "IP",
        "1",
        "1",
        "IP",
        "HASH",
        "socket_instances",
        "IP",
        "1",
        "2",
        "PORT",
        "HASH",
        "socket_instances",
        "PRIMARY",
        "0",
        "1",
        "OBJECT_INSTANCE_BEGIN",
        "HASH",
        "socket_instances",
        "SOCKET_ID",
        "1",
        "1",
        "SOCKET_ID",
        "HASH",
        "socket_instances",
        "THREAD_ID",
        "1",
        "1",
        "THREAD_ID",
        "HASH",
    };
    static const char *const information_schema_constraints_columns[] = {
        "TABLE_NAME",
        "CONSTRAINT_NAME",
        "CONSTRAINT_TYPE",
        "ENFORCED",
    };
    static const char *const information_schema_constraints_rows[] = {
        "cond_instances",   "PRIMARY", "PRIMARY KEY",     "YES",     "file_instances",   "PRIMARY",
        "PRIMARY KEY",      "YES",     "mutex_instances", "PRIMARY", "PRIMARY KEY",      "YES",
        "rwlock_instances", "PRIMARY", "PRIMARY KEY",     "YES",     "socket_instances", "PRIMARY",
        "PRIMARY KEY",      "YES",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "ENGINE",
        "ROW_FORMAT",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const information_schema_tables_rows[] = {
        "cond_instances",   "PERFORMANCE_SCHEMA", "Dynamic", NULL, "utf8mb4_0900_ai_ci",
        "file_instances",   "PERFORMANCE_SCHEMA", "Dynamic", NULL, "utf8mb4_0900_ai_ci",
        "mutex_instances",  "PERFORMANCE_SCHEMA", "Dynamic", NULL, "utf8mb4_0900_ai_ci",
        "rwlock_instances", "PERFORMANCE_SCHEMA", "Dynamic", NULL, "utf8mb4_0900_ai_ci",
        "socket_instances", "PERFORMANCE_SCHEMA", "Dynamic", NULL, "utf8mb4_0900_ai_ci",
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
        "SELECT COUNT(*) FROM performance_schema.cond_instances",
        "cond_instances row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.mutex_instances",
        "mutex_instances row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.rwlock_instances",
        "rwlock_instances row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.file_instances",
        "file_instances row count"
    );
    failures += expect_empty_count(
        database,
        "SELECT COUNT(*) FROM performance_schema.socket_instances",
        "socket_instances row count"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM performance_schema.cond_instances",
            .column_names = show_columns_columns,
            .values = show_columns_cond_values,
            .column_count = show_columns_column_count,
            .row_count = 2U,
            .context = "cond_instances show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESC performance_schema.file_instances",
            .column_names = show_columns_columns,
            .values = describe_file_values,
            .column_count = show_columns_column_count,
            .row_count = 3U,
            .context = "file_instances describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, COLUMN_KEY, "
                   "IS_NULLABLE, COLLATION_NAME "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('cond_instances', 'mutex_instances', "
                   "'rwlock_instances', 'file_instances', 'socket_instances') "
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
            .sql = "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, "
                   "COLUMN_NAME, INDEX_TYPE "
                   "FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME IN ('cond_instances', 'mutex_instances', "
                   "'rwlock_instances', 'file_instances', 'socket_instances') "
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
                   "AND TABLE_NAME IN ('cond_instances', 'mutex_instances', "
                   "'rwlock_instances', 'file_instances', 'socket_instances') "
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
                   "AND TABLE_NAME IN ('cond_instances', 'mutex_instances', "
                   "'rwlock_instances', 'file_instances', 'socket_instances') "
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
            .sql = "SHOW INDEX FROM performance_schema.socket_instances",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = show_index_socket_row_count,
            .context = "socket_instances show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'cond_instances' AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Dynamic' AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status cond_instances",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM socket_instances",
            .column_names = count_columns,
            .values = zero_count_rows,
            .column_count = count_projection_count,
            .row_count = 1U,
            .context = "selected performance_schema socket_instances row count",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.cond_instances VALUES ('wait/synch/cond/sql/test', 1)",
        (struct expected_sql_error){
            .code = mysql_error_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied",
        }
    );
    failures += expect_row_count_state(database, "row count after instance table error");

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
