#include <mylite/mylite.h>

#include <stdio.h>
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
    mysql_error_key_does_not_exist = 1176,
    mysql_error_wrong_usage = 1221,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_update_index_hints_noop(void);
static int prepare_fixture(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_grid(
    mylite_db *database,
    const char *sql,
    const char *const *values,
    size_t row_count,
    size_t column_count,
    const char *context
);
static int expect_empty_statement(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_update_index_hints_noop() == 0 ? 0 : 1;
}

static int test_update_index_hints_noop(void) {
    static const char *const rows_after_use[] = {
        "1",
        "10",
        "100",
        "2",
        "20",
        "201",
        "3",
        "20",
        "300",
    };
    static const char *const rows_after_force[] = {
        "1",
        "10",
        "100",
        "2",
        "20",
        "201",
        "3",
        "20",
        "301",
    };
    static const char *const rows_after_mixed_hints[] = {
        "1",
        "10",
        "103",
        "2",
        "20",
        "201",
        "3",
        "20",
        "302",
    };
    static const char *const schema_qualified_value[] = {"104"};
    static const char *const temp_rows_after_use[] = {
        "1",
        "1",
        "10",
        "2",
        "2",
        "25",
        "3",
        "2",
        "30",
    };
    static const char *const temp_value_after_force[] = {"35"};
    static const char *const persisted_value[] = {"302"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "update-index-hints") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open update hints");
    failures += prepare_fixture(database);

    failures += expect_update_ok(
        database,
        "UPDATE t USE INDEX (k_n) SET other = 201 WHERE n = 20 ORDER BY id LIMIT 1",
        1
    );
    failures += expect_query_grid(
        database,
        "SELECT id, n, other FROM t ORDER BY id",
        rows_after_use,
        3U,
        3U,
        "use index update rows"
    );

    failures += expect_update_ok(
        database,
        "UPDATE t USE KEY FOR GROUP BY (k_n) SET other = 100 WHERE id = 1",
        0
    );
    failures += expect_update_ok(
        database,
        "UPDATE t FORCE KEY FOR ORDER BY (PRIMARY) SET other = 301 WHERE id = 3",
        1
    );
    failures += expect_query_grid(
        database,
        "SELECT id, n, other FROM t ORDER BY id",
        rows_after_force,
        3U,
        3U,
        "force key update rows"
    );

    failures += expect_update_ok(
        database,
        "UPDATE t IGNORE INDEX (k_other) SET other = 102 WHERE id = 1",
        1
    );
    failures += expect_update_ok(database, "UPDATE t USE INDEX () SET other = 103 WHERE id = 1", 1);
    failures +=
        expect_update_ok(database, "UPDATE t USE INDEX (k_n, k_n) SET other = 103 WHERE id = 1", 0);
    failures +=
        expect_update_ok(database, "UPDATE t FORCE INDEX (k_ot) SET other = 302 WHERE id = 3", 1);
    failures += expect_query_grid(
        database,
        "SELECT id, n, other FROM t ORDER BY id",
        rows_after_mixed_hints,
        3U,
        3U,
        "mixed update hint rows"
    );

    failures += expect_update_ok(
        database,
        "UPDATE app.t USE INDEX (PRIMARY) SET other = 104 WHERE id = 1",
        1
    );
    failures += expect_query_grid(
        database,
        "SELECT other FROM app.t WHERE id = 1",
        schema_qualified_value,
        1U,
        1U,
        "schema-qualified hinted update row"
    );

    failures += expect_update_ok(
        database,
        "UPDATE temp_hint USE INDEX (k_n) SET other = 25 WHERE n = 2 ORDER BY id LIMIT 1",
        1
    );
    failures += expect_query_grid(
        database,
        "SELECT id, n, other FROM temp_hint ORDER BY id",
        temp_rows_after_use,
        3U,
        3U,
        "temporary hinted update rows"
    );
    failures += expect_update_ok(
        database,
        "UPDATE temp_hint FORCE KEY FOR ORDER BY (PRIMARY) SET other = 35 WHERE id = 3",
        1
    );
    failures += expect_query_grid(
        database,
        "SELECT other FROM temp_hint WHERE id = 3",
        temp_value_after_force,
        1U,
        1U,
        "temporary force key hinted update row"
    );
    failures += execute_error(
        database,
        "UPDATE temp_hint USE INDEX (missing) SET other = 36 WHERE id = 3",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'missing' doesn't exist in table 'temp_hint'",
        }
    );

    failures += execute_error(
        database,
        "UPDATE t USE INDEX (missing) SET other = 105 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'missing' doesn't exist in table 't'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t USE INDEX (kind) SET other = 105 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'kind' doesn't exist in table 't'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t USE INDEX (k_n) FORCE INDEX (PRIMARY) SET other = 105 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of USE INDEX and FORCE INDEX",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t FORCE INDEX () SET other = 105 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t IGNORE INDEX () SET other = 105 WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen update hints");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_grid(
        database,
        "SELECT other FROM t WHERE id = 3",
        persisted_value,
        1U,
        1U,
        "hinted update persists after reopen"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int prepare_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE t ("
        "id INT PRIMARY KEY, n INT, other INT, ka INT, kb INT, "
        "KEY k_n (n), KEY k_other (other), KEY kind_a (ka), KEY kind_b (kb))"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO t VALUES (1,10,100,1,10),(2,20,200,2,20),(3,20,300,3,30)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE temp_hint ("
        "id INT PRIMARY KEY, n INT, other INT, KEY k_n (n), KEY k_other (other))"
    );
    failures +=
        execute_statement_ok(database, "INSERT INTO temp_hint VALUES (1,1,10),(2,2,20),(3,2,30)");

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected ok, got %d %s %s\n",
            sql,
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

    failures += expect_empty_statement(result, mylite_result_affected_rows(result), sql);
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got ok\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures +=
        expect_text_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_empty_statement(result, affected_rows, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_grid(
    mylite_db *database,
    const char *sql,
    const char *const *values,
    size_t row_count,
    size_t column_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            size_t value_index = (row_index * column_count) + column_index;

            failures += expect_text(
                mylite_result_value_text(result, row_index, column_index),
                values[value_index],
                context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_empty_statement(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    return failures;
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected != NULL ? expected : "(null)",
            actual != NULL ? actual : "(null)"
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
            actual != NULL ? actual : "(null)",
            needle != NULL ? needle : "(null)"
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written =
        snprintf(path, path_size, "/tmp/mylite-runtime-%s-%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char full_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(full_path, sizeof(full_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(full_path)) {
        (void)remove(full_path);
    }
}
