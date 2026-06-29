#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_not_supported_yet = 1235,
    mysql_error_json_null_member_name = 3158,
    seed_json_count = 5,
    metadata_array_column = 0,
    metadata_object_column = 1,
    metadata_column_count = 2,
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

static int test_json_aggregate_window_results(void);
static int test_json_aggregate_window_metadata(void);
static int test_json_aggregate_window_diagnostics(void);
static int seed_json_values(mylite_db *database);
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

    failures += test_json_aggregate_window_results();
    failures += test_json_aggregate_window_metadata();
    failures += test_json_aggregate_window_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_json_aggregate_window_results(void) {
    static const char *const no_source_columns[] = {"ja", "jo"};
    static const char *const no_source_values[] = {"[null]", "{\"a\": 1}"};
    static const char *const partition_columns[] = {"id", "g", "ja", "jo"};
    static const char *const partition_values[] = {
        "1",
        "1",
        "[\"alpha\", \"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "2",
        "1",
        "[\"alpha\", \"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "3",
        "1",
        "[\"alpha\", \"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "4",
        "2",
        "[\"carrot\"]",
        "{\"c\": \"carrot\"}",
        "5",
        "3",
        "[null]",
        "{\"n\": null}",
    };
    static const char *const running_columns[] = {"id", "ja", "jo"};
    static const char *const running_values[] = {
        "1",
        "[\"alpha\"]",
        "{\"a\": \"alpha\"}",
        "2",
        "[\"alpha\", \"beta\"]",
        "{\"a\": \"alpha\", \"b\": \"beta\"}",
        "3",
        "[\"alpha\", \"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "4",
        "[\"carrot\"]",
        "{\"c\": \"carrot\"}",
        "5",
        "[null]",
        "{\"n\": null}",
    };
    static const char *const moving_columns[] = {"id", "ja", "jo"};
    static const char *const moving_values[] = {
        "1",
        "[\"alpha\"]",
        "{\"a\": \"alpha\"}",
        "2",
        "[\"alpha\", \"beta\"]",
        "{\"a\": \"alpha\", \"b\": \"beta\"}",
        "3",
        "[\"beta\", null]",
        "{\"a\": null, \"b\": \"beta\"}",
        "4",
        "[\"carrot\"]",
        "{\"c\": \"carrot\"}",
        "5",
        "[null]",
        "{\"n\": null}",
    };
    static const char *const empty_columns[] = {"id", "ja", "jo"};
    static const char *const empty_values[] = {
        "1",
        NULL,
        NULL,
        "2",
        "[\"alpha\"]",
        "{\"a\": \"alpha\"}",
        "3",
        "[\"beta\"]",
        "{\"b\": \"beta\"}",
        "4",
        NULL,
        NULL,
        "5",
        NULL,
        NULL,
    };
    static const char *const json_value_columns[] = {"id", "jj"};
    static const char *const json_value_values[] = {
        "1",
        "[{\"x\": 1}]",
        "2",
        "[{\"x\": 1}, [2]]",
        "3",
        "[{\"x\": 1}, [2], null]",
        "4",
        "[{\"x\": 4}]",
        "5",
        "[null]",
    };
    static const char *const named_columns[] = {"id", "ja"};
    static const char *const named_values[] = {
        "1",
        "[\"alpha\"]",
        "2",
        "[\"alpha\", \"beta\"]",
        "3",
        "[\"alpha\", \"beta\", null]",
        "4",
        "[\"alpha\", \"beta\", null, \"carrot\"]",
        "5",
        "[\"alpha\", \"beta\", null, \"carrot\", null]",
    };
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_json_values(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAYAGG(NULL) OVER () AS ja, "
                   "JSON_OBJECTAGG('a', 1) OVER () AS jo",
            .columns = no_source_columns,
            .column_count = sizeof(no_source_columns) / sizeof(no_source_columns[0]),
            .values = no_source_values,
            .row_count = 1U,
            .context = "source-free JSON aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, g, JSON_ARRAYAGG(s) OVER (PARTITION BY g) AS ja, "
                   "JSON_OBJECTAGG(k, s) OVER (PARTITION BY g) AS jo "
                   "FROM t ORDER BY g, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_json_count,
            .context = "partitioned JSON aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS ja, "
                   "JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS jo "
                   "FROM t ORDER BY g, id",
            .columns = running_columns,
            .column_count = sizeof(running_columns) / sizeof(running_columns[0]),
            .values = running_values,
            .row_count = seed_json_count,
            .context = "running JSON aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS ja, "
                   "JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS jo "
                   "FROM t ORDER BY g, id",
            .columns = moving_columns,
            .column_count = sizeof(moving_columns) / sizeof(moving_columns[0]),
            .values = moving_values,
            .row_count = seed_json_count,
            .context = "moving JSON aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS ja, "
                   "JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS jo "
                   "FROM t ORDER BY g, id",
            .columns = empty_columns,
            .column_count = sizeof(empty_columns) / sizeof(empty_columns[0]),
            .values = empty_values,
            .row_count = seed_json_count,
            .context = "empty frame JSON aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_ARRAYAGG(j) OVER (PARTITION BY g ORDER BY id "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS jj "
                   "FROM t ORDER BY g, id",
            .columns = json_value_columns,
            .column_count = sizeof(json_value_columns) / sizeof(json_value_columns[0]),
            .values = json_value_values,
            .row_count = seed_json_count,
            .context = "JSON descriptor aggregate window values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_ARRAYAGG(s) OVER w AS ja FROM t "
                   "WINDOW w AS (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING "
                   "AND CURRENT ROW) ORDER BY id",
            .columns = named_columns,
            .column_count = sizeof(named_columns) / sizeof(named_columns[0]),
            .values = named_values,
            .row_count = seed_json_count,
            .context = "named JSON aggregate window",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_json_aggregate_window_metadata(void) {
    const char *const metadata_sql = "SELECT JSON_ARRAYAGG(s) OVER () AS ja, "
                                     "JSON_OBJECTAGG(k, s) OVER () AS jo FROM t ORDER BY id "
                                     "LIMIT 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_json_values(database);
    }
    if (failures == 0) {
        rc = mylite_execute(database, metadata_sql, strlen(metadata_sql), &result);
        failures += expect_int(rc, MYLITE_OK, "JSON aggregate window metadata query");
        if (rc != MYLITE_OK) {
            fprintf(stderr, "metadata query: %s\n", mylite_errmsg(database));
        }
    }
    if (failures != 0) {
        mylite_result_free(result);
        mylite_close(database);
        return failures;
    }

    failures +=
        expect_size(mylite_result_column_count(result), metadata_column_count, "metadata count");
    failures += expect_int(
        mylite_result_column_type(result, metadata_array_column),
        MYLITE_RESULT_COLUMN_TYPE_JSON,
        "JSON_ARRAYAGG metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_array_column),
        1,
        "JSON_ARRAYAGG nullable"
    );
    failures += expect_int(
        mylite_result_column_type(result, metadata_object_column),
        MYLITE_RESULT_COLUMN_TYPE_JSON,
        "JSON_OBJECTAGG metadata type"
    );
    failures += expect_int(
        mylite_result_column_nullable(result, metadata_object_column),
        1,
        "JSON_OBJECTAGG nullable"
    );

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_json_aggregate_window_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");

    if (failures == 0) {
        failures += seed_json_values(database);
    }

    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(s) OVER () FROM t",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "group_concat as window function",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OBJECTAGG(NULL, s) OVER () FROM t",
        (struct expected_sql_error){
            .code = mysql_error_json_null_member_name,
            .sqlstate = "22032",
            .message_part = "JSON documents may not contain NULL member names.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAYAGG(1) OVER (ORDER BY 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_ARRAYAGG() without a table source supports only OVER ()",
        }
    );

    mylite_close(database);
    return failures;
}

static int seed_json_values(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE t (g INT, id INT, k VARCHAR(20), s VARCHAR(20), j JSON, b TINYINT)"
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,1,'a','alpha','{\"x\":1}',1),"
        "(1,2,'b','beta','[2]',0),"
        "(1,3,'a',NULL,NULL,NULL),"
        "(2,4,'c','carrot','{\"x\":4}',1),"
        "(3,5,'n',NULL,NULL,NULL)"
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
