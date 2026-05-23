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
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
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

static int test_no_source_dual_and_do_str_to_date(void);
static int test_table_backed_str_to_date_and_reopen(void);
static int test_str_to_date_warnings_and_sql_modes(void);
static int test_str_to_date_diagnostics(void);
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
static int expect_bytes(const void *actual, const void *expected, size_t size, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_str_to_date();
    failures += test_table_backed_str_to_date_and_reopen();
    failures += test_str_to_date_warnings_and_sql_modes();
    failures += test_str_to_date_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_str_to_date(void) {
    static const char *const columns_values[] = {
        "STR_TO_DATE('01,5,2013', '%d,%m,%Y')",
        "STR_TO_DATE('2024-01-02 03:04:05', '%Y-%m-%d %H:%i:%s')",
        "STR_TO_DATE('09:30:17', '%H:%i:%s')",
        "STR_TO_DATE('11:22 PM', '%h:%i %p')",
        "STR_TO_DATE('12:05 AM', '%h:%i %p')",
        "STR_TO_DATE('23:22:01', '%T')",
        "STR_TO_DATE('11:22:01 PM', '%r')",
        "STR_TO_DATE(NULL, '%Y-%m-%d')",
        "STR_TO_DATE('2024-01-02', NULL)",
        "STR_TO_DATE(NULL, '%f')",
        "STR_TO_DATE(1, NULL)",
        "STR_TO_DATE('20240102', '%Y%m%d')",
        "STR_TO_DATE('69-01-02', '%y-%m-%d')",
        "STR_TO_DATE('70-01-02', '%y-%m-%d')",
        "STR_TO_DATE('123-01-02', '%Y-%m-%d')",
        "STR_TO_DATE('00/00/0000', '%m/%d/%Y')",
        "STR_TO_DATE('9', '%m')",
        "STR_TO_DATE('9', '%s')",
    };
    static const char *const values[] = {
        "2013-05-01",
        "2024-01-02 03:04:05",
        "09:30:17",
        "23:22:00",
        "00:05:00",
        "23:22:01",
        "23:22:01",
        NULL,
        NULL,
        NULL,
        NULL,
        "2024-01-02",
        "2069-01-02",
        "1970-01-02",
        "0123-01-02",
        "0000-00-00",
        "0000-09-00",
        "00:00:09",
    };
    static const char *const columns_dual[] = {
        "STR_TO_DATE ('2024-01-02', '%Y-%m-%d')",
        "parsed",
    };
    static const char *const values_dual[] = {"2024-01-02", "09:30:17"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('01,5,2013', '%d,%m,%Y'), "
                   "STR_TO_DATE('2024-01-02 03:04:05', '%Y-%m-%d %H:%i:%s'), "
                   "STR_TO_DATE('09:30:17', '%H:%i:%s'), "
                   "STR_TO_DATE('11:22 PM', '%h:%i %p'), "
                   "STR_TO_DATE('12:05 AM', '%h:%i %p'), "
                   "STR_TO_DATE('23:22:01', '%T'), "
                   "STR_TO_DATE('11:22:01 PM', '%r'), "
                   "STR_TO_DATE(NULL, '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-01-02', NULL), "
                   "STR_TO_DATE(NULL, '%f'), "
                   "STR_TO_DATE(1, NULL), "
                   "STR_TO_DATE('20240102', '%Y%m%d'), "
                   "STR_TO_DATE('69-01-02', '%y-%m-%d'), "
                   "STR_TO_DATE('70-01-02', '%y-%m-%d'), "
                   "STR_TO_DATE('123-01-02', '%Y-%m-%d'), "
                   "STR_TO_DATE('00/00/0000', '%m/%d/%Y'), "
                   "STR_TO_DATE('9', '%m'), STR_TO_DATE('9', '%s')",
            .columns = columns_values,
            .column_count = sizeof(columns_values) / sizeof(columns_values[0]),
            .values = values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "scalar str_to_date values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE ('2024-01-02', '%Y-%m-%d'), "
                   "STR_TO_DATE(\"09:30:17\", \"%H:%i:%s\") AS parsed FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual str_to_date",
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
            .context = "row count after str_to_date select",
        }
    );

    failures += execute_ok(
        database,
        "DO STR_TO_DATE('2024-01-02', '%Y-%m-%d'), STR_TO_DATE(NULL, '%Y-%m-%d')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "str_to_date do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "str_to_date do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "str_to_date do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "str_to_date do warnings");
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
            .context = "row count after str_to_date do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_str_to_date_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "STR_TO_DATE(v, '%Y-%m-%d')",
        "STR_TO_DATE(body, '%H:%i:%s')",
    };
    static const char *const values_table[] = {
        "1",
        "2024-01-02",
        "09:30:17",
        "2",
        "2024-01-02",
        "09:30:17",
        "3",
        NULL,
        NULL,
        "4",
        NULL,
        NULL,
    };
    static const char *const columns_reopen[] = {
        "STR_TO_DATE(v, '%Y-%m-%d')",
        "STR_TO_DATE(body, '%H:%i:%s')",
    };
    static const char *const values_reopen[] = {"2024-01-02", "09:30:17"};
    static const char *const columns_null_short_circuit[] = {
        "STR_TO_DATE(n, NULL)",
        "STR_TO_DATE(NULL, v)",
    };
    static const char *const values_null_short_circuit[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_nested_null_short_circuit[] = {"STR_TO_DATE(NULL, n + 1)"};
    static const char *const values_nested_null_short_circuit[] = {NULL, NULL, NULL, NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    mylite_file_preamble_init(expected_preamble);
    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(32), body TEXT, n INT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2024-01-02', '09:30:17', 1), "
        "(2, '2024-01-02x', '09:30:17a', 2), "
        "(3, 'bad', NULL, 3), "
        "(4, NULL, 'bad', 4)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, STR_TO_DATE(v, '%Y-%m-%d'), STR_TO_DATE(body, '%H:%i:%s') "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .warning_count = 4U,
            .context = "table str_to_date values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE(n, NULL), STR_TO_DATE(NULL, v) FROM t ORDER BY id",
            .columns = columns_null_short_circuit,
            .column_count =
                sizeof(columns_null_short_circuit) / sizeof(columns_null_short_circuit[0]),
            .values = values_null_short_circuit,
            .row_count = 4U,
            .warning_count = 0U,
            .context = "table str_to_date null short circuit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE(NULL, n + 1) FROM t ORDER BY id",
            .columns = columns_nested_null_short_circuit,
            .column_count = sizeof(columns_nested_null_short_circuit) /
                            sizeof(columns_nested_null_short_circuit[0]),
            .values = values_nested_null_short_circuit,
            .row_count = 4U,
            .warning_count = 0U,
            .context = "table str_to_date nested null short circuit",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "str_to_date preamble unchanged"
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen str_to_date file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE(v, '%Y-%m-%d'), STR_TO_DATE(body, '%H:%i:%s') "
                   "FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopen str_to_date values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_str_to_date_warnings_and_sql_modes(void) {
    static const char *const column_trailing[] = {"STR_TO_DATE('2024-01-02x', '%Y-%m-%d')"};
    static const char *const value_trailing[] = {"2024-01-02"};
    static const char *const column_bad[] = {"STR_TO_DATE('bad', '%Y-%m-%d')"};
    static const char *const value_null[] = {NULL};
    static const char *const column_zero[] = {"STR_TO_DATE('00/00/0000', '%m/%d/%Y')"};
    static const char *const value_zero[] = {"0000-00-00"};
    static const char *const columns_no_zero_in_date[] = {
        "STR_TO_DATE('0000-09-01', '%Y-%m-%d')",
        "STR_TO_DATE('0000-00-00', '%Y-%m-%d')",
        "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
    };
    static const char *const values_no_zero_in_date[] = {"0000-09-01", "0000-00-00", NULL};
    static const char *const columns_orphan_meridiem[] = {
        "STR_TO_DATE('PM', '%p')",
        "STR_TO_DATE('PM 11', '%p %h')",
    };
    static const char *const values_orphan_meridiem[] = {NULL, NULL};
    static const char *const columns_allow_invalid[] = {
        "STR_TO_DATE('2024-02-31', '%Y-%m-%d')",
        "STR_TO_DATE('2024-13-01', '%Y-%m-%d')",
        "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
    };
    static const char *const values_allow_invalid[] = {"2024-02-31", NULL, "2024-00-01"};
    static const char *const columns_allow_invalid_nozero[] = {
        "STR_TO_DATE('2024-02-31', '%Y-%m-%d')",
        "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
        "STR_TO_DATE('0000-00-00', '%Y-%m-%d')",
    };
    static const char *const values_allow_invalid_nozero[] = {"2024-02-31", NULL, "0000-00-00"};
    static const char *const columns_allow_invalid_no_zero_date[] = {
        "STR_TO_DATE('2024-02-31', '%Y-%m-%d')",
        "STR_TO_DATE('0000-02-31', '%Y-%m-%d')",
        "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
    };
    static const char *const values_allow_invalid_no_zero_date[] = {"2024-02-31", NULL, NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "warnings", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('2024-01-02x', '%Y-%m-%d')",
            .columns = column_trailing,
            .column_count = 1U,
            .values = value_trailing,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "trailing str_to_date warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('bad', '%Y-%m-%d')",
            .columns = column_bad,
            .column_count = 1U,
            .values = value_null,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid str_to_date warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('PM', '%p'), STR_TO_DATE('PM 11', '%p %h')",
            .columns = columns_orphan_meridiem,
            .column_count = sizeof(columns_orphan_meridiem) / sizeof(columns_orphan_meridiem[0]),
            .values = values_orphan_meridiem,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "orphan meridiem str_to_date warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('00/00/0000', '%m/%d/%Y')",
            .columns = column_zero,
            .column_count = 1U,
            .values = value_zero,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "relaxed zero str_to_date",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ZERO_DATE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('00/00/0000', '%m/%d/%Y')",
            .columns = column_zero,
            .column_count = 1U,
            .values = value_null,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "no zero date str_to_date warning",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_ZERO_IN_DATE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('0000-09-01', '%Y-%m-%d'), "
                   "STR_TO_DATE('0000-00-00', '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
            .columns = columns_no_zero_in_date,
            .column_count = sizeof(columns_no_zero_in_date) / sizeof(columns_no_zero_in_date[0]),
            .values = values_no_zero_in_date,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "no zero in date str_to_date warning",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'ALLOW_INVALID_DATES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('2024-02-31', '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-13-01', '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
            .columns = columns_allow_invalid,
            .column_count = sizeof(columns_allow_invalid) / sizeof(columns_allow_invalid[0]),
            .values = values_allow_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "allow invalid dates str_to_date",
        }
    );
    failures +=
        execute_ok(database, "SET SESSION sql_mode = 'ALLOW_INVALID_DATES,NO_ZERO_IN_DATE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('2024-02-31', '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-00-01', '%Y-%m-%d'), "
                   "STR_TO_DATE('0000-00-00', '%Y-%m-%d')",
            .columns = columns_allow_invalid_nozero,
            .column_count =
                sizeof(columns_allow_invalid_nozero) / sizeof(columns_allow_invalid_nozero[0]),
            .values = values_allow_invalid_nozero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "allow invalid dates no zero in date str_to_date",
        }
    );
    failures +=
        execute_ok(database, "SET SESSION sql_mode = 'ALLOW_INVALID_DATES,NO_ZERO_DATE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT STR_TO_DATE('2024-02-31', '%Y-%m-%d'), "
                   "STR_TO_DATE('0000-02-31', '%Y-%m-%d'), "
                   "STR_TO_DATE('2024-00-01', '%Y-%m-%d')",
            .columns = columns_allow_invalid_no_zero_date,
            .column_count = sizeof(columns_allow_invalid_no_zero_date) /
                            sizeof(columns_allow_invalid_no_zero_date[0]),
            .values = values_allow_invalid_no_zero_date,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "allow invalid dates no zero date str_to_date",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_str_to_date_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(32), n INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '2024-01-02', 1)", NULL);
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'STR_TO_DATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE('2024-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'STR_TO_DATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE('2024-01-02', '%Y', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'STR_TO_DATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(missing, '%Y') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(missing, NULL) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(NULL, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(NULL, missing + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(missing + 1, NULL) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(NULL, missing + 1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(v, v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STR_TO_DATE() supports only string format literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(n, '%Y') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STR_TO_DATE() supports only nonbinary string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE(1, '%Y')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STR_TO_DATE() supports only string and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT STR_TO_DATE('2024-01-02', '%f')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "STR_TO_DATE() supports only baseline date and time format specifiers",
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
        "/tmp/mylite-str-to-date-function-%s-%d.mylite",
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
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_size != size) {
        fprintf(stderr, "%s: expected to read %zu bytes, got %zu\n", path, size, read_size);
        return 1;
    }
    return 0;
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
