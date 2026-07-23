#include "mylite_test_support.h"

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
    related_file_suffix_capacity = 8,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    information_schema_columns_field_count = 5,
    information_schema_statistics_field_count = 5,
    show_table_status_query_capacity = 128,
    show_table_status_field_count = 18,
    show_table_status_name_column = 0,
    show_table_status_auto_increment_column = 10,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_wrong_auto_key = 1075,
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

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_table_status_auto_increment {
    const char *table_name;
    const char *value;
    const char *context;
};

static int test_serial_alias_metadata_dml_and_persistence(void);
static int test_serial_alias_forms_and_diagnostics(void);
static int test_serial_alias_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_show_table_status_auto_increment(
    mylite_db *database,
    struct expected_table_status_auto_increment expected
);
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

    failures += test_serial_alias_metadata_dml_and_persistence();
    failures += test_serial_alias_forms_and_diagnostics();
    failures += test_serial_alias_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_serial_alias_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "bigint unsigned",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "serial_t",
        "0",
        "id",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const show_create_initial[] = {
        "serial_t",
        "CREATE TABLE `serial_t` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_columns[] = {
        "id",
        "bigint unsigned",
        "NO",
        "PRI",
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        "",
    };
    static const char *const table_constraints[] = {"id", "UNIQUE"};
    static const char *const key_column_usage[] = {"id", "id", "1"};
    static const char *const statistics_rows[] = {"id", "0", "1", "id", ""};
    static const char *const generated_state[] = {"2", "0", "1"};
    static const char *const generated_rows[] = {"1", "10", "2", "20"};
    static const char *const explicit_state[] = {"1", "0", "1"};
    static const char *const final_state[] = {"1", "0", "11"};
    static const char *const final_rows[] = {
        "1",
        "10",
        "2",
        "20",
        "10",
        "100",
        "11",
        "110",
    };
    static const char *const show_create_advanced[] = {
        "serial_t",
        "CREATE TABLE `serial_t` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open serial metadata file"
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE serial_t (id SERIAL, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM serial_t",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "serial SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM serial_t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "serial SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_t",
            .values = show_create_initial,
            .column_count = 2U,
            .row_count = 1U,
            .context = "initial serial SHOW CREATE",
        }
    );
    failures += expect_show_table_status_auto_increment(
        database,
        (struct expected_table_status_auto_increment){
            .table_name = "serial_t",
            .value = "1",
            .context = "initial serial table status",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, EXTRA "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'serial_t' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns,
            .column_count = information_schema_columns_field_count,
            .row_count = 2U,
            .context = "serial information schema columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'serial_t' "
                   "ORDER BY CONSTRAINT_NAME",
            .values = table_constraints,
            .column_count = 2U,
            .row_count = 1U,
            .context = "serial information schema constraints",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'serial_t' "
                   "ORDER BY CONSTRAINT_NAME",
            .values = key_column_usage,
            .column_count = 3U,
            .row_count = 1U,
            .context = "serial information schema key column usage",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'serial_t' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = information_schema_statistics_field_count,
            .row_count = 1U,
            .context = "serial information schema statistics",
        }
    );

    failures += expect_statement_result(
        database,
        "INSERT INTO serial_t (v) VALUES (10), (20)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = generated_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "serial generated insert state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM serial_t ORDER BY id",
            .values = generated_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "serial generated rows",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO serial_t VALUES (10,100)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = explicit_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "serial explicit insert state",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO serial_t (v) VALUES (110)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = final_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "serial generated after explicit state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM serial_t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "serial final rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_t",
            .values = show_create_advanced,
            .column_count = 2U,
            .row_count = 1U,
            .context = "advanced serial SHOW CREATE",
        }
    );
    failures += expect_show_table_status_auto_increment(
        database,
        (struct expected_table_status_auto_increment){
            .table_name = "serial_t",
            .value = "12",
            .context = "advanced serial table status",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "serial lifecycle preserves preamble"
    );

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen serial metadata file"
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM serial_t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "serial rows persist after reopen",
        }
    );
    failures += expect_show_table_status_auto_increment(
        database,
        (struct expected_table_status_auto_increment){
            .table_name = "serial_t",
            .value = "12",
            .context = "reopened serial table status",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_serial_alias_forms_and_diagnostics(void) {
    static const char *const serial_null_columns[] = {
        "id",
        "bigint unsigned",
        "YES",
        "UNI",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const serial_null_show_create[] = {
        "serial_null",
        "CREATE TABLE `serial_null` (\n"
        "  `id` bigint unsigned AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const serial_primary_show_create[] = {
        "serial_primary",
        "CREATE TABLE `serial_primary` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const serial_key_show_create[] = {
        "serial_key",
        "CREATE TABLE `serial_key` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`),\n"
        "  KEY `id_2` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const serial_unique_show_create[] = {
        "serial_unique",
        "CREATE TABLE `serial_unique` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const option_rows[] = {"9", "90"};
    static const char *const option_last_insert_id[] = {"9"};
    static const char *const option_show_create[] = {
        "serial_option",
        "CREATE TABLE `serial_option` (\n"
        "  `id` bigint unsigned NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const auto_unique_columns[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const auto_key_columns[] = {
        "id",
        "int",
        "NO",
        "MUL",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const auto_null_columns[] = {
        "id",
        "int",
        "YES",
        "UNI",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const secondary_rows[] = {"1", "10", "2", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "forms") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open serial forms file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE serial_null (id SERIAL NULL, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM serial_null",
            .values = serial_null_columns,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "SERIAL NULL SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_null",
            .values = serial_null_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL NULL SHOW CREATE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE serial_primary (id SERIAL PRIMARY KEY, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_primary",
            .values = serial_primary_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL PRIMARY KEY SHOW CREATE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE serial_key (id SERIAL, v INT, KEY(id))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_key",
            .values = serial_key_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL plus KEY SHOW CREATE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE serial_unique (id SERIAL UNIQUE, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_unique",
            .values = serial_unique_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL UNIQUE SHOW CREATE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE serial_option (id SERIAL, v INT) AUTO_INCREMENT=9"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO serial_option (v) VALUES (90)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM serial_option",
            .values = option_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL table option generated row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = option_last_insert_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "SERIAL table option last insert id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE serial_option",
            .values = option_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SERIAL table option SHOW CREATE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_unique (id INT AUTO_INCREMENT UNIQUE, v INT)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM auto_unique",
            .values = auto_unique_columns,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "secondary unique auto increment columns",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO auto_unique (v) VALUES (10), (20)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_unique ORDER BY id",
            .values = secondary_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "secondary unique auto increment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_key (id INT AUTO_INCREMENT, KEY(id), v INT)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM auto_key",
            .values = auto_key_columns,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "secondary nonunique auto increment columns",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO auto_key (v) VALUES (10), (20)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_key ORDER BY id",
            .values = secondary_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "secondary nonunique auto increment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_null (id INT NULL AUTO_INCREMENT UNIQUE, v INT)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM auto_null",
            .values = auto_null_columns,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "nullable secondary auto increment columns",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE no_key (id INT AUTO_INCREMENT, v INT)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part = "there can be only one auto column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE serial_default_int (id SERIAL DEFAULT 7, v INT)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'id'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE serial_default_value (id SERIAL DEFAULT VALUE, v INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE two_auto (id SERIAL, other INT AUTO_INCREMENT, KEY(other))",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part = "there can be only one auto column",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE alter_t (id INT, v INT)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_t ADD COLUMN c SERIAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE ADD COLUMN does not support AUTO_INCREMENT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_t MODIFY v SERIAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE MODIFY COLUMN does not support AUTO_INCREMENT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_t CHANGE v changed SERIAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE CHANGE COLUMN does not support AUTO_INCREMENT",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_serial_alias_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10"};
    static const char *const second_rows[] = {"5", "50"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first serial file"
    );
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id SERIAL, v INT)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second serial file"
    );
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id SERIAL, v INT) AUTO_INCREMENT=5");
    failures += expect_statement_result(
        second,
        "INSERT INTO t (v) VALUES (50)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first serial handle rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second serial handle rows",
        }
    );
    failures += expect_show_table_status_auto_increment(
        first,
        (struct expected_table_status_auto_increment){
            .table_name = "t",
            .value = "2",
            .context = "first serial status",
        }
    );
    failures += expect_show_table_status_auto_increment(
        second,
        (struct expected_table_status_auto_increment){
            .table_name = "t",
            .value = "5",
            .context = "second serial status",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            sql
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            sql
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_table_status_auto_increment(
    mylite_db *database,
    struct expected_table_status_auto_increment expected
) {
    char sql[show_table_status_query_capacity];
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "SHOW TABLE STATUS LIKE '%s'", expected.table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format SHOW TABLE STATUS query\n", expected.context);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            show_table_status_field_count,
            expected.context
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
        failures += expect_result_value(
            result,
            0U,
            show_table_status_name_column,
            expected.table_name,
            expected.context
        );
        failures += expect_result_value(
            result,
            0U,
            show_table_status_auto_increment_column,
            expected.value,
            expected.context
        );
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
    return mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    if (bytes_read != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, bytes_read);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
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
    (void)fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
