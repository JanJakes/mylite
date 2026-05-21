#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    core_column_count = 12,
    label_column_count = 2,
    whitespace_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_argument_count = 1582,
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

static int test_addtime_subtime_values_and_file_safety(void);
static int test_addtime_subtime_sql_modes_and_errors(void);
static int test_addtime_subtime_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_bytes(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected,
    const char *context
);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_addtime_subtime_values_and_file_safety();
    failures += test_addtime_subtime_sql_modes_and_errors();
    failures += test_addtime_subtime_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_addtime_subtime_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "ADDTIME('2008-01-02 13:29:17','00:00:01')",
        "ADDTIME('2008-01-02 13:29:17','-00:00:01')",
        "SUBTIME('2008-01-02 13:29:17','00:00:01')",
        "SUBTIME('2008-01-02 13:29:17','-00:00:01')",
        "ADDTIME('01:02:03','00:00:04')",
        "SUBTIME('01:02:03','00:00:04')",
        "ADDTIME('-01:02:03','00:00:04')",
        "SUBTIME('-01:02:03','00:00:04')",
        "ADDTIME('01:02:03','100:00:00')",
        "ADDTIME(NULL,'bad')",
        "ADDTIME('01:02:03',NULL)",
        "@@warning_count",
    };
    static const char *const core_values[] = {
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:18",
        "01:02:07",
        "01:01:59",
        "-01:01:59",
        "-01:02:07",
        "101:02:03",
        NULL,
        NULL,
        "0",
    };
    static const char *const label_columns[] = {
        "ADDTIME('01:02:03','00:00:01')",
        "shifted",
    };
    static const char *const label_values[] = {
        "01:02:04",
        "2008-01-02 13:29:16",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open ADDTIME values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ADDTIME('2008-01-02 13:29:17','00:00:01'),"
                   "ADDTIME('2008-01-02 13:29:17','-00:00:01'),"
                   "SUBTIME('2008-01-02 13:29:17','00:00:01'),"
                   "SUBTIME('2008-01-02 13:29:17','-00:00:01'),"
                   "ADDTIME('01:02:03','00:00:04'),"
                   "SUBTIME('01:02:03','00:00:04'),"
                   "ADDTIME('-01:02:03','00:00:04'),"
                   "SUBTIME('-01:02:03','00:00:04'),"
                   "ADDTIME('01:02:03','100:00:00'),"
                   "ADDTIME(NULL,'bad'),"
                   "ADDTIME('01:02:03',NULL),"
                   "@@warning_count",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core ADDTIME and SUBTIME values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ADDTIME('01:02:03','00:00:01'), "
                   "SUBTIME('2008-01-02 13:29:17','00:00:01') AS shifted FROM DUAL",
            .columns = label_columns,
            .column_count = label_column_count,
            .values = label_values,
            .row_count = 1U,
            .context = "ADDTIME and SUBTIME labels",
        }
    );

    failures +=
        execute_ok(database, "DO ADDTIME('01:02:03','00:00:01'), SUBTIME(NULL,'bad')", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "ADDTIME DO columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "ADDTIME DO rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "ADDTIME DO affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "ADDTIME DO warnings");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count after ADDTIME DO",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "ADDTIME leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "ADDTIME leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read ADDTIME preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "ADDTIME leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_addtime_subtime_sql_modes_and_errors(void) {
    static const char *const whitespace_columns[] = {
        "ADDTIME ('01:02:03','00:00:01')",
        "SUBTIME ('01:02:03','00:00:01')",
    };
    static const char *const whitespace_values[] = {
        "01:02:04",
        "01:02:02",
    };
    static const char embedded_nul_sql[] = "SELECT ADDTIME('01:02:\0"
                                           "03','00:00:01')";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open ADDTIME errors file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ADDTIME ('01:02:03','00:00:01'), "
                   "SUBTIME ('01:02:03','00:00:01')",
            .columns = whitespace_columns,
            .column_count = whitespace_column_count,
            .values = whitespace_values,
            .row_count = 1U,
            .context = "default mode accepts ADDTIME whitespace",
        }
    );
    failures += execute_ok(database, "CREATE TABLE addtime(id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE subtime(id INT)", NULL);
    failures += execute_ok(database, "DROP TABLE addtime", NULL);
    failures += execute_ok(database, "DROP TABLE subtime", NULL);

    failures += execute_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ADDTIME ('01:02:03','00:00:01'), "
                   "SUBTIME ('01:02:03','00:00:01')",
            .columns = whitespace_columns,
            .column_count = whitespace_column_count,
            .values = whitespace_values,
            .row_count = 1U,
            .context = "IGNORE_SPACE accepts ADDTIME whitespace",
        }
    );
    failures += execute_ok(database, "CREATE TABLE addtime(id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE subtime(id INT)", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT ADDTIME(\"01:02:03\", \"00:00:01\")",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_error(
        database,
        "SELECT ADDTIME()",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME('01:02:03')",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBTIME('01:02:03','00:00:01','x')",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME(1,'00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ADDTIME() supports only canonical datetime string literals, canonical time "
                "string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME('01:02:03',1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ADDTIME() time argument supports only canonical time string literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME('bad','00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ADDTIME() supports only canonical YYYY-MM-DD HH:MM:SS datetime or "
                            "canonical [-]HH:MM:SS time values",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBTIME('01:02:03','bad')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SUBTIME() time argument supports only canonical [-]HH:MM:SS time values",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME('838:59:59','00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ADDTIME() result is outside the supported time or datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBTIME('1000-01-01 00:00:00','00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBTIME() result is outside the supported time or datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDTIME(missing_column,'00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error_bytes(
        database,
        embedded_nul_sql,
        sizeof(embedded_nul_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ADDTIME() time literals do not support NUL bytes",
        },
        "embedded NUL ADDTIME literal"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_addtime_subtime_independent_handles(void) {
    static const char *const first_columns[] = {"first_result"};
    static const char *const first_values[] = {"01:02:04"};
    static const char *const second_columns[] = {"second_result", "third_result"};
    static const char *const second_values[] = {"2008-01-02 13:29:16", "-01:02:07"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first ADDTIME handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second ADDTIME handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT ADDTIME('01:02:03','00:00:01') AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first ADDTIME handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT SUBTIME('2008-01-02 13:29:17','00:00:01') AS second_result, "
                   "SUBTIME('-01:02:03','00:00:04') AS third_result",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .context = "second ADDTIME handle",
        }
    );

    mylite_close(second);
    mylite_close(first);
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
    return execute_error_bytes(database, sql, strlen(sql), expected, sql);
}

static int execute_error_bytes(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", context);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, context);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-addtime-subtime-%s-%d.mylite",
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
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
