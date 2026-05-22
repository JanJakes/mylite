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
    path_suffix_capacity = 16,
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

static int test_no_source_dual_ansi_and_do(void);
static int test_table_backed_concat_and_reopen(void);
static int test_pipes_as_concat_diagnostics(void);
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
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
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_ansi_and_do();
    failures += test_table_backed_concat_and_reopen();
    failures += test_pipes_as_concat_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_ansi_and_do(void) {
    static const char *const columns_values[] = {
        "ab",
        "null_value",
        "numeric_value",
        "chain",
        "plus_right_concat",
        "plus_left_concat",
        "paren_numeric",
        "paren_concat",
        "db_value",
        "subquery_value",
    };
    static const char *const values_values[] = {
        "ab",
        NULL,
        "12",
        "abc",
        "24",
        "15",
        "15",
        "15",
        "app:x",
        "test-app",
    };
    static const char *const columns_dual[] = {"dual_value"};
    static const char *const values_dual[] = {"dual-ab"};
    static const char *const columns_ansi[] = {"ansi_value", "@@warning_count"};
    static const char *const values_ansi[] = {"xy", "0"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'PIPES_AS_CONCAT'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'a'||'b' AS ab, 'a'||NULL AS null_value, "
                   "1||2 AS numeric_value, ('a'||'b')||'c' AS chain, "
                   "1+2||3 AS plus_right_concat, 1||2+3 AS plus_left_concat, "
                   "(1||2)+3 AS paren_numeric, 1||(2+3) AS paren_concat, "
                   "DATABASE()||':x' AS db_value, "
                   "'test-'||(SELECT DATABASE()) AS subquery_value",
            .columns = columns_values,
            .column_count = sizeof(columns_values) / sizeof(columns_values[0]),
            .values = values_values,
            .row_count = 1U,
            .context = "pipes no-source values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'dual-'||'ab' AS dual_value FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "pipes dual values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after pipes select",
        }
    );

    failures += execute_ok(database, "DO 'a'||'b', 1||2", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "pipes do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "pipes do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "pipes do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "pipes do warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after pipes do",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 'x'||'y' AS ansi_value, @@warning_count",
            .columns = columns_ansi,
            .column_count = sizeof(columns_ansi) / sizeof(columns_ansi[0]),
            .values = values_ansi,
            .row_count = 1U,
            .context = "ansi activates pipes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_concat_and_reopen(void) {
    static const char *const columns_table[] = {"id", "sn", "sd", "snullable", "sdec"};
    static const char *const values_table[] = {
        "1",
        "a:12",
        "a:2024-01-02",
        "ax",
        "a:12.30",
        "2",
        "b:-3",
        NULL,
        NULL,
        "b:-4.50",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_reopen[] = {"id", "s||':'||n"};
    static const char *const values_reopen[] = {"3", NULL, "2", "b:-3"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = 'PIPES_AS_CONCAT'", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, s VARCHAR(10), n INT, d DATE, nullable VARCHAR(10), amount DECIMAL(5,2)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'a', 12, '2024-01-02', 'x', 12.30), "
        "(2, 'b', -3, NULL, NULL, -4.50), "
        "(3, NULL, NULL, '2024-12-31', 'z', NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, s||':'||n AS sn, s||':'||d AS sd, "
                   "s||nullable AS snullable, s||':'||amount AS sdec "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table-backed pipes values",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "pipes preamble before reopen"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen table");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = 'PIPES_AS_CONCAT'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, s||':'||n FROM t ORDER BY id DESC LIMIT 2",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen pipes values",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "pipes preamble after reopen"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_pipes_as_concat_diagnostics(void) {
    static const char *const columns_wide_literal[] = {"wide_value", "@@warning_count"};
    static const char *const values_wide_literal[] = {"19223372036854775808", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_error(
        database,
        "SELECT 1||0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'PIPES_AS_CONCAT'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1||9223372036854775808 AS wide_value, @@warning_count",
            .columns = columns_wide_literal,
            .column_count = sizeof(columns_wide_literal) / sizeof(columns_wide_literal[0]),
            .values = values_wide_literal,
            .row_count = 1U,
            .context = "wide unsigned literal remains concat text",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT('a')||'b'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support nested CONCAT",
        }
    );
    failures += execute_error(
        database,
        "SELECT 'a'||CONCAT_WS('-', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "does not support nested CONCAT",
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
        "/tmp/mylite-pipes-concat-%s-%d.mylite",
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = NULL;
    size_t read_count = 0U;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "%s: failed to seek file\n", path);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, read_count);
        return 1;
    }
    return 0;
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
