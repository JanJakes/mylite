#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 18,
    no_op_scalar_value_count = 5,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_invalid_system_variable_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_value {
    const char *sql;
    const char *name;
    const char *value;
    const char *context;
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics_and_session_state(void);
static int test_user_variable_assignments_and_rollback(void);
static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
);
static int expect_show_value(mylite_db *database, struct expected_show_value expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_diagnostics_and_session_state();
    failures += test_user_variable_assignments_and_rollback();

    return failures == 0 ? 0 : 1;
}

static int test_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open MyISAM db");
    failures += expect_values(
        database,
        "SELECT @@myisam_data_pointer_size, @@GLOBAL.myisam_data_pointer_size, "
        "@@myisam_max_sort_file_size, @@GLOBAL.myisam_max_sort_file_size, "
        "@@myisam_mmap_size, @@GLOBAL.myisam_mmap_size, "
        "@@myisam_recover_options, @@GLOBAL.myisam_recover_options, "
        "@@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size, "
        "@@SESSION.myisam_sort_buffer_size, @@LOCAL.myisam_sort_buffer_size, "
        "@@myisam_stats_method, @@GLOBAL.myisam_stats_method, "
        "@@SESSION.myisam_stats_method, @@LOCAL.myisam_stats_method, "
        "@@myisam_use_mmap, @@GLOBAL.myisam_use_mmap",
        (const char *[]){"6",
                         "6",
                         "9223372036853727232",
                         "9223372036853727232",
                         "18446744073709551615",
                         "18446744073709551615",
                         "OFF",
                         "OFF",
                         "8388608",
                         "8388608",
                         "8388608",
                         "8388608",
                         "nulls_unequal",
                         "nulls_unequal",
                         "nulls_unequal",
                         "nulls_unequal",
                         "0",
                         "0"},
        default_scalar_value_count,
        "MyISAM defaults"
    );

    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'myisam_data_pointer_size'",
                                     .name = "myisam_data_pointer_size",
                                     .value = "6",
                                     .context = "myisam_data_pointer_size show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql =
                                         "SHOW GLOBAL VARIABLES LIKE 'myisam_max_sort_file_size'",
                                     .name = "myisam_max_sort_file_size",
                                     .value = "9223372036853727232",
                                     .context = "myisam_max_sort_file_size global show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'myisam_mmap_size'",
                                     .name = "myisam_mmap_size",
                                     .value = "18446744073709551615",
                                     .context = "myisam_mmap_size session show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'myisam_recover_options'",
                                     .name = "myisam_recover_options",
                                     .value = "OFF",
                                     .context = "myisam_recover_options show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'myisam_sort_buffer_size'",
                                     .name = "myisam_sort_buffer_size",
                                     .value = "8388608",
                                     .context = "myisam_sort_buffer_size show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'myisam_stats_method'",
                                     .name = "myisam_stats_method",
                                     .value = "nulls_unequal",
                                     .context = "myisam_stats_method show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'myisam_use_mmap'",
                                     .name = "myisam_use_mmap",
                                     .value = "OFF",
                                     .context = "myisam_use_mmap show"}
    );

    failures +=
        execute_error(database, "SELECT @@SESSION.myisam_data_pointer_size", global_only_read);
    failures +=
        execute_error(database, "SELECT @@SESSION.myisam_max_sort_file_size", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.myisam_mmap_size", global_only_read);
    failures +=
        execute_error(database, "SELECT @@SESSION.myisam_recover_options", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.myisam_use_mmap", global_only_read);

    mylite_close(database);
    return failures;
}

static int test_set_diagnostics_and_session_state(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only_set = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "not supported",
    };
    struct expected_sql_error generic_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error incorrect_sort_argument = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    struct expected_sql_error invalid_stats_method = {
        .code = mysql_error_invalid_system_variable_value,
        .sqlstate = "42000",
        .message_part = "can't be set",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open MyISAM SET db");

    failures += execute_statement_ok(database, "SET GLOBAL myisam_data_pointer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_data_pointer_size = 6");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_max_sort_file_size = DEFAULT");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL myisam_max_sort_file_size = 9223372036853727232"
    );
    failures += execute_statement_ok(database, "SET GLOBAL myisam_sort_buffer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_sort_buffer_size = 8388608");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_stats_method = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_stats_method = 'nulls_unequal'");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_use_mmap = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL myisam_use_mmap = OFF");
    failures += expect_values(
        database,
        "SELECT @@GLOBAL.myisam_data_pointer_size, @@GLOBAL.myisam_max_sort_file_size, "
        "@@GLOBAL.myisam_sort_buffer_size, @@GLOBAL.myisam_stats_method, "
        "@@GLOBAL.myisam_use_mmap",
        (const char *[]){"6", "9223372036853727232", "8388608", "nulls_unequal", "0"},
        no_op_scalar_value_count,
        "MyISAM global no-op values"
    );

    failures += execute_error(database, "SET myisam_data_pointer_size = DEFAULT", global_only_set);
    failures += execute_error(database, "SET myisam_max_sort_file_size = DEFAULT", global_only_set);
    failures += execute_error(database, "SET myisam_use_mmap = DEFAULT", global_only_set);
    failures += execute_error(database, "SET GLOBAL myisam_mmap_size = DEFAULT", read_only_set);
    failures +=
        execute_error(database, "SET GLOBAL myisam_recover_options = DEFAULT", read_only_set);
    failures +=
        execute_error(database, "SET GLOBAL myisam_data_pointer_size = 7", unsupported_fixed_noop);
    failures += execute_error(
        database,
        "SET GLOBAL myisam_sort_buffer_size = 4096",
        unsupported_fixed_noop
    );
    failures += execute_error(
        database,
        "SET GLOBAL myisam_stats_method = 'nulls_equal'",
        unsupported_fixed_noop
    );
    failures += execute_error(database, "SET GLOBAL myisam_use_mmap = ON", generic_fixed_noop);

    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = 4096");
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size, "
        "@@SESSION.myisam_sort_buffer_size",
        (const char *[]){"4096", "8388608", "4096"},
        3U,
        "myisam_sort_buffer_size session"
    );
    failures += execute_statement_ok(database, "SET myisam_sort_buffer_size = 5000");
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size, @@GLOBAL.myisam_sort_buffer_size",
        (const char *[]){"5000", "8388608"},
        2U,
        "myisam_sort_buffer_size unscoped"
    );
    failures += execute_statement_ok(database, "SET LOCAL myisam_sort_buffer_size = 6000");
    failures += execute_statement_ok(database, "SET @@myisam_sort_buffer_size = 7000");
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size",
        (const char *[]){"7000"},
        1U,
        "myisam_sort_buffer_size direct"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size, @@warning_count, @@error_count, ROW_COUNT()",
        (const char *[]){"8388608", "0", "0", "0"},
        4U,
        "myisam_sort_buffer_size default"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect myisam_sort_buffer_size value: '0'"
        },
        3U,
        "myisam_sort_buffer_size zero warning"
    );
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size, @@warning_count",
        (const char *[]){"4096", "1"},
        2U,
        "myisam_sort_buffer_size zero clamp"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = TRUE");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect myisam_sort_buffer_size value: '1'"
        },
        3U,
        "myisam_sort_buffer_size true warning"
    );
    failures += execute_error(
        database,
        "SET SESSION myisam_sort_buffer_size = '4096'",
        incorrect_sort_argument
    );

    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = 'nulls_equal'");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method, @@GLOBAL.myisam_stats_method, "
        "@@SESSION.myisam_stats_method",
        (const char *[]){"nulls_equal", "nulls_unequal", "nulls_equal"},
        3U,
        "myisam_stats_method string"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = nulls_ignored");
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = 0");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_unequal"},
        1U,
        "myisam_stats_method integer zero"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = 1");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_equal"},
        1U,
        "myisam_stats_method integer one"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = 2");
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = TRUE");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_equal"},
        1U,
        "myisam_stats_method true"
    );
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = FALSE");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_unequal"},
        1U,
        "myisam_stats_method false"
    );
    failures +=
        execute_error(database, "SET SESSION myisam_stats_method = 'bad'", invalid_stats_method);
    failures +=
        execute_error(database, "SET SESSION myisam_stats_method = 3", invalid_stats_method);
    failures +=
        execute_error(database, "SET SESSION myisam_stats_method = NULL", invalid_stats_method);

    mylite_close(database);
    return failures;
}

