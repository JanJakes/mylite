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
    show_warnings_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_column_specified_twice = 1110,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_warnings {
    size_t row_count;
    const char *const *levels;
    const char *const *codes;
    const char *const *message_parts;
    const char *context;
};

struct expected_dml {
    const char *sql;
    int64_t affected_rows;
    size_t warning_count;
};

static const char *const show_warnings_names[show_warnings_column_count] = {
    "Level",
    "Code",
    "Message",
};

static int test_insert_ignore_adjustments_and_persistence(void);
static int test_insert_ignore_schema_resolution_and_diagnostics(void);
static int test_insert_ignore_independent_handles(void);
static int seed_schema(mylite_db *database);
static int create_ignore_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, struct expected_dml expectation);
static int expect_warning_count(mylite_db *database, const char *expected, const char *context);
static int expect_show_warnings(mylite_db *database, struct expected_warnings expectation);
static int expect_query_values(mylite_db *database, struct expected_query query);
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
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_insert_ignore_adjustments_and_persistence();
    failures += test_insert_ignore_schema_resolution_and_diagnostics();
    failures += test_insert_ignore_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_insert_ignore_adjustments_and_persistence(void) {
    enum {
        adjustment_warning_count = 11,
        adjusted_values_column_count = 14,
    };

    static const char *const adjustment_levels[] = {
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
        "Warning",
    };
    static const char *const adjustment_codes[] = {
        "1048",
        "1264",
        "1264",
        "1264",
        "1264",
        "1264",
        "1264",
        "1264",
        "1264",
        "1264",
        "1048",
    };
    static const char *const adjustment_messages[] = {
        "Column 'id' cannot be null",
        "Out of range value for column 'ti' at row 2",
        "Out of range value for column 'tu' at row 2",
        "Out of range value for column 'si' at row 2",
        "Out of range value for column 'su' at row 2",
        "Out of range value for column 'mi' at row 2",
        "Out of range value for column 'mu' at row 2",
        "Out of range value for column 'i' at row 2",
        "Out of range value for column 'iu' at row 2",
        "Out of range value for column 'b' at row 2",
        "Column 'nn' cannot be null",
    };
    static const char *const adjusted_rows[] = {
        "0",
        "-128",
        "255",
        "-32768",
        "65535",
        "-8388608",
        "16777215",
        "-2147483648",
        "4294967295",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "0",
        "7",
        "1",
        "-128",
        "255",
        "-32768",
        "65535",
        "-8388608",
        "16777215",
        "-2147483648",
        "4294967295",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "10",
        "7",
    };
    static const char *const omitted_levels[] = {"Warning"};
    static const char *const omitted_codes[] = {"1364"};
    static const char *const omitted_messages[] = {
        "Field 'nn' doesn't have a default value",
    };
    static const char *const omitted_rows[] = {
        "10",
        "7",
        NULL,
        "0",
        "11",
        "7",
        NULL,
        "0",
    };
    static const char *const drop_default_levels[] = {"Warning"};
    static const char *const drop_default_codes[] = {"1364"};
    static const char *const drop_default_messages[] = {
        "Field 'n' doesn't have a default value",
    };
    static const char *const drop_default_rows[] = {
        "20",
        NULL,
        "21",
        NULL,
    };
    static const char *const mixed_levels[] = {"Warning", "Warning", "Warning"};
    static const char *const mixed_codes[] = {"1264", "1364", "1264"};
    static const char *const mixed_messages[] = {
        "Out of range value for column 'a' at row 1",
        "Field 'b' doesn't have a default value",
        "Out of range value for column 'a' at row 2",
    };
    static const char *const mixed_rows[] = {
        "127",
        "0",
        "127",
        "0",
    };
    static const char *const diagnostics_count_values[] = {"1", "1", "0"};
    static const char *const low_set_levels[] = {"Warning", "Warning", "Warning", "Warning"};
    static const char *const low_set_codes[] = {"1048", "1264", "1264", "1048"};
    static const char *const low_set_messages[] = {
        "Column 'id' cannot be null",
        "Out of range value for column 'ti' at row 1",
        "Out of range value for column 'tu' at row 1",
        "Column 'nn' cannot be null",
    };
    static const char *const low_set_row[] = {"0", "127", "0", "0"};
    static const char *const high_set_row[] = {"5", "1", "0", "6"};
    static const char *const delayed_levels[] = {"Warning", "Warning", "Warning"};
    static const char *const delayed_codes[] = {"3005", "1048", "1048"};
    static const char *const delayed_messages[] = {
        "INSERT DELAYED is no longer supported",
        "Column 'id' cannot be null",
        "Column 'nn' cannot be null",
    };
    static const char *const delayed_row[] = {"0", NULL, NULL, "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "adjustments") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open adjustments database");
    failures += seed_schema(database);

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO ints(id, ti, tu, si, su, mi, mu, i, iu, b, bu, n, nn) "
                   "VALUES "
                   "(1, -128, 255, -32768, 65535, -8388608, 16777215, -2147483648, "
                   "4294967295, -9223372036854775808, 9223372036854775807, NULL, 10), "
                   "(NULL, -129, 256, -32769, 65536, -8388609, 16777216, -2147483649, "
                   "4294967296, -9223372036854775809, 9223372036854775807, NULL, NULL)",
            .affected_rows = 2,
            .warning_count = adjustment_warning_count,
        }
    );
    failures += expect_warning_count(database, "11", "adjustment warning count");
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = adjustment_warning_count,
            .levels = adjustment_levels,
            .codes = adjustment_codes,
            .message_parts = adjustment_messages,
            .context = "adjustment warning rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tu, si, su, mi, mu, i, iu, b, bu, n, nn, d "
                   "FROM ints ORDER BY id",
            .values = adjusted_rows,
            .column_count = adjusted_values_column_count,
            .row_count = 2U,
            .context = "adjusted values rows",
        }
    );

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO order_t(a) VALUES (128), (129)",
            .affected_rows = 2,
            .warning_count = 3U,
        }
    );
    failures += expect_warning_count(database, "3", "mixed warning count");
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 3U,
            .levels = mixed_levels,
            .codes = mixed_codes,
            .message_parts = mixed_messages,
            .context = "mixed warning order rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b FROM order_t ORDER BY a",
            .values = mixed_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "mixed adjusted rows",
        }
    );

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO defaults_t(id) VALUES (10), (11)",
            .affected_rows = 2,
            .warning_count = 1U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = omitted_levels,
            .codes = omitted_codes,
            .message_parts = omitted_messages,
            .context = "omitted no default warning row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, n, nn FROM defaults_t ORDER BY id",
            .values = omitted_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "omitted no default rows",
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO drop_default_t(id) VALUES (20), (21)",
            .affected_rows = 2,
            .warning_count = 1U,
        }
    );
    failures += expect_warning_count(database, "1", "drop-default nullable warning count");
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = drop_default_levels,
            .codes = drop_default_codes,
            .message_parts = drop_default_messages,
            .context = "drop-default nullable warning row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM drop_default_t ORDER BY id",
            .values = drop_default_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "drop-default nullable rows",
        }
    );
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO defaults_t(id) VALUES (12)",
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .values = diagnostics_count_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "insert ignore diagnostics counts",
        }
    );

    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT LOW_PRIORITY IGNORE INTO set_t "
                   "SET id = NULL, ti = 128, tu = -1, nn = NULL",
            .affected_rows = 1,
            .warning_count = 4U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 4U,
            .levels = low_set_levels,
            .codes = low_set_codes,
            .message_parts = low_set_messages,
            .context = "low priority ignore set warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tu, nn FROM set_t ORDER BY id",
            .values = low_set_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "low priority ignore set row",
        }
    );

    failures += execute_statement_ok(database, "TRUNCATE TABLE set_t");
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT HIGH_PRIORITY IGNORE INTO set_t "
                   "SET id = 5, ti = TRUE, tu = FALSE, nn = 6",
            .affected_rows = 1,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tu, nn FROM set_t ORDER BY id",
            .values = high_set_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "high priority ignore set row",
        }
    );

    failures += execute_statement_ok(database, "TRUNCATE TABLE set_t");
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT DELAYED IGNORE INTO set_t SET id = NULL, nn = NULL",
            .affected_rows = 1,
            .warning_count = 3U,
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 3U,
            .levels = delayed_levels,
            .codes = delayed_codes,
            .message_parts = delayed_messages,
            .context = "delayed ignore warning rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tu, nn FROM set_t ORDER BY id",
            .values = delayed_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "delayed ignore set row",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0, actual_preamble, sizeof(actual_preamble)),
        0,
        "read adjusted file preamble"
    );
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(expected_preamble), "preamble");

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen adjustments database");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tu, nn FROM app.set_t ORDER BY id",
            .values = delayed_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "delayed ignore row persists",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_ignore_schema_resolution_and_diagnostics(void) {
    static const char *const shape_error_levels[] = {"Error"};
    static const char *const shape_error_codes[] = {"1136"};
    static const char *const shape_error_messages[] = {
        "Column count doesn't match value count at row 2",
    };
    static const char *const insert_select_count[] = {"2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures +=
        execute_statement_ok(database, "CREATE TABLE app.simple(id INT NOT NULL, nn INT NOT NULL)");

    failures += execute_error(
        database,
        "INSERT IGNORE INTO simple VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO missing_schema.simple VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "INSERT IGNORE INTO _mylite_hidden VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_hidden'",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO missing_table VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO simple(id, id, nn) VALUES (1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_specified_twice,
            .sqlstate = "42000",
            .message_part = "Column 'id' specified twice",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO simple(id, nn) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO simple(id) VALUES (1), (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );
    failures += expect_show_warnings(
        database,
        (struct expected_warnings){
            .row_count = 1U,
            .levels = shape_error_levels,
            .codes = shape_error_codes,
            .message_parts = shape_error_messages,
            .context = "shape error warning rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO simple(missing, nn) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE LOW_PRIORITY INTO simple VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT LOW_PRIORITY HIGH_PRIORITY IGNORE INTO simple VALUES (1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_statement_ok(database, "INSERT INTO simple VALUES (9, 9)");
    failures += expect_dml_ok(
        database,
        (struct expected_dml){
            .sql = "INSERT LOW_PRIORITY IGNORE INTO simple SELECT id, nn FROM simple",
            .affected_rows = 1,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM simple",
            .values = insert_select_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert ignore select copied row",
        }
    );
    failures += execute_error(
        database,
        "INSERT DELAYED IGNORE INTO simple SELECT id, nn FROM simple",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT IGNORE ... SELECT does not support DELAYED",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_insert_ignore_independent_handles(void) {
    static const char *const first_rows[] = {"0", "0"};
    static const char *const second_rows[] = {"7", "7"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);

    failures += expect_dml_ok(
        first,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO app.set_t SET id = NULL, nn = NULL",
            .affected_rows = 1,
            .warning_count = 2U,
        }
    );
    failures += expect_dml_ok(
        second,
        (struct expected_dml){
            .sql = "INSERT IGNORE INTO app.set_t SET id = 7, nn = 7",
            .affected_rows = 1,
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, nn FROM app.set_t ORDER BY id",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first handle adjusted rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, nn FROM app.set_t ORDER BY id",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += create_ignore_tables(database);

    return failures;
}

static int create_ignore_tables(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(
        database,
        "CREATE TABLE ints("
        "id INT NOT NULL, ti TINYINT, tu TINYINT UNSIGNED, si SMALLINT, "
        "su SMALLINT UNSIGNED, mi MEDIUMINT, mu MEDIUMINT UNSIGNED, i INT, "
        "iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, n INT NULL, "
        "nn INT NOT NULL, d INT DEFAULT 7)"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE defaults_t(id INT NOT NULL, d INT DEFAULT 7, n INT NULL, nn INT NOT NULL)"
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE drop_default_t(id INT NOT NULL, n INT NULL)");
    failures += execute_statement_ok(database, "ALTER TABLE drop_default_t ALTER n DROP DEFAULT");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE set_t(id INT NOT NULL, ti TINYINT, tu TINYINT UNSIGNED, nn INT NOT NULL)"
    );
    failures += execute_statement_ok(database, "CREATE TABLE order_t(a TINYINT, b INT NOT NULL)");

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, struct expected_dml expectation) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expectation.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expectation.affected_rows,
        "DML affected rows"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expectation.warning_count,
        "DML warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_warning_count(mylite_db *database, const char *expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures +=
        expect_text(mylite_result_column_name(result, 0U), "@@session.warning_count", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);

    return failures;
}

static int expect_show_warnings(mylite_db *database, struct expected_warnings expectation) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    failures += expect_size(
        mylite_result_column_count(result),
        show_warnings_column_count,
        expectation.context
    );
    for (size_t column = 0U; column < show_warnings_column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            show_warnings_names[column],
            expectation.context
        );
    }
    failures +=
        expect_size(mylite_result_row_count(result), expectation.row_count, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);
    for (size_t row = 0U; row < expectation.row_count; ++row) {
        failures += expect_text(
            mylite_result_value_text(result, row, 0U),
            expectation.levels[row],
            expectation.context
        );
        failures += expect_text(
            mylite_result_value_text(result, row, 1U),
            expectation.codes[row],
            expectation.context
        );
        failures += expect_contains(
            mylite_result_value_text(result, row, 2U),
            expectation.message_parts[row],
            expectation.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }

    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected '%s', got '%s'\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected '%s' to contain '%s'\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_insert_ignore_lifecycle_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "failed to seek %s\n", path);
        return 1;
    }

    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        return 1;
    }

    return 0;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}
