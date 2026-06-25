#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct mysqlx_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    bool allows_session;
    bool read_only;
    bool fixed_global;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_value {
    const char *sql;
    const char *value;
    const char *context;
};

struct expected_show_variable {
    const char *sql;
    const char *name;
    const char *value;
    const char *context;
};

static int test_mysqlx_values_show_and_scope(void);
static int test_mysqlx_set_and_diagnostics(void);
static int expect_single_value(mylite_db *database, struct expected_single_value expected);
static int expect_show_value(mylite_db *database, struct expected_show_variable expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const struct mysqlx_variable mysqlx_variables[] = {
    {"mysqlx_bind_address", "*", "*", false, true, false},
    {"mysqlx_compression_algorithms",
     "DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM",
     "DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM",
     false,
     false,
     true},
    {"mysqlx_connect_timeout", "30", "30", false, false, true},
    {"mysqlx_deflate_default_compression_level", "3", "3", false, false, true},
    {"mysqlx_deflate_max_client_compression_level", "5", "5", false, false, true},
    {"mysqlx_document_id_unique_prefix", "0", "0", false, false, true},
    {"mysqlx_enable_hello_notice", "1", "ON", false, false, true},
    {"mysqlx_idle_worker_thread_timeout", "60", "60", false, false, true},
    {"mysqlx_interactive_timeout", "28800", "28800", false, false, true},
    {"mysqlx_lz4_default_compression_level", "2", "2", false, false, true},
    {"mysqlx_lz4_max_client_compression_level", "8", "8", false, false, true},
    {"mysqlx_max_allowed_packet", "67108864", "67108864", false, false, true},
    {"mysqlx_max_connections", "100", "100", false, false, true},
    {"mysqlx_min_worker_threads", "2", "2", false, false, true},
    {"mysqlx_port", "33060", "33060", false, true, false},
    {"mysqlx_port_open_timeout", "0", "0", false, true, false},
    {"mysqlx_read_timeout", "30", "30", true, false, false},
    {"mysqlx_socket",
     "/var/run/mysqld/mysqlx.sock",
     "/var/run/mysqld/mysqlx.sock",
     false,
     true,
     false},
    {"mysqlx_ssl_ca", NULL, "", false, true, false},
    {"mysqlx_ssl_capath", NULL, "", false, true, false},
    {"mysqlx_ssl_cert", NULL, "", false, true, false},
    {"mysqlx_ssl_cipher", NULL, "", false, true, false},
    {"mysqlx_ssl_crl", NULL, "", false, true, false},
    {"mysqlx_ssl_crlpath", NULL, "", false, true, false},
    {"mysqlx_ssl_key", NULL, "", false, true, false},
    {"mysqlx_wait_timeout", "28800", "28800", true, false, false},
    {"mysqlx_write_timeout", "60", "60", true, false, false},
    {"mysqlx_zstd_default_compression_level", "3", "3", false, false, true},
    {"mysqlx_zstd_max_client_compression_level", "11", "11", false, false, true},
};

int main(void) {
    int failures = 0;

    failures += test_mysqlx_values_show_and_scope();
    failures += test_mysqlx_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_mysqlx_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open mysqlx db");
    for (size_t index = 0U; index < sizeof(mysqlx_variables) / sizeof(mysqlx_variables[0]);
         ++index) {
        const struct mysqlx_variable *variable = &mysqlx_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = variable->scalar_value,
                .context = "mysqlx scalar",
            }
        );
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = variable->scalar_value,
                .context = "mysqlx global scalar",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = variable->show_value,
                .context = "mysqlx show",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = variable->show_value,
                .context = "mysqlx show global",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = variable->show_value,
                .context = "mysqlx show session",
            }
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        if (variable->allows_session) {
            failures += expect_single_value(
                database,
                (struct expected_single_value){
                    .sql = sql,
                    .value = variable->scalar_value,
                    .context = "mysqlx session scalar",
                }
            );
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable->name);
        if (variable->allows_session) {
            failures += expect_single_value(
                database,
                (struct expected_single_value){
                    .sql = sql,
                    .value = variable->scalar_value,
                    .context = "mysqlx local scalar",
                }
            );
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_mysqlx_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET supports only fixed no-op system variable assignments",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "X Plugin system variables from user variables are not supported",
    };

    static const struct mysqlx_session_assignment {
        const char *name;
        const char *default_value;
    } session_assignments[] = {
        {"mysqlx_read_timeout", "30"},
        {"mysqlx_wait_timeout", "28800"},
        {"mysqlx_write_timeout", "60"},
    };

    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open mysqlx SET db");
    for (size_t index = 0U; index < sizeof(mysqlx_variables) / sizeof(mysqlx_variables[0]);
         ++index) {
        const struct mysqlx_variable *variable = &mysqlx_variables[index];

        if (variable->read_only) {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only);
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only);
        }
        if (variable->fixed_global) {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
        }
    }

    for (size_t index = 0U; index < sizeof(session_assignments) / sizeof(session_assignments[0]);
         ++index) {
        const struct mysqlx_session_assignment *assignment = &session_assignments[index];

        snprintf(sql, sizeof(sql), "SET %s = 31", assignment->name);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@%s", assignment->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = "31",
                .context = "mysqlx session set",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", assignment->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = assignment->name,
                .value = "31",
                .context = "mysqlx session show set",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", assignment->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = assignment->name,
                .value = assignment->default_value,
                .context = "mysqlx global unchanged",
            }
        );
        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", assignment->name);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@%s", assignment->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = assignment->default_value,
                .context = "mysqlx session reset",
            }
        );
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", assignment->name);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 31", assignment->name);
        failures += execute_error(database, sql, unsupported_global_set);
    }

    failures +=
        execute_error(database, "SET GLOBAL mysqlx_connect_timeout = 31", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL mysqlx_enable_hello_notice = OFF",
        unsupported_global_set
    );
    failures += execute_statement_ok(database, "SET @mysqlx_connect_timeout = 30");
    failures += execute_error(
        database,
        "SET GLOBAL mysqlx_connect_timeout = @mysqlx_connect_timeout",
        unsupported_user_variable_set
    );

    mylite_close(database);
    return failures;
}

static int expect_single_value(mylite_db *database, struct expected_single_value expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            expected.context,
            expected.sql,
            rc,
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), expected.value, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_variable expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            expected.context,
            expected.sql,
            rc,
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 2U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), expected.name, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 1U), expected.value, expected.context);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
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

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
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
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}
