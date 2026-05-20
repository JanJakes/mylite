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
    nonstrict_update_two_row_three_column_warning_count = 6,
    strict_scalar_string_column_count = 7,
    mysql_error_bad_null = 1048,
    mysql_error_duplicate_key = 1062,
    mysql_error_data_truncated = 1265,
    mysql_error_data_too_long = 1406,
    mysql_error_no_default = 1364,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_nonstrict_insert_replace_defaults_and_persistence(void);
static int test_nonstrict_update_null_and_default(void);
static int test_nonstrict_insert_select_coercion(void);
static int test_nonstrict_string_truncation(void);
static int test_nonstrict_guardrails(void);
static int seed_schema(mylite_db *database);
static int create_coercion_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
);
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

    failures += test_nonstrict_insert_replace_defaults_and_persistence();
    failures += test_nonstrict_update_null_and_default();
    failures += test_nonstrict_insert_select_coercion();
    failures += test_nonstrict_string_truncation();
    failures += test_nonstrict_guardrails();

    return failures == 0 ? 0 : 1;
}

static int test_nonstrict_insert_replace_defaults_and_persistence(void) {
    static const char *const missing_default_warnings[] = {
        "Warning",
        "1364",
        "Field 'i' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'dt' doesn't have a default value",
    };
    static const char *const inserted_rows[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "1",
        "0",
        "",
        "0000-00-00 00:00:00",
        "2",
    };
    static const char *const default_row[] = {"0", "", "0000-00-00 00:00:00", "3"};
    static const char *const set_row[] = {"0", "", "0000-00-00 00:00:00", "4"};
    static const char *const replaced_rows[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "5",
        "0",
        "",
        "0000-00-00 00:00:00",
        "6",
    };
    static const char *const replace_set_row[] = {"0", "", "0000-00-00 00:00:00", "7"};
    static const char *const empty_default_row[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        NULL,
    };
    static const char *const reopened_row[] = {"0", "", "0000-00-00 00:00:00", "7"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "insert_replace") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open insert replace file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "set empty sql mode"
    );
    failures += create_coercion_table(database);

    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(n) VALUES (1), (2)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "nonstrict insert omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = missing_default_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "insert omitted default warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t ORDER BY n",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "insert omitted default rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for explicit default"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t VALUES (DEFAULT, DEFAULT, DEFAULT, 3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict insert explicit defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = default_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert explicit default row",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for insert set"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t SET n = 4",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict insert set omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = set_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert set omitted default row",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for replace values"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO coerce_t(n) VALUES (5), (6)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "nonstrict replace omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t ORDER BY n",
            .values = replaced_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "replace omitted default rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for replace set"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO coerce_t SET n = 7",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict replace set omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = replace_set_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "replace set omitted default row",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for empty insert values"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict insert empty values omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = empty_default_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert empty values row",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for empty insert value"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t VALUE ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict insert empty value omitted defaults"
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for empty insert row"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t VALUES ROW()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict insert empty row omitted defaults"
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for empty replace values"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO coerce_t VALUES ()",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict replace empty values omitted defaults"
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate before persistence row"
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO coerce_t SET n = 7",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "restore persistence row"
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "nonstrict DML preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen insert replace file");
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){0, 0U},
        "use reopened schema"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = reopened_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened adjusted row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_nonstrict_update_null_and_default(void) {
    static const char *const null_warnings[] = {
        "Warning",
        "1048",
        "Column 'i' cannot be null",
        "Warning",
        "1048",
        "Column 'v' cannot be null",
        "Warning",
        "1048",
        "Column 'dt' cannot be null",
        "Warning",
        "1048",
        "Column 'i' cannot be null",
        "Warning",
        "1048",
        "Column 'v' cannot be null",
        "Warning",
        "1048",
        "Column 'dt' cannot be null",
    };
    static const char *const default_warnings[] = {
        "Warning",
        "1364",
        "Field 'i' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'dt' doesn't have a default value",
    };
    static const char *const implicit_rows[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "8",
        "0",
        "",
        "0000-00-00 00:00:00",
        "9",
    };
    static const char *const dropped_warnings[] = {
        "Warning",
        "1364",
        "Field 'n' doesn't have a default value",
    };
    static const char *const dropped_rows[] = {"1", NULL, "2", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "update") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open update file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "set update sql mode"
    );
    failures += create_coercion_table(database);
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t VALUES "
        "(11, 'abc', '2020-01-02 03:04:05', 8), "
        "(12, 'def', '2020-01-03 03:04:05', 9)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "seed update rows"
    );

    failures += expect_statement_ok(
        database,
        "UPDATE coerce_t SET i = NULL, v = NULL, dt = NULL WHERE n IN (8, 9)",
        (struct expected_statement){
            .affected_rows = 2,
            .warning_count = nonstrict_update_two_row_three_column_warning_count,
        },
        "nonstrict update null assignments"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = null_warnings,
            .column_count = 3U,
            .row_count = nonstrict_update_two_row_three_column_warning_count,
            .context = "update null warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t ORDER BY n",
            .values = implicit_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "update null implicit row",
        }
    );

    failures += expect_statement_ok(
        database,
        "UPDATE coerce_t SET i = NULL WHERE n = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict update null no match"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE coerce_t SET i = 12, v = 'def', dt = '2021-01-02 03:04:05' WHERE n = 8",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "restore row before default update"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE coerce_t SET i = DEFAULT, v = DEFAULT, dt = DEFAULT WHERE n = 8",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict update default assignments"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = default_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "update default warnings",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE coerce_t SET i = DEFAULT WHERE n = 8 LIMIT 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict update default limit zero"
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE drop_t(id INT NOT NULL, n INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create dropped default table"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE drop_t ALTER n DROP DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "drop nullable default"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO drop_t(id) VALUES (1), (2)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 1U},
        "nonstrict nullable dropped default insert"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = dropped_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "dropped nullable insert warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, n FROM drop_t ORDER BY id",
            .values = dropped_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "dropped nullable insert rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE drop_t SET n = DEFAULT WHERE id = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 1U},
        "nonstrict nullable dropped default update"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = dropped_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "dropped nullable update warning",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_nonstrict_insert_select_coercion(void) {
    static const char *const omitted_warnings[] = {
        "Warning",
        "1364",
        "Field 'i' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'dt' doesn't have a default value",
    };
    static const char *const selected_null_warnings[] = {
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'dt' doesn't have a default value",
        "Warning",
        "1048",
        "Column 'i' cannot be null",
    };
    static const char *const range_warnings[] = {
        "Warning",
        "1364",
        "Field 'v' doesn't have a default value",
        "Warning",
        "1364",
        "Field 'dt' doesn't have a default value",
        "Warning",
        "1264",
        "Out of range value for column 'i' at row 2",
        "Warning",
        "1264",
        "Out of range value for column 'i' at row 3",
    };
    static const char *const omitted_rows[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "10",
        "0",
        "",
        "0000-00-00 00:00:00",
        "30",
    };
    static const char *const selected_null_rows[] = {
        "10",
        "",
        "0000-00-00 00:00:00",
        "1",
        "0",
        "",
        "0000-00-00 00:00:00",
        "2",
    };
    static const char *const clipped_rows[] = {
        "1",
        "1",
        "2147483647",
        "2",
        "-2147483648",
        "3",
    };
    static const char *const zero_rows[] = {"0"};
    static const char *const scalar_omitted_row[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "4",
    };
    static const char *const scalar_null_row[] = {
        "0",
        "",
        "0000-00-00 00:00:00",
        "5",
    };
    static const char *const strict_scalar_varchar_trailing_warnings[] = {
        "Note",
        "1265",
        "Data truncated for column 'v' at row 1",
    };
    static const char *const strict_scalar_string_rows[] = {
        "1",
        "[ab ]",
        "3",
        "3",
        "[ab]",
        "2",
        "2",
    };
    static const char *const insert_select_string_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 2",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 2",
    };
    static const char *const insert_select_string_rows[] = {
        "1",
        "[abc]",
        "[abc]",
        "2",
        "[ééé]",
        "[ééé]",
    };
    static const char *const row_scalar_string_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 1",
    };
    static const char *const row_scalar_string_row[] = {
        "1",
        "[abc]",
        "[abc]",
    };
    static const char *const reopened_string_row[] = {
        "2",
        "[ééé]",
        "[ééé]",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "insert_select") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open insert select file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "set insert select sql mode"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE src(id INT NOT NULL, n INT NULL, b BIGINT)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create insert select source"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO src VALUES (1, 10, 1), (2, NULL, 2147483648), (3, 30, -2147483649)",
        (struct expected_statement){.affected_rows = 3, .warning_count = 0U},
        "seed insert select source"
    );
    failures += create_coercion_table(database);

    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(n) SELECT n FROM src WHERE id IN (1, 3) ORDER BY id",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "nonstrict insert select omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = omitted_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "insert select omitted warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t ORDER BY n",
            .values = omitted_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "insert select omitted rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for selected null"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(i, n) SELECT n, id FROM src WHERE id IN (1, 2) ORDER BY id",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "nonstrict insert select selected null"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = selected_null_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "insert select selected null warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t ORDER BY n",
            .values = selected_null_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "insert select selected null rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for clipped integers"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(i, n) SELECT b, id FROM src WHERE id IN (1, 2, 3) ORDER BY id",
        (struct expected_statement){.affected_rows = 3, .warning_count = 4U},
        "nonstrict insert select integer clipping"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = range_warnings,
            .column_count = 3U,
            .row_count = 4U,
            .context = "insert select integer clipping warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, n FROM coerce_t ORDER BY n",
            .values = clipped_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "insert select clipped integer rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for zero source"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(i, n) SELECT n, id FROM src WHERE id > 100",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict insert select zero source"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM coerce_t",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert select zero source rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(n) SELECT 4",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict row-scalar insert select omitted defaults"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = scalar_omitted_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "row-scalar insert select omitted row",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE coerce_t",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for row-scalar null"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO coerce_t(i, n) SELECT NULL, 5",
        (struct expected_statement){.affected_rows = 1, .warning_count = 3U},
        "nonstrict row-scalar insert select selected null"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = scalar_null_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "row-scalar insert select selected null row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE string_src(id INT NOT NULL, v VARCHAR(8), c CHAR(8))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create insert select string source"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_src VALUES (1, 'abcd', 'abcd'), (2, 'éééx', 'éééx'), "
        "(3, 'ab  ', 'ab  '), (4, 'ok', 'abcd')",
        (struct expected_statement){.affected_rows = 4, .warning_count = 0U},
        "seed insert select string source"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE string_dst(id INT NOT NULL, v VARCHAR(3), c CHAR(3))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create insert select string target"
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "set strict insert select string mode"
    );
    failures += execute_error(
        database,
        "INSERT INTO string_dst SELECT 1, 'abcd', 'ok'",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO string_dst SELECT id, v, c FROM string_src WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_truncated,
            .sqlstate = "01000",
            .message_part = "Data truncated for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO string_dst SELECT 1, 'ok', 'abcd'",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'c' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO string_dst SELECT id, v, c FROM string_src WHERE id = 4",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'c' at row 1",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE string_dst",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for strict row-scalar string trailing spaces"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_dst SELECT 1, 'ab  ', 'ab  '",
        (struct expected_statement){.affected_rows = 1, .warning_count = 1U},
        "strict row-scalar insert select string trailing spaces"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = strict_scalar_varchar_trailing_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "strict row-scalar varchar trailing warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql =
                "SELECT id, CONCAT('[', v, ']'), LENGTH(v), CHAR_LENGTH(v), CONCAT('[', c, ']'), "
                "LENGTH(c), CHAR_LENGTH(c) FROM string_dst",
            .values = strict_scalar_string_rows,
            .column_count = strict_scalar_string_column_count,
            .row_count = 1U,
            .context = "strict row-scalar string trailing row",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "set nonstrict insert select string mode"
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE string_dst",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for nonstrict insert select string truncation"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_dst SELECT id, v, c FROM string_src WHERE id IN (1, 2) ORDER BY id",
        (struct expected_statement){.affected_rows = 2, .warning_count = 4U},
        "nonstrict table-backed insert select string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = insert_select_string_warnings,
            .column_count = 3U,
            .row_count = 4U,
            .context = "nonstrict insert select string truncation warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT('[', v, ']'), CONCAT('[', c, ']') "
                   "FROM string_dst ORDER BY id",
            .values = insert_select_string_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict insert select string truncation rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE string_dst",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for row-scalar insert select string truncation"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_dst SELECT 1, 'abcd', 'abcd'",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "nonstrict row-scalar insert select string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = row_scalar_string_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict row-scalar string truncation warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT('[', v, ']'), CONCAT('[', c, ']') FROM string_dst",
            .values = row_scalar_string_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict row-scalar string truncation row",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "set strict insert ignore select string mode"
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE string_dst",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for insert ignore select string truncation"
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO string_dst SELECT id, v, c FROM string_src WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "strict insert ignore select string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = row_scalar_string_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "insert ignore select string truncation warnings",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "reset nonstrict insert select string mode"
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE string_dst",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate for insert select string zero source"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_dst SELECT id, v, c FROM string_src WHERE id > 99",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict insert select string zero source"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM string_dst",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "insert select string zero source rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO string_dst SELECT id, v, c FROM string_src WHERE id = 2",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "nonstrict insert select string persistence row"
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "insert select coercion preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen insert select file");
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){0, 0U},
        "use reopened insert select schema"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, v, dt, n FROM coerce_t",
            .values = scalar_null_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened insert select adjusted row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONCAT('[', v, ']'), CONCAT('[', c, ']') FROM string_dst",
            .values = reopened_string_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "reopened insert select string adjusted row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_nonstrict_string_truncation(void) {
    static const char *const count_zero[] = {"0"};
    static const char *const strict_trailing_warnings[] = {
        "Note",
        "1265",
        "Data truncated for column 'v' at row 1",
    };
    static const char *const strict_trailing_row[] = {"1", "abc", "xyz"};
    static const char *const insert_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 1",
    };
    static const char *const inserted_row[] = {"1", "abc", "wxy"};
    static const char *const insert_set_row[] = {"3", "abc", "pqr"};
    static const char *const replace_row[] = {"3", "zzz", "yyy"};
    static const char *const insert_ignore_row[] = {"4", "mno", "qrs"};
    static const char *const zero_length_warnings[] = {
        "Note",
        "1265",
        "Data truncated for column 'vz' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'vz' at row 2",
        "Warning",
        "1265",
        "Data truncated for column 'cz' at row 2",
    };
    static const char *const zero_length_rows[] = {"5", "", "", "6", "", ""};
    static const char *const utf8_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
    };
    static const char *const utf8_row[] = {
        "7",
        "\xC3\xA9\xC3\xA9"
        "a"
    };
    static const char *const update_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 2",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 2",
    };
    static const char *const updated_rows[] = {"1", "abc", "pqr", "2", "abc", "pqr"};
    static const char *const same_value_warnings[] = {
        "Warning",
        "1265",
        "Data truncated for column 'v' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'c' at row 1",
    };
    static const char *const reopened_row[] = {"1", "abc", "pqr"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "string_truncation") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open string file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE strings("
        "id INT NOT NULL PRIMARY KEY, "
        "v VARCHAR(3), "
        "c CHAR(3), "
        "vz VARCHAR(0), "
        "cz CHAR(0))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create string truncation table"
    );

    failures += execute_error(
        database,
        "INSERT INTO strings VALUES (1, 'abcd', 'xyz', '', '')",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM strings",
            .values = count_zero,
            .column_count = 1U,
            .row_count = 1U,
            .context = "strict overlength insert rolled back",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strings VALUES (1, 'abc ', 'xyz ', '', '')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 1U},
        "strict trailing-space truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = strict_trailing_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "strict varchar trailing-space note",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 1",
            .values = strict_trailing_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "strict trailing-space stored row",
        }
    );
    failures += execute_error(
        database,
        "UPDATE strings SET v = 'abcd' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );

    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "set nonstrict string sql mode"
    );
    failures += expect_statement_ok(
        database,
        "TRUNCATE strings",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "truncate before nonstrict strings"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strings VALUES (1, 'abcd', 'wxyz', '', ''), (2, 'ab', 'pq', '', '')",
        (struct expected_statement){.affected_rows = 2, .warning_count = 2U},
        "nonstrict insert string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = insert_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict insert string warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 1",
            .values = inserted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict inserted string row",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strings SET id = 3, v = 'abcdef', c = 'pqrs', vz = '', cz = ''",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "nonstrict insert set string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 3",
            .values = insert_set_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict insert set row",
        }
    );
    failures += expect_statement_ok(
        database,
        "REPLACE INTO strings SET id = 3, v = 'zzzz', c = 'yyyy', vz = '', cz = ''",
        (struct expected_statement){.affected_rows = 2, .warning_count = 2U},
        "nonstrict replace set string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 3",
            .values = replace_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "nonstrict replace row",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=DEFAULT",
        (struct expected_statement){0, 0U},
        "restore strict mode before insert ignore"
    );
    failures += expect_statement_ok(
        database,
        "INSERT IGNORE INTO strings VALUES (4, 'mnop', 'qrst', '', '')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 2U},
        "insert ignore string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 4",
            .values = insert_ignore_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "insert ignore truncated row",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "restore nonstrict before zero-length insert"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strings VALUES (5, 'ok', 'ok', '   ', '   '), "
        "(6, 'ok', 'ok', 'x', 'y')",
        (struct expected_statement){.affected_rows = 2, .warning_count = 3U},
        "nonstrict zero-length string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = zero_length_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "zero-length string warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, vz, cz FROM strings WHERE id IN (5, 6) ORDER BY id",
            .values = zero_length_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "zero-length string rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strings VALUES (7, '\xC3\xA9\xC3\xA9"
        "ab', 'ok', '', '')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 1U},
        "nonstrict UTF-8 string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = utf8_warnings,
            .column_count = 3U,
            .row_count = 1U,
            .context = "UTF-8 truncation warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM strings WHERE id = 7",
            .values = utf8_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "UTF-8 truncates on character boundary",
        }
    );

    failures += expect_statement_ok(
        database,
        "UPDATE strings SET v = 'abcdef', c = 'pqrs' WHERE id IN (1, 2) ORDER BY id",
        (struct expected_statement){.affected_rows = 2, .warning_count = 4U},
        "nonstrict update string truncation"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = update_warnings,
            .column_count = 3U,
            .row_count = 4U,
            .context = "nonstrict update string warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id IN (1, 2) ORDER BY id",
            .values = updated_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict updated string rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strings SET v = 'abcd', c = 'pqrs' WHERE id = 1",
        (struct expected_statement){.affected_rows = 0, .warning_count = 2U},
        "nonstrict update truncates to current row"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = same_value_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict update same string warnings",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strings SET v = 'abcd' WHERE id = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict update string no match"
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strings SET v = 'abcd' LIMIT 0",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict update string limit zero"
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "string truncation preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen string file");
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){0, 0U},
        "use reopened string schema"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, c FROM strings WHERE id = 1",
            .values = reopened_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "reopened truncated string row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_nonstrict_guardrails(void) {
    static const char *const no_engine_row[] = {"0", "9"};
    static const char *const auto_rows[] = {"0", "20", "1", "30"};
    static const char *const duplicate_string_rows[] = {"1", "abc"};
    static const char *const failed_update_warnings[] = {
        "Warning",
        "1048",
        "Column 'i' cannot be null",
        "Warning",
        "1048",
        "Column 'i' cannot be null",
        "Error",
        "1062",
        "Duplicate entry '0' for key 'fail_t.i'",
    };
    static const char *const failed_update_rows[] = {"1", "5", "2", "6"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "guardrails") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open guardrails file");
    failures += seed_schema(database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE strict_t(i INT NOT NULL, n INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create strict table"
    );
    failures += execute_error(
        database,
        "INSERT INTO strict_t(n) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'i' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strict_t VALUES (1, 1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "seed strict table"
    );
    failures += execute_error(
        database,
        "UPDATE strict_t SET i = NULL WHERE n = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'i' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE strict_t SET i = DEFAULT WHERE n = 1",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'i' doesn't have a default value",
        }
    );

    failures += expect_statement_ok(
        database,
        "SET sql_mode=''",
        (struct expected_statement){0, 0U},
        "clear strict mode"
    );
    failures += execute_error(
        database,
        "INSERT INTO strict_t(i, n) VALUES (NULL, 2)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'i' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO strict_t(i, n) VALUES (NULL, 3)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'i' cannot be null",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE strict_t SET i = NULL WHERE n = 999",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "nonstrict update null no rows"
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode='NO_ENGINE_SUBSTITUTION'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "set nonstrict no engine mode"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO strict_t(n) VALUES (9)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 1U},
        "no engine substitution insert omitted default"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, n FROM strict_t WHERE n = 9",
            .values = no_engine_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "no engine substitution row",
        }
    );
    failures += expect_statement_ok(
        database,
        "SET sql_mode='NO_AUTO_VALUE_ON_ZERO'",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "set nonstrict no auto value on zero mode"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_t(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, v INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create auto increment guardrail table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO auto_t(id, v) VALUES (0, 20)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "no auto value on zero stores explicit zero"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO auto_t(v) VALUES (30)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "nonstrict later omitted auto increment generates"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_t ORDER BY v",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "nonstrict auto increment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE duplicate_string_t(id INT NOT NULL PRIMARY KEY, v VARCHAR(3))",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create duplicate string guardrail table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO duplicate_string_t VALUES (1, 'abc')",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "seed duplicate string guardrail table"
    );
    failures += execute_error(
        database,
        "INSERT INTO duplicate_string_t VALUES (1, 'ok') "
        "ON DUPLICATE KEY UPDATE v = 'abcd'",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM duplicate_string_t",
            .values = duplicate_string_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "duplicate string guardrail preserves row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE fail_t(id INT NOT NULL PRIMARY KEY, i INT NOT NULL UNIQUE)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create failed update guardrail table"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO fail_t VALUES (1, 5), (2, 6)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U},
        "seed failed update guardrail rows"
    );
    failures += execute_error(
        database,
        "UPDATE fail_t SET i = NULL WHERE id IN (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '0' for key 'fail_t.i'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = failed_update_warnings,
            .column_count = 3U,
            .row_count = 3U,
            .context = "failed adjusted update warnings and error",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM fail_t ORDER BY id",
            .values = failed_update_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "failed adjusted update preserves rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(
        database,
        "CREATE DATABASE app",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U},
        "create schema"
    );
    failures += expect_statement_ok(
        database,
        "USE app",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "use schema"
    );

    return failures;
}

static int create_coercion_table(mylite_db *database) {
    return expect_statement_ok(
        database,
        "CREATE TABLE coerce_t("
        "i INT NOT NULL, "
        "v VARCHAR(5) NOT NULL, "
        "dt DATETIME NOT NULL, "
        "n INT NULL)",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U},
        "create coercion table"
    );
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s' failed: %d %s %s\n",
            sql,
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

static int expect_statement_ok(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, context);
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
        "%s/mylite_nonstrict_dml_coercion_%d_%s.mylite",
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
            "%s: expected '%s', got '%s'\n",
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
        fprintf(stderr, "%s: byte buffers differ\n", context);
        return 1;
    }

    return 0;
}
