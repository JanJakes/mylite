#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    small_posts_sql_capacity = 256,
    wildcard_projection_column_count = 5,
    mysql_error_not_group_by = 1055,
    mysql_error_parse = 1064,
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_group_by_primary_key_projection_values_and_persistence(void);
static int test_group_by_primary_key_projection_diagnostics(void);
static int test_independent_group_by_primary_key_projection_handles(void);
static int seed_schema(mylite_db *database);
static int seed_projection_tables(mylite_db *database);
static int seed_small_posts(mylite_db *database, const char *title_prefix);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_group_by_primary_key_projection_values_and_persistence();
    failures += test_group_by_primary_key_projection_diagnostics();
    failures += test_independent_group_by_primary_key_projection_handles();

    return failures == 0 ? 0 : 1;
}

static int test_group_by_primary_key_projection_values_and_persistence(void) {
    static const char *const explicit_columns[] = {"id", "title", "created", "status"};
    static const char *const explicit_values[] = {
        "3",
        "Gamma",
        "2024-01-03 00:00:00",
        "publish",
        "2",
        "Beta",
        "2024-01-02 00:00:00",
        "draft",
    };
    static const char *const joined_columns[] = {"id", "title", "c"};
    static const char *const joined_values[] =
        {"1", "Alpha", "2", "2", "Beta", "1", "3", "Gamma", "0"};
    static const char *const wildcard_columns[] = {"id", "title", "created", "status", "c"};
    static const char *const wildcard_values[] = {
        "3",
        "Gamma",
        "2024-01-03 00:00:00",
        "publish",
        "0",
        "2",
        "Beta",
        "2024-01-02 00:00:00",
        "draft",
        "1",
    };
    static const char *const chained_left_columns[] = {"id", "title", "created", "status"};
    static const char *const chained_left_values[] = {
        "1",
        "Alpha",
        "2024-01-01 00:00:00",
        "publish",
    };
    static const char *const composite_columns[] = {"a", "b", "v"};
    static const char *const composite_values[] = {"1", "1", "aa", "1", "2", "ab", "2", "1", "ba"};
    static const char *const title_columns[] = {"title"};
    static const char *const title_values[] = {"Alpha", "Beta", "Gamma"};
    static const char *const aliased_pk_columns[] = {"post_id", "title"};
    static const char *const aliased_pk_values[] = {"1", "Alpha", "2", "Beta", "3", "Gamma"};
    static const char *const order_columns[] = {"id", "c"};
    static const char *const order_values[] = {"1", "1", "2", "1", "3", "1"};
    static const char *const like_order_columns[] = {"id"};
    static const char *const like_order_values[] = {"2", "3", "1"};
    static const char *const relaxed_columns[] = {
        "post_id",
        "id",
        "title",
        "created",
        "status",
        "comment_count",
    };
    static const char *const relaxed_values[] = {
        "1",
        "1",
        "Alpha",
        "2024-01-01 00:00:00",
        "publish",
        "2",
        "2",
        "2",
        "Beta",
        "2024-01-02 00:00:00",
        "draft",
        "1",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open grouped projection file");
    failures += seed_schema(database);
    failures += seed_projection_tables(database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, title, created, status FROM posts "
                   "GROUP BY id ORDER BY created DESC LIMIT 2",
            .columns = explicit_columns,
            .column_count = 4U,
            .values = explicit_values,
            .row_count = 2U,
            .context = "single table primary key projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM posts GROUP BY id ORDER BY created DESC LIMIT 2",
            .columns = explicit_columns,
            .column_count = 4U,
            .values = explicit_values,
            .row_count = 2U,
            .context = "single table unqualified wildcard primary key projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id, p.title, COUNT(c.id) AS c "
                   "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
                   "GROUP BY p.id ORDER BY p.title",
            .columns = joined_columns,
            .column_count = 3U,
            .values = joined_values,
            .row_count = 3U,
            .context = "left source primary key projection with aggregate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.*, COUNT(c.id) AS c "
                   "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
                   "GROUP BY p.id ORDER BY p.created DESC LIMIT 2",
            .columns = wildcard_columns,
            .column_count = wildcard_projection_column_count,
            .values = wildcard_values,
            .row_count = 2U,
            .context = "qualified wildcard primary key projection with aggregate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.* FROM posts AS p "
                   "LEFT JOIN comments AS c ON p.id = c.post_id "
                   "LEFT JOIN comments AS c2 ON p.id = c2.post_id "
                   "WHERE c.score IN (5, 7) AND c2.id IN (10, 11) AND p.status = 'publish' "
                   "GROUP BY p.id ORDER BY p.created DESC",
            .columns = chained_left_columns,
            .column_count = 4U,
            .values = chained_left_values,
            .row_count = 1U,
            .context = "chained left joins grouped by left primary key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk GROUP BY b, a ORDER BY v",
            .columns = composite_columns,
            .column_count = 3U,
            .values = composite_values,
            .row_count = 3U,
            .context = "composite primary key coverage is order independent",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT title FROM posts GROUP BY id ORDER BY title",
            .columns = title_columns,
            .column_count = 1U,
            .values = title_values,
            .row_count = 3U,
            .context = "unselected group key determines selected column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id AS post_id, title FROM posts GROUP BY post_id ORDER BY post_id",
            .columns = aliased_pk_columns,
            .column_count = 2U,
            .values = aliased_pk_values,
            .row_count = 3U,
            .context = "selected primary key alias determines selected column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, COUNT(*) AS c FROM posts GROUP BY id ORDER BY title",
            .columns = order_columns,
            .column_count = 2U,
            .values = order_values,
            .row_count = 3U,
            .context = "unselected primary key dependent order column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.id "
                   "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
                   "GROUP BY p.id ORDER BY p.title LIKE '%Beta%' DESC, p.created DESC",
            .columns = like_order_columns,
            .column_count = 1U,
            .values = like_order_values,
            .row_count = 3U,
            .context = "primary key dependent grouped LIKE order expression",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.post_id, p.*, COUNT(*) AS comment_count "
                   "FROM comments AS c LEFT JOIN posts AS p ON p.id = c.post_id "
                   "WHERE c.post_id IN (1, 2) GROUP BY p.id ORDER BY p.id",
            .columns = relaxed_columns,
            .column_count = 6U,
            .values = relaxed_values,
            .row_count = 2U,
            .context = "relaxed mode admits WordPress-style grouped outer join projection",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = DEFAULT", NULL);

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read grouped projection preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "grouped projection keeps preamble"
    );

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen grouped projection file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT title FROM posts GROUP BY id ORDER BY title",
            .columns = title_columns,
            .column_count = 1U,
            .values = title_values,
            .row_count = 3U,
            .context = "primary key projection persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_group_by_primary_key_projection_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database);
    failures += seed_projection_tables(database);

    failures += execute_error(
        database,
        "SELECT title FROM no_pk GROUP BY id",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT a, b, v FROM cpk GROUP BY a",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #2 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, c.body, COUNT(c.id) "
        "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id GROUP BY p.id",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #2 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id GROUP BY p.id",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #5 of SELECT list is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(c.id) AS c "
        "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
        "GROUP BY p.id ORDER BY c.body",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of ORDER BY clause is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT p.id, COUNT(c.id) AS c "
        "FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id "
        "GROUP BY p.id ORDER BY c.body LIKE '%c%' DESC",
        (struct expected_sql_error){
            .code = mysql_error_not_group_by,
            .sqlstate = "42000",
            .message_part = "Expression #1 of ORDER BY clause is not in GROUP BY clause",
        }
    );
    failures += execute_error(
        database,
        "SELECT id AS x, title AS x FROM posts GROUP BY id ORDER BY x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports ORDER BY only on unique selected group aliases",
        }
    );
    failures += execute_error(
        database,
        "SELECT id, COUNT(*), title FROM posts GROUP BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GROUP BY supports selected descriptor group columns followed by aggregate results",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_group_by_primary_key_projection_handles(void) {
    static const char *const first_columns[] = {"title"};
    static const char *const first_values[] = {"first-a", "first-b"};
    static const char *const second_columns[] = {"title"};
    static const char *const second_values[] = {"second-a", "second-b"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += seed_small_posts(first, "first");
    failures += seed_small_posts(second, "second");

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT title FROM posts GROUP BY id ORDER BY title",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 2U,
            .context = "first handle primary key projection",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT title FROM posts GROUP BY id ORDER BY title",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 2U,
            .context = "second handle primary key projection",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);

    return failures;
}

static int seed_projection_tables(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE posts("
        "id INT NOT NULL PRIMARY KEY, "
        "title VARCHAR(20) NULL, "
        "created DATETIME NULL, "
        "status VARCHAR(20) NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE comments("
        "id INT NOT NULL PRIMARY KEY, post_id INT NULL, body VARCHAR(20) NULL, score INT NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "CREATE TABLE cpk("
        "a INT NOT NULL, b INT NOT NULL, v VARCHAR(20) NULL, PRIMARY KEY(a, b))",
        NULL
    );
    failures +=
        execute_ok(database, "CREATE TABLE no_pk(id INT NOT NULL, title VARCHAR(20) NULL)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO posts VALUES "
        "(1, 'Alpha', '2024-01-01 00:00:00', 'publish'), "
        "(2, 'Beta', '2024-01-02 00:00:00', 'draft'), "
        "(3, 'Gamma', '2024-01-03 00:00:00', 'publish')",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO comments VALUES "
        "(10, 1, 'c1', 5), (11, 1, 'c2', 7), (12, 2, 'c3', NULL), "
        "(13, 99, 'orphan', 3)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO cpk VALUES (1, 1, 'aa'), (1, 2, 'ab'), (2, 1, 'ba')",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO no_pk VALUES (1, 'x'), (1, 'y')", NULL);

    return failures;
}

static int seed_small_posts(mylite_db *database, const char *title_prefix) {
    char sql[small_posts_sql_capacity];
    int written = 0;
    int failures = execute_ok(
        database,
        "CREATE TABLE posts(id INT NOT NULL PRIMARY KEY, title VARCHAR(20) NULL)",
        NULL
    );

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO posts VALUES (1, '%s-a'), (2, '%s-b')",
        title_prefix,
        title_prefix
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "small posts insert SQL is too long\n");
        return failures + 1;
    }
    failures += execute_ok(database, sql, NULL);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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
        mylite_result_free(result);
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            query.columns[column_index],
            query.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row_index = 0U; row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            size_t value_index = (row_index * query.column_count) + column_index;

            failures += expect_result_value(
                result,
                row_index,
                column_index,
                query.values[value_index],
                query.context
            );
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_group_by_primary_key_projection_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
