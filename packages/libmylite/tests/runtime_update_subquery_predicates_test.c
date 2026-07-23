#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
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
    seeded_user_count = 5,
    seeded_name_non_null_count = 3U,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_update_table_used = 1093,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_not_supported_yet = 1235,
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

static int test_update_subquery_predicates_success(void);
static int test_update_subquery_predicates_errors(void);
static int open_seeded_database(const char *path, mylite_db **out_database);
static int seed_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_update_subquery_predicates_success();
    failures += test_update_subquery_predicates_errors();

    return failures == 0 ? 0 : 1;
}

static int test_update_subquery_predicates_success(void) {
    static const char *const in_values[] = {
        "1",
        "7",
        "2",
        "7",
        "3",
        "0",
        "4",
        "0",
        "5",
        "7",
    };
    static const char *const inner_where_values[] = {
        "1",
        "8",
        "2",
        "8",
        "3",
        "0",
        "4",
        "0",
        "5",
        "0",
    };
    static const char *const all_zero_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "0",
        "4",
        "0",
        "5",
        "0",
    };
    static const char *const not_in_no_null_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "4",
        "4",
        "4",
        "5",
        "0",
    };
    static const char *const exists_values[] = {
        "1",
        "6",
        "2",
        "6",
        "3",
        "6",
        "4",
        "6",
        "5",
        "6",
    };
    static const char *const not_exists_empty_values[] = {
        "1",
        "9",
        "2",
        "9",
        "3",
        "9",
        "4",
        "9",
        "5",
        "9",
    };
    static const char *const correlated_in_values[] = {
        "1",
        "1",
        "2",
        "1",
        "3",
        "0",
        "4",
        "0",
        "5",
        "0",
    };
    static const char *const correlated_exists_values[] = {
        "1",
        "2",
        "2",
        "2",
        "3",
        "0",
        "4",
        "0",
        "5",
        "2",
    };
    static const char *const correlated_null_safe_values[] = {
        "1",
        "3",
        "2",
        "3",
        "3",
        "0",
        "4",
        "3",
        "5",
        "0",
    };
    static const char *const order_limit_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "0",
        "4",
        "0",
        "5",
        "8",
    };
    static const char *const schema_qualified_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "11",
        "4",
        "11",
        "5",
        "0",
    };
    static const char *const string_values[] = {
        "Ann",
        "1",
        "bob",
        "0",
        "CAT",
        "1",
    };
    static const char *const persisted_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "11",
        "4",
        "11",
        "5",
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += open_seeded_database(path, &database);

    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 7 WHERE id IN (SELECT user_id FROM orders)",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = in_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "integer IN subquery update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 3);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 8 WHERE id IN ("
        "SELECT user_id FROM orders WHERE status = 'open'"
        ")",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = inner_where_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "IN subquery inner WHERE update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 2);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 3 WHERE id NOT IN (SELECT user_id FROM orders)",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = all_zero_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "NOT IN with inner NULL update",
        }
    );

    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 4 WHERE id NOT IN ("
        "SELECT user_id FROM orders WHERE user_id IS NOT NULL"
        ")",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = not_in_no_null_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "NOT IN without inner NULL update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 2);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 5 WHERE id IN (SELECT user_id FROM empty_orders)",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = all_zero_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "IN over empty subquery update",
        }
    );

    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 6 WHERE EXISTS (SELECT 1 FROM orders)",
        seeded_user_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = exists_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "EXISTS subquery update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", seeded_user_count);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 5 WHERE EXISTS (SELECT 1 FROM orders LIMIT 0)",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = all_zero_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "EXISTS LIMIT 0 update",
        }
    );

    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 9 WHERE NOT EXISTS (SELECT 1 FROM empty_orders)",
        seeded_user_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = not_exists_empty_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "NOT EXISTS empty update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", seeded_user_count);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 1 WHERE id IN ("
        "SELECT user_id FROM orders WHERE orders.group_id = users.group_id"
        ")",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = correlated_in_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "correlated IN update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 2);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 2 WHERE EXISTS ("
        "SELECT 1 FROM orders WHERE orders.user_id = users.id"
        ")",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = correlated_exists_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "correlated EXISTS update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 3);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 3 WHERE EXISTS ("
        "SELECT 1 FROM orders WHERE orders.group_id <=> users.group_id"
        ")",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = correlated_null_safe_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "correlated null-safe EXISTS update",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 3);
    failures += expect_update_ok(
        database,
        "UPDATE users SET flag = 8 "
        "WHERE id IN (SELECT user_id FROM orders) ORDER BY id DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = order_limit_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "subquery update with ORDER BY LIMIT",
        }
    );

    failures += expect_update_ok(database, "UPDATE users SET flag = 0", 1);
    failures += expect_update_ok(
        database,
        "UPDATE app.users SET flag = 11 WHERE id NOT IN ("
        "SELECT user_id FROM app.orders WHERE user_id IS NOT NULL"
        ")",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = schema_qualified_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "schema-qualified update subquery",
        }
    );

    failures += expect_update_ok(
        database,
        "UPDATE names SET flag = 1 WHERE name IN (SELECT name FROM selected_names)",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT name, flag FROM names WHERE name IS NOT NULL ORDER BY name",
            .values = string_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "string IN subquery update",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM users ORDER BY id",
            .values = persisted_values,
            .column_count = 2U,
            .row_count = seeded_user_count,
            .context = "persisted update subquery predicate rows",
        }
    );

    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "update subquery predicate preserves preamble"
    );
    remove_related_files(path);

    return failures;
}

