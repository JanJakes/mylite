#include "mylite_test_support.h"

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
    core_column_count = 8,
    date_interval_unit_column_count = 9,
    alias_core_column_count = 12,
    alias_label_column_count = 3,
    label_column_count = 2,
    rollover_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
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

static int test_date_add_second_values_and_file_safety(void);
static int test_date_add_second_sql_modes_and_errors(void);
static int test_date_add_second_independent_handles(void);
static int test_date_sub_second_aliases_values_and_file_safety(void);
static int test_date_sub_second_aliases_sql_modes_and_errors(void);
static int test_date_sub_second_aliases_independent_handles(void);
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
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_date_add_second_values_and_file_safety();
    failures += test_date_add_second_sql_modes_and_errors();
    failures += test_date_add_second_independent_handles();
    failures += test_date_sub_second_aliases_values_and_file_safety();
    failures += test_date_sub_second_aliases_sql_modes_and_errors();
    failures += test_date_sub_second_aliases_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_date_add_second_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "DATE_ADD(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND)",
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND)",
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 0 SECOND)",
        "DATE_ADD('2008-01-02', INTERVAL 1 SECOND)",
        "DATE_ADD(NULL, INTERVAL 1 SECOND)",
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL NULL SECOND)",
        "@@warning_count",
    };
    static const char *const core_values[] = {
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:17",
        "2008-01-02 00:00:01",
        NULL,
        NULL,
        "0",
    };
    static const char *const label_columns[] = {
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "shifted",
    };
    static const char *const label_values[] = {
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
    };
    static const char *const rollover_columns[] = {
        "DATE_ADD('2024-02-28 23:59:59', INTERVAL 1 SECOND)",
        "DATE_ADD('2024-02-29 23:59:59', INTERVAL 1 SECOND)",
    };
    static const char *const rollover_values[] = {
        "2024-02-29 00:00:00",
        "2024-03-01 00:00:00",
    };
    static const char *const unit_columns[] = {
        "DATE_ADD('2024-01-31', INTERVAL 1 DAY)",
        "DATE_ADD('2024-01-31', INTERVAL 2 MINUTE)",
        "DATE_ADD('2024-01-31', INTERVAL 1 MONTH)",
        "DATE_ADD('2024-01-31', INTERVAL 1 QUARTER)",
        "DATE_ADD('2024-02-29 23:59:59', INTERVAL 1 YEAR)",
        "DATE_ADD('2024-01-01', INTERVAL '1' WEEK)",
        "DATE_SUB('2024-01-08', INTERVAL 1 WEEK)",
        "SUBDATE('2024-01-01 00:00:00', INTERVAL -2 HOUR)",
        "ADDDATE('2024-01-01 00:00:00', INTERVAL +30 MINUTE)",
    };
    static const char *const unit_values[] = {
        "2024-02-01",
        "2024-01-31 00:02:00",
        "2024-02-29",
        "2024-04-30",
        "2025-02-28 23:59:59",
        "2024-01-08",
        "2024-01-01",
        "2024-01-01 02:00:00",
        "2024-01-01 00:30:00",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open DATE_ADD values file"
    );
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
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND),"
                   "DATE_ADD(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND),"
                   "DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND),"
                   "DATE_ADD('2008-01-02 13:29:17', INTERVAL 0 SECOND),"
                   "DATE_ADD('2008-01-02', INTERVAL 1 SECOND),"
                   "DATE_ADD(NULL, INTERVAL 1 SECOND),"
                   "DATE_ADD('2008-01-02 13:29:17', INTERVAL NULL SECOND),"
                   "@@warning_count",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core DATE_ADD SECOND values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
                   "DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND) AS shifted "
                   "FROM DUAL",
            .columns = label_columns,
            .column_count = label_column_count,
            .values = label_values,
            .row_count = 1U,
            .context = "DATE_ADD labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2024-02-28 23:59:59', INTERVAL 1 SECOND),"
                   "DATE_ADD('2024-02-29 23:59:59', INTERVAL 1 SECOND)",
            .columns = rollover_columns,
            .column_count = rollover_column_count,
            .values = rollover_values,
            .row_count = 1U,
            .context = "DATE_ADD leap rollover",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2024-01-31', INTERVAL 1 DAY),"
                   "DATE_ADD('2024-01-31', INTERVAL 2 MINUTE),"
                   "DATE_ADD('2024-01-31', INTERVAL 1 MONTH),"
                   "DATE_ADD('2024-01-31', INTERVAL 1 QUARTER),"
                   "DATE_ADD('2024-02-29 23:59:59', INTERVAL 1 YEAR),"
                   "DATE_ADD('2024-01-01', INTERVAL '1' WEEK),"
                   "DATE_SUB('2024-01-08', INTERVAL 1 WEEK),"
                   "SUBDATE('2024-01-01 00:00:00', INTERVAL -2 HOUR),"
                   "ADDDATE('2024-01-01 00:00:00', INTERVAL +30 MINUTE)",
            .columns = unit_columns,
            .column_count = date_interval_unit_column_count,
            .values = unit_values,
            .row_count = 1U,
            .context = "DATE interval core units",
        }
    );

    failures += execute_ok(
        database,
        "DO DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "DATE_ADD(NULL, INTERVAL 1 SECOND)",
        &result
    );
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "DATE_ADD DO columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "DATE_ADD DO rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "DATE_ADD DO affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "DATE_ADD DO warnings"
        );
    }
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
            .context = "row count after DATE_ADD DO",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "DATE_ADD leaves catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "DATE_ADD leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read DATE_ADD preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "DATE_ADD leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_date_add_second_sql_modes_and_errors(void) {
    static const char *const no_backslash_columns[] = {
        "DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)"
    };
    static const char *const no_backslash_values[] = {"2008-01-02 13:29:18"};
    static const char *const ignore_space_columns[] = {
        "DATE_ADD ('2008-01-02 13:29:17', INTERVAL 1 SECOND)"
    };
    static const char *const ignore_space_values[] = {"2008-01-02 13:29:18"};
    static const char *const table_columns[] = {"DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)"
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open DATE_ADD errors file"
    );
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += execute_error(
        database,
        "CREATE TABLE date_add(id INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "CREATE TABLE date_add (id INT)", NULL);
    failures += execute_ok(database, "DROP TABLE date_add", NULL);
    failures += execute_error(
        database,
        "SELECT DATE_ADD ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT DATE_ADD(\"2008-01-02 13:29:17\", INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
            .columns = no_backslash_columns,
            .column_count = 1U,
            .values = no_backslash_values,
            .row_count = 1U,
            .context = "DATE_ADD after NO_BACKSLASH_ESCAPES",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
            .columns = ignore_space_columns,
            .column_count = 1U,
            .values = ignore_space_values,
            .row_count = 1U,
            .context = "DATE_ADD after IGNORE_SPACE",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE date_add (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "CREATE TABLE `date_add` (id INT)", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_error(
        database,
        "SELECT DATE_ADD(1, INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() supports only date or datetime string literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('2016-07-00', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_ADD() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL '1x' SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_ADD() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 9223372036854775808 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() INTERVAL SECOND literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('9999-12-31 23:59:59', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('1000-01-01 00:00:00', INTERVAL -1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 MICROSECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, "
                            "MINUTE, and SECOND interval units",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1+1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_ADD() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND) FROM t",
            .columns = table_columns,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "DATE_ADD table-backed literal projection over empty table",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_date_add_second_independent_handles(void) {
    static const char *const first_columns[] = {"first_result"};
    static const char *const first_values[] = {"2008-01-02 13:29:18"};
    static const char *const second_columns[] = {"second_result"};
    static const char *const second_values[] = {"2008-01-02 13:29:16"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first DATE_ADD handle");
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second DATE_ADD handle"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first DATE_ADD handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT DATE_ADD('2008-01-02 13:29:17', INTERVAL -1 SECOND) AS second_result",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .context = "second DATE_ADD handle",
        }
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int test_date_sub_second_aliases_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "DATE_SUB(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND)",
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL -1 SECOND)",
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL 0 SECOND)",
        "DATE_SUB('2008-01-02', INTERVAL 1 SECOND)",
        "DATE_SUB(NULL, INTERVAL 1 SECOND)",
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL NULL SECOND)",
        "ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "ADDDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND)",
        "SUBDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "SUBDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND)",
        "@@warning_count",
    };
    static const char *const core_values[] = {
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:17",
        "2008-01-01 23:59:59",
        NULL,
        NULL,
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:18",
        "0",
    };
    static const char *const label_columns[] = {
        "DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "adddate_alias",
        "subdate_alias",
    };
    static const char *const label_values[] = {
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
    };
    static const char *const rollover_columns[] = {
        "DATE_SUB('2024-02-29 00:00:00', INTERVAL 1 SECOND)",
        "SUBDATE('2024-02-28 23:59:59', INTERVAL -1 SECOND)",
    };
    static const char *const rollover_values[] = {
        "2024-02-28 23:59:59",
        "2024-02-29 00:00:00",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "date-sub-alias-values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open DATE_SUB alias values file"
    );
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
            .sql = "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND),"
                   "DATE_SUB(\"2008-01-02 13:29:17\", INTERVAL +1 SECOND),"
                   "DATE_SUB('2008-01-02 13:29:17', INTERVAL -1 SECOND),"
                   "DATE_SUB('2008-01-02 13:29:17', INTERVAL 0 SECOND),"
                   "DATE_SUB('2008-01-02', INTERVAL 1 SECOND),"
                   "DATE_SUB(NULL, INTERVAL 1 SECOND),"
                   "DATE_SUB('2008-01-02 13:29:17', INTERVAL NULL SECOND),"
                   "ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND),"
                   "ADDDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND),"
                   "SUBDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND),"
                   "SUBDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND),"
                   "@@warning_count",
            .columns = core_columns,
            .column_count = alias_core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core DATE_SUB alias SECOND values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
                   "ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS adddate_alias, "
                   "SUBDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS subdate_alias "
                   "FROM DUAL",
            .columns = label_columns,
            .column_count = alias_label_column_count,
            .values = label_values,
            .row_count = 1U,
            .context = "DATE_SUB alias labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_SUB('2024-02-29 00:00:00', INTERVAL 1 SECOND),"
                   "SUBDATE('2024-02-28 23:59:59', INTERVAL -1 SECOND)",
            .columns = rollover_columns,
            .column_count = rollover_column_count,
            .values = rollover_values,
            .row_count = 1U,
            .context = "DATE_SUB alias leap rollover",
        }
    );

    failures += execute_ok(
        database,
        "DO DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
        "ADDDATE(NULL, INTERVAL 1 SECOND), "
        "SUBDATE('2008-01-02 13:29:17', INTERVAL NULL SECOND)",
        &result
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "DATE_SUB alias DO columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "DATE_SUB alias DO rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "DATE_SUB alias DO affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "DATE_SUB alias DO warnings"
        );
    }
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
            .context = "row count after DATE_SUB alias DO",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "DATE_SUB aliases leave catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "DATE_SUB aliases leave sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read DATE_SUB alias preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "DATE_SUB aliases leave preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_date_sub_second_aliases_sql_modes_and_errors(void) {
    static const char *const ignore_space_columns[] = {
        "DATE_SUB ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "ADDDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        "SUBDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
    };
    static const char *const ignore_space_values[] = {
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:18",
        "2008-01-02 13:29:16",
    };
    static const char *const table_columns[] = {"DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND)"
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "date-sub-alias-errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open DATE_SUB alias errors file"
    );
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_error(
        database,
        "SELECT DATE_SUB ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE date_sub(id INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "CREATE TABLE date_sub (id INT)", NULL);
    failures += execute_ok(database, "DROP TABLE date_sub", NULL);
    failures += execute_ok(database, "CREATE TABLE adddate(id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE subdate(id INT)", NULL);
    failures += execute_ok(database, "DROP TABLE adddate", NULL);
    failures += execute_ok(database, "DROP TABLE subdate", NULL);

    failures += execute_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_SUB ('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
                   "ADDDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND), "
                   "SUBDATE ('2008-01-02 13:29:17', INTERVAL 1 SECOND)",
            .columns = ignore_space_columns,
            .column_count = alias_label_column_count,
            .values = ignore_space_values,
            .row_count = 1U,
            .context = "DATE_SUB aliases after IGNORE_SPACE",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE date_sub (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_ok(database, "CREATE TABLE `date_sub` (id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE adddate(id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE subdate(id INT)", NULL);
    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT DATE_SUB(\"2008-01-02 13:29:17\", INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_error(
        database,
        "SELECT DATE_SUB(1, INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_SUB() supports only date or datetime string literals and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBDATE('2016-07-00', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SUBDATE() supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL '1x' SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_SUB() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDDATE('2008-01-02 13:29:17', INTERVAL 9223372036854775808 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ADDDATE() INTERVAL SECOND literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB('1000-01-01 00:00:00', INTERVAL 1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_SUB() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUBDATE('9999-12-31 23:59:59', INTERVAL -1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SUBDATE() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL -9223372036854775808 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_SUB() result is outside the supported datetime range",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 MICROSECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_SUB() supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, "
                            "MINUTE, and SECOND interval units",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1+1 SECOND)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_SUB() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDDATE('2008-01-02', 31)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND) FROM t",
            .columns = table_columns,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "DATE_SUB table-backed literal projection over empty table",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_date_sub_second_aliases_independent_handles(void) {
    static const char *const first_columns[] = {"first_result"};
    static const char *const first_values[] = {"2008-01-02 13:29:16"};
    static const char *const second_columns[] = {"second_result", "third_result"};
    static const char *const second_values[] = {"2008-01-02 13:29:18", "2008-01-02 13:29:18"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first DATE_SUB alias handle"
    );
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second DATE_SUB alias handle"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT DATE_SUB('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS first_result",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .context = "first DATE_SUB alias handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT ADDDATE('2008-01-02 13:29:17', INTERVAL 1 SECOND) AS second_result, "
                   "SUBDATE('2008-01-02 13:29:17', INTERVAL -1 SECOND) AS third_result",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .context = "second DATE_SUB alias handle",
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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

    return mylite_test_expect_text(actual, expected, context);
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
