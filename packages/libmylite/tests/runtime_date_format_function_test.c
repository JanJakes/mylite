#include <mylite/mylite.h>

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

static int test_no_source_dual_and_do_date_format(void);
static int test_table_backed_date_format(void);
static int test_date_format_predicates(void);
static int test_date_format_diagnostics(void);
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

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_date_format();
    failures += test_table_backed_date_format();
    failures += test_date_format_predicates();
    failures += test_date_format_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_date_format(void) {
    static const char tokens_expression[] =
        "DATE_FORMAT('2008-01-02 13:29:17', "
        "'%Y|%y|%m|%c|%d|%e|%H|%k|%h|%I|%l|%i|%S|%s|%T|%r|%p|%f|%%|%q|%')";
    static const char expected_tokens[] =
        "2008|08|01|1|02|2|13|13|01|01|1|29|17|17|13:29:17|01:29:17 "
        "PM|PM|000000|%|q|%";
    static const char *const columns_tokens[] = {
        tokens_expression,
        "labels",
        "DATE_FORMAT(NULL, '%Y')",
        "DATE_FORMAT('2008-01-02', NULL)",
        "DATE_FORMAT('2008-01-02', '%Y-%m-%d %H:%i:%s')",
        "DATE_FORMAT('2008-01-02 00:42:00', '%H.%i') = 0.42",
        "DATE_FORMAT('2008-01-02 13:29:17', '%H.%i') >= 12",
    };
    static const char *const values_tokens[] = {
        expected_tokens,
        "Wed|Wednesday|Jan|January|2nd|002|3",
        NULL,
        NULL,
        "2008-01-02 00:00:00",
        "1",
        "1",
    };
    static const char *const columns_dual[] = {"DATE_FORMAT ('2008-01-02', '%Y')", "fmt"};
    static const char *const values_dual[] = {"2008", "2008-01-02"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
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
            .sql = "SELECT DATE_FORMAT('2008-01-02 13:29:17', "
                   "'%Y|%y|%m|%c|%d|%e|%H|%k|%h|%I|%l|%i|%S|%s|%T|%r|%p|"
                   "%f|%%|%q|%'), "
                   "DATE_FORMAT('2008-01-02 13:29:17', "
                   "'%a|%W|%b|%M|%D|%j|%w') AS labels, "
                   "DATE_FORMAT(NULL, '%Y'), DATE_FORMAT('2008-01-02', NULL), "
                   "DATE_FORMAT('2008-01-02', '%Y-%m-%d %H:%i:%s'), "
                   "DATE_FORMAT('2008-01-02 00:42:00', '%H.%i') = 0.42, "
                   "DATE_FORMAT('2008-01-02 13:29:17', '%H.%i') >= 12",
            .columns = columns_tokens,
            .column_count = sizeof(columns_tokens) / sizeof(columns_tokens[0]),
            .values = values_tokens,
            .row_count = 1U,
            .context = "no-source date_format",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_FORMAT ('2008-01-02', '%Y'), "
                   "DATE_FORMAT(\"2008-01-02\", \"%Y-%m-%d\") AS fmt FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual date_format",
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
            .context = "row count after date_format select",
        }
    );

    failures += execute_ok(
        database,
        "DO DATE_FORMAT('2008-01-02', '%Y'), DATE_FORMAT(NULL, '%Y')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "date_format do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "date_format do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "date_format do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "date_format do warnings");
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
            .context = "row count after date_format do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_date_format(void) {
    static const char *const columns_table[] = {
        "id",
        "time_text",
        "date_text",
        "datetime_text",
        "timestamp_text",
        "text_value",
    };
    static const char *const values_table[] = {
        "1",
        "00.42",
        "2024-01-02 00:00:00",
        "13:29:17",
        "2008",
        "Jan 2nd",
        "2",
        "13.29",
        "2024-02-29 00:00:00",
        "23:59:59",
        "2024",
        "Feb 29th",
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_comparison[] = {"id", "matched"};
    static const char *const values_comparison[] = {"1", "1", "2", "0", "3", NULL};
    static const char *const columns_limited[] = {"DATE_FORMAT(option_value, '%H:%i')"};
    static const char *const values_limited[] = {NULL, "13:29"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE options("
        "id INT, option_value VARCHAR(32), d DATE, dt DATETIME, ts TIMESTAMP NULL, "
        "txt TEXT, tm TIME"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO options VALUES "
        "(1, '2008-01-02 00:42:00', '2024-01-02', '2008-01-02 13:29:17', "
        "'2008-01-02 13:29:17', '2008-01-02', '01:02:03'), "
        "(2, '2008-01-02 13:29:17', '2024-02-29', '2024-02-29 23:59:59', "
        "'2024-02-29 23:59:59', '2024-02-29', '02:03:04'), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE_FORMAT(option_value, '%H.%i') AS time_text, "
                   "DATE_FORMAT(d, '%Y-%m-%d %H:%i:%s') AS date_text, "
                   "DATE_FORMAT(dt, '%T') AS datetime_text, "
                   "DATE_FORMAT(ts, '%Y') AS timestamp_text, "
                   "DATE_FORMAT(txt, '%b %D') AS text_value "
                   "FROM options ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table date_format projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE_FORMAT(option_value, '%H.%i') = 0.42 AS matched "
                   "FROM options ORDER BY id",
            .columns = columns_comparison,
            .column_count = sizeof(columns_comparison) / sizeof(columns_comparison[0]),
            .values = values_comparison,
            .row_count = 3U,
            .context = "table date_format numeric comparison",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_FORMAT(option_value, '%H:%i') FROM options "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table date_format where order limit",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_date_format_predicates(void) {
    static const char *const columns_id[] = {"id"};
    static const char *const values_1_5[] = {"1", "5"};
    static const char *const values_2[] = {"2"};
    static const char *const values_1_2[] = {"1", "2"};
    static const char *const values_1_2_5[] = {"1", "2", "5"};
    static const char *const values_5[] = {"5"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const invalid_warning_values[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "predicates", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE options("
        "id INT, option_value VARCHAR(32), d DATE NULL, dt DATETIME NULL, "
        "ts TIMESTAMP NULL, tm TIME NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO options VALUES "
        "(1, '2008-01-02 00:42:00', '2008-01-02', '2008-01-02 00:42:00', "
        "'2008-01-02 00:42:00', '00:42:00'), "
        "(2, '2008-01-02 13:29:17', '2008-01-02', '2008-01-02 13:29:17', "
        "'2008-01-02 13:29:17', '13:29:17'), "
        "(3, NULL, NULL, NULL, NULL, NULL), "
        "(4, 'not-a-date', NULL, NULL, NULL, NULL), "
        "(5, '2008-01-02 00:42:59', NULL, NULL, NULL, NULL)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_1_5,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "date_format predicate decimal",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = invalid_warning_values,
            .row_count = 1U,
            .context = "date_format predicate invalid warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = +0.42 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_1_5,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "date_format predicate unary positive",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = -0.42 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 1U,
            .context = "date_format predicate unary negative",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 13.29 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate hour minute match",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i%s') = "
                   "13.291700 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate hour minute second match",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') >= 0.42 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_1_2_5,
            .row_count = 3U,
            .warning_count = 1U,
            .context = "date_format predicate inclusive lower bound",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') >= 9.00 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate greater equal",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') >= 9.00 "
                   "AND DATE_FORMAT(option_value, '%H.%i') <= 17.00 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate closed range",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') <> 0.42 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate not equal",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') < 1.00 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_1_5,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "date_format predicate less than",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(d, '%H.%i') = 0.00 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_1_2,
            .row_count = 2U,
            .context = "date_format date predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(dt, '%H.%i') = 13.29 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .context = "date_format datetime predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(ts, '%H.%i') = 13.29 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_2,
            .row_count = 1U,
            .context = "date_format timestamp predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42 "
                   "ORDER BY id DESC LIMIT 1",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_5,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "date_format predicate order limit",
        }
    );

    failures += execute_ok(database, "CREATE TABLE other(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO other VALUES (1)", NULL);
    failures += execute_error(
        database,
        "SELECT options.id FROM options JOIN other ON options.id = other.id "
        "WHERE DATE_FORMAT(options.option_value, '%H.%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric predicates support only one descriptor table source",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(missing, '%H.%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H:%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i' or "
                "'%H.%i%s')",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') <=> 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format)",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = '0.42'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax near",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax near",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 0 + 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM options WHERE DATE_FORMAT(tm, '%H.%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_FORMAT() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE options SET id = 10 WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor column WHERE predicates",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor column WHERE predicates",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_date_format_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(32), tm TIME)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '2008-01-02', '01:02:03')", NULL);

    failures += execute_ok(database, "SELECT DATE_FORMAT('not-a-date', '%Y')", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 1U, "invalid date_format column count");
        failures +=
            expect_size(mylite_result_row_count(result), 1U, "invalid date_format row count");
        failures += expect_result_value(result, 0U, 0U, NULL, "invalid date_format value");
        failures +=
            expect_size(mylite_result_warning_count(result), 1U, "invalid date_format warnings");
    }
    mylite_result_free(result);

    failures += execute_error(
        database,
        "SELECT DATE_FORMAT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'DATE_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'DATE_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02', '%Y', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'DATE_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT(missing, '%Y') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT(v, v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_FORMAT() supports only string format literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT(tm, '%H:%i:%s') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_FORMAT() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02', '%U')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_FORMAT() does not yet support week-based format specifiers",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT(1, '%Y')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and "
                            "NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02', '%Y') = '2008'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format)",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT('2008-01-02', '%Y-%m-%d') = 2008",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i' or "
                "'%H.%i%s')",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_FORMAT(v, '%Y-%m-%d') = 2008 FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i' or "
                "'%H.%i%s')",
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
        "/tmp/mylite-date-format-function-%s-%d.mylite",
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
