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
    mysql_error_no_tables_used = 1096,
    mysql_error_operand_should_contain_one_column = 1241,
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

static int test_scalar_subquery_projection_values(void);
static int test_scalar_subquery_concat_values(void);
static int test_scalar_subquery_no_selected_schema(void);
static int test_scalar_subquery_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
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
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_subquery_projection_values();
    failures += test_scalar_subquery_concat_values();
    failures += test_scalar_subquery_no_selected_schema();
    failures += test_scalar_subquery_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_subquery_projection_values(void) {
    static const char *const columns[] = {
        "(SELECT DATABASE())",
        "CONCAT('test-', (SELECT DATABASE()))",
        "((SELECT DATABASE()))",
        "(SELECT NULL)",
        "(SELECT 1)",
        "(SELECT +2)",
        "(SELECT -3)",
        "(SELECT TRUE)",
        "(SELECT FALSE)",
        "(SELECT @@warning_count)",
        "@@warning_count",
    };
    static const char *const values[] = {
        "app",
        "test-app",
        "app",
        NULL,
        "1",
        "2",
        "-3",
        "1",
        "0",
        "0",
        "0",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const row_count_values[] = {"-1", "0"};
    static const char *const dual_columns[] = {
        "(SELECT DATABASE() FROM DUAL)",
        "(SELECT 1 FROM DUAL)",
    };
    static const char *const dual_values[] = {"app", "1"};
    static const char *const label_columns[] = {"(SELECT DATABASE())", "named"};
    static const char *const label_values[] = {"app", "app"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "values", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (SELECT DATABASE()), CONCAT('test-', (SELECT DATABASE())), "
                   "((SELECT DATABASE())), (SELECT NULL), (SELECT 1), (SELECT +2), "
                   "(SELECT -3), (SELECT TRUE), (SELECT FALSE), (SELECT @@warning_count), "
                   "@@warning_count",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "scalar subquery projection values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_count_columns,
            .column_count = sizeof(row_count_columns) / sizeof(row_count_columns[0]),
            .values = row_count_values,
            .row_count = 1U,
            .context = "scalar subquery row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (SELECT DATABASE() FROM DUAL), (SELECT 1 FROM DUAL)",
            .columns = dual_columns,
            .column_count = sizeof(dual_columns) / sizeof(dual_columns[0]),
            .values = dual_values,
            .row_count = 1U,
            .context = "scalar subquery dual values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (SELECT DATABASE()), (SELECT DATABASE()) AS named",
            .columns = label_columns,
            .column_count = sizeof(label_columns) / sizeof(label_columns[0]),
            .values = label_values,
            .row_count = 1U,
            .context = "scalar subquery labels",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_subquery_concat_values(void) {
    static const char *const columns[] = {"id", "merged"};
    static const char *const values[] = {"1", "a-app", "2", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "concat", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t (id INT, v VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a'), (2, NULL)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT(v, '-', (SELECT DATABASE())) AS merged FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .context = "row-scalar concat scalar subquery",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_subquery_no_selected_schema(void) {
    static const char *const columns[] = {
        "(SELECT DATABASE())",
        "CONCAT('x', (SELECT DATABASE()))",
    };
    static const char *const values[] = {NULL, NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "no-schema") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open no selected schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (SELECT DATABASE()), CONCAT('x', (SELECT DATABASE()))",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "scalar subquery no selected schema",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_subquery_diagnostics(void) {
    static const char *const row_scalar_columns[] = {"CONCAT(id, '')", "(SELECT DATABASE())"};
    static const char *const row_scalar_values[] = {"1", "app"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t (id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);
    failures += execute_error(
        database,
        "SELECT (SELECT DATABASE(), SCHEMA())",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT (SELECT * FROM DUAL)",
        (struct expected_sql_error){
            .code = mysql_error_no_tables_used,
            .sqlstate = "HY000",
            .message_part = "No tables used",
        }
    );
    failures += execute_error(
        database,
        "SELECT (SELECT id FROM t)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar subquery does not support table-backed SELECT",
        }
    );
    failures += execute_error(
        database,
        "SELECT (SELECT CONCAT('a', 'b'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar subquery supports only DATABASE(), SCHEMA(), integer, boolean",
        }
    );
    failures += execute_error(
        database,
        "SELECT (SELECT ST_X(Point(1, 2))) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONCAT(id, ''), (SELECT DATABASE()) FROM t",
            .columns = row_scalar_columns,
            .column_count = 2U,
            .values = row_scalar_values,
            .row_count = 1U,
            .context = "row scalar scalar-subquery projection",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-scalar-subquery-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text(actual, expected, context);
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
