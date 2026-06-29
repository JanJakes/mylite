#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_not_supported_yet = 1235,
    seed_post_count = 7,
    metadata_count_star_column = 0,
    metadata_count_value_column = 1,
    metadata_sum_column = 2,
    metadata_avg_column = 3,
    metadata_min_column = 4,
    metadata_max_column = 5,
    metadata_column_count = 6,
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

static int test_core_aggregate_window_results(void);
static int test_core_aggregate_window_metadata(void);
static int test_core_aggregate_window_diagnostics(void);
static int seed_posts(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_core_aggregate_window_results();
    failures += test_core_aggregate_window_metadata();
    failures += test_core_aggregate_window_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_core_aggregate_window_results(void) {
    static const char *const no_source_columns[] = {"c", "s", "a"};
    static const char *const no_source_values[] = {"1", "1", "2.0000"};
    static const char *const partition_columns[] =
        {"id", "author_id", "c", "cs", "s", "a", "mi", "ma"};
    static const char *const partition_values[] = {
        "6",  NULL,     "2", "1", "2",  "2.0000", "2", "2", "7",  NULL,     "2", "1",
        "2",  "2.0000", "2", "2", "1",  "10",     "3", "2", "12", "6.0000", "5", "7",
        "2",  "10",     "3", "2", "12", "6.0000", "5", "7", "3",  "10",     "3", "2",
        "12", "6.0000", "5", "7", "4",  "20",     "2", "2", "13", "6.5000", "4", "9",
        "5",  "20",     "2", "2", "13", "6.5000", "4", "9",
    };
    static const char *const frame_columns[] = {"id", "running", "moving_avg"};
    static const char *const frame_values[] = {
        "6",      "2", "2.0000", "7",      "2", "2.0000", "1",      "5", "5.0000", "2",      "12",
        "6.0000", "3", "12",     "7.0000", "4", "4",      "4.0000", "5", "13",     "6.5000",
    };
    static const char *const named_columns[] = {"id", "c", "s"};
    static const char *const named_values[] = {
        "6",  "1", "2", "7",  "2", "2", "1", "1", "5", "2",  "2",
        "12", "3", "3", "12", "4", "1", "4", "5", "2", "13",
    };
    static const char *const count_literal_columns[] = {"id", "cn", "co"};
    static const char *const count_literal_values[] = {
        "1", "0", "3", "2", "0", "3", "3", "0", "3", "4", "0",
        "2", "5", "0", "2", "6", "0", "2", "7", "0", "2",
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) OVER () AS c, SUM(1) OVER () AS s, "
                   "AVG(2) OVER () AS a",
            .columns = no_source_columns,
            .column_count = 3U,
            .values = no_source_values,
            .row_count = 1U,
            .context = "source-free aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, author_id, COUNT(*) OVER (PARTITION BY author_id) AS c, "
                   "COUNT(score) OVER (PARTITION BY author_id) AS cs, "
                   "SUM(score) OVER (PARTITION BY author_id) AS s, "
                   "AVG(score) OVER (PARTITION BY author_id) AS a, "
                   "MIN(score) OVER (PARTITION BY author_id) AS mi, "
                   "MAX(score) OVER (PARTITION BY author_id) AS ma "
                   "FROM posts ORDER BY author_id, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_post_count,
            .context = "partitioned aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "SUM(score) OVER (PARTITION BY author_id ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running, "
                   "AVG(score) OVER (PARTITION BY author_id ORDER BY created_at "
                   "ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS moving_avg "
                   "FROM posts ORDER BY author_id, created_at, id",
            .columns = frame_columns,
            .column_count = 3U,
            .values = frame_values,
            .row_count = seed_post_count,
            .context = "ordered frame aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, COUNT(*) OVER w AS c, SUM(score) OVER w AS s FROM posts "
                   "WINDOW w AS (PARTITION BY author_id ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "
                   "ORDER BY author_id, created_at, id",
            .columns = named_columns,
            .column_count = 3U,
            .values = named_values,
            .row_count = seed_post_count,
            .context = "named aggregate window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, COUNT(NULL) OVER () AS cn, "
                   "COUNT(1) OVER (PARTITION BY author_id) AS co "
                   "FROM posts ORDER BY id",
            .columns = count_literal_columns,
            .column_count = 3U,
            .values = count_literal_values,
            .row_count = seed_post_count,
            .context = "count literal aggregate windows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_core_aggregate_window_metadata(void) {
    const char *const metadata_sql = "SELECT COUNT(*) OVER () AS c, COUNT(score) OVER () AS cv, "
                                     "SUM(score) OVER () AS s, AVG(score) OVER () AS a, "
                                     "MIN(score) OVER () AS mi, MAX(score) OVER () AS ma "
                                     "FROM posts ORDER BY id LIMIT 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_posts(database);
    }
    if (failures == 0) {
        rc = mylite_execute(database, metadata_sql, strlen(metadata_sql), &result);
        failures += expect_int(rc, MYLITE_OK, "aggregate window metadata query");
        if (rc != MYLITE_OK) {
            fprintf(stderr, "metadata query: %s\n", mylite_errmsg(database));
        }
    }
    if (failures != 0) {
        mylite_result_free(result);
        mylite_close(database);
        return failures;
    }

    failures += expect_size(
        mylite_result_column_count(result),
        metadata_column_count,
        "metadata column count"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_count_star_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "count star metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_count_star_column),
        0,
        "count star nullable"
    );
    failures += expect_int(
        (int)(mylite_result_column_flags(result, metadata_count_star_column) &
              MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        0,
        "count star signed"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_count_value_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "count value metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_count_value_column),
        0,
        "count value nullable"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_sum_column),
        MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
        "sum metadata type"
    );
    failures +=
        expect_int(mylite_result_column_nullable(result, metadata_sum_column), 1, "sum nullable");
    failures += expect_int(
        mylite_result_column_type(result, metadata_avg_column),
        MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
        "avg metadata type"
    );
    failures +=
        expect_int(mylite_result_column_nullable(result, metadata_avg_column), 1, "avg nullable");
    failures += expect_int(
        mylite_result_column_type(result, metadata_min_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "min metadata type"
    );
    failures +=
        expect_int(mylite_result_column_nullable(result, metadata_min_column), 1, "min nullable");
    failures += expect_int(
        mylite_result_column_type(result, metadata_max_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "max metadata type"
    );
    failures +=
        expect_int(mylite_result_column_nullable(result, metadata_max_column), 1, "max nullable");

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_core_aggregate_window_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += execute_error(
        database,
        "SELECT SUM(DISTINCT score) OVER () FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "<window function>(DISTINCT ..)",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(title) OVER () FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "group_concat as window function",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(title) OVER () FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate-window arguments support only signed 64-bit integer "
                            "expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(1) OVER (ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUM() without a table source supports only OVER ()",
        }
    );

    mylite_close(database);
    return failures;
}

static int seed_posts(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE posts(id INT, author_id INT, created_at INT, score INT, title VARCHAR(20))"
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1,10,100,5,'a'),"
        "(2,10,200,7,'b'),"
        "(3,10,200,NULL,'c'),"
        "(4,20,NULL,4,'d'),"
        "(5,20,50,9,'e'),"
        "(6,NULL,10,2,'f'),"
        "(7,NULL,20,NULL,'g')"
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.sql, mylite_errmsg(database));
        mylite_result_free(result);
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
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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
