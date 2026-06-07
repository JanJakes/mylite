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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_column = 1054,
    mysql_error_table_does_not_exist = 1146,
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
    size_t warning_count;
    const char *context;
};

static int test_exists_values_and_persistence(void);
static int test_exists_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_exists_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
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
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_exists_values_and_persistence();
    failures += test_exists_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_exists_values_and_persistence(void) {
    static const char *const id_column[] = {"id"};
    static const char *const all_user_ids[] = {"1", "2", "3", "4"};
    static const char *const matched_user_ids[] = {"1", "2"};
    static const char *const unmatched_user_ids[] = {"3", "4"};
    static const char *const closed_user_id[] = {"1"};
    static const char *const null_safe_user_ids[] = {"1", "2", "4"};
    static const char *const joined_not_exists_user_ids[] = {"2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "values", path, sizeof(path));
    failures += seed_exists_tables(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE EXISTS (SELECT * FROM orders) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = 4U,
            .context = "uncorrelated exists over nonempty table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE NOT EXISTS (SELECT * FROM empty_orders) "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = 4U,
            .context = "not exists over empty table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT NULL FROM orders AS o WHERE o.user_id = u.id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "correlated equality exists",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE NOT EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = u.id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = unmatched_user_ids,
            .row_count = 2U,
            .context = "correlated equality not exists",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT o.user_id, o.status FROM orders AS o "
                   "WHERE o.user_id = u.id AND o.status = 'closed') ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = closed_user_id,
            .row_count = 1U,
            .context = "correlated equality with inner literal predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id <=> u.group_id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = null_safe_user_ids,
            .row_count = 3U,
            .context = "correlated null safe equality",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = u.id LIMIT 0) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "limit zero makes exists false",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.status = 'closed' LIMIT 0) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "limit zero skips inner literal predicate parameters",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = u.id LIMIT 1) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "limit one keeps exists true",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE EXISTS (SELECT 1) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = 4U,
            .context = "tableless exists is true",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE EXISTS (SELECT * FROM DUAL) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = 4U,
            .context = "dual exists is true",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE id = u.id) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "inner unqualified names resolve before outer names",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = group_id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "unqualified missing inner names fall back to outer names",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.users "
                   "WHERE EXISTS (SELECT 1 FROM app.orders) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = 4U,
            .context = "schema qualified outer and inner tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE NOT EXISTS ("
                   "SELECT 1 FROM term_relationships AS tr "
                   "INNER JOIN term_taxonomy AS tt "
                   "ON tt.term_taxonomy_id = tr.term_taxonomy_id "
                   "WHERE tt.taxonomy = 'post_format' AND tr.object_id = u.id"
                   ") ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = joined_not_exists_user_ids,
            .row_count = 2U,
            .context = "joined inner not exists predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SQL_CALC_FOUND_ROWS u.id FROM users AS u "
                   "WHERE NOT EXISTS ("
                   "SELECT 1 FROM term_relationships AS tr "
                   "INNER JOIN term_taxonomy AS tt "
                   "ON tt.term_taxonomy_id = tr.term_taxonomy_id "
                   "WHERE tt.taxonomy = 'post_format' AND tr.object_id = u.id"
                   ") GROUP BY u.id ORDER BY u.id LIMIT 0, 5",
            .columns = id_column,
            .column_count = 1U,
            .values = joined_not_exists_user_ids,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "grouped joined inner not exists predicate",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen exists database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u "
                   "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = u.id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "exists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_exists_diagnostics(void) {
    static const char *const id_column[] = {"id"};
    static const char *const remaining_users[] = {"3", "4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "no-schema") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open no selected schema");
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE EXISTS (SELECT 1 FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    mylite_close(database);
    remove_related_files(path);

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += seed_exists_tables(database);
    failures += execute_error(
        database,
        "SELECT id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM missing_orders AS m WHERE m.user_id = u.id)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users AS u "
        "WHERE EXISTS (SELECT missing_value FROM orders AS o WHERE o.user_id = u.id)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_value' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.missing_value = u.id)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'o.missing_value' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = u.missing_value)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'u.missing_value' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXISTS (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_ok(
        database,
        "DELETE FROM users "
        "WHERE EXISTS (SELECT 1 FROM orders AS o WHERE o.user_id = users.id)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = remaining_users,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "delete exists subquery remaining users",
        }
    );
    failures += execute_error(
        database,
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders AS o ORDER BY o.id)",
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

static int seed_exists_tables(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE users ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(20), "
        "group_id INT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE orders ("
        "id INT PRIMARY KEY, "
        "user_id INT NULL, "
        "status VARCHAR(20), "
        "total INT"
        ")",
        NULL
    );
    failures += execute_ok(database, "CREATE TABLE empty_orders (id INT PRIMARY KEY)", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE term_relationships ("
        "object_id INT, "
        "term_taxonomy_id INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE term_taxonomy ("
        "term_taxonomy_id INT PRIMARY KEY, "
        "taxonomy VARCHAR(20)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO users VALUES "
        "(1, 'Ann', 1), "
        "(2, 'Bob', 2), "
        "(3, 'Cat', 99), "
        "(4, 'Don', NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO orders VALUES "
        "(10, 1, 'open', 50), "
        "(11, 1, 'closed', 20), "
        "(12, 2, 'open', 30), "
        "(13, NULL, 'open', 99)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO term_taxonomy VALUES "
        "(100, 'post_format'), "
        "(101, 'category')",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO term_relationships VALUES "
        "(1, 100), "
        "(2, 101), "
        "(4, 100)",
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-exists-subquery-%s-%d.mylite",
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
