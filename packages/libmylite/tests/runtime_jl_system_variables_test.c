#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 17,
    session_set_scalar_value_count = 5,
    mysql_error_parse = 1064,
    mysql_error_invalid_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
    mysql_error_unknown_locale = 1649,
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
static int test_set_and_diagnostics(void);
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

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_and_diagnostics();

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

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open J/L db");
    failures += expect_values(
        database,
        "SELECT @@internal_tmp_mem_storage_engine, @@GLOBAL.internal_tmp_mem_storage_engine, "
        "@@SESSION.internal_tmp_mem_storage_engine, @@join_buffer_size, "
        "@@GLOBAL.join_buffer_size, @@SESSION.join_buffer_size, @@key_buffer_size, "
        "@@key_cache_age_threshold, @@key_cache_block_size, @@key_cache_division_limit, "
        "@@keyring_operations, @@large_files_support, @@large_page_size, @@large_pages, "
        "@@lc_messages, @@local_infile, @@mandatory_roles",
        (const char *[]){"TempTable",
                         "TempTable",
                         "TempTable",
                         "262144",
                         "262144",
                         "262144",
                         "8388608",
                         "300",
                         "1024",
                         "100",
                         "1",
                         "1",
                         "0",
                         "0",
                         "en_US",
                         "0",
                         ""},
        default_scalar_value_count,
        "J/L defaults"
    );

    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'internal_tmp_mem_storage_engine'",
                                     .name = "internal_tmp_mem_storage_engine",
                                     .value = "TempTable",
                                     .context = "internal_tmp_mem_storage_engine show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'join_buffer_size'",
                                     .name = "join_buffer_size",
                                     .value = "262144",
                                     .context = "join_buffer_size show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'keyring_operations'",
                                     .name = "keyring_operations",
                                     .value = "ON",
                                     .context = "keyring_operations show session"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'local_infile'",
                                     .name = "local_infile",
                                     .value = "OFF",
                                     .context = "local_infile show global"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'mandatory_roles'",
                                     .name = "mandatory_roles",
                                     .value = "",
                                     .context = "mandatory_roles show"}
    );

    failures += execute_error(database, "SELECT @@SESSION.key_buffer_size", global_only_read);
    failures += execute_error(database, "SELECT @@LOCAL.local_infile", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.mandatory_roles", global_only_read);

    mylite_close(database);
    return failures;
}

