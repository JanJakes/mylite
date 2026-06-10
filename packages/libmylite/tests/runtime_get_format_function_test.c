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
    size_t warning_count;
    const char *context;
};

static int test_scalar_get_format_mappings(void);
static int test_get_format_consumers_dual_and_do(void);
static int test_table_backed_get_format_and_reopen(void);
static int test_get_format_diagnostics(void);
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
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_get_format_mappings();
    failures += test_get_format_consumers_dual_and_do();
    failures += test_table_backed_get_format_and_reopen();
    failures += test_get_format_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_get_format_mappings(void) {
    static const char *const columns_mappings[] = {
        "GET_FORMAT(DATE, 'USA')",
        "GET_FORMAT(DATE, 'JIS')",
        "GET_FORMAT(DATE, 'ISO')",
        "GET_FORMAT(DATE, 'EUR')",
        "GET_FORMAT(DATE, 'INTERNAL')",
        "GET_FORMAT(TIME, 'USA')",
        "GET_FORMAT(TIME, 'ISO')",
        "GET_FORMAT(DATETIME, 'EUR')",
        "GET_FORMAT(TIMESTAMP, 'INTERNAL')",
    };
    static const char *const values_mappings[] = {
        "%m.%d.%Y",
        "%Y-%m-%d",
        "%Y-%m-%d",
        "%d.%m.%Y",
        "%Y%m%d",
        "%h:%i:%s %p",
        "%H:%i:%s",
        "%Y-%m-%d %H.%i.%s",
        "%Y%m%d%H%i%s",
    };
    static const char *const columns_nulls[] =
        {"low", "mixed", "n", "unknown", "i", "neg", "t", "f"};
    static const char *const values_nulls[] = {
        "%m.%d.%Y",
        "%m.%d.%Y",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "scalar", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GET_FORMAT(DATE, 'USA'), GET_FORMAT(DATE, 'JIS'), "
                   "GET_FORMAT(DATE, 'ISO'), GET_FORMAT(DATE, 'EUR'), "
                   "GET_FORMAT(DATE, 'INTERNAL'), GET_FORMAT(TIME, 'USA'), "
                   "GET_FORMAT(TIME, 'ISO'), GET_FORMAT(DATETIME, 'EUR'), "
                   "GET_FORMAT(TIMESTAMP, 'INTERNAL')",
            .columns = columns_mappings,
            .column_count = sizeof(columns_mappings) / sizeof(columns_mappings[0]),
            .values = values_mappings,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar get_format mappings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT get_format(date, 'usa') AS low, GET_FORMAT(DaTe, 'UsA') AS mixed, "
                   "GET_FORMAT(DATE, NULL) AS n, GET_FORMAT(TIME, 'bogus') AS unknown, "
                   "GET_FORMAT(DATETIME, 123) AS i, GET_FORMAT(DATE, -1) AS neg, "
                   "GET_FORMAT(DATE, TRUE) AS t, GET_FORMAT(DATE, FALSE) AS f",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar get_format null unknown and case",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_get_format_consumers_dual_and_do(void) {
    static const char *const columns_consumers[] = {"d", "t", "dt"};
    static const char *const values_consumers[] = {
        "2008-01-02",
        "13:29:17",
        "2008-01-02 13:29:17",
    };
    static const char *const columns_dual[] = {"GET_FORMAT(DATE, 'USA')", "fmt"};
    static const char *const values_dual[] = {"%m.%d.%Y", "%H:%i:%s"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "consumers", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_FORMAT('2008-01-02', GET_FORMAT(DATE, 'ISO')) AS d, "
                   "TIME_FORMAT('13:29:17', GET_FORMAT(TIME, 'ISO')) AS t, "
                   "STR_TO_DATE('2008-01-02 13:29:17', GET_FORMAT(DATETIME, 'ISO')) AS dt",
            .columns = columns_consumers,
            .column_count = sizeof(columns_consumers) / sizeof(columns_consumers[0]),
            .values = values_consumers,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "get_format as temporal consumers",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GET_FORMAT(DATE, 'USA'), GET_FORMAT(TIME, 'ISO') AS fmt FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual get_format",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after get_format select",
        }
    );

    failures += execute_ok(database, "DO GET_FORMAT(DATE, 'USA')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "get_format do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "get_format do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "get_format do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "get_format do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after get_format do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_get_format_and_reopen(void) {
    static const char *const columns_projection[] = {"id", "get_format", "fmt", "d"};
    static const char *const values_projection[] = {
        "1",
        "alpha",
        "%m.%d.%Y",
        "2024-01-02",
        "2",
        "beta",
        "%m.%d.%Y",
        "2024-02-29",
    };
    static const char *const columns_reopen[] = {"id", "fmt"};
    static const char *const values_reopen[] = {
        "1",
        "%Y%m%d%H%i%s",
        "2",
        "%Y%m%d%H%i%s",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE get_format(id INT, get_format VARCHAR(16), d DATE)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO get_format VALUES (2, 'beta', '2024-02-29'), (1, 'alpha', '2024-01-02')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, get_format, GET_FORMAT(DATE, 'USA') AS fmt, "
                   "DATE_FORMAT(d, GET_FORMAT(DATE, 'ISO')) AS d "
                   "FROM get_format ORDER BY id",
            .columns = columns_projection,
            .column_count = sizeof(columns_projection) / sizeof(columns_projection[0]),
            .values = values_projection,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table get_format projection",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "get_format preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen get_format file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT id, GET_FORMAT(TIMESTAMP, 'INTERNAL') AS fmt FROM get_format ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "reopen get_format projection",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_get_format_diagnostics(void) {
    char path[test_path_capacity];
    char nul_sql[] = "SELECT GET_FORMAT(DATE, 'US\0A')";
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_error(
        database,
        "SELECT GET_FORMAT(DATE, CONCAT('U', 'SA'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GET_FORMAT() supports only literal format names",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02', GET_FORMAT(DATE, CONCAT('I', 'SO')))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GET_FORMAT() supports only literal format names",
        }
    );
    failures += execute_error(
        database,
        "SELECT GET_FORMAT()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT GET_FORMAT('DATE', 'USA')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error_bytes(
        database,
        nul_sql,
        sizeof(nul_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GET_FORMAT() format names do not support NUL bytes",
        },
        "get_format nul literal"
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
        "/tmp/mylite-get-format-function-%s-%d.mylite",
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
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures = 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        failures = 1;
    }
    fclose(file);
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
    const void *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: expected matching bytes\n", context);
        return 1;
    }
    return 0;
}
