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
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
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

static int test_order_by_field_success(void);
static int test_order_by_field_diagnostics(void);
static int setup_order_table(mylite_db *database);
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

    failures += test_order_by_field_success();
    failures += test_order_by_field_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_order_by_field_success(void) {
    static const char *const id_column[] = {"id"};
    static const char *const columns_rank[] = {"id", "field_rank"};
    static const char *const columns_pos[] = {"pos"};
    static const char *const values_ascending[] = {"2", "1", "3"};
    static const char *const values_descending[] = {"3", "1", "2"};
    static const char *const values_integer[] = {"6", "4", "3", "2"};
    static const char *const values_where_limit[] = {"3", "4"};
    static const char *const values_null_nomatch[] = {"5", "0", "4", "1", "3", "2"};
    static const char *const values_row_scalar[] = {"1", "2", "3"};
    static const char *const values_joined_field[] = {"2", "1", "3"};
    static const char *const values_joined_numeric[] = {"2", "3", "1"};
    static const char *const values_joined_cast_desc[] = {"1", "3", "2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "success", path, sizeof(path));
    failures += setup_order_table(database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(name, 'User 0000019', 'User 0000018', "
                   "'User 0000020')",
            .columns = id_column,
            .column_count = 1U,
            .values = values_ascending,
            .row_count = 3U,
            .context = "string FIELD ascending",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(name, 'User 0000019', 'User 0000018', "
                   "'User 0000020') DESC",
            .columns = id_column,
            .column_count = 1U,
            .values = values_descending,
            .row_count = 3U,
            .context = "string FIELD descending",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id IN (1,2,3) "
                   "ORDER BY (FIELD(name, 'user 0000019', 'USER 0000018', "
                   "'user 0000020')) DESC",
            .columns = id_column,
            .column_count = 1U,
            .values = values_descending,
            .row_count = 3U,
            .context = "case-insensitive parenthesized FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id IN (2,3,4,6) "
                   "ORDER BY FIELD(n, TRUE, 21, 20, 19)",
            .columns = id_column,
            .column_count = 1U,
            .values = values_integer,
            .row_count = 4U,
            .context = "integer FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id BETWEEN 2 AND 4 "
                   "ORDER BY FIELD(name, 'User 0000020', 'Other', 'User 0000019') "
                   "LIMIT 2",
            .columns = id_column,
            .column_count = 1U,
            .values = values_where_limit,
            .row_count = 2U,
            .context = "FIELD order with where and limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.t AS o WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(o.name, 'User 0000019', 'User 0000018', "
                   "'User 0000020')",
            .columns = id_column,
            .column_count = 1U,
            .values = values_ascending,
            .row_count = 3U,
            .context = "qualified FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(t.name, 'User 0000019', 'User 0000018', "
                   "'User 0000020')",
            .columns = id_column,
            .column_count = 1U,
            .values = values_ascending,
            .row_count = 3U,
            .context = "table-qualified FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.t WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(app.t.name, 'User 0000019', 'User 0000018', "
                   "'User 0000020')",
            .columns = id_column,
            .column_count = 1U,
            .values = values_ascending,
            .row_count = 3U,
            .context = "schema-qualified FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FIELD(name, 'Other', 'User 0000020') AS field_rank "
                   "FROM t WHERE id IN (3,4,5) "
                   "ORDER BY FIELD(name, 'Other', 'User 0000020') ASC",
            .columns = columns_rank,
            .column_count = 2U,
            .values = values_null_nomatch,
            .row_count = 3U,
            .context = "FIELD order null and no-match ranks",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIELD(name, 'User 0000019', 'User 0000018', 'User 0000020') "
                   "AS pos FROM t WHERE id IN (1,2,3) "
                   "ORDER BY FIELD(name, 'User 0000019', 'User 0000018', 'User 0000020')",
            .columns = columns_pos,
            .column_count = 1U,
            .values = values_row_scalar,
            .row_count = 3U,
            .context = "row-scalar FIELD projection ordered by FIELD",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT l.id FROM t AS l JOIN t AS r ON l.id = r.id "
                   "WHERE l.id IN (1,2,3) "
                   "ORDER BY FIELD(l.name, 'User 0000019', 'User 0000018', "
                   "'User 0000020')",
            .columns = id_column,
            .column_count = 1U,
            .values = values_joined_field,
            .row_count = 3U,
            .context = "joined FIELD order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.id FROM t JOIN meta ON t.id = meta.user_id "
                   "WHERE meta.key_name = 'age' ORDER BY meta.meta_value + 0 ASC",
            .columns = id_column,
            .column_count = 1U,
            .values = values_joined_numeric,
            .row_count = 3U,
            .context = "joined numeric string order expression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.id FROM t JOIN meta ON t.id = meta.user_id "
                   "WHERE meta.key_name = 'age' ORDER BY CAST(meta.meta_value AS CHAR) DESC",
            .columns = id_column,
            .column_count = 1U,
            .values = values_joined_cast_desc,
            .row_count = 3U,
            .context = "joined cast string order expression",
        }
    );
    failures += execute_ok(database, "SET sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT t.id FROM t JOIN meta ON t.id = meta.user_id "
                   "WHERE meta.key_name = 'age' ORDER BY meta.meta_value + 0 ASC",
            .columns = id_column,
            .column_count = 1U,
            .values = values_joined_numeric,
            .row_count = 3U,
            .context = "relaxed distinct joined numeric string order expression",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_order_by_field_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += setup_order_table(database);

    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY FIELD(missing, 'x')",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY FIELD(name, 'Other'), id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT ORDER BY FIELD() supports only one order key",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY FIELD(name, name)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT ORDER BY FIELD() supports only literal candidate values",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t ORDER BY FIELD(name, 0, 'Other')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() does not support mixed string and numeric arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT n FROM t ORDER BY FIELD(n, TRUE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT ORDER BY supports only descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT t.id FROM t JOIN meta ON t.id = meta.user_id "
        "WHERE meta.key_name = 'age' ORDER BY CAST(meta.meta_value AS SIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT ORDER BY supports only descriptor columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_order_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE TABLE t(id INT, name VARCHAR(32), n INT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'User 0000018', 18), "
        "(2, 'User 0000019', 19), "
        "(3, 'User 0000020', 20), "
        "(4, 'Other', 21), "
        "(5, NULL, NULL), "
        "(6, 'One', 1)",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE meta(user_id INT, key_name VARCHAR(16), meta_value VARCHAR(16))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO meta VALUES "
        "(1, 'age', '30'), "
        "(2, 'age', '10'), "
        "(3, 'age', '20'), "
        "(1, 'other', 'x')",
        NULL
    );
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
        "/tmp/mylite-order-by-field-function-%s-%d.mylite",
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected [%s] to contain [%s]\n", context, actual, needle);
        return 1;
    }
    return 0;
}
