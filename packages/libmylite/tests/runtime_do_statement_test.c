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
    diagnostics_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_bigint_out_of_range = 1690,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_statement {
    const char *sql;
    size_t warning_count;
    const char *context;
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

static int test_do_statement_values_and_file_safety(void);
static int test_do_statement_warnings(void);
static int test_do_statement_unsupported_forms(void);
static int test_do_statement_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement(mylite_db *database, struct expected_statement expected);
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

    failures += test_do_statement_values_and_file_safety();
    failures += test_do_statement_warnings();
    failures += test_do_statement_unsupported_forms();
    failures += test_do_statement_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_do_statement_values_and_file_safety(void) {
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"0"};
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
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "DO 1, NULL, TRUE, IF(0,5,6), IFNULL(NULL,7), COALESCE(NULL,8), "
                   "NULLIF(9,9), ISNULL(NULL), 1+2, 5 DIV 2, 5%2, 1=1, 1<=>NULL, "
                   "NOT 0 AND 1, 1 XOR 0, 1 IS TRUE, NULL IS UNKNOWN, "
                   "CASE WHEN 1 THEN 10 ELSE 11 END, CASE 2 WHEN 1 THEN 11 WHEN 2 THEN 12 END, "
                   "VERSION(), ROW_COUNT(), @@warning_count",
            .warning_count = 0U,
            .context = "multi-expression DO",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = row_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row count after DO",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "DO leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "DO leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read DO preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "DO leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_do_statement_warnings(void) {
    static const char *const diagnostics_columns[] = {
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const division_warning_values[] = {"2", "0", "0"};
    static const char *const no_warning_values[] = {"0", "0", "0"};
    static const char *const selected_warning_values[] = {"1", "0", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning handle");
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "DO 5 DIV 0, MOD(5,0)",
            .warning_count = 2U,
            .context = "division warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .columns = diagnostics_columns,
            .column_count = diagnostics_column_count,
            .values = division_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "division warning diagnostics",
        }
    );
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "DO 0 AND 5 DIV 0, 1 OR 5 DIV 0, CASE WHEN 1 THEN 2 ELSE 5 DIV 0 END",
            .warning_count = 0U,
            .context = "short-circuit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .columns = diagnostics_columns,
            .column_count = diagnostics_column_count,
            .values = no_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "short-circuit diagnostics",
        }
    );
    failures += expect_statement(
        database,
        (struct expected_statement){
            .sql = "DO CASE WHEN 1 THEN 5 DIV 0 ELSE 2 END",
            .warning_count = 1U,
            .context = "selected CASE warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .columns = diagnostics_columns,
            .column_count = diagnostics_column_count,
            .values = selected_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "selected CASE warning diagnostics",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_do_statement_unsupported_forms(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open unsupported handle");

    failures += execute_error(
        database,
        "DO 'x'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO 1.5",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO 0x31",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO b'1'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO 1+1/0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DO supports only",
        }
    );
    failures += execute_error(
        database,
        "DO ?",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '?'",
        }
    );
    failures += execute_error(
        database,
        "DO @x := 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '@x'",
        }
    );
    failures += execute_error(
        database,
        "DO (SELECT 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'SELECT'",
        }
    );
    failures += execute_error(
        database,
        "DO !0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '!'",
        }
    );
    failures += execute_error(
        database,
        "DO 1 && 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '&&'",
        }
    );
    failures += execute_error(
        database,
        "DO 1 || 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '||'",
        }
    );
    failures += execute_error(
        database,
        "DO IFNULL(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'IFNULL'",
        }
    );
    failures += execute_error(
        database,
        "DO 9223372036854775807 + 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "DO",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "end of input",
        }
    );
    failures += execute_error(
        database,
        "DO 1 FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'FROM'",
        }
    );
    failures += execute_error(
        database,
        "DO 1 AS x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'AS'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_do_statement_independent_handles(void) {
    static const char *const columns[] = {"@@warning_count", "@@error_count", "ROW_COUNT()"};
    static const char *const first_values[] = {"1", "0", "0"};
    static const char *const second_values[] = {"0", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_statement(
        first,
        (struct expected_statement){
            .sql = "DO 5 DIV 0",
            .warning_count = 1U,
            .context = "first handle warning",
        }
    );
    failures += expect_statement(
        second,
        (struct expected_statement){
            .sql = "DO 1",
            .warning_count = 0U,
            .context = "second handle success",
        }
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .columns = columns,
            .column_count = diagnostics_column_count,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle diagnostics",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count, ROW_COUNT()",
            .columns = columns,
            .column_count = diagnostics_column_count,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle diagnostics",
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

static int expect_statement(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 0U, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
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
        "/tmp/mylite-do-statement-%s-%d.mylite",
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
