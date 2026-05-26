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
    test_path_suffix_capacity = 16,
    mysql_error_parse = 1064,
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

struct expected_statement_result {
    int64_t affected_rows;
    size_t warning_count;
    const char *context;
};

static int test_update_ignore_adjustments_and_persistence(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int reset_update_ignore_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
);
static int expect_statement_result(
    mylite_result *result,
    struct expected_statement_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_update_ignore_adjustments_and_persistence();

    return failures == 0 ? 0 : 1;
}

static int test_update_ignore_adjustments_and_persistence(void) {
    static const char *const null_adjusted[] = {"1", "0", "2", "0", "3", "30"};
    static const char *const string_adjusted[] = {"1", "abc", "2", "abc", "3", "ghi"};
    static const char *const tinyint_clipped[] = {"1", "127", "2", "127", "3", "3"};
    static const char *const datetime_adjusted[] = {
        "1",
        "0000-00-00 00:00:00",
        "2",
        "0000-00-00 00:00:00",
        "3",
        "2020-01-03 00:00:00",
    };
    static const char *const order_limit_rows[] = {"1", "10", "2", "20", "3", "0"};
    static const char *const multiple_assignment_rows[] = {
        "1",
        "0",
        "abc",
        "2",
        "0",
        "abc",
        "3",
        "0",
        "ghi",
    };
    static const char *const defaults_rows[] = {"1", "0", "2", "0"};
    static const char *const default_function_rows[] = {"1", "0", "2", "0"};
    static const char *const scalar_subquery_rows[] = {"1", "0", "2", "0"};
    static const char *const warning_rows[] = {
        "Warning",
        "1048",
        "Column 'nn' cannot be null",
        "Warning",
        "1048",
        "Column 'nn' cannot be null",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "adjustments", path, sizeof(path));
    failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", NULL);
    failures += reset_update_ignore_table(database);

    failures += expect_update_result(
        database,
        "UPDATE LOW_PRIORITY t SET nn = 99 WHERE id = 1",
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "low priority update",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET nn = NULL WHERE id IN (1, 2)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore null into not null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM t ORDER BY id",
            .values = null_adjusted,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ignore null adjusted rows",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET v = 'abcdef' WHERE id IN (1, 2)",
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 2U,
            .context = "ignore string truncation",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = string_adjusted,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ignore string adjusted rows",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET i = 999 WHERE id IN (1, 2)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore integer clipping",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM t ORDER BY id",
            .values = tinyint_clipped,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ignore integer clipped rows",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET d = 'bad' WHERE id IN (1, 2)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore invalid datetime",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d FROM t ORDER BY id",
            .values = datetime_adjusted,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ignore datetime adjusted rows",
        }
    );

    failures += reset_update_ignore_table(database);
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET nn = NULL ORDER BY id DESC LIMIT 1",
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "ignore order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM t ORDER BY id",
            .values = order_limit_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "ignore order limit rows",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET nn = NULL LIMIT 0",
        (struct expected_statement_result){
            .affected_rows = 0,
            .warning_count = 0U,
            .context = "ignore limit zero",
        }
    );
    failures += expect_update_result(
        database,
        "UPDATE IGNORE t SET nn = NULL, v = 'abcdef' WHERE id IN (1, 2)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 4U,
            .context = "ignore multiple assignments",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn, v FROM t ORDER BY id",
            .values = multiple_assignment_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "ignore multiple assignment rows",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE defaults_t(id INT PRIMARY KEY, nn INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO defaults_t VALUES (1, 5), (2, 6)", NULL);
    failures += expect_update_result(
        database,
        "UPDATE IGNORE defaults_t SET nn = DEFAULT",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore no explicit default",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM defaults_t ORDER BY id",
            .values = defaults_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "ignore no explicit default rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE default_function_t(id INT PRIMARY KEY, nul INT DEFAULT NULL, nn INT NOT "
        "NULL)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO default_function_t(id, nn) VALUES (1, 5), (2, 6)", NULL);
    failures += expect_update_result(
        database,
        "UPDATE IGNORE default_function_t SET nn = DEFAULT(nul)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore default function null into not null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM default_function_t ORDER BY id",
            .values = default_function_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "ignore default function adjusted rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE scalar_source(n INT)", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE scalar_target(id INT PRIMARY KEY, nn INT NOT NULL)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO scalar_target VALUES (1, 5), (2, 6)", NULL);
    failures += expect_update_result(
        database,
        "UPDATE IGNORE scalar_target SET nn = (SELECT n FROM scalar_source)",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore scalar subquery null into not null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM scalar_target ORDER BY id",
            .values = scalar_subquery_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "ignore scalar subquery adjusted rows",
        }
    );

    failures +=
        execute_ok(database, "CREATE TABLE warnings_t(id INT PRIMARY KEY, nn INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO warnings_t VALUES (1, 5), (2, 6)", NULL);
    failures += expect_update_result(
        database,
        "UPDATE IGNORE warnings_t SET nn = NULL",
        (struct expected_statement_result){
            .affected_rows = 2,
            .warning_count = 2U,
            .context = "ignore warning statement",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "ignore warning rows",
        }
    );

    failures += execute_ok(database, "CREATE TABLE key_t(id INT PRIMARY KEY, u INT UNIQUE)", NULL);
    failures += execute_ok(database, "INSERT INTO key_t VALUES (1, 1), (2, 2), (3, 3)", NULL);
    failures += execute_error(
        database,
        "UPDATE IGNORE key_t SET u = 1 WHERE id IN (2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UPDATE IGNORE does not yet support key assignment conflict demotion",
        }
    );
    failures += execute_error(
        database,
        "UPDATE IGNORE LOW_PRIORITY t SET nn = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "update ignore preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen update ignore file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM defaults_t ORDER BY id",
            .values = defaults_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "reopened update ignore rows",
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
    int failures = make_test_path(path, path_size, name);

    if (failures != 0) {
        return failures;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    return failures;
}

static int reset_update_ignore_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "DROP TABLE IF EXISTS t", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT PRIMARY KEY, "
        "u INT UNIQUE, "
        "nn INT NOT NULL, "
        "v VARCHAR(3) NOT NULL, "
        "i TINYINT, "
        "d DATETIME NOT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 1, 10, 'abc', 1, '2020-01-01 00:00:00'), "
        "(2, 2, 20, 'def', 2, '2020-01-02 00:00:00'), "
        "(3, 3, 30, 'ghi', 3, '2020-01-03 00:00:00')",
        NULL
    );

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
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_update_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_statement_result(result, expected);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_result *result,
    struct expected_statement_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 0U, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
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
        "%s/mylite_update_ignore_modifier_%d_%s.mylite",
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
    char related[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
