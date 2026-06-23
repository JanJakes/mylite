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
    and_column_count = 7,
    or_column_count = 6,
    xor_column_count = 7,
    not_column_count = 9,
    precedence_column_count = 9,
    operand_column_count = 8,
    comparison_column_count = 4,
    boundary_column_count = 4,
    warning_column_count = 11,
    logical_warning_count = 6,
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

static int test_scalar_logical_values_and_file_safety(void);
static int test_scalar_logical_warnings_and_diagnostics(void);
static int test_scalar_logical_overflow_and_unsupported_forms(void);
static int test_scalar_logical_independent_handles(void);
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

    failures += test_scalar_logical_values_and_file_safety();
    failures += test_scalar_logical_warnings_and_diagnostics();
    failures += test_scalar_logical_overflow_and_unsupported_forms();
    failures += test_scalar_logical_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_logical_values_and_file_safety(void) {
    static const char *const and_columns[] = {
        "1 AND 1",
        "1 AND 0",
        "1 AND NULL",
        "0 AND NULL",
        "NULL AND 0",
        "NULL AND 1",
        "NULL AND NULL",
    };
    static const char *const and_values[] = {"1", "0", NULL, "0", "0", NULL, NULL};
    static const char *const or_columns[] = {
        "1 OR 1",
        "1 OR 0",
        "0 OR 0",
        "0 OR NULL",
        "1 OR NULL",
        "NULL OR NULL",
    };
    static const char *const or_values[] = {"1", "1", "0", NULL, "1", NULL};
    static const char *const xor_columns[] = {
        "1 XOR 1",
        "1 XOR 0",
        "0 XOR 0",
        "1 XOR NULL",
        "0 XOR NULL",
        "NULL XOR NULL",
        "1 XOR 1 XOR 1",
    };
    static const char *const xor_values[] = {"0", "1", "0", NULL, NULL, NULL, "1"};
    static const char *const not_columns[] = {
        "NOT 10",
        "NOT 0",
        "NOT NULL",
        "NOT -1",
        "NOT TRUE",
        "NOT FALSE",
        "!1",
        "!0",
        "!NULL",
    };
    static const char *const not_values[] = {"0", "1", NULL, "0", "0", "1", "0", "1", NULL};
    static const char *const precedence_columns[] = {
        "1<2 AND 2<3",
        "1<2 AND 2>3",
        "1<2 OR 2>3",
        "1>2 OR 2>3",
        "1<2 XOR 2<3",
        "NOT 1<2",
        "NOT (1>2)",
        "0 OR 0 AND 1",
        "1 XOR 1 AND 0",
    };
    static const char *const precedence_values[] = {
        "1",
        "0",
        "1",
        "0",
        "0",
        "0",
        "1",
        "0",
        "1",
    };
    static const char *const operand_columns[] = {
        "1+2 AND 0",
        "0 OR 2*3",
        "5 DIV 2 AND 1",
        "5 % 2 XOR 0",
        "a",
        "b",
        "c",
        "d",
    };
    static const char *const operand_values[] = {"0", "1", "1", "1", "1", NULL, "1", "1"};
    static const char *const comparison_columns[] = {
        "(1 AND 1)=1",
        "(1 AND 0)=0",
        "(1 OR 0)<=>1",
        "(NULL OR 0)<=>NULL",
    };
    static const char *const comparison_values[] = {"1", "1", "1", "1"};
    static const char *const boundary_columns[] = {
        "9223372036854775807 AND 1",
        "-9223372036854775807 AND 1",
        "(-9223372036854775807-1) OR 0",
        "NOT (-9223372036854775807-1)",
    };
    static const char *const boundary_values[] = {"1", "1", "1", "0"};
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
            .sql = "SELECT 1 AND 1, 1 AND 0, 1 AND NULL, 0 AND NULL, NULL AND 0, "
                   "NULL AND 1, NULL AND NULL",
            .columns = and_columns,
            .column_count = and_column_count,
            .values = and_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical and truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 OR 1, 1 OR 0, 0 OR 0, 0 OR NULL, 1 OR NULL, NULL OR NULL",
            .columns = or_columns,
            .column_count = or_column_count,
            .values = or_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical or truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 XOR 1, 1 XOR 0, 0 XOR 0, 1 XOR NULL, 0 XOR NULL, "
                   "NULL XOR NULL, 1 XOR 1 XOR 1",
            .columns = xor_columns,
            .column_count = xor_column_count,
            .values = xor_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical xor truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NOT 10, NOT 0, NOT NULL, NOT -1, NOT TRUE, NOT FALSE, "
                   "!1, !0, !NULL",
            .columns = not_columns,
            .column_count = not_column_count,
            .values = not_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical not truth table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1<2 AND 2<3, 1<2 AND 2>3, 1<2 OR 2>3, "
                   "1>2 OR 2>3, 1<2 XOR 2<3, NOT 1<2, NOT (1>2), "
                   "0 OR 0 AND 1, 1 XOR 1 AND 0",
            .columns = precedence_columns,
            .column_count = precedence_column_count,
            .values = precedence_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical precedence",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+2 AND 0, 0 OR 2*3, 5 DIV 2 AND 1, 5 % 2 XOR 0, "
                   "IFNULL(NULL,1) AND 1 AS a, NULLIF(1,1) OR 0 AS b, "
                   "ISNULL(NULL) XOR FALSE AS c, NOT COALESCE(NULL,0) AS d FROM DUAL",
            .columns = operand_columns,
            .column_count = operand_column_count,
            .values = operand_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical arithmetic and function operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (1 AND 1)=1, (1 AND 0)=0, (1 OR 0)<=>1, "
                   "(NULL OR 0)<=>NULL",
            .columns = comparison_columns,
            .column_count = comparison_column_count,
            .values = comparison_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical results compared",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807 AND 1, -9223372036854775807 AND 1, "
                   "(-9223372036854775807-1) OR 0, NOT (-9223372036854775807-1)",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical signed boundaries",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "scalar logical leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "scalar logical leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, path);
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar logical leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_logical_warnings_and_diagnostics(void) {
    static const char *const warning_columns[] = {
        "0 AND 5 DIV 0",
        "1 AND 5 DIV 0",
        "NULL AND 5 DIV 0",
        "1 OR 5 DIV 0",
        "0 OR 5 DIV 0",
        "NULL OR 5 DIV 0",
        "1 XOR 5 DIV 0",
        "0 XOR 5 DIV 0",
        "NULL XOR 5 DIV 0",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {
        "0",
        NULL,
        NULL,
        "1",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        "0",
    };
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const show_warning_values[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const following_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const following_values[] = {"6", "-1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "warnings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open warnings file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE rc_seed(id INT)", NULL);
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 0 AND 5 DIV 0, 1 AND 5 DIV 0, NULL AND 5 DIV 0, "
                   "1 OR 5 DIV 0, 0 OR 5 DIV 0, NULL OR 5 DIV 0, "
                   "1 XOR 5 DIV 0, 0 XOR 5 DIV 0, NULL XOR 5 DIV 0, "
                   "@@warning_count, ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = logical_warning_count,
            .affected_rows = 0,
            .context = "logical child warning values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = 3U,
            .values = show_warning_values,
            .row_count = logical_warning_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = following_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "logical following diagnostics",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_logical_overflow_and_unsupported_forms(void) {
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
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (0), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500 AND 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 'a' AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1.5 OR 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 0x31 AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT b'1' XOR 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count AND 1",
            .columns = (const char *const[]){"@@warning_count AND 1"},
            .column_count = 1U,
            .values = (const char *const[]){"1"},
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "system variable logical operand",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 + (1 AND 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1&&1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1||0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT (1,2) AND 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT id AND 1 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_logical_independent_handles(void) {
    static const char *const first_columns[] = {"first_result", "NOT NULL"};
    static const char *const first_values[] = {"1", NULL};
    static const char *const second_columns[] = {"second_result", "1 XOR 1"};
    static const char *const second_values[] = {NULL, "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,1) AND 1 AS first_result, NOT NULL",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle logical",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT NULLIF(1,1) OR 0 AS second_result, 1 XOR 1",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle logical",
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
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
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
        "/tmp/mylite-scalar-logical-projection-%s-%d.mylite",
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
