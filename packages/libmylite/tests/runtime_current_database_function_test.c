#include <mylite/mylite.h>

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
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_current_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_current_database_values(void);
static int test_current_database_independent_handles(void);
static int test_current_database_unsupported_forms(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_current_result(
    const mylite_result *result,
    struct expected_current_result expected
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_current_database_values();
    failures += test_current_database_independent_handles();
    failures += test_current_database_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_current_database_values(void) {
    static const char *const default_columns[] = {"DATABASE()", "SCHEMA()"};
    static const char *const no_schema_values[] = {NULL, NULL};
    static const char *const app_values[] = {"app", "app"};
    static const char *const lower_columns[] = {"database()", "schema()"};
    static const char *const spaced_columns[] = {"DATABASE ()", "SCHEMA ()"};
    static const char *const parenthesized_columns[] = {"(DATABASE())"};
    static const char *const parenthesized_values[] = {"app"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");

    failures += execute_ok(database, "SELECT DATABASE(), SCHEMA()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = default_columns,
            .values = no_schema_values,
            .count = 2U,
            .context = "no selected schema",
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

    failures += execute_ok(database, "SELECT DATABASE(), SCHEMA()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = default_columns,
            .values = app_values,
            .count = 2U,
            .context = "selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT database(), schema() FROM DUAL", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = lower_columns,
            .values = app_values,
            .count = 2U,
            .context = "lower-case functions",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT DATABASE (), SCHEMA ()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = spaced_columns,
            .values = app_values,
            .count = 2U,
            .context = "spaced functions",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT (DATABASE())", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = parenthesized_columns,
            .values = parenthesized_values,
            .count = 1U,
            .context = "parenthesized function",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT DATABASE(), SCHEMA()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = default_columns,
            .values = no_schema_values,
            .count = 2U,
            .context = "drop clears selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    mylite_close(database);
    database = NULL;
    result = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "SELECT DATABASE(), SCHEMA()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = default_columns,
            .values = no_schema_values,
            .count = 2U,
            .context = "reopen has no selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT DATABASE(), SCHEMA()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = default_columns,
            .values = app_values,
            .count = 2U,
            .context = "use after reopen",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_current_database_independent_handles(void) {
    static const char *const database_column[] = {"DATABASE()"};
    static const char *const first_values[] = {"first_app"};
    static const char *const second_values[] = {"second_app"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");
    failures += execute_ok(first, "CREATE DATABASE first_app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE DATABASE second_app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(first, "USE first_app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE second_app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(first, "SELECT DATABASE()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = database_column,
            .values = first_values,
            .count = 1U,
            .context = "first selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT DATABASE()", &result);
    failures += expect_current_result(
        result,
        (struct expected_current_result){
            .columns = database_column,
            .values = second_values,
            .count = 1U,
            .context = "second selected schema",
        }
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_current_database_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT DATABASE(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SCHEMA(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATABASE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SCHEMA",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATABASE() LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATABASE(), 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATABASE() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only unqualified table columns",
        }
    );

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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, "error");
    mylite_result_free(result);

    return failures;
}

static int expect_current_result(
    const mylite_result *result,
    struct expected_current_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_current_database_function_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}
