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
    ordered_people_row_count = 5,
    mysql_error_invalid_collation_charset = 1253,
    mysql_error_unknown_collation = 1273,
    mysql_error_duplicate_key = 1062,
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

static int test_expression_collate_predicates(void);
static int populate_people(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_expression_collate_predicates();

    return failures == 0 ? 0 : 1;
}

static int test_expression_collate_predicates(void) {
    static const char *const ai_ci_equal_ids[] = {"1", "2", "3"};
    static const char *const as_cs_equal_ids[] = {"1"};
    static const char *const ai_ci_like_ids[] = {"1", "2", "3", "4"};
    static const char *const as_cs_like_ids[] = {"1", "4"};
    static const char *const as_cs_order_values[] = {
        "5",
        NULL,
        "4",
        "joel",
        "1",
        "john",
        "2",
        "John",
        "3",
        "JOHN",
    };
    static const char *const latin1_equal_ids[] = {"1", "2"};
    static const char *const unicode_ai_ci_equal_ids[] = {"1", "2", "3"};
    static const char *const unicode_as_equal_ids[] = {"1", "3"};
    static const char *const unicode_binary_equal_ids[] = {"1"};
    static const char *const positioned_accent_ids[] = {"1", "3"};
    static const char *const unicode_group_values[] = {
        "1",
        "3",
        "4",
        "2",
        "6",
        "1",
        "7",
        "2",
        "9",
        "2",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "predicates", path, sizeof(path));
    failures += populate_people(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname = 'john' COLLATE utf8mb4_0900_ai_ci ORDER BY id",
            .values = ai_ci_equal_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "right-side ai_ci collation equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname = 'john' COLLATE utf8mb4_0900_as_cs ORDER BY id",
            .values = as_cs_equal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "right-side as_cs collation equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname COLLATE utf8mb4_0900_as_cs = 'john' ORDER BY id",
            .values = as_cs_equal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "left-side as_cs collation equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname COLLATE utf8mb4_0900_ai_ci = 'john' ORDER BY id",
            .values = ai_ci_equal_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "left-side ai_ci collation equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname LIKE 'jo%' COLLATE utf8mb4_0900_ai_ci ORDER BY id",
            .values = ai_ci_like_ids,
            .column_count = 1U,
            .row_count = 4U,
            .context = "right-side ai_ci LIKE pattern",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE firstname COLLATE utf8mb4_0900_as_cs LIKE 'jo%' ORDER BY id",
            .values = as_cs_like_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "left-side as_cs LIKE subject",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, firstname FROM people "
                   "ORDER BY firstname COLLATE utf8mb4_0900_as_cs, id",
            .values = as_cs_order_values,
            .column_count = 2U,
            .row_count = ordered_people_row_count,
            .context = "order by explicit as_cs collation",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM people "
                   "WHERE latin1_name = CONVERT('a' USING latin1) "
                   "COLLATE latin1_swedish_ci ORDER BY id",
            .values = latin1_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "latin1 converted literal collation",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unicode_names "
                   "WHERE name = 'e' COLLATE utf8mb4_0900_ai_ci ORDER BY id",
            .values = unicode_ai_ci_equal_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "Unicode ai_ci accent and normalization equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM positioned_accents "
                   "WHERE name = 'áa' COLLATE utf8mb4_0900_as_ci ORDER BY id",
            .values = positioned_accent_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "accent-sensitive comparison preserves accent position",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unicode_names "
                   "WHERE name = 'é' COLLATE utf8mb4_0900_as_ci ORDER BY id",
            .values = unicode_as_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "Unicode as_ci canonical equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unicode_names "
                   "WHERE name = 'é' COLLATE utf8mb4_0900_as_cs ORDER BY id",
            .values = unicode_as_equal_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "Unicode as_cs canonical equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unicode_names "
                   "WHERE name = 'é' COLLATE utf8mb4_0900_bin ORDER BY id",
            .values = unicode_binary_equal_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "Unicode binary byte equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT MIN(id), COUNT(*) FROM unicode_names "
                   "GROUP BY name ORDER BY MIN(id)",
            .values = unicode_group_values,
            .column_count = 2U,
            .row_count = 5U,
            .context = "Unicode default collation grouping",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO unique_unicode VALUES ('e')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'e' for key 'unique_unicode.name'",
        }
    );
    failures += execute_ok(database, "INSERT INTO unique_positioned VALUES ('áa')", NULL);
    failures += execute_error(
        database,
        "INSERT INTO unique_positioned VALUES ('áa')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'áa' for key 'unique_positioned.name'",
        }
    );
    failures += execute_ok(database, "INSERT INTO unique_positioned VALUES ('aá')", NULL);
    failures += execute_error(
        database,
        "SELECT id FROM people WHERE firstname = 'john' COLLATE latin1_swedish_ci",
        (struct expected_sql_error){
            .code = mysql_error_invalid_collation_charset,
            .sqlstate = "42000",
            .message_part =
                "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM people WHERE firstname = 'john' COLLATE utf8mb4_not_real_ci",
        (struct expected_sql_error){
            .code = mysql_error_unknown_collation,
            .sqlstate = "HY000",
            .message_part = "Unknown collation: 'utf8mb4_not_real_ci'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int populate_people(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "SET NAMES utf8mb4", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE people ("
        "id INT NOT NULL, "
        "firstname VARCHAR(20), "
        "latin1_name VARCHAR(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci"
        ") CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO people VALUES "
        "(1, 'john', 'a'), "
        "(2, 'John', 'A'), "
        "(3, 'JOHN', 'b'), "
        "(4, 'joel', 'B'), "
        "(5, NULL, NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE unicode_names (id INT NOT NULL, name VARCHAR(40)) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO unicode_names VALUES "
        "(1, 'é'), (2, 'e'), (3, 'é'), (4, 'A'), (5, 'a'), (6, 'a '), "
        "(7, 'ß'), (8, 'ss'), (9, 'æ'), (10, 'ae')",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE unique_unicode (name VARCHAR(40) UNIQUE) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO unique_unicode VALUES ('é')", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE unique_positioned ("
        "name VARCHAR(40) COLLATE utf8mb4_0900_as_ci UNIQUE) "
        "CHARACTER SET utf8mb4",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE positioned_accents (id INT, name VARCHAR(20))"
        " CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO positioned_accents VALUES (1, 'áa'), (2, 'aá'), (3, 'áa')",
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
        "/tmp/mylite-expression-collate-predicates-%s-%d.mylite",
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
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected \"%s\" to contain \"%s\"\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
        );
        return 1;
    }
    return 0;
}
