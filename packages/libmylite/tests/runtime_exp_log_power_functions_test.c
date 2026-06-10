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
    value_column_count = 29,
    child_column_count = 8,
    warning_column_count = 6,
    show_warning_column_count = 3,
    diagnostic_column_count = 2,
    diagnostic_status_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
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

static int test_exp_log_power_values_and_file_safety(void);
static int test_exp_log_power_warnings_do_and_independent_handles(void);
static int test_exp_log_power_errors_and_unsupported_forms(void);
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

    failures += test_exp_log_power_values_and_file_safety();
    failures += test_exp_log_power_warnings_do_and_independent_handles();
    failures += test_exp_log_power_errors_and_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_exp_log_power_values_and_file_safety(void) {
    static const char *const value_columns[] = {
        "EXP(NULL)", "EXP(TRUE)",   "EXP(FALSE)",  "EXP(0)",     "EXP(1)",      "EXP(-1)",
        "EXP(2)",    "EXP(10)",     "LN(NULL)",    "LN(TRUE)",   "LN(1)",       "LN(2)",
        "LOG(NULL)", "LOG(1)",      "LOG(2)",      "LOG10(100)", "LOG2(8)",     "LOG(10,100)",
        "LOG(2,8)",  "POW(NULL,2)", "POW(2,NULL)", "POW(2,3)",   "POW(2,-3)",   "POW(-2,3)",
        "POW(-2,2)", "POW(-2,-3)",  "POW(0,0)",    "POWER(3,2)", "ROW_COUNT()",
    };
    static const char *const value_values[] = {
        NULL,
        "2.718281828459045",
        "1",
        "1",
        "2.718281828459045",
        "0.36787944117144233",
        "7.38905609893065",
        "22026.465794806718",
        NULL,
        "0",
        "0",
        "0.6931471805599453",
        NULL,
        "0",
        "0.6931471805599453",
        "2",
        "3",
        "2",
        "3",
        NULL,
        NULL,
        "8",
        "0.125",
        "-8",
        "4",
        "-0.125",
        "1",
        "9",
        "0",
    };
    static const char *const child_columns[] = {
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
    };
    static const char *const child_values[] = {
        "2.718281828459045",
        "0",
        "3",
        "3",
        "1",
        "4",
        "256",
        NULL,
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open exp log power file");
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
            .sql = "SELECT EXP(NULL),EXP(TRUE),EXP(FALSE),EXP(0),EXP(1),EXP(-1),"
                   "EXP(2),EXP(10),LN(NULL),LN(TRUE),LN(1),LN(2),LOG(NULL),"
                   "LOG(1),LOG(2),LOG10(100),LOG2(8),LOG(10,100),LOG(2,8),"
                   "POW(NULL,2),POW(2,NULL),POW(2,3),POW(2,-3),POW(-2,3),"
                   "POW(-2,2),POW(-2,-3),POW(0,0),POWER(3,2),ROW_COUNT()",
            .columns = value_columns,
            .column_count = value_column_count,
            .values = value_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp log power values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXP(5&3) a,LN(5&3) b,LOG2(1<<3) c,LOG(2,1<<3) d,"
                   "POW(5&3,2) e,POW(2,5 DIV 2) f,POW(2,1<<3) g,"
                   "POW(NULLIF(1,1),2) h FROM DUAL",
            .columns = child_columns,
            .column_count = child_column_count,
            .values = child_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp log power child operands",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "exp log power catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "exp log power sqlite schema generation unchanged"
    );
    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "exp log power preamble"
    );
    remove_related_files(path);
    return failures;
}

static int test_exp_log_power_warnings_do_and_independent_handles(void) {
    static const char *const warning_columns[] = {
        "LOG(NULL,5 DIV 0)",
        "LOG(0,5 DIV 0)",
        "LOG(2,5 DIV 0)",
        "POW(NULL,5 DIV 0)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, NULL, NULL, NULL, "0", "0"};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_rows[] = {
        "Warning",
        "3020",
        "Invalid argument for logarithm",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const diagnostic_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const do_warning_values[] = {"2", "0"};
    static const char *const handle_columns[] = {"EXP(1)", "LOG2(8)", "POWER(2,3)"};
    static const char *const handle_values[] = {"2.718281828459045", "3", "8"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning handle");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LOG(NULL,5 DIV 0),LOG(0,5 DIV 0),LOG(2,5 DIV 0),"
                   "POW(NULL,5 DIV 0),@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 3U,
            .affected_rows = 0,
            .context = "exp log power select warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = warning_rows,
            .row_count = 3U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp log power warning rows",
        }
    );

    failures += execute_ok(database, "DO EXP(5 DIV 0), LN(0), LOG(2,8), POW(2,3)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "DO column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "DO row count");
        failures += expect_size(mylite_result_warning_count(result), 2U, "DO warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "DO affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = diagnostic_columns,
            .column_count = diagnostic_column_count,
            .values = do_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp log power DO warning snapshot",
        }
    );
    mylite_close(database);

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return failures + 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT EXP(1),LOG2(8),POWER(2,3)",
            .columns = handle_columns,
            .column_count = 3U,
            .values = handle_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first exp log power handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT EXP(1),LOG2(8),POWER(2,3)",
            .columns = handle_columns,
            .column_count = 3U,
            .values = handle_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second exp log power handle",
        }
    );
    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_exp_log_power_errors_and_unsupported_forms(void) {
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_before_error_rows[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Error",
        "1690",
        "DOUBLE value is out of range in 'exp()'",
    };
    static const char *const diagnostic_status_columns[] = {
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_before_error_status[] = {"2", "1", "-1"};
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
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (4), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT EXP()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'EXP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LN()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LN'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOG10(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LOG10'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOG2()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LOG2'",
        }
    );
    failures += execute_error(
        database,
        "SELECT POW(2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'POW'",
        }
    );
    failures += execute_error(
        database,
        "SELECT POWER(2,3,4)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'POWER'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOG()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOG(1,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXP",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'EXP' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXP(710)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "DOUBLE value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT POW(0,-1)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "DOUBLE value is out of range",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_error(
        database,
        "SELECT EXP(5 DIV 0),EXP(710)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "DOUBLE value is out of range",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = warning_before_error_rows,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp warning before error diagnostics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,@@error_count,ROW_COUNT()",
            .columns = diagnostic_status_columns,
            .column_count = diagnostic_status_column_count,
            .values = warning_before_error_status,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "exp warning before error status",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXP('2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXP()/LN()/LOG()",
        }
    );
    failures += execute_error(
        database,
        "SELECT LOG(2,'8')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXP()/LN()/LOG()",
        }
    );
    failures += execute_error(
        database,
        "SELECT POW(2,0.5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXP()/LN()/LOG()",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXP(id) FROM t ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+EXP(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT ABS(EXP(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ABS() supports",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
        "%s/mylite_exp_log_power_functions_%d_%s.mylite",
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
