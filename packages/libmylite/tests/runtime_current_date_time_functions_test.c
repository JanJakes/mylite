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
    mysql_error_parse = 1064,
    current_scalar_column_count = 7,
    current_dml_row_count = 5,
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

static int test_current_date_time_scalar_and_do(void);
static int test_current_date_time_dml_persistence_and_file_safety(void);
static int test_current_date_time_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_current_date_time_scalar_and_do();
    failures += test_current_date_time_dml_persistence_and_file_safety();
    failures += test_current_date_time_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_current_date_time_scalar_and_do(void) {
    static const char *const scalar_values[] = {
        "2023-11-14",
        "2023-11-14",
        "2023-11-14",
        "22:13:20",
        "22:13:20",
        "22:13:20",
        "2023-11-14 22:13:20",
    };
    static const char *const do_counts[] = {"0", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar memory");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CURDATE(), CURRENT_DATE, CURRENT_DATE(), CURTIME(), CURRENT_TIME, "
                   "CURRENT_TIME(), NOW()",
            .values = scalar_values,
            .column_count = current_scalar_column_count,
            .row_count = 1U,
            .context = "current date and time scalar synonyms",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CURRENT_DATE, CURRENT_TIME FROM DUAL",
            .values = (const char *const[]){"2023-11-14", "22:13:20"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "current date and time from dual",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CURDATE (), CURTIME ()",
            .values = (const char *const[]){"2023-11-14", "22:13:20"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "current date and time after IGNORE_SPACE",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION sql_mode = ''");
    failures +=
        expect_statement_ok(database, "DO CURDATE(), CURRENT_DATE, CURTIME(), CURRENT_TIME");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = do_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "current date and time DO counts",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_current_date_time_dml_persistence_and_file_safety(void) {
    static const char *const after_insert_rows[] = {
        "1",
        "2023-11-14",
        "22:13:20",
        "2",
        "2023-11-14",
        "22:14:20",
        "3",
        "2023-11-14",
        "22:15:20",
        "4",
        "2023-11-14",
        "22:16:20",
        "5",
        "2023-11-14",
        "22:17:20",
    };
    static const char *const row_scalar_values[] = {
        "1",
        "2023-11-14",
        "22:18:20",
        "2",
        "2023-11-14",
        "22:18:20",
        "3",
        "2023-11-14",
        "22:18:20",
        "4",
        "2023-11-14",
        "22:18:20",
        "5",
        "2023-11-14",
        "22:18:20",
    };
    static const char *const after_update_counts[] = {"1", "0"};
    static const char *const after_update_rows[] = {
        "1",
        "2023-11-14",
        "22:18:20",
        "2",
        "2023-11-14",
        "22:14:20",
        "3",
        "2023-11-14",
        "22:15:20",
        "4",
        "2023-11-14",
        "22:16:20",
        "5",
        "2023-11-14",
        "22:17:20",
    };
    static const char *const after_noop_counts[] = {"0", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *independent = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "dml") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open dml file");
    failures += expect_int(mylite_open_memory(&independent), MYLITE_OK, "open independent memory");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE current_values (id INT, d DATE, tm TIME)");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures +=
        expect_dml_ok(database, "INSERT INTO current_values VALUES (1, CURDATE(), CURTIME())", 1);
    failures += expect_statement_ok(database, "SET timestamp = 1700000060");
    failures += expect_dml_ok(
        database,
        "INSERT INTO current_values VALUES (2, CURRENT_DATE, CURRENT_TIME)",
        1
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000120");
    failures += expect_dml_ok(
        database,
        "INSERT INTO current_values SET id = 3, d = CURDATE(), tm = CURTIME()",
        1
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000180");
    failures += expect_dml_ok(
        database,
        "REPLACE INTO current_values VALUES (4, CURRENT_DATE, CURRENT_TIME)",
        1
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000240");
    failures += expect_dml_ok(
        database,
        "REPLACE INTO current_values SET id = 5, d = CURDATE(), tm = CURTIME()",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM current_values ORDER BY id",
            .values = after_insert_rows,
            .column_count = 3U,
            .row_count = current_dml_row_count,
            .context = "current date and time insert values",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000300");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, CURRENT_DATE, CURRENT_TIME FROM current_values ORDER BY id",
            .values = row_scalar_values,
            .column_count = 3U,
            .row_count = current_dml_row_count,
            .context = "current date and time row scalar values",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE current_values SET d = CURDATE(), tm = CURTIME() WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_update_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "current date and time update counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM current_values ORDER BY id",
            .values = after_update_rows,
            .column_count = 3U,
            .row_count = current_dml_row_count,
            .context = "current date and time update values",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE current_values SET d = CURRENT_DATE, tm = CURRENT_TIME WHERE id = 1",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_noop_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "current date and time no-op counts",
        }
    );
    failures += expect_statement_ok(independent, "SET timestamp = 1700000240");
    failures += expect_query_values(
        independent,
        (struct expected_query){
            .sql = "SELECT CURDATE(), CURTIME()",
            .values = (const char *const[]){"2023-11-14", "22:17:20"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "independent handle current date and time",
        }
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "current date and time leaves preamble"
    );

    mylite_close(independent);
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen dml file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm FROM current_values ORDER BY id",
            .values = after_update_rows,
            .column_count = 3U,
            .row_count = current_dml_row_count,
            .context = "current date and time values persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_current_date_time_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE t (i INT, d DATE, tm TIME, dt DATETIME)");
    failures += expect_statement_ok(database, "INSERT INTO t(i) VALUES (1)");
    failures += execute_error(
        database,
        "INSERT INTO t(i) VALUES (CURRENT_DATE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CURRENT_DATE values are supported only for DATE columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t(dt) VALUES (CURRENT_DATE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CURRENT_DATE values are supported only for DATE columns",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET i = CURRENT_TIME WHERE i = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CURRENT_TIME values are supported only for TIME columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURTIME(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURDATE(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CURDATE ()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "dml affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "dml warning count");
    mylite_result_free(result);

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
        "%s/mylite_current_date_time_functions_%d_%s.mylite",
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
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return 1;
    }
    return read_count == size ? 0 : 1;
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected [%s] to contain [%s]\n",
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
