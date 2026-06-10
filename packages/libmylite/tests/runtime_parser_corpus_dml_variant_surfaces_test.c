#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_dml_variant_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_dml_variant_surfaces() == 0 ? 0 : 1;
}

static int test_dml_variant_surfaces(void) {
    static const char *const delete_low_priority_count[] = {"2"};
    static const char *const delete_quick_count[] = {"1"};
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT, v INT)");
    failures += execute_ok(database, "CREATE TABLE u (id INT, v INT)");
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 10), (2, 20), (3, 30)");
    failures += execute_ok(database, "INSERT INTO u VALUES (1, 100), (2, 200)");

    failures += execute_ok(database, "DELETE LOW_PRIORITY FROM t WHERE id = 1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = delete_low_priority_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DELETE LOW_PRIORITY no-op modifier",
        }
    );
    failures += execute_ok(database, "DELETE QUICK FROM t WHERE id = 2");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = delete_quick_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DELETE QUICK no-op modifier",
        }
    );

    failures += execute_error(database, "DELETE IGNORE FROM t WHERE id = 3", unsupported);
    failures +=
        execute_error(database, "DELETE LOW_PRIORITY IGNORE FROM t WHERE id = 3", unsupported);
    failures += execute_error(database, "DELETE t FROM t WHERE id = 3", unsupported);
    failures += execute_error(database, "DELETE LOW_PRIORITY t FROM t WHERE id = 3", unsupported);
    failures +=
        execute_error(database, "DELETE LOW_PRIORITY QUICK t FROM t WHERE id = 3", unsupported);
    failures += execute_error(database, "DELETE FROM a USING t AS a WHERE a.id = 3", unsupported);
    failures += execute_error(
        database,
        "DELETE LOW_PRIORITY FROM a USING t AS a WHERE a.id = 3",
        unsupported
    );
    failures += execute_error(
        database,
        "DELETE LOW_PRIORITY QUICK FROM a USING t AS a WHERE a.id = 3",
        unsupported
    );
    failures += execute_error(database, "DELETE t.*, u.* FROM t, u WHERE t.id = u.id", unsupported);
    failures += execute_error(database, "DELETE FROM t ORDER BY id, v DESC LIMIT 1", unsupported);
    failures +=
        execute_error(database, "UPDATE IGNORE t, u SET t.v = u.v WHERE t.id = u.id", unsupported);
    failures +=
        execute_error(database, "UPDATE t LEFT JOIN u USING(id) SET t.v = u.v", unsupported);
    failures +=
        execute_error(database, "UPDATE t SET v = 10 ORDER BY id, v DESC LIMIT 1", unsupported);
    failures += execute_error(database, "INSERT INTO t (id, v) VALUES (id, v)", unsupported);
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 10) ON DUPLICATE KEY UPDATE v = v",
        unsupported
    );
    failures += execute_error(
        database,
        "REPLACE INTO t SELECT id, v FROM u UNION ALL SELECT id, v FROM t",
        unsupported
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

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
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "failed result columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t row_index = 0U;
    int failures = 0;

    failures += expect_int(
        mylite_execute(database, query.sql, strlen(query.sql), &result),
        MYLITE_OK,
        query.context
    );
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (row_index = 0U; row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            const char *actual = mylite_result_value_text(result, row_index, column_index);
            const char *expected = query.values[(row_index * query.column_count) + column_index];

            failures += expect_text(actual, expected, query.context);
        }
    }
    mylite_result_free(result);
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
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
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
