#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_access_denied = 1044,
    performance_timers_column_count = 4,
    performance_timers_row_count = 5,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    show_table_status_column_count = 18,
    information_schema_columns_projection_count = 6,
    information_schema_tables_projection_count = 7,
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

static int test_performance_schema_performance_timers(void);
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
    return test_performance_schema_performance_timers() == 0 ? 0 : 1;
}

static int test_performance_schema_performance_timers(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const timer_columns[] = {
        "TIMER_NAME",
        "TIMER_FREQUENCY",
        "TIMER_RESOLUTION",
        "TIMER_OVERHEAD",
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
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "DATA_TYPE",
        "COLUMN_TYPE",
        "COLUMN_KEY",
    };
    static const char *const information_schema_tables_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "AUTO_INCREMENT",
        "TABLE_COLLATION",
    };
    static const char *const five_count[] = {"5"};
    static const char *const zero_count[] = {"0"};
    static const char *const timer_values[] = {
        "CYCLE", "1000000000",  "1",          "1",          "NANOSECOND", "1000000000",  "1",
        "1",     "MICROSECOND", "1000000",    "1",          "1",          "MILLISECOND", "1000",
        "1",     "1",           "THREAD_CPU", "1000000000", "1",          "1",
    };
    static const char *const full_columns[] = {
        "TIMER_NAME",
        "enum('CYCLE','NANOSECOND','MICROSECOND','MILLISECOND','THREAD_CPU')",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "TIMER_FREQUENCY",
        "bigint",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "TIMER_RESOLUTION",
        "bigint",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "TIMER_OVERHEAD",
        "bigint",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_columns[] = {
        "TIMER_NAME",
        "1",
        "NO",
        "enum",
        "enum('CYCLE','NANOSECOND','MICROSECOND','MILLISECOND','THREAD_CPU')",
        "",
        "TIMER_FREQUENCY",
        "2",
        "YES",
        "bigint",
        "bigint",
        "",
        "TIMER_RESOLUTION",
        "3",
        "YES",
        "bigint",
        "bigint",
        "",
        "TIMER_OVERHEAD",
        "4",
        "YES",
        "bigint",
        "bigint",
        "",
    };
    static const char *const information_schema_tables[] = {
        "performance_timers",
        "BASE TABLE",
        "PERFORMANCE_SCHEMA",
        "Fixed",
        "5",
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

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM performance_schema.performance_timers",
            .column_names = count_column,
            .values = five_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "performance_timers row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMER_NAME, TIMER_FREQUENCY, TIMER_RESOLUTION, TIMER_OVERHEAD "
                   "FROM performance_schema.performance_timers",
            .column_names = timer_columns,
            .values = timer_values,
            .column_count = performance_timers_column_count,
            .row_count = performance_timers_row_count,
            .context = "performance_timers rows",
        }
    );
    failures += execute_ok(database, "USE performance_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMER_NAME FROM performance_timers WHERE TIMER_NAME = 'THREAD_CPU'",
            .column_names = (const char *const[]){"TIMER_NAME"},
            .values = (const char *const[]){"THREAD_CPU"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "selected performance_schema table resolution",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM performance_schema.performance_timers",
            .column_names = show_full_columns_columns,
            .values = full_columns,
            .column_count = show_full_columns_column_count,
            .row_count = performance_timers_column_count,
            .context = "show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM performance_schema.performance_timers",
            .column_names = show_index_columns,
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM performance_schema "
                   "WHERE Name = 'performance_timers' "
                   "AND Engine = 'PERFORMANCE_SCHEMA' "
                   "AND Row_format = 'Fixed' "
                   "AND Auto_increment IS NULL "
                   "AND Collation = 'utf8mb4_0900_ai_ci'",
            .column_names = show_table_status_columns,
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 1U,
            .context = "show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, DATA_TYPE, "
                   "COLUMN_TYPE, COLUMN_KEY "
                   "FROM information_schema.columns "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .values = information_schema_columns,
            .column_count = information_schema_columns_projection_count,
            .row_count = performance_timers_column_count,
            .context = "information schema columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, ROW_FORMAT, TABLE_ROWS, "
                   "AUTO_INCREMENT, TABLE_COLLATION "
                   "FROM information_schema.tables "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers'",
            .column_names = information_schema_tables_columns,
            .values = information_schema_tables,
            .column_count = information_schema_tables_projection_count,
            .row_count = 1U,
            .context = "information schema tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.statistics "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers'",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema statistics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.table_constraints "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers'",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema table constraints",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.key_column_usage "
                   "WHERE TABLE_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers'",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema key column usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.table_constraints_extensions "
                   "WHERE CONSTRAINT_SCHEMA = 'performance_schema' "
                   "AND TABLE_NAME = 'performance_timers'",
            .column_names = count_column,
            .values = zero_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema table constraints extensions",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO performance_schema.performance_timers VALUES ('X', 1, 1, 1)",
        access_denied
    );
    failures += expect_row_count_state(database, "row count after select");

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
