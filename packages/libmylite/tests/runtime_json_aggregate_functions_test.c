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
    mysql_error_json_null_member_name = 3158,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_json_aggregate_values(void);
static int test_json_aggregate_diagnostics(void);
static int seed_json_aggregate_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_discard(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_json_aggregate_values();
    failures += test_json_aggregate_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_json_aggregate_values(void) {
    static const char *const no_source_columns[] = {"ja", "jo"};
    static const char *const no_source_values[] = {"[null]", "{\"a\": 1}"};
    static const char *const source_columns[] = {"a", "j", "n", "o"};
    static const char *const source_values[] = {
        "[\"alpha\", \"beta\", null]",
        "[{\"x\": 1}, [2], null]",
        "[null, null, null]",
        "{\"k\": 3}",
    };
    static const char *const grouped_columns[] = {"g", "a", "o"};
    static const char *const grouped_values[] = {
        "1",
        "[\"alpha\", \"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "2",
        "[\"carrot\"]",
        "{\"c\": \"carrot\"}",
        "3",
        "[null]",
        "{\"n\": null}",
    };
    static const char *const empty_columns[] = {"a", "o"};
    static const char *const empty_values[] = {NULL, NULL};
    static const char *const nonstring_key_columns[] = {"id_key", "bool_key"};
    static const char *const nonstring_key_values[] = {
        "{\"4\": \"carrot\"}",
        "{\"1\": \"carrot\"}",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAYAGG(NULL) AS ja, JSON_OBJECTAGG('a', 1) AS jo",
            .columns = no_source_columns,
            .column_count = sizeof(no_source_columns) / sizeof(no_source_columns[0]),
            .values = no_source_values,
            .row_count = 1U,
            .context = "no-source JSON aggregates",
        }
    );
    failures += seed_json_aggregate_table(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAYAGG(s) AS a, JSON_ARRAYAGG(j) AS j, "
                   "JSON_ARRAYAGG(NULL) AS n, JSON_OBJECTAGG('k', id) AS o "
                   "FROM t WHERE g = 1",
            .columns = source_columns,
            .column_count = sizeof(source_columns) / sizeof(source_columns[0]),
            .values = source_values,
            .row_count = 1U,
            .context = "source JSON aggregates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, JSON_ARRAYAGG(s) AS a, JSON_OBJECTAGG(k, s) AS o "
                   "FROM t GROUP BY g ORDER BY g",
            .columns = grouped_columns,
            .column_count = sizeof(grouped_columns) / sizeof(grouped_columns[0]),
            .values = grouped_values,
            .row_count = 3U,
            .context = "grouped JSON aggregates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAYAGG(s) AS a, JSON_OBJECTAGG(k, s) AS o "
                   "FROM t WHERE g = 99",
            .columns = empty_columns,
            .column_count = sizeof(empty_columns) / sizeof(empty_columns[0]),
            .values = empty_values,
            .row_count = 1U,
            .context = "empty JSON aggregates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_OBJECTAGG(id, s) AS id_key, "
                   "JSON_OBJECTAGG(b, s) AS bool_key FROM t WHERE g = 2",
            .columns = nonstring_key_columns,
            .column_count = sizeof(nonstring_key_columns) / sizeof(nonstring_key_columns[0]),
            .values = nonstring_key_values,
            .row_count = 1U,
            .context = "nonstring JSON object aggregate keys",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_aggregate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostic database");
    failures += seed_json_aggregate_table(database);
    failures += execute_error(
        database,
        "SELECT JSON_OBJECTAGG(NULL, id) FROM t WHERE g = 1",
        (struct expected_sql_error){
            .code = mysql_error_json_null_member_name,
            .sqlstate = "22032",
            .message_part = "JSON documents may not contain NULL member names.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OBJECTAGG(s, id) FROM t WHERE g = 1",
        (struct expected_sql_error){
            .code = mysql_error_json_null_member_name,
            .sqlstate = "22032",
            .message_part = "JSON documents may not contain NULL member names.",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_json_aggregate_table(mylite_db *database) {
    int failures = 0;

    failures += execute_discard(database, "CREATE DATABASE app");
    failures += execute_discard(database, "USE app");
    failures += execute_discard(
        database,
        "CREATE TABLE t ("
        "g INT, id INT, k VARCHAR(20), s VARCHAR(20), j JSON, b TINYINT)"
    );
    failures += execute_discard(
        database,
        "INSERT INTO t VALUES "
        "(1, 1, 'a', 'alpha', '{\"x\":1}', 1),"
        "(1, 2, 'b', 'beta', '[2]', 0),"
        "(1, 3, 'a', NULL, NULL, NULL),"
        "(2, 4, 'c', 'carrot', '{\"x\":4}', 1),"
        "(3, 5, 'n', NULL, NULL, NULL)"
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    return 0;
}

static int execute_discard(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    size_t value_count = expected.column_count * expected.row_count;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        failures += expect_result_value(
            result,
            value_index / expected.column_count,
            value_index % expected.column_count,
            expected.values[value_index],
            expected.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "runtime-json-aggregate-%s-%d.mylite",
        name,
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    remove(buffer);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL, got [%s]\n", context, actual);
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(stderr, "%s: expected [%s], got NULL\n", context, expected);
        return 1;
    }
    return expect_text(actual, expected, context);
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
