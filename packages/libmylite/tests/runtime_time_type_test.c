#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
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
    show_columns_field_count = 6,
    information_schema_column_count = 9,
    inserted_time_row_count = 5,
    ignored_time_row_count = 7,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_incorrect_time_value = 1292,
    mysql_error_no_default = 1364,
    mysql_error_precision_too_big = 1426,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_time_success_metadata_dml_and_persistence(void);
static int test_time_diagnostics(void);
static int test_time_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
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
static int expect_true(int condition, const char *context);
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

    failures += test_time_success_metadata_dml_and_persistence();
    failures += test_time_diagnostics();
    failures += test_time_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_time_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "t",
        "time",
        "YES",
        "",
        NULL,
        "",
        "nn",
        "time",
        "NO",
        "",
        "01:02:03",
        "",
    };
    static const char *const show_create_rows[] = {
        "times",
        "CREATE TABLE `times` (\n"
        "  `id` int NOT NULL,\n"
        "  `t` time DEFAULT NULL,\n"
        "  `nn` time NOT NULL DEFAULT '01:02:03'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "t",
        "time",
        "time",
        "YES",
        NULL,
        "0",
        NULL,
        NULL,
        NULL,
        "nn",
        "time",
        "time",
        "NO",
        "01:02:03",
        "0",
        NULL,
        NULL,
        NULL,
    };
    static const char *const initial_rows[] = {
        "1",
        "-838:59:59",
        "01:02:03",
        "2",
        "00:00:00",
        "01:02:03",
        "3",
        "24:00:00",
        "01:02:03",
        "4",
        "838:59:59",
        "01:02:03",
        "5",
        NULL,
        "01:02:03",
    };
    static const char *const negative_rows[] = {"1"};
    static const char *const nseq_rows[] = {"3"};
    static const char *const between_rows[] = {"2", "3"};
    static const char *const in_rows[] = {"1", "4"};
    static const char *const null_rows[] = {"5"};
    static const char *const order_asc_rows[] = {"5", "1", "2", "3", "4"};
    static const char *const order_desc_rows[] = {"4", "3", "2", "1", "5"};
    static const char *const updated_rows[] = {
        "1",
        "-838:59:59",
        "12:00:00",
        "2",
        "-00:00:01",
        "01:02:03",
        "3",
        "24:00:00",
        "01:02:03",
        "4",
        "838:59:59",
        "01:02:03",
        "5",
        NULL,
        "12:00:00",
    };
    static const char *const after_delete_rows[] = {"1", "2", "3", "5"};
    static const char *const added_rows[] =
        {"1", "00:00:00", "2", "00:00:00", "3", "00:00:00", "5", "00:00:00"};
    static const char *const added_default_rows[] = {
        "added",
        "time",
        "NO",
        "",
        "-01:02:03",
        "",
    };
    static const char *const inserted_default_rows[] = {"6", "01:00:00", "-01:02:03"};
    static const char *const copied_rows[] = {"6", "01:00:00"};
    static const char *const renamed_rows[] = {"1", "02:00:00"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open time success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE times (id INT NOT NULL, t TIME, nn TIME NOT NULL DEFAULT '01:02:03')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM times",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "time SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE times",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "time SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                   "DATETIME_PRECISION, NUMERIC_PRECISION, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = "
                   "'app' AND TABLE_NAME = 'times' AND COLUMN_NAME <> 'id' ORDER BY "
                   "ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = 2U,
            .context = "time INFORMATION_SCHEMA.COLUMNS",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO times (id, t) VALUES "
        "(1, '-838:59:59'), (2, '00:00:00'), (3, '24:00:00'), "
        "(4, '838:59:59'), (5, NULL)",
        inserted_time_row_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t, nn FROM times ORDER BY id",
            .values = initial_rows,
            .column_count = 3U,
            .row_count = (size_t)inserted_time_row_count,
            .context = "initial time rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times WHERE t < '00:00:00' ORDER BY id",
            .values = negative_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "time less-than predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times WHERE t <=> '24:00:00' ORDER BY id",
            .values = nseq_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "time null-safe equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times WHERE t BETWEEN '-00:00:01' AND '24:00:00' ORDER BY id",
            .values = between_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "time BETWEEN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times WHERE t IN ('-838:59:59','838:59:59',NULL) ORDER BY id",
            .values = in_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "time IN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times WHERE t IS NULL",
            .values = null_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "time IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times ORDER BY t ASC",
            .values = order_asc_rows,
            .column_count = 1U,
            .row_count = (size_t)inserted_time_row_count,
            .context = "time ORDER BY ASC",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times ORDER BY t DESC",
            .values = order_desc_rows,
            .column_count = 1U,
            .row_count = (size_t)inserted_time_row_count,
            .context = "time ORDER BY DESC",
        }
    );

    failures += expect_dml_ok(database, "UPDATE times SET t = '-838:59:59' WHERE id = 1", 0);
    failures += expect_dml_ok(database, "UPDATE times SET t = '-00:00:01' WHERE id = 2", 1);
    failures += expect_dml_ok(database, "UPDATE times SET nn = '12:00:00' ORDER BY t LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t, nn FROM times ORDER BY id",
            .values = updated_rows,
            .column_count = 3U,
            .row_count = (size_t)inserted_time_row_count,
            .context = "time updated rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM times ORDER BY t DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM times ORDER BY id",
            .values = after_delete_rows,
            .column_count = 1U,
            .row_count = 4U,
            .context = "time DELETE ORDER BY LIMIT",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE times ADD COLUMN added TIME NOT NULL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM times ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "time ADD COLUMN zero backfill",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE times ALTER COLUMN added SET DEFAULT '-01:02:03'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM times LIKE 'added'",
            .values = added_default_rows,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "time ALTER COLUMN SET DEFAULT",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO times (id, t) VALUES (6, '01:00:00')", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t, added FROM times WHERE id = 6",
            .values = inserted_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "time inserted explicit default",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE times_like LIKE times");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE times_copy AS SELECT id, t FROM times WHERE id = 6"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM times_copy",
            .values = copied_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "time CREATE TABLE SELECT copy",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE times_insert_copy (id INT, t TIME)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO times_insert_copy SELECT id, t FROM times WHERE id = 6",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM times_insert_copy",
            .values = copied_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "time INSERT SELECT copy",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE rename_source (id INT, t TIME)");
    failures += expect_dml_ok(database, "INSERT INTO rename_source VALUES (1, '01:00:00')", 1);
    failures += expect_statement_ok(database, "RENAME TABLE rename_source TO rename_target");
    failures += expect_dml_ok(database, "UPDATE rename_target SET t = '02:00:00' WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM rename_target",
            .values = renamed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "time update after rename",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE rename_target");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen time success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM times WHERE id = 6",
            .values = copied_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "time persistence",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read time preamble"
    );
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "time preamble");

    remove_related_files(path);
    return failures;
}

