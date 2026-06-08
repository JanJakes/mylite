#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_explain_placeholder_results(void);
static int test_explain_placeholder_errors(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error_contains(mylite_db *database, const char *sql, const char *message_part);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_explain_placeholder_results();
    failures += test_explain_placeholder_errors();

    return failures == 0 ? 0 : 1;
}

static int test_explain_placeholder_results(void) {
    static const char *const traditional_columns[] = {
        "id",
        "select_type",
        "table",
        "partitions",
        "type",
        "possible_keys",
        "key",
        "key_len",
        "ref",
        "rows",
        "filtered",
        "Extra",
    };
    static const char *const traditional_values[] = {
        "1",
        "SIMPLE",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "MyLite EXPLAIN placeholder",
    };
    static const char *const json_columns[] = {"EXPLAIN"};
    static const char *const json_values[] = {
        "{\"query_block\":{\"message\":\"MyLite EXPLAIN placeholder\"}}",
    };
    static const char *const tree_values[] = {"-> MyLite EXPLAIN placeholder\n"};
    static const char *const analyze_values[] = {"-> MyLite EXPLAIN ANALYZE placeholder\n"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN SELECT 1",
            .columns = traditional_columns,
            .column_count = sizeof(traditional_columns) / sizeof(traditional_columns[0]),
            .values = traditional_values,
            .row_count = 1U,
            .context = "traditional EXPLAIN SELECT",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN INSERT INTO t VALUES (1)",
            .columns = traditional_columns,
            .column_count = sizeof(traditional_columns) / sizeof(traditional_columns[0]),
            .values = traditional_values,
            .row_count = 1U,
            .context = "traditional EXPLAIN INSERT placeholder",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN SELECT * FROM t1 WHERE MATCH(title) AGAINST ('needle')",
            .columns = traditional_columns,
            .column_count = sizeof(traditional_columns) / sizeof(traditional_columns[0]),
            .values = traditional_values,
            .row_count = 1U,
            .context = "traditional EXPLAIN unsupported child placeholder",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN UPDATE t SET id = 1",
            .columns = traditional_columns,
            .column_count = sizeof(traditional_columns) / sizeof(traditional_columns[0]),
            .values = traditional_values,
            .row_count = 1U,
            .context = "traditional EXPLAIN UPDATE placeholder",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN FORMAT=JSON SELECT 1",
            .columns = json_columns,
            .column_count = 1U,
            .values = json_values,
            .row_count = 1U,
            .context = "json EXPLAIN SELECT",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN FORMAT=JSON SELECT ROW_NUMBER() RESPECT NULLS OVER () FROM t1",
            .columns = json_columns,
            .column_count = 1U,
            .values = json_values,
            .row_count = 1U,
            .context = "json EXPLAIN unsupported child placeholder",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN FORMAT=TRADITIONAL SELECT 1",
            .columns = traditional_columns,
            .column_count = sizeof(traditional_columns) / sizeof(traditional_columns[0]),
            .values = traditional_values,
            .row_count = 1U,
            .context = "traditional explicit EXPLAIN SELECT",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN FORMAT=TREE SELECT 1",
            .columns = json_columns,
            .column_count = 1U,
            .values = tree_values,
            .row_count = 1U,
            .context = "tree EXPLAIN SELECT",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "EXPLAIN ANALYZE FORMAT=TREE SELECT 1",
            .columns = json_columns,
            .column_count = 1U,
            .values = analyze_values,
            .row_count = 1U,
            .context = "analyze EXPLAIN SELECT",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_explain_placeholder_errors(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open error memory");
    failures += execute_error_contains(
        database,
        "EXPLAIN ANALYZE FORMAT=JSON SELECT 1",
        "EXPLAIN ANALYZE supports only TREE format"
    );
    failures += execute_error_contains(
        database,
        "EXPLAIN ANALYZE FORMAT=TRADITIONAL SELECT 1",
        "EXPLAIN ANALYZE supports only TREE format"
    );
    failures += execute_error_contains(
        database,
        "EXPLAIN FORMAT=CSV SELECT 1",
        "EXPLAIN supports FORMAT=TRADITIONAL, JSON, or TREE"
    );
    failures += execute_error_contains(
        database,
        "EXPLAIN FORMAT=CSV SELECT * FROM t1 WHERE MATCH(title) AGAINST ('needle')",
        "EXPLAIN supports FORMAT=TRADITIONAL, JSON, or TREE"
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error_contains(mylite_db *database, const char *sql, const char *message_part) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_text_contains(mylite_errmsg(database), message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    return expect_text(mylite_result_value_text(result, row, column), expected, context);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}
