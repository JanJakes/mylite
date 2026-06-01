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
    mysql_error_parse = 1064,
    mysql_error_json_used_as_key = 3152,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_functional_and_multivalued_index_diagnostics(void);
static int open_test_database(mylite_db **out_database, char *path, size_t path_size);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_functional_and_multivalued_index_diagnostics() == 0 ? 0 : 1;
}

static int test_functional_and_multivalued_index_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_test_database(&database, path, sizeof(path));
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE table_func (a INT, b INT, KEY k ((a + b)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Functional key parts are not yet supported",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE table_func (a INT, b INT, j JSON)");

    failures += execute_error(
        database,
        "ALTER TABLE table_func ADD INDEX ((a + b))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Functional key parts are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX idx_expr ON table_func ((a - b) DESC)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Functional key parts are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_expr ON table_func ((a + 1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Functional key parts are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE table_func ADD PRIMARY KEY ((a + 1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Functional key parts are not yet supported",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE table_mv (j JSON, KEY mv ((CAST(j->'$.ids' AS UNSIGNED ARRAY))))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Multi-valued indexes are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE table_func ADD INDEX mv ((CAST(j->'$.ids' AS UNSIGNED ARRAY)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Multi-valued indexes are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX mv2 ON table_func ((CAST(j->'$.ids' AS UNSIGNED ARRAY)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Multi-valued indexes are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX direct_j ON table_func (j)",
        (struct expected_sql_error){
            .code = mysql_error_json_used_as_key,
            .sqlstate = "42000",
            .message_part = "JSON column 'j' supports indexing",
        }
    );
    failures += expect_query_row_count(
        database,
        "SHOW INDEX FROM table_func",
        0U,
        "failed expression indexes do not mutate catalog"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_test_database(mylite_db **out_database, char *path, size_t path_size) {
    int failures = 0;

    if (make_test_path(path, path_size) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open test file");

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    failures += expect_int(mylite_errcode(database), MYLITE_OK, "public error code");
    failures += expect_text_or_null(mylite_sqlstate(database), "00000", "public SQLSTATE");
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_ERROR, sql);

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_row_count(
    mylite_db *database,
    const char *sql,
    size_t expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_row_count(result), expected, context);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_functional_multivalued_index_diagnostics_%d.mylite",
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? -1 : 0;
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + 4U];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }

    (void)remove(buffer);
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == expected ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)) {
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

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text containing [%s], got [%s]\n",
        context,
        needle,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}
