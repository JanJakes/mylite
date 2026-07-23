#include "mylite_test_support.h"

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
    session_row_column_count = 18,
    session_thd_id_column = 0,
    session_conn_id_column = 1,
    session_user_column = 2,
    session_db_column = 3,
    session_command_column = 4,
    session_state_column = 5,
    session_time_column = 6,
    session_current_statement_column = 7,
    session_execution_engine_column = 8,
    session_counter_first_column = 9,
    session_counter_last_column = 13,
    session_full_scan_column = 14,
    session_trx_state_column = 15,
    session_trx_autocommit_column = 16,
    session_pid_column = 17,
    ssl_status_column_count = 4,
    ssl_status_thread_id_column = 0,
    ssl_status_version_column = 1,
    ssl_status_cipher_column = 2,
    ssl_status_reused_column = 3,
    show_columns_column_count = 6,
    information_schema_views_column_count = 6,
    information_schema_view_table_usage_column_count = 4,
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

struct current_session_expectation {
    const char *sql;
    const char *context;
};

static int test_sys_session_views(void);
static int read_connection_id(mylite_db *database, char *buffer, size_t buffer_size);
static int build_session_sql(
    char *buffer,
    size_t buffer_size,
    const char *table_name,
    const char *connection_id
);
static int build_ssl_status_sql(char *buffer, size_t buffer_size, const char *connection_id);
static int expect_current_session_row(
    mylite_db *database,
    struct current_session_expectation expected
);
static int expect_ssl_status_row(mylite_db *database, const char *sql);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_decimal_text(const char *actual, const char *context);

