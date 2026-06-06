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
    mysql_error_not_supported_yet = 1235,
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

static int test_in_subquery_values_and_persistence(void);
static int test_in_subquery_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_in_subquery_tables(mylite_db *database);
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

    failures += test_in_subquery_values_and_persistence();
    failures += test_in_subquery_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_in_subquery_values_and_persistence(void) {
    enum { all_user_id_count = 5 };

    static const char *const id_column[] = {"id"};
    static const char *const name_column[] = {"name"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const matched_user_ids[] = {"1", "2"};
    static const char *const unmatched_user_ids[] = {"3", "4", "5"};
    static const char *const all_user_ids[] = {"1", "2", "3", "4", "5"};
    static const char *const string_matches[] = {"Ann", "Cat"};
    static const char *const joined_user_ids[] = {"1", "2"};
    static const char *const left_joined_user_ids[] = {"1", "3"};
    static const char *const left_joined_not_in_user_ids[] = {"2", "4", "5"};
    static const char *const joined_left_joined_not_in_user_ids[] = {"2"};
    static const char *const scalar_count_subquery_matches[] = {"1", "3"};
    static const char *const joined_scalar_count_subquery_matches[] = {"1"};
    static const char *const scalar_subquery_count[] = {"3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "values", path, sizeof(path));
    failures += seed_in_subquery_tables(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN "
                   "(SELECT user_id FROM orders WHERE user_id IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "integer IN subquery with inner predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN "
                   "(SELECT DISTINCT user_id FROM orders WHERE user_id IS NOT NULL) "
                   "ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "integer IN distinct subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id NOT IN "
                   "(SELECT user_id FROM orders WHERE user_id IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = unmatched_user_ids,
            .row_count = 3U,
            .context = "integer NOT IN subquery without inner nulls",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id NOT IN "
                   "(SELECT user_id FROM orders) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "NOT IN subquery with inner null filters every row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN (SELECT id FROM empty_orders) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "IN subquery over empty table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id NOT IN "
                   "(SELECT id FROM empty_orders) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = all_user_ids,
            .row_count = all_user_id_count,
            .context = "NOT IN subquery over empty table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT name FROM users WHERE name IN "
                   "(SELECT name FROM selected_names) ORDER BY id",
            .columns = name_column,
            .column_count = 1U,
            .values = string_matches,
            .row_count = 2U,
            .context = "string IN subquery uses registered ASCII collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE u.id IN "
                   "(SELECT o.user_id FROM orders AS o WHERE o.user_id = u.id) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "correlated equality IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE u.group_id IN "
                   "(SELECT o.user_id FROM orders AS o WHERE o.user_id <=> u.group_id) "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "correlated null-safe equality preserves IN null semantics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM app.users WHERE id IN "
                   "(SELECT user_id FROM app.orders WHERE user_id IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "schema-qualified outer and inner IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users u JOIN orders o ON u.id = o.user_id "
                   "WHERE o.status = 'open' AND u.id IN "
                   "(SELECT user_id FROM orders WHERE user_id IS NOT NULL) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = joined_user_ids,
            .row_count = 2U,
            .context = "outer joined source IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN "
                   "(SELECT DISTINCT o.user_id FROM orders o "
                   "JOIN users u2 ON o.user_id = u2.id "
                   "WHERE u2.name IN ('Ann', 'Bob')) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "inner joined source distinct IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN "
                   "(SELECT ut.user_id FROM user_terms ut "
                   "LEFT JOIN term_taxonomy tt "
                   "ON ut.term_taxonomy_id = tt.term_taxonomy_id "
                   "WHERE tt.term_id IN (9)) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = left_joined_user_ids,
            .row_count = 2U,
            .context = "left joined source IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id NOT IN "
                   "(SELECT ut.user_id FROM user_terms ut "
                   "LEFT JOIN term_taxonomy tt "
                   "ON ut.term_taxonomy_id = tt.term_taxonomy_id "
                   "WHERE tt.term_id IN (9)) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = left_joined_not_in_user_ids,
            .row_count = 3U,
            .context = "left joined source NOT IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users u JOIN orders o ON u.id = o.user_id "
                   "WHERE o.status = 'open' AND u.id NOT IN "
                   "(SELECT ut.user_id FROM user_terms ut "
                   "LEFT JOIN term_taxonomy tt "
                   "ON ut.term_taxonomy_id = tt.term_taxonomy_id "
                   "WHERE tt.term_id IN (9)) ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = joined_left_joined_not_in_user_ids,
            .row_count = 1U,
            .context = "outer joined source with left joined NOT IN subquery",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE "
                   "(SELECT o.status FROM orders AS o "
                   "WHERE o.user_id = u.id AND o.status = 'open') NOT IN ('closed') "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "scalar subquery NOT IN literal list",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM orders, users AS u "
                   "WHERE u.id = orders.user_id AND "
                   "(orders.status IN ('open') OR "
                   "(orders.status = 'closed' AND "
                   "(SELECT name FROM users WHERE id = u.id) IN ('ANN')))",
            .columns = count_column,
            .column_count = 1U,
            .values = scalar_subquery_count,
            .row_count = 1U,
            .context = "scalar subquery IN literal list in joined COUNT predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users AS u WHERE "
                   "(SELECT COUNT(1) FROM user_terms "
                   "WHERE term_taxonomy_id IN (100,101) AND user_id = u.id) = 1 "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = scalar_count_subquery_matches,
            .row_count = 2U,
            .context = "correlated scalar COUNT subquery comparison",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT u.id FROM users u JOIN orders o ON u.id = o.user_id "
                   "WHERE o.status = 'open' AND "
                   "(SELECT COUNT(*) FROM user_terms "
                   "WHERE term_taxonomy_id IN (100,101) AND user_id = u.id) >= 1 "
                   "ORDER BY u.id",
            .columns = id_column,
            .column_count = 1U,
            .values = joined_scalar_count_subquery_matches,
            .row_count = 1U,
            .context = "joined outer source scalar COUNT subquery comparison",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen IN subquery database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM users WHERE id IN "
                   "(SELECT user_id FROM orders WHERE user_id IS NOT NULL) ORDER BY id",
            .columns = id_column,
            .column_count = 1U,
            .values = matched_user_ids,
            .row_count = 2U,
            .context = "IN subquery after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_in_subquery_diagnostics(void) {
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
        "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    mylite_close(database);
    remove_related_files(path);

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += seed_in_subquery_tables(database);
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT user_id FROM missing_orders)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT missing_value FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_value' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN "
        "(SELECT user_id FROM orders WHERE missing_value = 1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_value' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users AS u WHERE id IN "
        "(SELECT user_id FROM orders AS o WHERE o.user_id = u.missing_value)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'u.missing_value' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support one descriptor table source",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT * FROM single_values)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support one explicit descriptor select column",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT user_id, status FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "LIMIT & IN/ALL/ANY/SOME subquery",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN "
        "(SELECT user_id FROM orders ORDER BY user_id LIMIT 1)",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "LIMIT & IN/ALL/ANY/SOME subquery",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM users WHERE id IN (SELECT user_id FROM orders ORDER BY user_id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries support only WHERE",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM users WHERE id IN (SELECT user_id FROM orders)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "IN subqueries are supported only in SELECT WHERE",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM users WHERE (SELECT name FROM selected_names) IN ('Ann')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar subquery IN predicates are supported only in SELECT WHERE",
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

static int seed_in_subquery_tables(mylite_db *database) {
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
    failures += execute_ok(database, "CREATE TABLE single_values (id INT PRIMARY KEY)", NULL);
    failures += execute_ok(database, "CREATE TABLE selected_names (name VARCHAR(20))", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE user_terms ("
        "id INT PRIMARY KEY, "
        "user_id INT NOT NULL, "
        "term_taxonomy_id INT NOT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE term_taxonomy ("
        "term_taxonomy_id INT PRIMARY KEY, "
        "term_id INT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO users VALUES "
        "(1, 'Ann', 1), "
        "(2, 'Bob', 2), "
        "(3, 'Cat', 99), "
        "(4, 'Don', NULL), "
        "(5, 'Eve', 5)",
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
    failures += execute_ok(database, "INSERT INTO single_values VALUES (2)", NULL);
    failures += execute_ok(database, "INSERT INTO selected_names VALUES ('ann'), ('CAT')", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO user_terms VALUES "
        "(1, 1, 100), "
        "(2, 2, 999), "
        "(3, 3, 101), "
        "(4, 4, 102)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO term_taxonomy VALUES "
        "(100, 9), "
        "(101, 9), "
        "(102, NULL)",
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-in-subquery-%s-%d.mylite",
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