static int test_time_diagnostics(void) {
    static const char *const ignored_rows[] = {
        "1",
        "838:59:59",
        "2",
        "-838:59:59",
        "3",
        "00:00:00",
        "4",
        "00:00:00",
        "5",
        "00:00:00",
        "6",
        "00:00:00",
        "7",
        "00:00:00",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open time diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE times (id INT, t TIME NOT NULL)");

    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, '839:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_time_value,
            .sqlstate = "22007",
            .message_part = "Incorrect time value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, '12:60:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_time_value,
            .sqlstate = "22007",
            .message_part = "Incorrect time value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, '-00:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_time_value,
            .sqlstate = "22007",
            .message_part = "Incorrect time value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, '012:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_time_value,
            .sqlstate = "22007",
            .message_part = "Incorrect time value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO times VALUES (1, DEFAULT)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (t TIME DEFAULT '839:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_fsp (t TIME(3))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "fractional temporal column precision is not supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_big_fsp (t TIME(7))",
        (struct expected_sql_error){
            .code = mysql_error_precision_too_big,
            .sqlstate = "42000",
            .message_part = "Too-big precision 7 specified for 't'. Maximum is 6.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM times WHERE t = '839:00:00'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_time_value,
            .sqlstate = "22007",
            .message_part = "Incorrect time value",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO times VALUES "
        "(1, '839:00:00'), (2, '-839:00:00'), (3, '12:60:00'), (4, NULL), "
        "(5, DEFAULT), (6, '012:00:00'), (7, 'bad')",
        (struct expected_dml_result){
            .affected_rows = ignored_time_row_count,
            .warning_count = (size_t)ignored_time_row_count,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM times ORDER BY id",
            .values = ignored_rows,
            .column_count = 2U,
            .row_count = (size_t)ignored_time_row_count,
            .context = "time INSERT IGNORE adjusted rows",
        }
    );
    failures += execute_error(
        database,
        "UPDATE times SET t = NULL WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE time_text_source (id INT, t VARCHAR(16))");
    failures += expect_dml_ok(database, "INSERT INTO time_text_source VALUES (8, '01:00:00')", 1);
    failures += expect_statement_ok(database, "CREATE TABLE time_target (id INT, t TIME)");
    failures +=
        expect_dml_ok(database, "INSERT INTO time_target SELECT id, t FROM time_text_source", 1);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_time_independent_handles(void) {
    static const char *const first_rows[] = {"1", "-00:00:02"};
    static const char *const second_rows[] = {"1", "03:00:00"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first time file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second time file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE times (id INT, t TIME)");
    failures += expect_statement_ok(second, "CREATE TABLE times (id INT, t TIME)");
    failures += expect_dml_ok(first, "INSERT INTO times VALUES (1, '-00:00:01')", 1);
    failures += expect_dml_ok(second, "INSERT INTO times VALUES (1, '02:00:00')", 1);
    failures += expect_dml_ok(first, "UPDATE times SET t = '-00:00:02' WHERE id = 1", 1);
    failures += expect_dml_ok(second, "UPDATE times SET t = '03:00:00' WHERE id = 1", 1);
    mylite_close(first);
    mylite_close(second);
    first = NULL;
    second = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first time file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "reopen second time file");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, t FROM times",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent time file",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, t FROM times",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent time file",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected MYLITE_OK, got %d (%d %s %s)\n",
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warnings"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "dml affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "dml warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
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
        "%s/mylite_time_type_%d_%s.mylite",
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
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
