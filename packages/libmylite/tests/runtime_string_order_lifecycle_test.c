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
    five_row_update_count = 5,
    update_create_sql_capacity = 128,
    update_insert_sql_capacity = 512,
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

static int test_string_order_selects(void);
static int test_string_order_dml_and_persistence(void);
static int test_string_order_insert_select_source(void);
static int test_independent_string_order_handles(void);
static int test_string_order_diagnostics(void);
static int populate_strings(mylite_db *database);
static int populate_update_rows(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query expected);
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

    failures += test_string_order_selects();
    failures += test_string_order_dml_and_persistence();
    failures += test_string_order_insert_select_source();
    failures += test_independent_string_order_handles();
    failures += test_string_order_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_string_order_selects(void) {
    static const char *const varchar_ascending[] = {
        "1",
        NULL,
        "3",
        "A",
        "5",
        "aa",
        "7",
        "abc  ",
        "2",
        "b",
    };
    static const char *const varchar_descending[] = {
        "6",
        "d",
        "4",
        "c",
        "2",
        "b",
        "7",
        "abc  ",
    };
    static const char *const text_ascending[] = {
        "1",
        NULL,
        "3",
        "A",
        "5",
        "aa",
        "7",
        "abc  ",
    };
    static const char *const char_ascending[] = {"1", NULL, "3", "A", "5", "aa", "7", "abc"};
    static const char *const alias_order[] = {NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "selects", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k FROM strings ORDER BY k LIMIT 5",
            .values = varchar_ascending,
            .column_count = 2U,
            .row_count = (size_t)five_row_update_count,
            .context = "varchar default ascending string order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k FROM strings ORDER BY k DESC LIMIT 4",
            .values = varchar_descending,
            .column_count = 2U,
            .row_count = 4U,
            .context = "varchar descending string order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM strings ORDER BY t ASC LIMIT 4",
            .values = text_ascending,
            .column_count = 2U,
            .row_count = 4U,
            .context = "text ascending string order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c FROM strings ORDER BY c LIMIT 4",
            .values = char_ascending,
            .column_count = 2U,
            .row_count = 4U,
            .context = "char canonical string order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT k AS sort_key FROM strings ORDER BY sort_key LIMIT 1",
            .values = alias_order,
            .column_count = 1U,
            .row_count = 1U,
            .context = "string order by select alias",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_order_dml_and_persistence(void) {
    static const char *const after_varchar_order_update[] = {
        "1",
        "hit",
        "2",
        "two",
        "3",
        "hit",
        "4",
        "four",
        "5",
        "five",
    };
    static const char *const after_desc_order_update[] = {
        "1",
        "hit",
        "2",
        "desc",
        "3",
        "hit",
        "4",
        "desc",
        "5",
        "five",
    };
    static const char *const after_order_without_limit[] = {
        "1",
        "all",
        "2",
        "all",
        "3",
        "all",
        "4",
        "all",
        "5",
        "all",
    };
    static const char *const after_wp_update[] = {
        "a",
        "x",
        "b",
        "old",
        "c",
        "old",
    };
    static const char *const after_order_delete[] = {"2", "b", "4", "c", "5", "aa"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "dml", path, sizeof(path));
    failures += populate_update_rows(database, "upd");
    failures +=
        expect_dml_ok(database, "UPDATE upd SET v = 'hit' WHERE v <> 'skip' ORDER BY k LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = after_varchar_order_update,
            .column_count = 2U,
            .row_count = (size_t)five_row_update_count,
            .context = "ordered limited update by varchar key",
        }
    );
    failures += expect_dml_ok(database, "UPDATE upd SET v = 'desc' ORDER BY k DESC LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = after_desc_order_update,
            .column_count = 2U,
            .row_count = (size_t)five_row_update_count,
            .context = "ordered limited update by varchar key descending",
        }
    );
    failures +=
        expect_dml_ok(database, "UPDATE upd SET v = 'all' ORDER BY v", five_row_update_count);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = after_order_without_limit,
            .column_count = 2U,
            .row_count = (size_t)five_row_update_count,
            .context = "ordered update without limit accepted",
        }
    );
    failures += execute_ok(database, "CREATE TABLE wp_update (k VARCHAR(191), v LONGTEXT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO wp_update VALUES ('b', 'old'), ('a', 'old'), ('c', 'old')",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "UPDATE wp_update SET v = 'x' WHERE k = 'a' ORDER BY k LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT k, v FROM wp_update ORDER BY k",
            .values = after_wp_update,
            .column_count = 2U,
            .row_count = 3U,
            .context = "wp shaped ordered limited update without explicit id",
        }
    );
    failures += populate_update_rows(database, "del");
    failures += expect_dml_ok(database, "DELETE FROM del WHERE id <> 999 ORDER BY k LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k FROM del ORDER BY id",
            .values = after_order_delete,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ordered limited delete by varchar key",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string order file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = after_order_without_limit,
            .column_count = 2U,
            .row_count = (size_t)five_row_update_count,
            .context = "ordered string update persisted after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT k, v FROM wp_update ORDER BY k",
            .values = after_wp_update,
            .column_count = 2U,
            .row_count = 3U,
            .context = "wp shaped ordered update persisted after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_order_insert_select_source(void) {
    static const char *const inserted_rows[] = {"1", NULL, "3", "A", "5", "aa"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "insert-select", path, sizeof(path));
    failures += populate_strings(database);
    failures += execute_ok(database, "CREATE TABLE copied (id INT, k VARCHAR(10))", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO copied SELECT id, k FROM strings ORDER BY k LIMIT 3",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, k FROM copied ORDER BY k",
            .values = inserted_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "insert select source string order",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_string_order_handles(void) {
    static const char *const first_rows[] = {"1", "first", "2", "two", "3", "three"};
    static const char *const second_rows[] = {"1", "one", "2", "two", "3", "three"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += open_app_database(&first, "first", first_path, sizeof(first_path));
    failures += open_app_database(&second, "second", second_path, sizeof(second_path));
    failures +=
        execute_ok(first, "CREATE TABLE rows_t (id INT, k VARCHAR(10), v VARCHAR(10))", NULL);
    failures += execute_ok(
        first,
        "INSERT INTO rows_t VALUES (1, 'a', 'one'), (2, 'b', 'two'), (3, 'c', 'three')",
        NULL
    );
    failures +=
        execute_ok(second, "CREATE TABLE rows_t (id INT, k VARCHAR(10), v VARCHAR(10))", NULL);
    failures += execute_ok(
        second,
        "INSERT INTO rows_t VALUES (1, 'a', 'one'), (2, 'b', 'two'), (3, 'c', 'three')",
        NULL
    );
    failures += expect_dml_ok(first, "UPDATE rows_t SET v = 'first' ORDER BY k LIMIT 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM rows_t ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "first handle ordered update",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM rows_t ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "second handle remains independent",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_string_order_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE diag ("
        "id INT, b VARBINARY(4), f DOUBLE, d DECIMAL(5,2), "
        "e ENUM('a','b'), s SET('a','b'), j JSON)",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY f",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY d",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY e",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY s",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY does not yet support SET columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY j",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int populate_strings(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE strings (id INT, k VARCHAR(10), c CHAR(5), t TEXT, v VARCHAR(10))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, NULL, NULL, NULL, 'one'), "
        "(2, 'b', 'b', 'b', 'two'), "
        "(3, 'A', 'A', 'A', 'three'), "
        "(4, 'c', 'c', 'c', 'four'), "
        "(5, 'aa', 'aa', 'aa', 'five'), "
        "(6, 'd', 'd', 'd', 'six'), "
        "(7, 'abc  ', 'abc  ', 'abc  ', 'seven')",
        NULL
    );
    return failures;
}

static int populate_update_rows(mylite_db *database, const char *table_name) {
    char create_sql[update_create_sql_capacity];
    char insert_sql[update_insert_sql_capacity];
    int written = snprintf(
        create_sql,
        sizeof(create_sql),
        "CREATE TABLE %s (id INT, k VARCHAR(10), v VARCHAR(10))",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(create_sql)) {
        return 1;
    }
    written = snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO %s VALUES "
        "(1, NULL, 'one'), (2, 'b', 'two'), (3, 'a', 'three'), "
        "(4, 'c', 'four'), (5, 'aa', 'five')",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(insert_sql)) {
        return 1;
    }
    return execute_ok(database, create_sql, NULL) + execute_ok(database, insert_sql, NULL);
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

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query expected) {
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
        "/tmp/mylite-string-order-lifecycle-%s-%d.mylite",
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
