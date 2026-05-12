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
    mysql_error_parse = 1064,
    mysql_error_column_length_too_big = 1074,
    mysql_error_row_size_too_large = 1118,
    mysql_error_storage_engine_cant_index_column = 1167,
    mysql_error_invalid_default = 1067,
    mysql_error_bad_null = 1048,
    mysql_error_no_default = 1364,
    mysql_error_data_too_long = 1406,
    full_strings_row_count = 5,
    wide_table_column_count = 5,
    wide_table_row_count = 5,
    max_varchar_value_length = 16383,
};

#define WIDE_64_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define WIDE_64_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define WIDE_64_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define WIDE_256_A WIDE_64_A WIDE_64_A WIDE_64_A WIDE_64_A
#define WIDE_256_B WIDE_64_B WIDE_64_B WIDE_64_B WIDE_64_B
#define WIDE_256_C WIDE_64_C WIDE_64_C WIDE_64_C WIDE_64_C
#define WIDE_512_C WIDE_256_C WIDE_256_C

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

static int test_varchar_success_persistence_and_introspection(void);
static int test_varchar_wide_length_lifecycle(void);
static int test_varchar_diagnostics(void);
static int test_varchar_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
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

    failures += test_varchar_success_persistence_and_introspection();
    failures += test_varchar_wide_length_lifecycle();
    failures += test_varchar_diagnostics();
    failures += test_varchar_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_varchar_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",        "NO", "", NULL, "", "v", "varchar(5)", "YES", "", NULL, "",
        "nn", "varchar(3)", "NO", "", NULL, "", "z", "varchar(0)", "YES", "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "strings",
        "CREATE TABLE `strings` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` varchar(5) DEFAULT NULL,\n"
        "  `nn` varchar(3) NOT NULL,\n"
        "  `z` varchar(0) DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const initial_rows[] = {
        "1",
        "abc",
        "xy",
        "",
        "2",
        NULL,
        "nn",
        NULL,
        "3",
        "a  ",
        "zzz",
        "",
    };
    static const char *const after_set_rows[] = {
        "1",   "abc", "xy", "",  "2", NULL, "nn", NULL,  "3",  "a  ",
        "zzz", "",    "4",  "q", "r", "",   "5",  "rep", "rr", "",
    };
    static const char *const null_v_ids[] = {"2"};
    static const char *const updated_id_one[] = {"1", "done", "xy"};
    static const char *const order_limited_rows[] = {
        "1",
        "xy",
        "2",
        "nn",
        "3",
        "zzz",
        "4",
        "u",
        "5",
        "u",
    };
    static const char *const clone_row[] = {"1", "done", "xy", ""};
    static const char *const copied_row[] = {"1", "done"};
    static const char *const add_column_rows[] = {
        "1",
        NULL,
        "",
        "2",
        NULL,
        "",
        "3",
        NULL,
        "",
        "4",
        NULL,
        "",
        "5",
        NULL,
        "",
    };
    static const char *const reopened_row[] = {"5", "rep", "u"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE strings (id INT NOT NULL, v VARCHAR(5), nn VARCHAR(3) NOT NULL, "
        "z VARCHAR(0))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM strings",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "varchar SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE strings",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "varchar DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN strings",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "varchar EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE strings",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "varchar SHOW CREATE TABLE",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO strings VALUES (1, 'abc', 'xy', ''), (2, NULL, 'nn', NULL), "
        "(3, 'a  ', 'zzz', '')",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nn, z FROM strings ORDER BY id",
            .values = initial_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "varchar inserted values",
        }
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO strings SET id = 4, v = 'q', nn = 'r', z = ''", 1);
    failures += expect_dml_ok(database, "REPLACE INTO strings VALUES (5, 'rep', 'rr', '')", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nn, z FROM strings ORDER BY id",
            .values = after_set_rows,
            .column_count = 4U,
            .row_count = full_strings_row_count,
            .context = "varchar insert set and replace values",
        }
    );

    failures += expect_dml_ok(database, "UPDATE strings SET v = 'done' WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE strings SET v = 'done' WHERE id = 1", 0);
    failures += expect_dml_ok(database, "UPDATE strings SET v = NULL WHERE id = 2", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE v IS NULL ORDER BY id",
            .values = null_v_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "varchar IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nn FROM strings WHERE id = 1",
            .values = updated_id_one,
            .column_count = 3U,
            .row_count = 1U,
            .context = "varchar update readback",
        }
    );
    failures += expect_dml_ok(database, "UPDATE strings SET nn = 'u' ORDER BY id DESC LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM strings ORDER BY id",
            .values = order_limited_rows,
            .column_count = 2U,
            .row_count = full_strings_row_count,
            .context = "varchar ordered limited update",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE strings");
    failures += expect_dml_ok(
        database,
        "INSERT INTO clone SELECT id, v, nn, z FROM strings WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nn, z FROM clone ORDER BY id",
            .values = clone_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "varchar create table like insert select",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE copied AS SELECT id, v FROM strings WHERE id = 1"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM copied ORDER BY id",
            .values = copied_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "varchar create table select",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE strings ADD COLUMN extra VARCHAR(2)");
    failures +=
        expect_statement_ok(database, "ALTER TABLE strings ADD COLUMN req VARCHAR(1) NOT NULL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, extra, req FROM strings ORDER BY id",
            .values = add_column_rows,
            .column_count = 3U,
            .row_count = full_strings_row_count,
            .context = "varchar alter add column values",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "varchar preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nn FROM strings WHERE id = 5",
            .values = reopened_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "varchar persisted after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_varchar_wide_length_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "id",   "int",          "NO",  "", NULL, "", "v256", "varchar(256)", "YES", "", NULL, "",
        "v512", "varchar(512)", "YES", "", "xy", "", "a256", "varchar(256)", "YES", "", NULL, "",
        "a512", "varchar(512)", "YES", "", NULL, "",
    };
    static const char *const information_schema_rows[] = {
        "v256", "varchar", "varchar(256)", "256", "1024",
        "v512", "varchar", "varchar(512)", "512", "2048",
        "a256", "varchar", "varchar(256)", "256", "1024",
        "a512", "varchar", "varchar(512)", "512", "2048",
    };
    static const char *const wide_row[] = {
        "1",
        WIDE_256_A,
        "xy",
        WIDE_256_B,
        WIDE_512_C,
    };
    static const char *const updated_row[] = {"1", WIDE_256_B, "zz"};
    static const char *const clone_row[] = {"1", WIDE_256_B, "zz"};
    static const char *const copied_row[] = {"1", WIDE_256_B, "zz"};
    static const char insert_max_prefix[] = "INSERT INTO max_one VALUES ('";
    static const char insert_max_suffix[] = "')";
    const char *max_row[] = {NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    char *max_value = NULL;
    char *insert_max_sql = NULL;
    size_t insert_max_sql_size = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "wide") != 0) {
        return 1;
    }
    remove_related_files(path);
    max_value = calloc((size_t)max_varchar_value_length + 1U, sizeof(*max_value));
    insert_max_sql_size = strlen(insert_max_prefix) + (size_t)max_varchar_value_length +
                          strlen(insert_max_suffix) + 1U;
    insert_max_sql = calloc(insert_max_sql_size, sizeof(*insert_max_sql));
    if (max_value == NULL || insert_max_sql == NULL) {
        free(insert_max_sql);
        free(max_value);
        return 1;
    }
    memset(max_value, 'm', (size_t)max_varchar_value_length);
    (void)snprintf(
        insert_max_sql,
        insert_max_sql_size,
        "%s%s%s",
        insert_max_prefix,
        max_value,
        insert_max_suffix
    );
    max_row[0] = max_value;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open wide file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wide (id INT NOT NULL, v256 VARCHAR(256), "
        "v512 VARCHAR(512) DEFAULT 'xy', a256 CHARACTER VARYING(256), "
        "a512 CHAR VARYING(512))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM wide",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = wide_table_row_count,
            .context = "wide varchar SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'wide' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = wide_table_column_count,
            .row_count = 4U,
            .context = "wide varchar information schema",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO wide (id, v256, a256, a512) VALUES (1, '" WIDE_256_A "', '" WIDE_256_B
        "', '" WIDE_512_C "')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v256, v512, a256, a512 FROM wide",
            .values = wide_row,
            .column_count = wide_table_column_count,
            .row_count = 1U,
            .context = "wide varchar inserted values",
        }
    );
    failures += expect_dml_ok(database, "UPDATE wide SET v256 = '" WIDE_256_B "' WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE wide SET v512 = 'zz' WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v256, v512 FROM wide",
            .values = updated_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "wide varchar update readback",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE max_one (v VARCHAR(16383))");
    failures += expect_dml_ok(database, insert_max_sql, 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM max_one",
            .values = max_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "max varchar inserted value",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE max_pair (a VARCHAR(8191), b VARCHAR(8191))");
    failures +=
        expect_statement_ok(database, "CREATE TABLE int_boundary (id INT, v VARCHAR(16382))");
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE wide");
    failures += expect_dml_ok(database, "INSERT INTO clone SELECT * FROM wide", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v256, v512 FROM clone",
            .values = clone_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "wide varchar create table like",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE copied AS SELECT id, v256, v512 FROM wide");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v256, v512 FROM copied",
            .values = copied_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "wide varchar create table select",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen wide file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM max_one",
            .values = max_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "max varchar persisted after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v256, v512 FROM wide",
            .values = updated_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "wide varchar persisted after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    free(insert_max_sql);
    free(max_value);

    return failures;
}

static int test_varchar_diagnostics(void) {
    static const char *const escaped_wildcard_rows[] = {"30", "\\%", "31", "\\_"};
    static const char *const ignore_null_row[] = {"10", ""};
    static const char *const ignore_default_row[] = {"11", ""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE diag (id INT NOT NULL, v VARCHAR(2), nn VARCHAR(1) NOT NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO diag (id, v, nn) VALUES (30, '\\%', 'x'), (31, '\\_', 'y')",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM diag WHERE id IN (30, 31) ORDER BY id",
            .values = escaped_wildcard_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "varchar escaped wildcard literals",
        }
    );

    failures += execute_error(
        database,
        "INSERT INTO diag VALUES (1, 'abc', 'x')",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag VALUES (1, 'a', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO diag VALUES (1, 'ok', 'x')", 1);
    failures += execute_error(
        database,
        "UPDATE diag SET v = 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'v' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE diag SET nn = NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag VALUES (2, 1, 'x')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VARCHAR values support only string, NULL, and DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "UPDATE diag SET v = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VARCHAR values support only string, NULL, and DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag VALUES (2, 'a\\0', 'x')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "VARCHAR values do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag WHERE v = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE supports only baseline integer columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM diag ORDER BY v",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ORDER BY supports only integer, DATE, DATETIME, or TIMESTAMP descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT v FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(DISTINCT v) FROM diag",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT(DISTINCT column) supports only integer descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_length (v VARCHAR(16384))",
        (struct expected_sql_error){
            .code = mysql_error_column_length_too_big,
            .sqlstate = "42000",
            .message_part = "Column length too big for column 'v' (max = 16383)",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_row_size (a VARCHAR(8192), b VARCHAR(8192))",
        (struct expected_sql_error){
            .code = mysql_error_row_size_too_large,
            .sqlstate = "42000",
            .message_part = "Row size too large",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int_row_size (id INT, v VARCHAR(16383))",
        (struct expected_sql_error){
            .code = mysql_error_row_size_too_large,
            .sqlstate = "42000",
            .message_part = "Row size too large",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE add_size (id INT, v VARCHAR(16382))");
    failures += execute_error(
        database,
        "ALTER TABLE add_size ADD COLUMN s SMALLINT",
        (struct expected_sql_error){
            .code = mysql_error_row_size_too_large,
            .sqlstate = "42000",
            .message_part = "Row size too large",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_varchar_pk (v VARCHAR(256) NOT NULL PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_varchar_unique (v VARCHAR(256) UNIQUE)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_varchar_key (v VARCHAR(256), KEY v_key (v))",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE alter_key (v VARCHAR(256))");
    failures += execute_error(
        database,
        "ALTER TABLE alter_key ADD INDEX v_key (v)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (v VARCHAR(2) DEFAULT 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag (id, v) VALUES (20, 'a')",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO diag (id, nn) VALUES (10, NULL)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM diag WHERE id = 10",
            .values = ignore_null_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "varchar insert ignore null adjustment",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO diag (id, v) VALUES (11, 'a')",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM diag WHERE id = 11",
            .values = ignore_default_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "varchar insert ignore no-default adjustment",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_varchar_independent_handles(void) {
    static const char *const first_expected[] = {"one"};
    static const char *const second_expected[] = {"two"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v VARCHAR(5))");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v VARCHAR(5))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 'one')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 'two')", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent varchar state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent varchar state",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

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
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
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
        "%s/mylite_varchar_type_%d_%s.mylite",
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
