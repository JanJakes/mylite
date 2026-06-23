#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    connection_id_text_capacity = 32,
    dynamic_sql_capacity = 1024,
    processlist_row_column_count = 18,
    processlist_thd_id_column = 0,
    processlist_conn_id_column = 1,
    processlist_user_column = 2,
    processlist_db_column = 3,
    processlist_command_column = 4,
    processlist_state_column = 5,
    processlist_time_column = 6,
    processlist_current_statement_column = 7,
    processlist_execution_engine_column = 8,
    processlist_counter_first_column = 9,
    processlist_counter_last_column = 13,
    processlist_full_scan_column = 14,
    processlist_trx_state_column = 15,
    processlist_trx_autocommit_column = 16,
    processlist_pid_column = 17,
    show_columns_column_count = 6,
    information_schema_views_column_count = 6,
    information_schema_view_routine_usage_column_count = 4,
    show_create_view_column_count = 4,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_query_contains {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    size_t row_count;
    size_t row_index;
    size_t column_index;
    const char *needle;
    const char *context;
};

struct current_processlist_expectation {
    const char *sql;
    const char *context;
};

static int test_sys_processlist_views(void);
static int read_connection_id(mylite_db *database, char *buffer, size_t buffer_size);
static int build_processlist_sql(
    char *buffer,
    size_t buffer_size,
    const char *table_name,
    const char *connection_id
);
static int expect_current_processlist_row(
    mylite_db *database,
    struct current_processlist_expectation expected
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_decimal_text(const char *actual, const char *context);

static const char *const processlist_row_columns[processlist_row_column_count] = {
    "thd_id",
    "conn_id",
    "user",
    "db",
    "command",
    "state",
    "time",
    "current_statement",
    "execution_engine",
    "rows_examined",
    "rows_sent",
    "rows_affected",
    "tmp_tables",
    "tmp_disk_tables",
    "full_scan",
    "trx_state",
    "trx_autocommit",
    "pid",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const information_schema_views_columns[information_schema_views_column_count] = {
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
};

static const char *const information_schema_view_routine_usage_columns
    [information_schema_view_routine_usage_column_count] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "SPECIFIC_SCHEMA",
        "SPECIFIC_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_processlist_views() == 0 ? 0 : 1;
}

static int test_sys_processlist_views(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const show_columns_formatted_values[] = {
        "statement_latency",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "current_memory",
        "varchar(11)",
        "YES",
        "",
        NULL,
        "",
        "last_wait_latency",
        "varchar(13)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_columns_raw_values[] = {
        "statement_latency",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
        "current_memory",
        "decimal(41,0)",
        "YES",
        "",
        NULL,
        "",
        "last_wait_latency",
        "varchar(20)",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const views_values[] = {
        "sys",
        "processlist",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "x$processlist",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const routine_values[] = {
        "sys",
        "processlist",
        "sys",
        "format_statement",
    };
    const char *const dependency_count_values[] = {"15"};
    const char *const formatted_count_values[] = {"1"};
    const char *const raw_count_values[] = {"1"};
    char connection_id[connection_id_text_capacity];
    char formatted_sql[dynamic_sql_capacity];
    char raw_sql[dynamic_sql_capacity];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += make_test_path(path, sizeof(path));
    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open sys processlist db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += read_connection_id(database, connection_id, sizeof(connection_id));
    failures += build_processlist_sql(
        formatted_sql,
        sizeof(formatted_sql),
        "sys.processlist",
        connection_id
    );
    failures +=
        build_processlist_sql(raw_sql, sizeof(raw_sql), "sys.`x$processlist`", connection_id);
    if (failures != 0) {
        mylite_close(database);
        remove_related_files(path);
        return failures;
    }
    failures += expect_current_processlist_row(
        database,
        (struct current_processlist_expectation){
            .sql = formatted_sql,
            .context = "sys.processlist current row",
        }
    );
    failures += expect_current_processlist_row(
        database,
        (struct current_processlist_expectation){
            .sql = raw_sql,
            .context = "sys.x processlist current row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.processlist "
                   "WHERE Field IN ('statement_latency', 'current_memory', 'last_wait_latency')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_formatted_values,
            .row_count = 3,
            .context = "formatted processlist SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.`x$processlist` "
                   "WHERE Field IN ('statement_latency', 'current_memory', 'last_wait_latency')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_raw_values,
            .row_count = 3,
            .context = "raw processlist SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('processlist', 'x$processlist') ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = 2,
            .context = "processlist information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE "
                   "WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ('processlist', 'x$processlist')",
            .column_names = count_column,
            .column_count = 1,
            .values = dependency_count_values,
            .row_count = 1,
            .context = "processlist view table usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, SPECIFIC_SCHEMA, SPECIFIC_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('processlist', 'x$processlist') ORDER BY TABLE_NAME",
            .column_names = information_schema_view_routine_usage_columns,
            .column_count = information_schema_view_routine_usage_column_count,
            .values = routine_values,
            .row_count = 1,
            .context = "processlist view routine usage",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.processlist",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`sys`.`format_statement`(`pps`.`PROCESSLIST_INFO`) AS "
                      "`current_statement`",
            .context = "formatted processlist show create",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$processlist`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`pps`.`PROCESSLIST_INFO` AS `current_statement`",
            .context = "raw processlist show create",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM processlist",
            .column_names = count_column,
            .column_count = 1,
            .values = formatted_count_values,
            .row_count = 1,
            .context = "selected schema formatted processlist",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$processlist`",
            .column_names = count_column,
            .column_count = 1,
            .values = raw_count_values,
            .row_count = 1,
            .context = "selected schema raw processlist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int read_connection_id(mylite_db *database, char *buffer, size_t buffer_size) {
    static const char sql[] = "SELECT CONNECTION_ID()";
    mylite_result *result = NULL;
    const char *connection_id = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, "read connection id");

    if (rc != MYLITE_OK) {
        fprintf(stderr, "read connection id: %s\n", mylite_errmsg(database));
        return failures + 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1, "connection id row count");
    connection_id = mylite_result_value_text(result, 0, 0);
    failures += expect_decimal_text(connection_id, "connection id");
    if (failures == 0) {
        size_t length = strlen(connection_id);

        if (length >= buffer_size) {
            fprintf(stderr, "connection id buffer too small\n");
            failures += 1;
        } else {
            memcpy(buffer, connection_id, length + 1U);
        }
    }
    mylite_result_free(result);
    return failures;
}

static int build_processlist_sql(
    char *buffer,
    size_t buffer_size,
    const char *table_name,
    const char *connection_id
) {
    int written = snprintf(
        buffer,
        buffer_size,
        "SELECT thd_id, conn_id, user, db, command, state, time, current_statement, "
        "execution_engine, rows_examined, rows_sent, rows_affected, tmp_tables, "
        "tmp_disk_tables, full_scan, trx_state, trx_autocommit, pid FROM %s "
        "WHERE conn_id = %s",
        table_name,
        connection_id
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        fprintf(stderr, "failed to build processlist SQL\n");
        return 1;
    }
    return 0;
}

static int expect_current_processlist_row(
    mylite_db *database,
    struct current_processlist_expectation expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures += expect_size(
        mylite_result_column_count(result),
        processlist_row_column_count,
        expected.context
    );
    failures += expect_size(mylite_result_row_count(result), 1, expected.context);
    for (size_t column_index = 0U; column_index < processlist_row_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            processlist_row_columns[column_index],
            expected.context
        );
    }
    if (mylite_result_row_count(result) == 1) {
        const char *thd_id = mylite_result_value_text(result, 0, processlist_thd_id_column);
        const char *conn_id = mylite_result_value_text(result, 0, processlist_conn_id_column);

        failures += expect_decimal_text(thd_id, "thd_id");
        failures += expect_decimal_text(conn_id, "conn_id");
        failures += expect_text_or_null(thd_id, conn_id, "thread id placeholder");
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_user_column),
            "root@%",
            "user"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_db_column),
            "app",
            "db"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_command_column),
            "Query",
            "command"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_state_column),
            "executing",
            "state"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_time_column),
            "0",
            "time"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_current_statement_column),
            expected.sql,
            "current statement"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_execution_engine_column),
            "PRIMARY",
            "execution engine"
        );
        for (size_t column_index = processlist_counter_first_column;
             column_index <= processlist_counter_last_column;
             ++column_index) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, 0, column_index),
                "0",
                "counter"
            );
        }
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_full_scan_column),
            "NO",
            "full scan"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_trx_state_column),
            "ACTIVE",
            "trx state"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_trx_autocommit_column),
            "YES",
            "trx autocommit"
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0, processlist_pid_column),
            NULL,
            "pid"
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    if (rc == MYLITE_OK) {
        mylite_result_free(result);
    }
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            const size_t value_index = (row_index * expected.column_count) + column_index;
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_text_contains(
        mylite_result_value_text(result, expected.row_index, expected.column_index),
        expected.needle,
        expected.context
    );
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_sys_processlist_views_%d.mylite",
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build sys processlist test path\n");
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected text to contain [%s], got [%s]\n",
            context,
            needle,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_decimal_text(const char *actual, const char *context) {
    if (actual == NULL || actual[0] == '\0') {
        fprintf(stderr, "%s: expected decimal text, got empty value\n", context);
        return 1;
    }
    for (size_t index = 0U; actual[index] != '\0'; ++index) {
        if (actual[index] < '0' || actual[index] > '9') {
            fprintf(stderr, "%s: expected decimal text, got [%s]\n", context, actual);
            return 1;
        }
    }
    return 0;
}
