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
    crc32_column_count = 9,
    format_column_count = 13,
    truncate_column_count = 15,
    row_column_count = 8,
    dual_column_count = 5,
    mysql_error_parse = 1064,
    mysql_error_native_function_arity = 1582,
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

static int test_numeric_values_and_file_safety(void);
static int test_numeric_dual_do_and_status(void);
static int test_numeric_errors_and_unsupported_forms(void);
static int test_numeric_independent_handles(void);
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

    failures += test_numeric_values_and_file_safety();
    failures += test_numeric_dual_do_and_status();
    failures += test_numeric_errors_and_unsupported_forms();
    failures += test_numeric_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_numeric_values_and_file_safety(void) {
    static const char *const crc32_columns[] = {
        "CRC32('MySQL')",
        "CRC32('mysql')",
        "CRC32('')",
        "CRC32(NULL)",
        "CRC32(123)",
        "CRC32(TRUE)",
        "CRC32(FALSE)",
        "CRC32(-1)",
        "CRC32(X'616263')",
    };
    static const char *const crc32_values[] = {
        "3259397556",
        "2501908538",
        "0",
        NULL,
        "2286445522",
        "2212294583",
        "4108050209",
        "808273962",
        "891568578",
    };
    static const char *const format_columns[] = {
        "FORMAT(12332.123456,4)",
        "FORMAT(12332.1,4)",
        "FORMAT(12332.2,0)",
        "FORMAT(-12332.555,2)",
        "FORMAT(NULL,2)",
        "FORMAT(123,NULL)",
        "FORMAT(123.55,-1)",
        "FORMAT(123.55,31)",
        "FORMAT(999.995,2)",
        "FORMAT(999.994,2)",
        "FORMAT(-999.995,2)",
        "FORMAT(1,TRUE)",
        "FORMAT(1,FALSE)",
    };
    static const char *const format_values[] = {
        "12,332.1235",
        "12,332.1000",
        "12,332",
        "-12,332.56",
        NULL,
        NULL,
        "124",
        "123.550000000000000000000000000000",
        "1,000.00",
        "999.99",
        "-1,000.00",
        "1.0",
        "1",
    };
    static const char *const truncate_columns[] = {
        "TRUNCATE(1.223,1)",
        "TRUNCATE(1.999,1)",
        "TRUNCATE(1.999,0)",
        "TRUNCATE(-1.999,1)",
        "TRUNCATE(122,-2)",
        "TRUNCATE(NULL,1)",
        "TRUNCATE(122,NULL)",
        "TRUNCATE(123.456,31)",
        "TRUNCATE(123.456,-31)",
        "TRUNCATE(1234,2)",
        "TRUNCATE(1234.000,2)",
        "TRUNCATE(1234.1,4)",
        "TRUNCATE(1234.100,2)",
        "TRUNCATE(1.9,TRUE)",
        "TRUNCATE(1.9,FALSE)",
    };
    static const char *const truncate_values[] = {
        "1.2",
        "1.9",
        "1",
        "-1.9",
        "100",
        NULL,
        NULL,
        "123.456",
        "0",
        "1234",
        "1234.00",
        "1234.1",
        "1234.10",
        "1.9",
        "1",
    };
    static const char *const row_columns[] = {
        "id",
        "label_crc",
        "payload_crc",
        "amount_format",
        "amount_truncate",
        "label_crc_text",
        "pi_value",
        "pi_text",
    };
    static const char *const row_values[] = {
        "1",        "3259397556",   "891568578", "1,234.56",   "1234.550", "c=3259397556",
        "3.141593", "p=3.141593",   "2",         "2501908538", NULL,       "-1.00",
        "-1.000",   "c=2501908538", "3.141593",  "p=3.141593", "3",        NULL,
        "0",        NULL,           NULL,        NULL,         "3.141593", "p=3.141593",
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
    failures += execute_ok(
        database,
        "CREATE TABLE metrics("
        "id INT,"
        "amount DECIMAL(8,3),"
        "places INT,"
        "label VARCHAR(20),"
        "payload VARBINARY(20)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO metrics VALUES "
        "(1,1234.555,2,'MySQL',X'616263'),"
        "(2,-1.004,2,'mysql',NULL),"
        "(3,NULL,NULL,NULL,X'')",
        NULL
    );
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CRC32('MySQL'),CRC32('mysql'),CRC32(''),CRC32(NULL),"
                   "CRC32(123),CRC32(TRUE),CRC32(FALSE),CRC32(-1),CRC32(X'616263')",
            .columns = crc32_columns,
            .column_count = crc32_column_count,
            .values = crc32_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "crc32 values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FORMAT(12332.123456,4),FORMAT(12332.1,4),"
                   "FORMAT(12332.2,0),FORMAT(-12332.555,2),FORMAT(NULL,2),"
                   "FORMAT(123,NULL),FORMAT(123.55,-1),FORMAT(123.55,31),"
                   "FORMAT(999.995,2),FORMAT(999.994,2),FORMAT(-999.995,2),"
                   "FORMAT(1,TRUE),FORMAT(1,FALSE)",
            .columns = format_columns,
            .column_count = format_column_count,
            .values = format_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "format values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TRUNCATE(1.223,1),TRUNCATE(1.999,1),"
                   "TRUNCATE(1.999,0),TRUNCATE(-1.999,1),TRUNCATE(122,-2),"
                   "TRUNCATE(NULL,1),TRUNCATE(122,NULL),TRUNCATE(123.456,31),"
                   "TRUNCATE(123.456,-31),TRUNCATE(1234,2),TRUNCATE(1234.000,2),"
                   "TRUNCATE(1234.1,4),TRUNCATE(1234.100,2),TRUNCATE(1.9,TRUE),"
                   "TRUNCATE(1.9,FALSE)",
            .columns = truncate_columns,
            .column_count = truncate_column_count,
            .values = truncate_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "truncate values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,CRC32(label) AS label_crc,CRC32(payload) AS payload_crc,"
                   "FORMAT(amount,places) AS amount_format,"
                   "TRUNCATE(amount,places) AS amount_truncate,"
                   "CONCAT('c=',CRC32(label)) AS label_crc_text,"
                   "PI() AS pi_value,CONCAT('p=',PI()) AS pi_text "
                   "FROM metrics ORDER BY id",
            .columns = row_columns,
            .column_count = row_column_count,
            .values = row_values,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row-backed numeric functions",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "numeric functions catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "numeric functions sqlite schema generation unchanged"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "numeric functions preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_numeric_dual_do_and_status(void) {
    static const char *const dual_columns[] = {"c", "f", "t", "z", "n"};
    static const char *const dual_values[] = {"2044517703", "1,000.00", "1234.10", "0.00", NULL};
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open dual/do handle");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CRC32('ok') AS c,FORMAT(999.995,2) f,"
                   "TRUNCATE(1234.100,2) t,TRUNCATE(-0.004,2) z,FORMAT(NULL,2) n "
                   "FROM DUAL",
            .columns = dual_columns,
            .column_count = dual_column_count,
            .values = dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "dual numeric functions",
        }
    );
    failures += execute_ok(database, "DO CRC32('ok'),FORMAT(1,2),TRUNCATE(1.9,0)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "do rows");
        failures += expect_size(mylite_result_warning_count(result), 0U, "do warnings");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "do affected");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "do status",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_numeric_errors_and_unsupported_forms(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open error handle");
    failures += execute_error(
        database,
        "SELECT CRC32()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT CRC32(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRUNCATE(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT(1,2,'de_DE')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "locale arguments are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT('abc',2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FORMAT() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT TRUNCATE('abc',2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TRUNCATE() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT(1,2.6)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FORMAT() supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CRC32(1+2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CRC32() supports only",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_numeric_independent_handles(void) {
    static const char *const first_columns[] = {"CRC32('first')"};
    static const char *const first_values[] = {"2456940119"};
    static const char *const second_columns[] = {"FORMAT(42.5,0)"};
    static const char *const second_values[] = {"43"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT CRC32('first')",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle crc32",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT FORMAT(42.5,0)",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle format",
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
        "%s/mylite_numeric_format_truncate_crc32_%d_%s.mylite",
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
