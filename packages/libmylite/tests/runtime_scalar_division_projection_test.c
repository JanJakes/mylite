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
    core_column_count = 10,
    rounding_column_count = 10,
    operand_column_count = 6,
    boundary_column_count = 6,
    warning_column_count = 4,
    null_warning_column_count = 8,
    null_warning_expected_count = 5,
    mysql_error_parse = 1064,
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

static int test_scalar_division_values_and_file_safety(void);
static int test_scalar_division_warnings_and_do(void);
static int test_scalar_division_errors_and_unsupported_forms(void);
static int test_scalar_division_independent_handles(void);
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

    failures += test_scalar_division_values_and_file_safety();
    failures += test_scalar_division_warnings_and_do();
    failures += test_scalar_division_errors_and_unsupported_forms();
    failures += test_scalar_division_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_division_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "3/5",
        "1/2",
        "1/3",
        "10/4",
        "-5/2",
        "5/-2",
        "-5/-2",
        "TRUE/2",
        "FALSE/2",
        "5/TRUE",
    };
    static const char *const core_values[] = {
        "0.6000",
        "0.5000",
        "0.3333",
        "2.5000",
        "-2.5000",
        "-2.5000",
        "2.5000",
        "0.5000",
        "0.0000",
        "5.0000",
    };
    static const char *const rounding_columns[] = {
        "1/6",
        "1/7",
        "2/3",
        "1/8",
        "1/20",
        "1/20000",
        "1/200000",
        "-1/6",
        "-1/20000",
        "-1/200000",
    };
    static const char *const rounding_values[] = {
        "0.1667",
        "0.1429",
        "0.6667",
        "0.1250",
        "0.0500",
        "0.0001",
        "0.0000",
        "-0.1667",
        "-0.0001",
        "0.0000",
    };
    static const char *const operand_columns[] = {"a", "b", "c", "d", "e", "f"};
    static const char *const operand_values[] = {
        "3.0000",
        "2.5000",
        "2.5000",
        "2.5000",
        "0.5000",
        "5.0000",
    };
    static const char *const boundary_columns[] = {
        "9223372036854775807/1",
        "(-9223372036854775807-1)/1",
        "(-9223372036854775807-1)/-1",
        "9223372036854775807/-1",
        "9223372036854775807/2",
        "(-9223372036854775807-1)/2",
    };
    static const char *const boundary_values[] = {
        "9223372036854775807.0000",
        "-9223372036854775808.0000",
        "9223372036854775808.0000",
        "-9223372036854775807.0000",
        "4611686018427387903.5000",
        "-4611686018427387904.0000",
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
            .sql = "SELECT 3/5,1/2,1/3,10/4,-5/2,5/-2,-5/-2,"
                   "TRUE/2,FALSE/2,5/TRUE",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core division values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1/6,1/7,2/3,1/8,1/20,1/20000,1/200000,"
                   "-1/6,-1/20000,-1/200000",
            .columns = rounding_columns,
            .column_count = rounding_column_count,
            .values = rounding_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division rounding",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1+5)/2 AS a,5/(5 DIV 2) b,IFNULL(NULL,5)/2 c,"
                   "5/IF(1,2,3) d,(5%2)/2 e,5/(5 MOD 2) f FROM DUAL",
            .columns = operand_columns,
            .column_count = operand_column_count,
            .values = operand_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division arithmetic operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807/1,(-9223372036854775807-1)/1,"
                   "(-9223372036854775807-1)/-1,9223372036854775807/-1,"
                   "9223372036854775807/2,(-9223372036854775807-1)/2",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division signed boundaries",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "division catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "division sqlite schema generation unchanged"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "division preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_division_warnings_and_do(void) {
    static const char *const warning_columns[] = {
        "5/0",
        "5/FALSE",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, NULL, "0", "0"};
    static const char *const count_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const select_warning_values[] = {"2", "-1"};
    static const char *const no_warning_values[] = {"0", "0"};
    static const char *const do_warning_values[] = {"1", "0"};
    static const char *const null_warning_columns[] = {
        "NULL/(5 DIV 0)",
        "NULLIF(1,1)/(5 DIV 0)",
        "(5 DIV 0)/(5 DIV 0)",
        "IF(0,1,NULL)/(5 DIV 0)",
        "IFNULL(NULL,NULL)/(5 DIV 0)",
        "COALESCE(NULL,NULL)/(5 DIV 0)",
        "(NULL+0)/(5 DIV 0)",
        "@@warning_count",
    };
    static const char *const null_warning_values[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
    };
    static const char *const null_warning_snapshot[] = {"5", "-1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning handle");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5/0,5/FALSE,@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "division select warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = select_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division warning snapshot",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DO 1/2,TRUE/2,NULL/0",
            .columns = NULL,
            .column_count = 0U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division do no warning",
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
            .context = "division do no warning snapshot",
        }
    );

    failures += execute_ok(database, "DO 5/0", &result);
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
            .context = "division do warning snapshot",
        }
    );

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL/(5 DIV 0),NULLIF(1,1)/(5 DIV 0),"
                   "(5 DIV 0)/(5 DIV 0),IF(0,1,NULL)/(5 DIV 0),"
                   "IFNULL(NULL,NULL)/(5 DIV 0),COALESCE(NULL,NULL)/(5 DIV 0),"
                   "(NULL+0)/(5 DIV 0),@@warning_count",
            .columns = null_warning_columns,
            .column_count = null_warning_column_count,
            .values = null_warning_values,
            .row_count = 1U,
            .warning_count = null_warning_expected_count,
            .affected_rows = 0,
            .context = "division null warning behavior",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = null_warning_snapshot,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division null warning snapshot",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_scalar_division_errors_and_unsupported_forms(void) {
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
        "SELECT 3037000500*3037000500/2",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT NULL/(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 9223372036854775808/1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit integer operands",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+5/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5/2/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT '5'/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5.5/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT 0x10/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT id/2 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table-backed signed integer arithmetic projection supports only +, "
                            "binary -, and * operators",
        }
    );
    failures += execute_error(
        database,
        "SELECT ABS(5/2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ABS() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(1,5/2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN TRUE THEN 5/2 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports",
        }
    );
    failures += execute_error(
        database,
        "DO 1+5/2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports",
        }
    );
    failures += execute_error(
        database,
        "DO IF(1,5/2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_division_independent_handles(void) {
    static const char *const first_columns[] = {"1/20000"};
    static const char *const first_values[] = {"0.0001"};
    static const char *const second_columns[] = {"(-9223372036854775807-1)/-1"};
    static const char *const second_values[] = {"9223372036854775808.0000"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT 1/20000",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle division",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT (-9223372036854775807-1)/-1",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle division",
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
        "%s/mylite_scalar_division_projection_%d_%s.mylite",
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
    if (actual == NULL || expected == NULL) {
        if (actual == expected) {
            return 0;
        }
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected message containing %s, got %s\n", context, needle, actual);
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
