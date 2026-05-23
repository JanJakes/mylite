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
    information_schema_column_count = 6,
    scalar_synonym_column_count = 8,
    automatic_temporal_show_row_count = 7,
    automatic_temporal_information_schema_row_count = 6,
    automatic_temporal_data_column_count = 7,
    parenthesized_current_timestamp_show_row_count = 10,
    parenthesized_current_timestamp_information_schema_column_count = 3,
    parenthesized_current_timestamp_data_column_count = 10,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_variable_cant_be_set = 1231,
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

static int test_current_timestamp_scalar_and_system_variable(void);
static int test_current_timestamp_defaults_updates_metadata_and_persistence(void);
static int test_parenthesized_current_timestamp_defaults(void);
static int test_current_timestamp_alter_add_and_file_safety(void);
static int test_current_timestamp_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
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

    failures += test_current_timestamp_scalar_and_system_variable();
    failures += test_current_timestamp_defaults_updates_metadata_and_persistence();
    failures += test_parenthesized_current_timestamp_defaults();
    failures += test_current_timestamp_alter_add_and_file_safety();
    failures += test_current_timestamp_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_current_timestamp_scalar_and_system_variable(void) {
    static const char *const function_values[] = {
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "1700000000.000000",
    };
    static const char *const at_at_values[] = {
        "1700000060.000000",
        "2023-11-14 22:14:20",
    };
    static const char *const session_values[] = {
        "1700000120.000000",
        "2023-11-14 22:15:20",
    };
    static const char *const at_at_session_values[] = {
        "1700000180.000000",
        "2023-11-14 22:16:20",
    };
    static const char *const plus_values[] = {
        "1700000240.000000",
        "2023-11-14 22:17:20",
    };
    static const char *const max_values[] = {"2038-01-19 03:14:07"};
    static const char *const show_timestamp_values[] = {
        "timestamp",
        "1700000240.000000",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar memory");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CURRENT_TIMESTAMP, CURRENT_TIMESTAMP(), NOW(), LOCALTIME, "
                   "LOCALTIME(), LOCALTIMESTAMP, LOCALTIMESTAMP(), @@timestamp",
            .values = function_values,
            .column_count = scalar_synonym_column_count,
            .row_count = 1U,
            .context = "current timestamp scalar synonyms",
        }
    );
    failures += expect_statement_ok(database, "SET @@timestamp = 1700000060");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@timestamp, NOW()",
            .values = at_at_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unqualified system timestamp assignment",
        }
    );
    failures += expect_statement_ok(database, "SET SESSION timestamp = 1700000120");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@timestamp, NOW()",
            .values = session_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "session timestamp assignment",
        }
    );
    failures += expect_statement_ok(database, "SET @@SESSION.timestamp = 1700000180");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@timestamp, NOW()",
            .values = at_at_session_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "scoped system timestamp assignment",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = +1700000240");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@timestamp, NOW()",
            .values = plus_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "positive timestamp assignment",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'timestamp'",
            .values = show_timestamp_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "timestamp show variables",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES LIKE 'timestamp'",
            .values = NULL,
            .column_count = 2U,
            .row_count = 0U,
            .context = "timestamp omitted from global variables",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = -1");
    failures += expect_statement_ok(database, "SET timestamp = DEFAULT");
    failures += expect_statement_ok(database, "SET timestamp = 2147483647");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT NOW()",
            .values = max_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "maximum timestamp assignment",
        }
    );
    failures += execute_error(
        database,
        "SET timestamp = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'timestamp' can't be set",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_current_timestamp_defaults_updates_metadata_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "ts",
        "timestamp",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "dt",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "ts_init",
        "timestamp",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_init",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "ts_up",
        "timestamp",
        "YES",
        "",
        NULL,
        "on update CURRENT_TIMESTAMP",
        "dt_up",
        "datetime",
        "YES",
        "",
        NULL,
        "on update CURRENT_TIMESTAMP",
    };
    static const char *const show_create_rows[] = {
        "automatic_temporals",
        "CREATE TABLE `automatic_temporals` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `ts` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,\n"
        "  `dt` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,\n"
        "  `ts_init` timestamp NULL DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_init` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `ts_up` timestamp NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,\n"
        "  `dt_up` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "ts",
        "timestamp",
        "CURRENT_TIMESTAMP",
        "YES",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "0",
        "dt",
        "datetime",
        "CURRENT_TIMESTAMP",
        "YES",
        "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
        "0",
        "ts_init",
        "timestamp",
        "CURRENT_TIMESTAMP",
        "YES",
        "DEFAULT_GENERATED",
        "0",
        "dt_init",
        "datetime",
        "CURRENT_TIMESTAMP",
        "YES",
        "DEFAULT_GENERATED",
        "0",
        "ts_up",
        "timestamp",
        NULL,
        "YES",
        "on update CURRENT_TIMESTAMP",
        "0",
        "dt_up",
        "datetime",
        NULL,
        "YES",
        "on update CURRENT_TIMESTAMP",
        "0",
    };
    static const char *const after_insert_rows[] = {
        "1",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        NULL,
        NULL,
    };
    static const char *const after_change_counts[] = {
        "1",
        "0",
    };
    static const char *const after_change_rows[] = {
        "2",
        "2023-11-14 22:14:20",
        "2023-11-14 22:14:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:14:20",
        "2023-11-14 22:14:20",
    };
    static const char *const after_noop_counts[] = {
        "0",
        "0",
    };
    static const char *const after_noop_rows[] = {
        "2",
        "2023-11-14 22:14:20",
        "2023-11-14 22:14:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:14:20",
        "2023-11-14 22:14:20",
    };
    static const char *const after_explicit_current_counts[] = {
        "1",
        "0",
    };
    static const char *const after_explicit_current_rows[] = {
        "2",
        "2023-11-14 22:16:20",
        "2023-11-14 22:16:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:16:20",
        "2023-11-14 22:16:20",
    };
    static const char *const persisted_rows[] = {
        "2",
        "2023-11-14 22:16:20",
        "2023-11-14 22:16:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:16:20",
        "2023-11-14 22:16:20",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "automatic") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open automatic file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE automatic_temporals ("
        "id INT, "
        "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
        "ts_init TIMESTAMP DEFAULT NOW(), "
        "dt_init DATETIME DEFAULT LOCALTIME, "
        "ts_up TIMESTAMP NULL ON UPDATE LOCALTIMESTAMP, "
        "dt_up DATETIME ON UPDATE CURRENT_TIMESTAMP())"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM automatic_temporals",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = automatic_temporal_show_row_count,
            .context = "automatic temporal SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE automatic_temporals",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "automatic temporal SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, "
                   "DATETIME_PRECISION FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'automatic_temporals' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = automatic_temporal_information_schema_row_count,
            .context = "automatic temporal information schema",
        }
    );
    failures += expect_statement_ok(database, "INSERT INTO automatic_temporals(id) VALUES(1)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals",
            .values = after_insert_rows,
            .column_count = automatic_temporal_data_column_count,
            .row_count = 1U,
            .context = "insert materializes current timestamp defaults",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000060");
    failures += expect_dml_ok(database, "UPDATE automatic_temporals SET id = 2 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_change_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "changed update counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals",
            .values = after_change_rows,
            .column_count = automatic_temporal_data_column_count,
            .row_count = 1U,
            .context = "changed update advances automatic temporal columns",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000120");
    failures += expect_dml_ok(database, "UPDATE automatic_temporals SET id = 2 WHERE id = 2", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_noop_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "no-op update counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals",
            .values = after_noop_rows,
            .column_count = automatic_temporal_data_column_count,
            .row_count = 1U,
            .context = "no-op update does not advance automatic temporal columns",
        }
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000180");
    failures += expect_dml_ok(
        database,
        "UPDATE automatic_temporals SET ts = CURRENT_TIMESTAMP WHERE id = 2",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_explicit_current_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "explicit current timestamp counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals",
            .values = after_explicit_current_rows,
            .column_count = automatic_temporal_data_column_count,
            .row_count = 1U,
            .context = "explicit current timestamp assignment",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen automatic file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt, ts_init, dt_init, ts_up, dt_up FROM automatic_temporals",
            .values = persisted_rows,
            .column_count = automatic_temporal_data_column_count,
            .row_count = 1U,
            .context = "automatic temporal values persist",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_parenthesized_current_timestamp_defaults(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "dt",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_call",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_now",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_local",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_local_call",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_lts",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_lts_call",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_nested",
        "datetime",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "ts",
        "timestamp",
        "YES",
        "",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
    };
    static const char *const information_schema_rows[] = {
        "id",
        NULL,
        "",
        "dt",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_call",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_now",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_local",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_local_call",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_lts",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_lts_call",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "dt_nested",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
        "ts",
        "CURRENT_TIMESTAMP",
        "DEFAULT_GENERATED",
    };
    static const char *const show_create_rows[] = {
        "current_exprs",
        "CREATE TABLE `current_exprs` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `dt` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_call` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_now` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_local` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_local_call` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_lts` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_lts_call` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `dt_nested` datetime DEFAULT CURRENT_TIMESTAMP,\n"
        "  `ts` timestamp NULL DEFAULT CURRENT_TIMESTAMP\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const inserted_rows[] = {
        "1",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
        "2023-11-14 22:13:20",
    };
    static const char *const insert_counts[] = {
        "1",
        "0",
    };
    static const char *const altered_show_create_rows[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `dt2` datetime DEFAULT CURRENT_TIMESTAMP\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const altered_rows[] = {
        "1",
        "2023-11-14 22:14:20",
        "2",
        "2023-11-14 22:14:20",
        "3",
        "2023-11-14 22:16:20",
    };
    static const char *const clone_show_create_rows[] = {
        "clone",
        "CREATE TABLE `clone` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `dt` datetime DEFAULT CURRENT_TIMESTAMP\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const clone_rows[] = {
        "1",
        "2023-11-14 22:17:20",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "parenthesized") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open parenthesized file");
    failures +=
        expect_int(mylite_open_memory(&second_database), MYLITE_OK, "open independent memory");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE current_exprs ("
        "id INT, "
        "dt DATETIME DEFAULT (CURRENT_TIMESTAMP), "
        "dt_call DATETIME DEFAULT (CURRENT_TIMESTAMP()), "
        "dt_now DATETIME DEFAULT (NOW()), "
        "dt_local DATETIME DEFAULT (LOCALTIME), "
        "dt_local_call DATETIME DEFAULT (LOCALTIME()), "
        "dt_lts DATETIME DEFAULT (LOCALTIMESTAMP), "
        "dt_lts_call DATETIME DEFAULT (LOCALTIMESTAMP()), "
        "dt_nested DATETIME DEFAULT ((NOW())), "
        "ts TIMESTAMP DEFAULT (NOW()))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM current_exprs",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = parenthesized_current_timestamp_show_row_count,
            .context = "parenthesized current timestamp SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'current_exprs' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = parenthesized_current_timestamp_information_schema_column_count,
            .row_count = parenthesized_current_timestamp_show_row_count,
            .context = "parenthesized current timestamp information schema",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE current_exprs",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized current timestamp SHOW CREATE",
        }
    );
    failures += expect_statement_ok(database, "INSERT INTO current_exprs(id) VALUES (1)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = insert_counts,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized current timestamp insert counts",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt, dt_call, dt_now, dt_local, dt_local_call, dt_lts, "
                   "dt_lts_call, dt_nested, ts FROM current_exprs",
            .values = inserted_rows,
            .column_count = parenthesized_current_timestamp_data_column_count,
            .row_count = 1U,
            .context = "parenthesized current timestamp insert materialization",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += expect_statement_ok(database, "INSERT INTO t(id) VALUES (1), (2)");
    failures += expect_statement_ok(database, "SET timestamp = 1700000060");
    failures +=
        expect_statement_ok(database, "ALTER TABLE t ADD COLUMN dt DATETIME DEFAULT (NOW())");
    failures += expect_statement_ok(database, "SET timestamp = 1700000120");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE t ALTER COLUMN dt SET DEFAULT (CURRENT_TIMESTAMP)"
    );
    failures += expect_statement_ok(database, "SET timestamp = 1700000180");
    failures += expect_statement_ok(database, "INSERT INTO t(id) VALUES (3)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE t MODIFY COLUMN dt DATETIME DEFAULT (LOCALTIMESTAMP)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE t CHANGE COLUMN dt dt2 DATETIME DEFAULT (LOCALTIME())"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = altered_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized altered current timestamp SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt2 FROM t ORDER BY id",
            .values = altered_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "parenthesized altered current timestamp rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE source_like(id INT, dt DATETIME DEFAULT (NOW()))"
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE source_like");
    failures += expect_statement_ok(database, "SET timestamp = 1700000240");
    failures += expect_statement_ok(database, "INSERT INTO clone(id) VALUES (1)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized current timestamp CREATE TABLE LIKE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt FROM clone",
            .values = clone_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized current timestamp clone rows",
        }
    );
    failures += expect_statement_ok(second_database, "SET timestamp = 1700000300");
    failures += expect_query_values(
        second_database,
        (struct expected_query){
            .sql = "SELECT NOW()",
            .values = (const char *const[]){"2023-11-14 22:18:20"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "parenthesized independent handle timestamp state",
        }
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "parenthesized current timestamp defaults leave preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen parenthesized file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt FROM clone",
            .values = clone_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parenthesized current timestamp clone persists",
        }
    );

    mylite_close(second_database);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_current_timestamp_alter_add_and_file_safety(void) {
    static const char *const after_add_rows[] = {
        "1",
        "2023-11-14 22:14:20",
        "2023-11-14 22:15:20",
        "2",
        "2023-11-14 22:14:20",
        "2023-11-14 22:15:20",
    };
    static const char *const after_update_rows[] = {
        "2",
        "2023-11-14 22:14:20",
        "2023-11-14 22:15:20",
        "11",
        "2023-11-14 22:16:20",
        "2023-11-14 22:16:20",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first_database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "alter_add") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &first_database), MYLITE_OK, "open alter file");
    failures +=
        expect_int(mylite_open_memory(&second_database), MYLITE_OK, "open independent memory");
    failures += expect_statement_ok(first_database, "CREATE DATABASE app");
    failures += expect_statement_ok(first_database, "USE app");
    failures += expect_statement_ok(first_database, "CREATE TABLE t (id INT)");
    failures += expect_statement_ok(first_database, "INSERT INTO t VALUES (1), (2)");
    failures += expect_statement_ok(first_database, "SET timestamp = 1700000060");
    failures += expect_statement_ok(
        first_database,
        "ALTER TABLE t ADD COLUMN ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE "
        "CURRENT_TIMESTAMP"
    );
    failures += expect_statement_ok(first_database, "SET timestamp = 1700000120");
    failures += expect_statement_ok(
        first_database,
        "ALTER TABLE t ADD COLUMN dt DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
    );
    failures += expect_query_values(
        first_database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt FROM t ORDER BY id",
            .values = after_add_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "alter add backfills current timestamp",
        }
    );
    failures += expect_statement_ok(first_database, "SET timestamp = 1700000180");
    failures += expect_dml_ok(first_database, "UPDATE t SET id = 11 WHERE id = 1", 1);
    failures += expect_query_values(
        first_database,
        (struct expected_query){
            .sql = "SELECT id, ts, dt FROM t ORDER BY id",
            .values = after_update_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "alter-added automatic temporal update",
        }
    );
    failures += expect_statement_ok(second_database, "SET timestamp = 1700000240");
    failures += expect_query_values(
        second_database,
        (struct expected_query){
            .sql = "SELECT NOW()",
            .values = (const char *const[]){"2023-11-14 22:17:20"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "independent handle timestamp state",
        }
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "current timestamp alter leaves preamble"
    );

    mylite_close(second_database);
    mylite_close(first_database);
    remove_related_files(path);
    return failures;
}

static int test_current_timestamp_diagnostics(void) {
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
    failures += execute_error(
        database,
        "CREATE TABLE bad_int_default (i INT DEFAULT CURRENT_TIMESTAMP)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'i'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_parenthesized_int_default (i INT DEFAULT (CURRENT_TIMESTAMP))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'i'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_fractional_default (dt DATETIME DEFAULT (CURRENT_TIMESTAMP(1)))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'dt'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_utc_default (dt DATETIME DEFAULT (UTC_TIMESTAMP()))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'dt'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_parenthesized_on_update (dt DATETIME ON UPDATE (CURRENT_TIMESTAMP))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '('",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int_update (i INT ON UPDATE CURRENT_TIMESTAMP)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'i'",
        }
    );
    failures += execute_error(
        database,
        "SELECT NOW(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SET timestamp = 2147483648",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'timestamp' can't be set",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE ints (i INT)");
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (CURRENT_TIMESTAMP)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CURRENT_TIMESTAMP values are supported only",
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
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "dml affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "dml warning count");
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
        "%s/mylite_current_timestamp_defaults_%d_%s.mylite",
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
