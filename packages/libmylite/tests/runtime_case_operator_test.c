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
    searched_column_count = 5,
    simple_column_count = 5,
    expression_column_count = 5,
    label_column_count = 3,
    short_circuit_column_count = 4,
    warning_column_count = 6,
    evaluated_warning_count = 6,
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
    int64_t affected_rows;
    const char *context;
};

static int test_case_operator_values_and_file_safety(void);
static int test_case_operator_warnings(void);
static int test_case_operator_unsupported_forms(void);
static int test_case_operator_independent_handles(void);
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

    failures += test_case_operator_values_and_file_safety();
    failures += test_case_operator_warnings();
    failures += test_case_operator_unsupported_forms();
    failures += test_case_operator_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_case_operator_values_and_file_safety(void) {
    static const char *const searched_columns[] = {
        "searched_true",
        "searched_false",
        "searched_null",
        "searched_negative",
        "no_match_no_else",
    };
    static const char *const searched_values[] = {"2", "3", "3", "2", NULL};
    static const char *const simple_columns[] = {
        "simple_first",
        "simple_second",
        "simple_else",
        "simple_null_equal",
        "simple_when_null",
    };
    static const char *const simple_values[] = {"10", "20", "30", "30", "30"};
    static const char *const expression_columns[] = {
        "comparison_condition",
        "is_condition",
        "logical_condition",
        "arithmetic_simple",
        "function_result",
    };
    static const char *const expression_values[] = {"2", "2", "4", "6", "8"};
    static const char *const label_columns[] = {
        "CASE WHEN 1 THEN 2 ELSE 3 END",
        "chosen",
        "parenthesized",
    };
    static const char *const label_values[] = {"2", "2", "2"};
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"-1"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 1 THEN 2 ELSE 3 END AS searched_true, "
                   "CASE WHEN 0 THEN 2 ELSE 3 END AS searched_false, "
                   "CASE WHEN NULL THEN 2 ELSE 3 END AS searched_null, "
                   "CASE WHEN -1 THEN 2 END AS searched_negative, "
                   "CASE WHEN 0 THEN 2 END AS no_match_no_else",
            .columns = searched_columns,
            .column_count = searched_column_count,
            .values = searched_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "searched CASE truth values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE 1 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_first, "
                   "CASE 2 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_second, "
                   "CASE 3 WHEN 1 THEN 10 WHEN 2 THEN 20 ELSE 30 END AS simple_else, "
                   "CASE NULL WHEN NULL THEN 10 ELSE 30 END AS simple_null_equal, "
                   "CASE 1 WHEN NULL THEN 10 ELSE 30 END AS simple_when_null",
            .columns = simple_columns,
            .column_count = simple_column_count,
            .values = simple_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "simple CASE matching",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 1=1 THEN 2 ELSE 3 END AS comparison_condition, "
                   "CASE WHEN 1 IS TRUE THEN 2 ELSE 3 END AS is_condition, "
                   "CASE WHEN 1 AND NOT 0 THEN 4 ELSE 5 END AS logical_condition, "
                   "CASE 1 + 1 WHEN 2 THEN 6 ELSE 7 END AS arithmetic_simple, "
                   "CASE WHEN IF(1, 1, 0) THEN COALESCE(NULL, 8) ELSE 9 END "
                   "AS function_result",
            .columns = expression_columns,
            .column_count = expression_column_count,
            .values = expression_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CASE child expressions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 1 THEN 2 ELSE 3 END, "
                   "CASE WHEN 1 THEN 2 ELSE 3 END chosen, "
                   "(CASE WHEN 1 THEN 2 ELSE 3 END) AS parenthesized FROM DUAL",
            .columns = label_columns,
            .column_count = label_column_count,
            .values = label_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CASE labels",
        }
    );
    failures += execute_ok(database, "SELECT CASE WHEN 1 THEN TRUE ELSE FALSE END", &result);
    failures += expect_size(mylite_result_column_count(result), 1U, "boolean CASE column count");
    failures += expect_result_value(result, 0U, 0U, "1", "boolean CASE result");
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
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row count after CASE select",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "CASE select leaves catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "CASE select leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read CASE preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "CASE select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_case_operator_warnings(void) {
    static const char *const short_circuit_columns[] = {
        "result_short_true",
        "result_short_false",
        "condition_short_true",
        "simple_when_short_true",
    };
    static const char *const short_circuit_values[] = {"2", "2", "2", "2"};
    static const char *const warning_columns[] = {
        "condition_warning",
        "simple_case_warning",
        "evaluated_second_condition",
        "evaluated_simple_compare",
        "selected_result_warning",
        "selected_simple_result_warning",
    };
    static const char *const warning_values[] = {"2", "3", "3", "3", NULL, NULL};
    static const char *const warning_count_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const zero_warning_count_values[] = {"0", "-1"};
    static const char *const six_warning_count_values[] = {"6", "-1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warnings memory");
    failures += execute_ok(database, "SELECT 1", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 1 THEN 2 ELSE 5 DIV 0 END AS result_short_true, "
                   "CASE WHEN 0 THEN 5 DIV 0 ELSE 2 END AS result_short_false, "
                   "CASE WHEN 1 THEN 2 WHEN 5 DIV 0 THEN 3 ELSE 4 END "
                   "AS condition_short_true, "
                   "CASE 1 WHEN 1 THEN 2 WHEN 5 DIV 0 THEN 3 ELSE 4 END "
                   "AS simple_when_short_true",
            .columns = short_circuit_columns,
            .column_count = short_circuit_column_count,
            .values = short_circuit_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CASE skipped warning expressions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = warning_count_columns,
            .column_count = 2U,
            .values = zero_warning_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "warning count after skipped CASE warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 5 DIV 0 THEN 1 ELSE 2 END AS condition_warning, "
                   "CASE 5 DIV 0 WHEN 1 THEN 1 WHEN 2 THEN 2 ELSE 3 END "
                   "AS simple_case_warning, "
                   "CASE WHEN 0 THEN 1 WHEN 5 DIV 0 THEN 2 ELSE 3 END "
                   "AS evaluated_second_condition, "
                   "CASE 2 WHEN 1 THEN 1 WHEN 5 DIV 0 THEN 2 ELSE 3 END "
                   "AS evaluated_simple_compare, "
                   "CASE WHEN 1 THEN 5 DIV 0 ELSE 2 END AS selected_result_warning, "
                   "CASE 1 WHEN 1 THEN 5 DIV 0 ELSE 2 END "
                   "AS selected_simple_result_warning",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = evaluated_warning_count,
            .affected_rows = 0,
            .context = "CASE evaluated warning expressions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = warning_count_columns,
            .column_count = 2U,
            .values = six_warning_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "warning count after evaluated CASE warnings",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_case_operator_unsupported_forms(void) {
    static const char *const row_case_columns[] = {"CASE WHEN id THEN 2 ELSE 3 END"};
    static const char *const row_case_values[] = {"2"};
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
    failures += execute_ok(database, "CREATE TABLE t(id INT NOT NULL)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);

    failures += execute_error(
        database,
        "SELECT CASE END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 'x' THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1 THEN 'x' ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1.5 THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE 0x31 WHEN 49 THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE b'1' WHEN 1 THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN @@warning_count THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1 THEN 2 ELSE 'x' END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 0 THEN 9223372036854775808 ELSE 1 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1 THEN CASE WHEN 0 THEN 2 ELSE 3 END ELSE 4 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE CASE WHEN 1 THEN 2 ELSE 3 END WHEN 2 THEN 4 ELSE 5 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports only",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CASE WHEN id THEN 2 ELSE 3 END FROM t",
            .columns = row_case_columns,
            .column_count = 1U,
            .values = row_case_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "row-column CASE condition",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE CASE WHEN 1 THEN TRUE ELSE FALSE END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1 THEN 2 ELSE 3 END LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN 1 THEN 2 ELSE 3 END ORDER BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'ORDER'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET id = CASE WHEN 1 THEN 2 ELSE 3 END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'CASE'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_case_operator_independent_handles(void) {
    static const char *const first_columns[] = {"first_result"};
    static const char *const first_values[] = {"10"};
    static const char *const second_columns[] = {"second_result"};
    static const char *const second_values[] = {"20"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT CASE WHEN 1 THEN 10 ELSE 0 END AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle CASE",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT CASE 0 WHEN 1 THEN 0 ELSE 20 END AS second_result",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle CASE",
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
        "/tmp/mylite-case-operator-%s-%d.mylite",
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