static int test_set_and_diagnostics(void) {
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
    struct expected_sql_error invalid_internal_tmp = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part =
            "Variable 'internal_tmp_mem_storage_engine' can't be set to the value of 'InnoDB'",
    };
    struct expected_sql_error incorrect_join_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'join_buffer_size'",
    };
    struct expected_sql_error unknown_locale = {
        .code = mysql_error_unknown_locale,
        .sqlstate = "HY000",
        .message_part = "Unknown locale: 'bogus'",
    };
    struct expected_sql_error unsupported_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "not supported",
    };
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open J/L SET db");

    failures += execute_statement_ok(database, "SET internal_tmp_mem_storage_engine = MEMORY");
    failures += execute_statement_ok(database, "SET join_buffer_size = 262272");
    failures += execute_statement_ok(database, "SET lc_messages = en_US");
    failures += expect_values(
        database,
        "SELECT @@internal_tmp_mem_storage_engine, @@GLOBAL.internal_tmp_mem_storage_engine, "
        "@@join_buffer_size, @@GLOBAL.join_buffer_size, @@lc_messages",
        (const char *[]){"MEMORY", "TempTable", "262272", "262144", "en_US"},
        session_set_scalar_value_count,
        "J/L session SET values"
    );

    failures += execute_statement_ok(database, "SET internal_tmp_mem_storage_engine = 1");
    failures += execute_statement_ok(database, "SET internal_tmp_mem_storage_engine = 0");
    failures += expect_values(
        database,
        "SELECT @@internal_tmp_mem_storage_engine",
        (const char *[]){"MEMORY"},
        1U,
        "internal_tmp_mem_storage_engine numeric enum"
    );

    failures += execute_statement_ok(database, "SET join_buffer_size = 129");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect join_buffer_size value: '129'"},
        3U,
        "join_buffer_size round warning"
    );
    failures += expect_values(
        database,
        "SELECT @@join_buffer_size, @@warning_count",
        (const char *[]){"128", "1"},
        2U,
        "join_buffer_size rounded"
    );
    failures += execute_statement_ok(database, "SET join_buffer_size = TRUE");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning", "1292", "Truncated incorrect join_buffer_size value: '1'"},
        3U,
        "join_buffer_size boolean warning"
    );
    failures += expect_values(
        database,
        "SELECT @@join_buffer_size, @@warning_count",
        (const char *[]){"128", "1"},
        2U,
        "join_buffer_size boolean clamp"
    );

    failures += execute_statement_ok(database, "SET @join_value = 262272");
    failures += execute_statement_ok(database, "SET join_buffer_size = @join_value");
    failures += execute_statement_ok(database, "SET @tmp_engine = 'TempTable'");
    failures += execute_statement_ok(database, "SET internal_tmp_mem_storage_engine = @tmp_engine");
    failures += expect_values(
        database,
        "SELECT @@join_buffer_size, @@internal_tmp_mem_storage_engine",
        (const char *[]){"262272", "TempTable"},
        2U,
        "J/L user variables"
    );

    failures += execute_error(
        database,
        "SET internal_tmp_mem_storage_engine = InnoDB",
        invalid_internal_tmp
    );
    failures += execute_error(database, "SET join_buffer_size = '262144'", incorrect_join_type);
    failures += execute_error(database, "SET join_buffer_size = ON", incorrect_join_type);
    failures += execute_error(database, "SET lc_messages = bogus", unknown_locale);
    failures += execute_error(database, "SET key_buffer_size = DEFAULT", global_only_set);
    failures += execute_error(database, "SET GLOBAL large_files_support = DEFAULT", read_only_set);

    failures +=
        execute_statement_ok(database, "SET GLOBAL internal_tmp_mem_storage_engine = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL internal_tmp_mem_storage_engine = TempTable");
    failures += execute_statement_ok(database, "SET GLOBAL join_buffer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL join_buffer_size = 262144");
    failures += execute_statement_ok(database, "SET GLOBAL key_buffer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL key_buffer_size = 8388608");
    failures += execute_statement_ok(database, "SET GLOBAL keyring_operations = ON");
    failures += execute_statement_ok(database, "SET GLOBAL local_infile = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL mandatory_roles = ''");
    failures += execute_error(
        database,
        "SET GLOBAL internal_tmp_mem_storage_engine = MEMORY",
        unsupported_set
    );
    failures += execute_error(database, "SET GLOBAL join_buffer_size = 262272", unsupported_set);
    failures += execute_error(database, "SET GLOBAL key_buffer_size = 8388609", unsupported_set);
    failures +=
        execute_error(database, "SET GLOBAL keyring_operations = OFF", unsupported_fixed_noop);
    failures += execute_error(database, "SET GLOBAL local_infile = ON", unsupported_fixed_noop);
    failures +=
        execute_error(database, "SET GLOBAL mandatory_roles = 'role1'", unsupported_fixed_noop);

    failures += execute_statement_ok(database, "SET internal_tmp_mem_storage_engine = MEMORY");
    failures += execute_statement_ok(database, "SET join_buffer_size = DEFAULT");
    failures += execute_error(
        database,
        "SET join_buffer_size = 262272, internal_tmp_mem_storage_engine = InnoDB",
        invalid_internal_tmp
    );
    failures += expect_values(
        database,
        "SELECT @@join_buffer_size, @@internal_tmp_mem_storage_engine",
        (const char *[]){"262144", "MEMORY"},
        2U,
        "J/L SET rollback"
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
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    for (size_t column = 0U; column < expected_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, column),
            expected[column],
            context
        );
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

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}
