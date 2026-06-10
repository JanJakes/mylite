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
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_native_function_argument_count = 1582,
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

static int test_no_source_dual_and_do_bitmask(void);
static int test_table_backed_bitmask_and_reopen(void);
static int test_independent_file_backed_bitmask_handles(void);
static int test_bitmask_diagnostics(void);
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

    failures += test_no_source_dual_and_do_bitmask();
    failures += test_table_backed_bitmask_and_reopen();
    failures += test_independent_file_backed_bitmask_handles();
    failures += test_bitmask_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_bitmask(void) {
    static const char *const columns_no_source[] = {
        "es",
        "es_zero_bits",
        "es_empty",
        "es_null_bits",
        "es_null_on",
        "es_true",
        "es_false",
        "es_numeric",
        "ms_one",
        "ms_two",
        "ms_empty",
        "ms_null_bits",
        "ms_skip_null",
        "ms_negative",
        "ms_true",
        "ms_false",
        "ms_numeric",
        "warnings",
    };
    static const char *const values_no_source[] = {
        "Y,N,Y,N",
        "N,N,N,N",
        "",
        NULL,
        NULL,
        "Y",
        "N",
        "1,0,1,0",
        "a",
        "a,b",
        "",
        NULL,
        "a,c",
        "a,b,c",
        "a",
        "",
        "1,1",
        "0",
    };
    static const char *const columns_dual[] = {"es", "ms"};
    static const char *const values_dual[] = {"Y:N:Y:N", "b"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXPORT_SET(5,'Y','N',',',4) AS es, "
                   "EXPORT_SET(0,'Y','N',',',4) AS es_zero_bits, "
                   "EXPORT_SET(1,'Y','N',',',0) AS es_empty, "
                   "EXPORT_SET(NULL,'Y','N',',',4) AS es_null_bits, "
                   "EXPORT_SET(1,NULL,'N',',',4) AS es_null_on, "
                   "EXPORT_SET(TRUE,'Y','N',',',TRUE) AS es_true, "
                   "EXPORT_SET(FALSE,'Y','N',',',TRUE) AS es_false, "
                   "EXPORT_SET(5,1,0,',',4) AS es_numeric, "
                   "MAKE_SET(1,'a','b','c') AS ms_one, "
                   "MAKE_SET(3,'a','b','c') AS ms_two, "
                   "MAKE_SET(0,'a','b') AS ms_empty, MAKE_SET(NULL,'a') AS ms_null_bits, "
                   "MAKE_SET(7,'a',NULL,'c') AS ms_skip_null, "
                   "MAKE_SET(-1,'a','b','c') AS ms_negative, "
                   "MAKE_SET(TRUE,'a','b') AS ms_true, "
                   "MAKE_SET(FALSE,'a','b') AS ms_false, "
                   "MAKE_SET(3,1,TRUE,NULL) AS ms_numeric, @@warning_count AS warnings",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source bitmask values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXPORT_SET (5,'Y','N',':',4) AS es, "
                   "MAKE_SET (2,'a','b') AS ms FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual bitmask whitespace",
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
            .context = "row count after bitmask select",
        }
    );

    failures +=
        execute_ok(database, "DO EXPORT_SET(5,'Y','N',',',4), MAKE_SET(3,'a','b')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "bitmask do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "bitmask do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "bitmask do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "bitmask do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after bitmask do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_bitmask_and_reopen(void) {
    static const char *const columns_table[] = {"id", "exported", "made"};
    static const char *const values_table[] = {
        "1",
        "Y:N:Y:N",
        "first,10",
        "2",
        "off|on|off|off",
        "on",
        "3",
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "made"};
    static const char *const values_limited[] = {
        "3",
        NULL,
        "2",
        "on",
    };
    static const char *const columns_reopen[] = {"id", "exported"};
    static const char *const values_reopen[] = {
        "1",
        "Y,N,Y,N",
        "2",
        "off,on,off,off",
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
        "CREATE TABLE t("
        "id INT, bits INT, on_label VARCHAR(20), off_label VARCHAR(20), sep CHAR(1), "
        "txt TEXT, i INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 5, 'Y', 'N', ':', 'first', 10), "
        "(2, 2, 'on', 'off', '|', 'second', -7), "
        "(3, NULL, 'Y', 'N', ',', NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, EXPORT_SET(bits, on_label, off_label, sep, 4) AS exported, "
                   "MAKE_SET(bits, txt, on_label, i) AS made FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table bitmask values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, MAKE_SET(bits, off_label, on_label) AS made FROM t "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table bitmask row envelope",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "bitmask preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen bitmask");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, EXPORT_SET(bits, on_label, off_label, ',', 4) AS exported "
                   "FROM t WHERE id <= 2 ORDER BY id",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen bitmask values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_file_backed_bitmask_handles(void) {
    static const char *const columns[] = {"made"};
    static const char *const values_one[] = {"a"};
    static const char *const values_two[] = {"b"};
    char path_one[test_path_capacity];
    char path_two[test_path_capacity];
    mylite_db *one = NULL;
    mylite_db *two = NULL;
    int failures = 0;

    failures += open_app_database(&one, "independent-one", path_one, sizeof(path_one));
    failures += open_app_database(&two, "independent-two", path_two, sizeof(path_two));
    failures += execute_ok(one, "CREATE TABLE t(bits INT)", NULL);
    failures += execute_ok(two, "CREATE TABLE t(bits INT)", NULL);
    failures += execute_ok(one, "INSERT INTO t VALUES (1)", NULL);
    failures += execute_ok(two, "INSERT INTO t VALUES (2)", NULL);
    failures += expect_query(
        one,
        (struct expected_query){
            .sql = "SELECT MAKE_SET(bits, 'a', 'b') AS made FROM t",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_one,
            .row_count = 1U,
            .context = "independent bitmask handle one",
        }
    );
    failures += expect_query(
        two,
        (struct expected_query){
            .sql = "SELECT MAKE_SET(bits, 'a', 'b') AS made FROM t",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_two,
            .row_count = 1U,
            .context = "independent bitmask handle two",
        }
    );

    mylite_close(two);
    mylite_close(one);
    remove_related_files(path_two);
    remove_related_files(path_one);
    return failures;
}

static int test_bitmask_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, bits INT, v VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 5, 'abc')", NULL);

    failures += execute_error(
        database,
        "SELECT EXPORT_SET()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'EXPORT_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(1, 'Y')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'EXPORT_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(1, 'Y', 'N', ',', 4, 5)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'EXPORT_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKE_SET()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'MAKE_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKE_SET(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'MAKE_SET'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(1 + 0, 'Y', 'N')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string bitmask functions support only signed integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(1, 'Y', 'N', ',', 1 + 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXPORT_SET() number_of_bits supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKE_SET(1 + 0, 'a')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "string bitmask functions support only signed integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKE_SET(1, 1 + 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MAKE_SET() supports only string, integer, boolean, NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(9223372036854775808, 'Y', 'N')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string bitmask function bitmask literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(missing, 'Y', 'N')",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXPORT_SET(v, 'Y', 'N') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string bitmask functions support only integer descriptor bitmask columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKE_SET(missing, 'a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM t WHERE MAKE_SET(bits, 'a') = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
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
    int written =
        snprintf(path, path_size, "/tmp/mylite-bitmask-%s-%d.mylite", name, current_process_id());

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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