static int test_update_subquery_predicates_errors(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += open_seeded_database(path, &database);
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE id IN (SELECT id FROM users)",
        (struct expected_sql_error){
            .code = mysql_error_update_table_used,
            .sqlstate = "HY000",
            .message_part = "You can't specify target table 'users' for update in FROM clause",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE EXISTS (SELECT 1 FROM users)",
        (struct expected_sql_error){
            .code = mysql_error_update_table_used,
            .sqlstate = "HY000",
            .message_part = "You can't specify target table 'users' for update in FROM clause",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE id IN "
        "(SELECT o.user_id FROM orders o JOIN users u ON o.user_id = u.id)",
        (struct expected_sql_error){
            .code = mysql_error_update_table_used,
            .sqlstate = "HY000",
            .message_part = "You can't specify target table 'users' for update in FROM clause",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE id IN (SELECT user_id FROM orders LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "LIMIT & IN/ALL/ANY/SOME subquery",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE id IN (SELECT missing FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE id IN (SELECT user_id FROM missing_orders)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "UPDATE users SET flag = 5 WHERE EXISTS (SELECT 1 FROM orders ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXISTS subqueries support only WHERE and LIMIT",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int open_seeded_database(const char *path, mylite_db **out_database) {
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open seeded database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", &result);
        mylite_result_free(result);
        result = NULL;
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", &result);
        mylite_result_free(result);
        result = NULL;
    }
    if (failures == 0) {
        failures += seed_tables(*out_database);
    }

    return failures;
}

static int seed_tables(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE users ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(20), "
        "group_id INT NULL, "
        "flag INT"
        ")",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE orders ("
        "id INT PRIMARY KEY, "
        "user_id INT NULL, "
        "status VARCHAR(20), "
        "group_id INT NULL"
        ")",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE empty_orders (id INT PRIMARY KEY, user_id INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE names (name VARCHAR(20), flag INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE selected_names (name VARCHAR(20))", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO users VALUES "
        "(1, 'Ann', 1, 0), "
        "(2, 'Bob', 2, 0), "
        "(3, 'Cat', 99, 0), "
        "(4, 'Don', NULL, 0), "
        "(5, 'Eve', 3, 0)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO orders VALUES "
        "(10, 1, 'open', 1), "
        "(11, 1, 'closed', NULL), "
        "(12, 2, 'open', 2), "
        "(13, NULL, 'open', 2), "
        "(14, 5, 'closed', NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO names VALUES ('Ann', 0), ('bob', 0), ('CAT', 0), (NULL, 0)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO selected_names VALUES ('ann'), ('cat'), (NULL)", &result);
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        affected_rows,
        "update affected"
    );
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "update warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