static const char *const session_row_columns[session_row_column_count] = {
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

static const char *const ssl_status_columns[ssl_status_column_count] = {
    "thread_id",
    "ssl_version",
    "ssl_cipher",
    "ssl_sessions_reused",
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

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_SCHEMA",
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

int main(void) {
    return test_sys_session_views() == 0 ? 0 : 1;
}

static int test_sys_session_views(void) {
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
    static const char *const show_columns_ssl_values[] = {
        "thread_id",           "bigint unsigned", "NO",  "", NULL, "",
        "ssl_version",         "varchar(1024)",   "YES", "", NULL, "",
        "ssl_cipher",          "varchar(1024)",   "YES", "", NULL, "",
        "ssl_sessions_reused", "varchar(1024)",   "YES", "", NULL, "",
    };
    static const char *const views_values[] = {
        "sys",
        "session",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "session_ssl_status",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
        "sys",
        "x$session",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const dependency_values[] = {
        "sys",
        "session",
        "sys",
        "processlist",
        "sys",
        "session",
        "sys",
        "sys_config",
        "sys",
        "session_ssl_status",
        "performance_schema",
        "status_by_thread",
        "sys",
        "x$session",
        "sys",
        "x$processlist",
    };
    const char *const formatted_count_values[] = {"1"};
    const char *const raw_count_values[] = {"1"};
    const char *const ssl_count_values[] = {"1"};
    char connection_id[connection_id_text_capacity];
    char formatted_sql[dynamic_sql_capacity];
    char raw_sql[dynamic_sql_capacity];
    char ssl_sql[dynamic_sql_capacity];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_make_default_path(path, sizeof(path));
    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open sys session db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += read_connection_id(database, connection_id, sizeof(connection_id));
    failures +=
        build_session_sql(formatted_sql, sizeof(formatted_sql), "sys.session", connection_id);
    failures += build_session_sql(raw_sql, sizeof(raw_sql), "sys.`x$session`", connection_id);
    failures += build_ssl_status_sql(ssl_sql, sizeof(ssl_sql), connection_id);
    if (failures != 0) {
        mylite_close(database);
        remove_related_files(path);
        return failures;
    }
    failures += expect_current_session_row(
        database,
        (struct current_session_expectation){
            .sql = formatted_sql,
            .context = "sys.session current row",
        }
    );
    failures += expect_current_session_row(
        database,
        (struct current_session_expectation){
            .sql = raw_sql,
            .context = "sys.x session current row",
        }
    );
    failures += expect_ssl_status_row(database, ssl_sql);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.session "
                   "WHERE Field IN ('statement_latency', 'current_memory', 'last_wait_latency')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_formatted_values,
            .row_count = 3,
            .context = "formatted session SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.`x$session` "
                   "WHERE Field IN ('statement_latency', 'current_memory', 'last_wait_latency')",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_raw_values,
            .row_count = 3,
            .context = "raw session SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.session_ssl_status",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_ssl_values,
            .row_count = 4,
            .context = "session SSL SHOW COLUMNS",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME IN ('session', 'x$session', 'session_ssl_status') "
                   "ORDER BY TABLE_NAME",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = views_values,
            .row_count = 3,
            .context = "session information_schema views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME IN ('session', 'x$session', 'session_ssl_status') "
                   "ORDER BY VIEW_NAME, TABLE_SCHEMA, TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = dependency_values,
            .row_count = 4,
            .context = "session view table usage",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.session",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "from `sys`.`processlist` where ((`sys`.`processlist`.`conn_id` "
                      "is not null) and (`sys`.`processlist`.`command` <> 'Daemon'))",
            .context = "session show create",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.`x$session`",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "from `sys`.`x$processlist` where ((`sys`.`x$processlist`.`conn_id` "
                      "is not null) and (`sys`.`x$processlist`.`command` <> 'Daemon'))",
            .context = "raw session show create",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.session_ssl_status",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1,
            .row_index = 0,
            .column_index = 1,
            .needle = "`performance_schema`.`status_by_thread` `sslver`",
            .context = "session SSL show create",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM session",
            .column_names = count_column,
            .column_count = 1,
            .values = formatted_count_values,
            .row_count = 1,
            .context = "selected schema formatted session",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM `x$session`",
            .column_names = count_column,
            .column_count = 1,
            .values = raw_count_values,
            .row_count = 1,
            .context = "selected schema raw session",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM session_ssl_status",
            .column_names = count_column,
            .column_count = 1,
            .values = ssl_count_values,
            .row_count = 1,
            .context = "selected schema session SSL",
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
    int failures = mylite_test_expect_int(rc, MYLITE_OK, "read connection id");

    if (rc != MYLITE_OK) {
        fprintf(stderr, "read connection id: %s\n", mylite_errmsg(database));
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 1, "connection id row count");
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

static int build_session_sql(
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
        fprintf(stderr, "failed to build session SQL\n");
        return 1;
    }
    return 0;
}

static int build_ssl_status_sql(char *buffer, size_t buffer_size, const char *connection_id) {
    int written = snprintf(
        buffer,
        buffer_size,
        "SELECT thread_id, ssl_version, ssl_cipher, ssl_sessions_reused "
        "FROM sys.session_ssl_status WHERE thread_id = %s",
        connection_id
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        fprintf(stderr, "failed to build session SSL SQL\n");
        return 1;
    }
    return 0;
}

static int expect_current_session_row(
    mylite_db *database,
    struct current_session_expectation expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        session_row_column_count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1, expected.context);
    for (size_t column_index = 0U; column_index < session_row_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            session_row_columns[column_index],
            expected.context
        );
    }
    if (mylite_result_row_count(result) == 1) {
        const char *thd_id = mylite_result_value_text(result, 0, session_thd_id_column);
        const char *conn_id = mylite_result_value_text(result, 0, session_conn_id_column);

        failures += expect_decimal_text(thd_id, "thd_id");
        failures += expect_decimal_text(conn_id, "conn_id");
        failures += mylite_test_expect_text_or_null(thd_id, conn_id, "thread id placeholder");
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_user_column),
            "root@%",
            "user"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_db_column),
            "app",
            "db"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_command_column),
            "Query",
            "command"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_state_column),
            "executing",
            "state"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_time_column),
            "0",
            "time"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_current_statement_column),
            expected.sql,
            "current statement"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_execution_engine_column),
            "PRIMARY",
            "execution engine"
        );
        for (size_t column_index = session_counter_first_column;
             column_index <= session_counter_last_column;
             ++column_index) {
            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, 0, column_index),
                "0",
                "counter"
            );
        }
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_full_scan_column),
            "NO",
            "full scan"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_trx_state_column),
            "ACTIVE",
            "trx state"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_trx_autocommit_column),
            "YES",
            "trx autocommit"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, session_pid_column),
            NULL,
            "pid"
        );
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_ssl_status_row(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += mylite_test_expect_int(rc, MYLITE_OK, "session SSL status row");
    if (rc != MYLITE_OK) {
        fprintf(stderr, "session SSL status row: %s\n", mylite_errmsg(database));
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        ssl_status_column_count,
        "session SSL column count"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 1, "session SSL row count");
    for (size_t column_index = 0U; column_index < ssl_status_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            ssl_status_columns[column_index],
            "session SSL column name"
        );
    }
    if (mylite_result_row_count(result) == 1) {
        const char *thread_id = mylite_result_value_text(result, 0, ssl_status_thread_id_column);

        failures += expect_decimal_text(thread_id, "session SSL thread id");
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, ssl_status_version_column),
            "",
            "session SSL version"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, ssl_status_cipher_column),
            "",
            "session SSL cipher"
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0, ssl_status_reused_column),
            "0",
            "session SSL reused count"
        );
    }
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "session SSL affected rows"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

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

    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            const size_t value_index = (row_index * expected.column_count) + column_index;
            failures += mylite_test_expect_text_or_null(
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

    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.context, mylite_errmsg(database));
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += mylite_test_expect_contains(
        mylite_result_value_text(result, expected.row_index, expected.column_index),
        expected.needle,
        expected.context
    );
    mylite_result_free(result);
    return failures;
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

static int expect_decimal_text(const char *actual, const char *context) {
    if (actual == NULL || actual[0] == '\0') {
        fprintf(
            stderr,
            "%s: expected decimal text, got %s\n",
            context,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    for (size_t index = 0U; actual[index] != '\0'; ++index) {
        if (actual[index] < '0' || actual[index] > '9') {
            fprintf(stderr, "%s: expected decimal text, got %s\n", context, actual);
            return 1;
        }
    }
    return 0;
}
