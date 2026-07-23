#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
    mixed_identity_alias_column_count = 6,
    current_role_mixed_column_count = 8,
    current_role_diagnostics_column_count = 4,
    current_role_pair_column_count = 2,
    diagnostics_count_column_count = 3,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_current_user_identity_values(void);
static int test_current_user_identity_independent_handles(void);
static int test_current_user_identity_unsupported_forms(void);
static int test_current_role_function_values_and_persistence(void);
static int test_current_role_function_independent_handles(void);
static int test_current_role_function_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_diagnostics_counts_cleared(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_current_user_identity_values();
    failures += test_current_user_identity_independent_handles();
    failures += test_current_user_identity_unsupported_forms();
    failures += test_current_role_function_values_and_persistence();
    failures += test_current_role_function_independent_handles();
    failures += test_current_role_function_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_current_user_identity_values(void) {
    static const char *const identity_columns[] = {"USER()", "CURRENT_USER()", "CURRENT_USER"};
    static const char *const identity_values[] = {"root@%", "root@%", "root@%"};
    static const char *const alias_columns[] = {"SESSION_USER()", "SYSTEM_USER()"};
    static const char *const alias_values[] = {"root@%", "root@%"};
    static const char *const lower_columns[] = {"user()", "current_user"};
    static const char *const lower_values[] = {"root@%", "root@%"};
    static const char *const lower_alias_columns[] = {"session_user()", "system_user()"};
    static const char *const commented_alias_columns[] = {
        "SESSION_USER(/* inside */)",
        "SYSTEM_USER(/* inside */)"
    };
    static const char *const spaced_columns[] = {"USER ()", "CURRENT_USER ()"};
    static const char *const parenthesized_columns[] = {"(USER())", "(CURRENT_USER)"};
    static const char *const parenthesized_alias_columns[] = {
        "(SESSION_USER())",
        "(System_User())"
    };
    static const char *const mixed_columns[] = {"USER()", "CURRENT_USER", "DATABASE()"};
    static const char *const mixed_app_values[] = {"root@%", "root@%", "app"};
    static const char *const mixed_no_schema_values[] = {"root@%", "root@%", NULL};
    static const char *const mixed_alias_columns[] =
        {"ROW_COUNT()", "SESSION_USER()", "SYSTEM_USER()", "USER()", "CURRENT_USER", "VERSION()"};
    static const char *const mixed_alias_values[] =
        {"0", "root@%", "root@%", "root@%", "root@%", MYLITE_MYSQL_SERVER_VERSION_STRING};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");

    failures += execute_ok(database, "SELECT USER(), CURRENT_USER(), CURRENT_USER", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = identity_columns,
            .values = identity_values,
            .count = 3U,
            .context = "identity values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT SESSION_USER(), SYSTEM_USER()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = alias_columns,
            .values = alias_values,
            .count = 2U,
            .context = "identity alias values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT user(), current_user FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = lower_columns,
            .values = lower_values,
            .count = 2U,
            .context = "lower identity values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT session_user(), system_user() FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = lower_alias_columns,
            .values = alias_values,
            .count = 2U,
            .context = "lower identity alias values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT SESSION_USER(/* inside */), SYSTEM_USER(/* inside */)",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = commented_alias_columns,
            .values = alias_values,
            .count = 2U,
            .context = "commented identity alias values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT USER (), CURRENT_USER ()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = spaced_columns,
            .values = lower_values,
            .count = 2U,
            .context = "spaced identity values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT (USER()), (CURRENT_USER)", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = parenthesized_columns,
            .values = lower_values,
            .count = 2U,
            .context = "parenthesized identity values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT (SESSION_USER()), (System_User())", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = parenthesized_alias_columns,
            .values = alias_values,
            .count = 2U,
            .context = "parenthesized identity alias values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT ROW_COUNT(), SESSION_USER(), SYSTEM_USER(), USER(), CURRENT_USER, VERSION()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_alias_columns,
            .values = mixed_alias_values,
            .count = mixed_identity_alias_column_count,
            .context = "mixed identity alias scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT USER(), CURRENT_USER, DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_app_values,
            .count = 3U,
            .context = "mixed identity and database values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT USER(), CURRENT_USER, DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_no_schema_values,
            .count = 3U,
            .context = "drop clears schema but not identity",
        }
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures +=
        execute_ok(database, "SELECT USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns =
                (const char *const[]){"USER()", "CURRENT_USER", "SESSION_USER()", "SYSTEM_USER()"},
            .values = (const char *const[]){"root@%", "root@%", "root@%", "root@%"},
            .count = 4U,
            .context = "identity after reopen",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_current_user_identity_independent_handles(void) {
    static const char *const identity_columns[] =
        {"USER()", "CURRENT_USER", "SESSION_USER()", "SYSTEM_USER()"};
    static const char *const identity_values[] = {"root@%", "root@%", "root@%", "root@%"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += mylite_test_expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");

    failures +=
        execute_ok(first, "SELECT USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = identity_columns,
            .values = identity_values,
            .count = 4U,
            .context = "first handle identity",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(second, "SELECT USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = identity_columns,
            .values = identity_values,
            .count = 4U,
            .context = "second handle identity",
        }
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_current_user_identity_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT USER(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_USER(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SESSION_USER(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSTEM_USER(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SESSION_USER ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSTEM_USER ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SESSION_USER/**/()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSTEM_USER/**/()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT USER",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT SESSION_USER",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT SYSTEM_USER",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_ok(database, "SELECT USER() LIMIT 1", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"USER()"},
            .values = (const char *const[]){"root@%"},
            .count = 1U,
            .context = "user with limit",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT CURRENT_USER LIMIT 1", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"CURRENT_USER"},
            .values = (const char *const[]){"root@%"},
            .count = 1U,
            .context = "current user keyword with limit",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT SESSION_USER() LIMIT 1", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"SESSION_USER()"},
            .values = (const char *const[]){"root@%"},
            .count = 1U,
            .context = "session user with limit",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "SELECT USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER(), id FROM t",
        &result
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        1U,
        "table-backed identity row count"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        "root@%",
        "table-backed user value"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 1U),
        "root@%",
        "table-backed current user value"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 2U),
        "root@%",
        "table-backed session user value"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 3U),
        "root@%",
        "table-backed system user value"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 4U),
        "1",
        "table-backed identity source column"
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_current_role_function_values_and_persistence(void) {
    static const char *const role_columns[] = {"CURRENT_ROLE()"};
    static const char *const role_values[] = {"NONE"};
    static const char *const lower_columns[] = {"current_role()"};
    static const char *const mixed_case_columns[] = {"Current_Role()"};
    static const char *const spaced_columns[] = {"CURRENT_ROLE ()"};
    static const char *const parenthesized_columns[] = {"(CURRENT_ROLE())"};
    static const char *const mixed_columns[] = {
        "CURRENT_ROLE()",
        "USER()",
        "CURRENT_USER",
        "SESSION_USER()",
        "SYSTEM_USER()",
        "VERSION()",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const mixed_values[] = {
        "NONE",
        "root@%",
        "root@%",
        "root@%",
        "root@%",
        MYLITE_MYSQL_SERVER_VERSION_STRING,
        "0",
        "-1",
    };
    static const char *const diagnostic_columns[] = {
        "CURRENT_ROLE()",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"NONE", "1", "0", "-1"};
    static const char *const error_values[] = {"NONE", "1", "1", "-1"};
    static const char *const role_database_columns[] = {"CURRENT_ROLE()", "DATABASE()"};
    static const char *const role_app_values[] = {"NONE", "app"};
    static const char *const role_no_schema_values[] = {"NONE", NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "current_role_values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open current role file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "SELECT CURRENT_ROLE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_columns,
            .values = role_values,
            .count = 1U,
            .context = "current role value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT current_role() FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = lower_columns,
            .values = role_values,
            .count = 1U,
            .context = "lower current role value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT Current_Role()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_case_columns,
            .values = role_values,
            .count = 1U,
            .context = "mixed-case current role value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT CURRENT_ROLE ()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = spaced_columns,
            .values = role_values,
            .count = 1U,
            .context = "spaced current role value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT (CURRENT_ROLE())", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = parenthesized_columns,
            .values = role_values,
            .count = 1U,
            .context = "parenthesized current role value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT CURRENT_ROLE(), USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER(), VERSION(), "
        "@@warning_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = current_role_mixed_column_count,
            .context = "mixed current role scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT CURRENT_ROLE(), @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = diagnostic_columns,
            .values = warning_values,
            .count = current_role_diagnostics_column_count,
            .context = "current role warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_diagnostics_counts_cleared(database, "current role clears warnings");

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += execute_ok(
        database,
        "SELECT CURRENT_ROLE(), @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = diagnostic_columns,
            .values = error_values,
            .count = current_role_diagnostics_column_count,
            .context = "current role error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_diagnostics_counts_cleared(database, "current role clears errors");

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by current role reads"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by current role reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(database, "SELECT CURRENT_ROLE(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_database_columns,
            .values = role_app_values,
            .count = current_role_pair_column_count,
            .context = "current role with selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1)");
    failures += execute_statement_ok(database, "DROP DATABASE app");
    failures += execute_ok(database, "SELECT CURRENT_ROLE(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_database_columns,
            .values = role_no_schema_values,
            .count = current_role_pair_column_count,
            .context = "current role after dropped schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen current role file");
    failures += execute_ok(database, "SELECT CURRENT_ROLE(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_database_columns,
            .values = role_no_schema_values,
            .count = current_role_pair_column_count,
            .context = "current role after reopen without selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(database, "SELECT CURRENT_ROLE(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_database_columns,
            .values = role_app_values,
            .count = current_role_pair_column_count,
            .context = "current role after reopen with selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read current role preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "current role preamble unchanged"
    );
    remove_related_files(path);

    return failures;
}

static int test_current_role_function_independent_handles(void) {
    static const char *const columns[] = {"CURRENT_ROLE()", "@@warning_count"};
    static const char *const values[] = {"NONE", "0"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "current_role_independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &first), MYLITE_OK, "open first role handle");
    failures +=
        mylite_test_expect_int(mylite_open(path, &second), MYLITE_OK, "open second role handle");

    failures += execute_ok(first, "SELECT CURRENT_ROLE(), @@warning_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = current_role_pair_column_count,
            .context = "first handle current role",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT CURRENT_ROLE(), @@warning_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = current_role_pair_column_count,
            .context = "second handle current role",
        }
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_current_role_function_unsupported_forms(void) {
    static const char *const role_alias_columns[] = {"role_name"};
    static const char *const role_alias_values[] = {"NONE"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "current_role_unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open unsupported role file"
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT)");

    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CURRENT_ROLE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CURRENT_ROLE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE('x')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CURRENT_ROLE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'CURRENT_ROLE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_ok(database, "SELECT CURRENT_ROLE() LIMIT 1", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = (const char *const[]){"CURRENT_ROLE()"},
            .values = (const char *const[]){"NONE"},
            .count = 1U,
            .context = "current role with limit",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT CURRENT_ROLE() + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1)");
    failures += execute_ok(database, "SELECT CURRENT_ROLE(), id FROM t", &result);
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        1U,
        "table-backed current role row count"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        "NONE",
        "table-backed current role value"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 1U),
        "1",
        "table-backed current role source column"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT CURRENT_ROLE() AS role_name", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = role_alias_columns,
            .values = role_alias_values,
            .count = 1U,
            .context = "current role select item alias",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, "error");
    mylite_result_free(result);

    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    return failures;
}

static int expect_diagnostics_counts_cleared(mylite_db *database, const char *context) {
    static const char *const columns[] = {"@@warning_count", "@@error_count", "ROW_COUNT()"};
    static const char *const values[] = {"0", "0", "-1"};
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);

    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = diagnostics_count_column_count,
            .context = context,
        }
    );
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        perror("fopen");
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (read_size != size) {
        fprintf(stderr, "expected to read %zu bytes, read %zu\n", size, read_size);
        fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte range did not match\n", context);
        return 1;
    }

    return 0;
}
