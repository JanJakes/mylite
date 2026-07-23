#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_window_name_not_defined = 3579,
    mysql_error_window_circularity = 3580,
    mysql_error_window_inheritance = 3583,
    mysql_error_window_duplicate_name = 3591,
    seed_post_count = 7,
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

static int test_named_window_results(void);
static int test_named_window_diagnostics(void);
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

int main(void) {
    int failures = 0;

    failures += test_named_window_results();
    failures += test_named_window_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_named_window_results(void) {
    static const char *const column_rn[] = {"rn"};
    static const char *const value_one[] = {"1"};
    static const char *const columns_id_rn[] = {"id", "rn"};
    static const char *const values_partitioned[] = {
        "7",
        "1",
        "6",
        "2",
        "2",
        "1",
        "3",
        "2",
        "1",
        "3",
        "5",
        "1",
        "4",
        "2",
    };
    static const char *const columns_first[] = {"id", "first_title"};
    static const char *const values_first[] = {
        "4",
        "d",
        "6",
        "d",
        "7",
        "d",
        "5",
        "d",
        "1",
        "d",
        "2",
        "d",
        "3",
        "d",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_NUMBER() OVER w AS rn WINDOW w AS ()",
            .columns = column_rn,
            .column_count = 1U,
            .values = value_one,
            .row_count = 1U,
            .context = "no-source empty named window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts "
                   "WINDOW w AS (PARTITION BY author_id ORDER BY created_at DESC) "
                   "ORDER BY author_id, created_at DESC, id",
            .columns = columns_id_rn,
            .column_count = 2U,
            .values = values_partitioned,
            .row_count = seed_post_count,
            .context = "direct named window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER (base ORDER BY created_at DESC) AS rn "
                   "FROM posts WINDOW base AS (PARTITION BY author_id) "
                   "ORDER BY author_id, created_at DESC, id",
            .columns = columns_id_rn,
            .column_count = 2U,
            .values = values_partitioned,
            .row_count = seed_post_count,
            .context = "extended named window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts "
                   "WINDOW w1 AS (w2), w2 AS (PARTITION BY author_id ORDER BY created_at DESC) "
                   "ORDER BY author_id, created_at DESC, id",
            .columns = columns_id_rn,
            .column_count = 2U,
            .values = values_partitioned,
            .row_count = seed_post_count,
            .context = "forward referenced named window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts "
                   "WINDOW W AS (PARTITION BY author_id ORDER BY created_at DESC) "
                   "ORDER BY author_id, created_at DESC, id",
            .columns = columns_id_rn,
            .column_count = 2U,
            .values = values_partitioned,
            .row_count = seed_post_count,
            .context = "case-insensitive named window",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FIRST_VALUE(title) OVER w AS first_title FROM posts "
                   "WINDOW w AS (ORDER BY created_at "
                   "ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "
                   "ORDER BY created_at, id",
            .columns = columns_first,
            .column_count = 2U,
            .values = values_first,
            .row_count = seed_post_count,
            .context = "named window frame",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_named_window_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");

    if (failures == 0) {
        failures += seed_posts(database);
    }

    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER missing_window AS rn FROM posts",
        (struct expected_sql_error){
            .code = mysql_error_window_name_not_defined,
            .sqlstate = "HY000",
            .message_part = "Window name 'missing_window' is not defined.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts WINDOW w AS (missing_window)",
        (struct expected_sql_error){
            .code = mysql_error_window_name_not_defined,
            .sqlstate = "HY000",
            .message_part = "Window name 'missing_window' is not defined.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts WINDOW w AS (), W AS ()",
        (struct expected_sql_error){
            .code = mysql_error_window_duplicate_name,
            .sqlstate = "HY000",
            .message_part = "Window 'w' is defined twice.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts WINDOW w1 AS (w2), w2 AS (w1)",
        (struct expected_sql_error){
            .code = mysql_error_window_circularity,
            .sqlstate = "HY000",
            .message_part = "There is a circularity in the window dependency graph.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER (w ORDER BY id) AS rn FROM posts "
        "WINDOW w AS (ORDER BY created_at)",
        (struct expected_sql_error){
            .code = mysql_error_window_inheritance,
            .sqlstate = "HY000",
            .message_part = "Window '<unnamed window>' cannot inherit 'w' since both contain an "
                            "ORDER BY clause.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts "
        "WINDOW w1 AS (w2 ORDER BY id), w2 AS (ORDER BY created_at)",
        (struct expected_sql_error){
            .code = mysql_error_window_inheritance,
            .sqlstate = "HY000",
            .message_part =
                "Window 'w1' cannot inherit 'w2' since both contain an ORDER BY clause.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id + 0 AS id FROM posts WINDOW w AS (ORDER BY missing)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports WINDOW clauses only when every named window definition is "
                            "used by a supported projection window function",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, ROW_NUMBER() OVER () AS rn FROM posts WINDOW w AS (ORDER BY missing)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "supports WINDOW clauses only when every named window definition is "
                            "used by a supported projection window function",
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
        "CREATE TABLE posts(id INT, author_id INT, created_at INT, title VARCHAR(20))"
    );
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1,10,100,'a'),"
        "(2,10,200,'b'),"
        "(3,10,200,'c'),"
        "(4,20,NULL,'d'),"
        "(5,20,50,'e'),"
        "(6,NULL,10,'f'),"
        "(7,NULL,20,'g')"
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

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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
    return mylite_test_expect_text(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}
