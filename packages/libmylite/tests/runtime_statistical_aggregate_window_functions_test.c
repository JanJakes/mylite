#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    seed_stat_count = 6,
    metadata_stddev_pop_column = 0,
    metadata_stddev_samp_column = 1,
    metadata_var_pop_column = 2,
    metadata_var_samp_column = 3,
    metadata_column_count = 4,
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

static int test_statistical_aggregate_window_results(void);
static int test_statistical_aggregate_window_metadata(void);
static int test_statistical_aggregate_window_diagnostics(void);
static int seed_stats(mylite_db *database);
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

    failures += test_statistical_aggregate_window_results();
    failures += test_statistical_aggregate_window_metadata();
    failures += test_statistical_aggregate_window_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_statistical_aggregate_window_results(void) {
    static const char *const no_source_columns[] = {"sp", "ss", "vp", "vs"};
    static const char *const no_source_values[] = {"0", NULL, "0", NULL};
    static const char *const partition_columns[] = {
        "id",
        "group_id",
        "std_alias",
        "stddev_alias",
        "sp",
        "ss",
        "vp",
        "vs",
        "variance_alias",
    };
    static const char *const partition_values[] = {
        "6",  NULL, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL,
        "1",  "10", "5",
        "5",  "5",  "7.0710678118654755",
        "25", "50", "25",
        "2",  "10", "5",
        "5",  "5",  "7.0710678118654755",
        "25", "50", "25",
        "3",  "10", "5",
        "5",  "5",  "7.0710678118654755",
        "25", "50", "25",
        "4",  "20", "5",
        "5",  "5",  "7.0710678118654755",
        "25", "50", "25",
        "5",  "20", "5",
        "5",  "5",  "7.0710678118654755",
        "25", "50", "25",
    };
    static const char *const moving_columns[] = {"id", "sp", "ss", "vp", "vs"};
    static const char *const moving_values[] = {
        "1",
        "0",
        NULL,
        "0",
        NULL,
        "2",
        "5",
        "7.0710678118654755",
        "25",
        "50",
        "3",
        "0",
        NULL,
        "0",
        NULL,
        "4",
        "0",
        NULL,
        "0",
        NULL,
        "5",
        "5",
        "7.0710678118654755",
        "25",
        "50",
        "6",
        "0",
        NULL,
        "0",
        NULL,
    };
    static const char *const empty_frame_columns[] = {"id", "sp", "ss", "vp", "vs"};
    static const char *const empty_frame_values[] = {
        "1", NULL, NULL, NULL, NULL, "2", "0", NULL, "0", NULL, "3", "0", NULL, "0", NULL,
        "4", NULL, NULL, NULL, NULL, "5", "0", NULL, "0", NULL, "6", "0", NULL, "0", NULL,
    };
    static const char *const named_columns[] = {"id", "vp"};
    static const char *const named_values[] = {
        "1",
        "0",
        "2",
        "25",
        "3",
        "25",
        "4",
        "66.66666666666667",
        "5",
        "125",
        "6",
        "125",
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_stats(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_POP(1) OVER () AS sp, STDDEV_SAMP(1) OVER () AS ss, "
                   "VAR_POP(1) OVER () AS vp, VAR_SAMP(1) OVER () AS vs",
            .columns = no_source_columns,
            .column_count = 4U,
            .values = no_source_values,
            .row_count = 1U,
            .context = "source-free statistical aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, group_id, "
                   "STD(n) OVER (PARTITION BY group_id) AS std_alias, "
                   "STDDEV(n) OVER (PARTITION BY group_id) AS stddev_alias, "
                   "STDDEV_POP(n) OVER (PARTITION BY group_id) AS sp, "
                   "STDDEV_SAMP(n) OVER (PARTITION BY group_id) AS ss, "
                   "VAR_POP(n) OVER (PARTITION BY group_id) AS vp, "
                   "VAR_SAMP(n) OVER (PARTITION BY group_id) AS vs, "
                   "VARIANCE(n) OVER (PARTITION BY group_id) AS variance_alias "
                   "FROM stats ORDER BY group_id, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_stat_count,
            .context = "partitioned statistical aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "STDDEV_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS sp, "
                   "STDDEV_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS ss, "
                   "VAR_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS vp, "
                   "VAR_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS vs "
                   "FROM stats ORDER BY id",
            .columns = moving_columns,
            .column_count = sizeof(moving_columns) / sizeof(moving_columns[0]),
            .values = moving_values,
            .row_count = seed_stat_count,
            .context = "moving statistical aggregate window frame",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "STDDEV_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS sp, "
                   "STDDEV_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS ss, "
                   "VAR_POP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS vp, "
                   "VAR_SAMP(n) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS vs "
                   "FROM stats ORDER BY id",
            .columns = empty_frame_columns,
            .column_count = sizeof(empty_frame_columns) / sizeof(empty_frame_columns[0]),
            .values = empty_frame_values,
            .row_count = seed_stat_count,
            .context = "empty frame statistical aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, VAR_POP(n) OVER w AS vp FROM stats "
                   "WINDOW w AS (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING "
                   "AND CURRENT ROW) ORDER BY id",
            .columns = named_columns,
            .column_count = sizeof(named_columns) / sizeof(named_columns[0]),
            .values = named_values,
            .row_count = seed_stat_count,
            .context = "named statistical aggregate window",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_statistical_aggregate_window_metadata(void) {
    const char *const metadata_sql = "SELECT STDDEV_POP(n) OVER () AS sp, "
                                     "STDDEV_SAMP(n) OVER () AS ss, "
                                     "VAR_POP(n) OVER () AS vp, "
                                     "VAR_SAMP(n) OVER () AS vs "
                                     "FROM stats ORDER BY id LIMIT 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_stats(database);
    }
    if (failures == 0) {
        rc = mylite_execute(database, metadata_sql, strlen(metadata_sql), &result);
        failures += expect_int(rc, MYLITE_OK, "statistical aggregate window metadata query");
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
        mylite_result_column_type(result, metadata_stddev_pop_column),
        MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
        "stddev_pop metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_stddev_pop_column),
        1,
        "stddev_pop nullable"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_stddev_samp_column),
        MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
        "stddev_samp metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_stddev_samp_column),
        1,
        "stddev_samp nullable"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_var_pop_column),
        MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
        "var_pop metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_var_pop_column),
        1,
        "var_pop nullable"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_var_samp_column),
        MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
        "var_samp metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_var_samp_column),
        1,
        "var_samp nullable"
    );

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_statistical_aggregate_window_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_stats(database);
    }

    failures += execute_error(
        database,
        "SELECT STDDEV_POP(DISTINCT n) OVER () FROM stats",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(label) OVER () FROM stats",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate-window arguments support only signed 64-bit integer "
                            "expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT STDDEV_POP(1) OVER (ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STDDEV_POP() without a table source supports only OVER ()",
        }
    );

    mylite_close(database);
    return failures;
}

static int seed_stats(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures +=
        execute_ok(database, "CREATE TABLE stats(id INT, group_id INT, n INT, label VARCHAR(10))");
    failures += execute_ok(
        database,
        "INSERT INTO stats VALUES "
        "(1,10,10,'a'),"
        "(2,10,20,'b'),"
        "(3,10,NULL,'c'),"
        "(4,20,30,'d'),"
        "(5,20,40,'e'),"
        "(6,NULL,NULL,'f')"
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