static int test_user_variable_assignments_and_rollback(void) {
    struct expected_sql_error incorrect_sort_argument = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    struct expected_sql_error invalid_stats_method = {
        .code = mysql_error_invalid_system_variable_value,
        .sqlstate = "42000",
        .message_part = "can't be set",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open MyISAM user var db");

    failures += execute_statement_ok(database, "SET @buf = 8192");
    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = @buf");
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size, @@warning_count",
        (const char *[]){"8192", "0"},
        2U,
        "myisam_sort_buffer_size user variable"
    );
    failures += execute_statement_ok(database, "SET @buf = -2");
    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = @buf");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]
        ){"Warning", "1292", "Truncated incorrect myisam_sort_buffer_size value: '-2'"},
        3U,
        "myisam_sort_buffer_size user variable warning"
    );
    failures += execute_statement_ok(database, "SET @buf = '4096'");
    failures += execute_error(
        database,
        "SET SESSION myisam_sort_buffer_size = @buf",
        incorrect_sort_argument
    );

    failures += execute_statement_ok(database, "SET @method = 'nulls_ignored'");
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = @method");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_ignored"},
        1U,
        "myisam_stats_method string user variable"
    );
    failures += execute_statement_ok(database, "SET @method = 2");
    failures += execute_statement_ok(database, "SET SESSION myisam_stats_method = @method");
    failures += expect_values(
        database,
        "SELECT @@myisam_stats_method",
        (const char *[]){"nulls_ignored"},
        1U,
        "myisam_stats_method integer user variable"
    );
    failures += execute_statement_ok(database, "SET @method = 'bad'");
    failures +=
        execute_error(database, "SET SESSION myisam_stats_method = @method", invalid_stats_method);

    failures += execute_statement_ok(database, "SET SESSION myisam_sort_buffer_size = DEFAULT");
    failures += execute_error(
        database,
        "SET myisam_sort_buffer_size = 9000, myisam_stats_method = 'bad'",
        invalid_stats_method
    );
    failures += expect_values(
        database,
        "SELECT @@myisam_sort_buffer_size",
        (const char *[]){"8388608"},
        1U,
        "MyISAM SET rollback"
    );

    mylite_close(database);
    return failures;
}

static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            context,
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), expected_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    for (size_t column = 0U; column < expected_count; ++column) {
        failures +=
            expect_text(mylite_result_value_text(result, 0U, column), expected[column], context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
    return expect_values(
        database,
        expected.sql,
        (const char *[]){expected.name, expected.value},
        2U,
        expected.context
    );
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
