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
    core_column_count = 20,
    dual_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_native_function_arity = 1582,
    mysql_error_bigint_out_of_range = 1690,
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
    int64_t affected_rows;
    const char *context;
};

static int test_sign_values_and_file_safety(void);
static int test_sign_warnings_and_do(void);
static int test_sign_errors_and_unsupported_forms(void);
static int test_sign_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_sign_values_and_file_safety();
    failures += test_sign_warnings_and_do();
    failures += test_sign_errors_and_unsupported_forms();
    failures += test_sign_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sign_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "SIGN(NULL)",
        "SIGN(0)",
        "SIGN(-0)",
        "SIGN(+0)",
        "SIGN(1)",
        "SIGN(-1)",
        "SIGN(TRUE)",
        "SIGN(FALSE)",
        "SIGN(+1)",
        "SIGN(-9223372036854775808)",
        "SIGN(-9223372036854775807)",
        "SIGN(9223372036854775807)",
        "SIGN(9223372036854775808)",
        "SIGN(-9223372036854775809)",
        "SIGN(18446744073709551615)",
        "SIGN(-18446744073709551615)",
        "SIGN(18446744073709551616)",
        "SIGN(-18446744073709551616)",
        "SIGN(999999999999999999999999999999999999999999999999999999999999999999)",
        "SIGN(-999999999999999999999999999999999999999999999999999999999999999999)",
    };
    static const char *const core_values[] = {
        NULL, "0", "0", "0",  "1", "-1", "1", "0",  "1", "-1",
        "-1", "1", "1", "-1", "1", "-1", "1", "-1", "1", "-1",
    };
    static const char *const dual_columns[] = {"a", "b", "c", "d", "e", "f"};
    static const char *const dual_values[] = {
        "1",
        "1",
        "1",
        "0",
        "1",
        "-1",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SIGN(NULL),SIGN(0),SIGN(-0),SIGN(+0),SIGN(1),SIGN(-1),"
                   "SIGN(TRUE),SIGN(FALSE),SIGN(+1),SIGN(-9223372036854775808),"
                   "SIGN(-9223372036854775807),"
                   "SIGN(9223372036854775807),SIGN(9223372036854775808),"
                   "SIGN(-9223372036854775809),SIGN(18446744073709551615),"
                   "SIGN(-18446744073709551615),SIGN(18446744073709551616),"
                   "SIGN(-18446744073709551616),"
                   "SIGN(999999999999999999999999999999999999999999999999999999999999999999),"
                   "SIGN(-999999999999999999999999999999999999999999999999999999999999999999)",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core sign values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SIGN(5&3) AS a,SIGN(~0) b,SIGN(1<<63) c,"
                   "SIGN(1<<64) d,SIGN(5 DIV 2) e,SIGN(IFNULL(NULL,-7)) f FROM DUAL",
            .columns = dual_columns,
            .column_count = dual_column_count,
            .values = dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "dual sign operands",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "sign catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sign sqlite schema generation unchanged"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "sign preamble");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sign_warnings_and_do(void) {
    static const char *const warning_columns[] = {
        "SIGN(5 DIV 0)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, "0", "0"};
    static const char *const count_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const no_warning_values[] = {"0", "0"};
    static const char *const do_warning_values[] = {"1", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning handle");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SIGN(5 DIV 0),@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = 3U,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "sign select warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DO SIGN(NULL),SIGN(-1),SIGN(18446744073709551615)",
            .columns = NULL,
            .column_count = 0U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sign do no warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = no_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sign do count snapshot",
        }
    );

    failures += execute_ok(database, "DO SIGN(5 DIV 0)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "do warning columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "do warning rows");
        failures += expect_size(mylite_result_warning_count(result), 1U, "do warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "do warning affected");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = do_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "sign do warning snapshot",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_sign_errors_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT SIGN()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "DO SIGN(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN('64')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(5.5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(X'40')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(b'1111')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(1/1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(id) FROM t ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+SIGN(7)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT SIGN(7)=7",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN TRUE THEN SIGN(7) END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports",
        }
    );
    failures += execute_error(
        database,
        "DO SIGN(@@warning_count)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SIGN() supports",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sign_independent_handles(void) {
    static const char *const first_columns[] = {"SIGN(~0)"};
    static const char *const first_values[] = {"1"};
    static const char *const second_columns[] = {"SIGN(-9223372036854775809)"};
    static const char *const second_values[] = {"-1"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT SIGN(~0)",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle sign",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT SIGN(-9223372036854775809)",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle sign",
        }
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *tmpdir = getenv("TMPDIR");
    int written = 0;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_sign_function_%d_%s.mylite",
        tmpdir,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
