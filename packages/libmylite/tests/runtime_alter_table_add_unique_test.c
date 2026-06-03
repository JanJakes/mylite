#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "sqlite3.h"
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
    statistics_probe_field_count = 5,
    added_unique_statistics_row_count = 8,
    added_unique_column_key_row_count = 10,
    added_unique_constraint_row_count = 9,
    added_unique_key_usage_row_count = 9,
    added_unique_physical_index_count = 9,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_duplicate_key = 1062,
    mysql_error_key_column_missing = 1072,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_storage_engine_cant_index_column = 1167,
    mysql_error_blob_key_without_length = 1170,
    mysql_error_incorrect_index_name = 1280,
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

static int test_alter_add_unique_success_metadata_and_persistence(void);
static int test_alter_add_unique_validation_dml_and_drop(void);
static int test_alter_add_unique_diagnostics(void);
static int test_alter_add_unique_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_alter_unique_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
);
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

    failures += test_alter_add_unique_success_metadata_and_persistence();
    failures += test_alter_add_unique_validation_dml_and_drop();
    failures += test_alter_add_unique_diagnostics();
    failures += test_alter_add_unique_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_add_unique_success_metadata_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "add_unique",
        "CREATE TABLE `add_unique` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `u` bigint unsigned DEFAULT NULL,\n"
        "  `name` varchar(10) DEFAULT NULL,\n"
        "  `c` char(3) DEFAULT NULL,\n"
        "  `amount` decimal(5,2) DEFAULT NULL,\n"
        "  `d` date DEFAULT NULL,\n"
        "  `dt` datetime DEFAULT NULL,\n"
        "  `ts` timestamp NULL DEFAULT NULL,\n"
        "  `txt` text,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `u_v` (`v`),\n"
        "  UNIQUE KEY `u_u` (`u`),\n"
        "  UNIQUE KEY `name` (`name`),\n"
        "  UNIQUE KEY `u_c` (`c`),\n"
        "  UNIQUE KEY `u_amount` (`amount`),\n"
        "  UNIQUE KEY `u_d` (`d`),\n"
        "  UNIQUE KEY `u_dt` (`dt`),\n"
        "  UNIQUE KEY `u_ts` (`ts`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const column_key_rows[] = {
        "amount", "UNI", "c",  "UNI", "d",   "UNI", "dt", "UNI", "id", "PRI",
        "name",   "UNI", "ts", "UNI", "txt", "",    "u",  "UNI", "v",  "UNI",
    };
    static const char *const statistics_rows[] = {
        "name", "0", "1", "name", "YES", "u_amount", "0", "1", "amount", "YES",
        "u_c",  "0", "1", "c",    "YES", "u_d",      "0", "1", "d",      "YES",
        "u_dt", "0", "1", "dt",   "YES", "u_ts",     "0", "1", "ts",     "YES",
        "u_u",  "0", "1", "u",    "YES", "u_v",      "0", "1", "v",      "YES",
    };
    static const char *const constraint_rows[] = {
        "name",
        "UNIQUE",
        "PRIMARY",
        "PRIMARY KEY",
        "u_amount",
        "UNIQUE",
        "u_c",
        "UNIQUE",
        "u_d",
        "UNIQUE",
        "u_dt",
        "UNIQUE",
        "u_ts",
        "UNIQUE",
        "u_u",
        "UNIQUE",
        "u_v",
        "UNIQUE",
    };
    static const char *const key_usage_rows[] = {
        "name", "name", "1", "PRIMARY", "id", "1", "u_amount", "amount", "1",
        "u_c",  "c",    "1", "u_d",     "d",  "1", "u_dt",     "dt",     "1",
        "u_ts", "ts",   "1", "u_u",     "u",  "1", "u_v",      "v",      "1",
    };
    static const char *const clone_index_count_rows[] = {"9"};
    static const char *const copied_index_count_rows[] = {"0"};
    static const char *const renamed_index_count_rows[] = {"9"};
    static const char *const zero_count_rows[] = {"0"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open add unique file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE add_unique ("
        "id INT PRIMARY KEY, v INT, u BIGINT UNSIGNED, name VARCHAR(10), c CHAR(3), "
        "amount DECIMAL(5,2), d DATE, dt DATETIME, ts TIMESTAMP NULL, txt TEXT)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO add_unique VALUES "
        "(1,10,100,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03',"
        "'2024-01-01 01:02:03','hello'),"
        "(2,NULL,200,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL,'body')",
        2
    );
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_v (v)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE KEY u_u (u)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE INDEX (name)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_c (c)");
    failures +=
        expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_amount (amount)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_d (d)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_dt (dt)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE add_unique ADD UNIQUE u_ts (ts)");
    failures += expect_physical_index_count(
        database,
        added_unique_physical_index_count,
        "physical indexes after ADD UNIQUE"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_unique",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after ADD UNIQUE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_unique' "
                   "ORDER BY COLUMN_NAME",
            .values = column_key_rows,
            .column_count = 2U,
            .row_count = added_unique_column_key_row_count,
            .context = "I_S COLUMNS after ADD UNIQUE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_unique' AND INDEX_NAME <> 'PRIMARY' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = added_unique_statistics_row_count,
            .context = "I_S STATISTICS after ADD UNIQUE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_unique' ORDER BY CONSTRAINT_NAME",
            .values = constraint_rows,
            .column_count = 2U,
            .row_count = added_unique_constraint_row_count,
            .context = "I_S TABLE_CONSTRAINTS after ADD UNIQUE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION FROM "
                   "INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_unique' ORDER BY CONSTRAINT_NAME",
            .values = key_usage_rows,
            .column_count = 3U,
            .row_count = added_unique_key_usage_row_count,
            .context = "I_S KEY_COLUMN_USAGE after ADD UNIQUE",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE add_unique");
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT * FROM add_unique");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone'",
            .values = clone_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones ADD UNIQUE descriptors",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'copied'",
            .values = copied_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE SELECT omits ADD UNIQUE descriptors",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after ADD UNIQUE"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen add unique file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_unique",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after ADD UNIQUE reopen",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE add_unique TO renamed_unique");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_unique'",
            .values = renamed_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statistics after ADD UNIQUE rename",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_unique");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_unique'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statistics after ADD UNIQUE drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_unique_validation_dml_and_drop(void) {
    static const char *const preserved_rows[] = {"1", "10", "2", "10"};
    static const char *const composite_preserved_rows[] = {"1", "2", "10", "1", "2", "20"};
    static const char *const duplicate_null_rows[] = {"1", NULL, "2", NULL, "3", "10", "4", "20"};
    static const char *const composite_rows[] = {
        "1",
        "2",
        "10",
        "1",
        NULL,
        "11",
        "1",
        NULL,
        "12",
        NULL,
        "2",
        "13",
    };
    static const char *const after_drop_rows[] = {"1", "a", "2", "A"};
    static const char *const suffix_show_create_rows[] = {
        "suffix",
        "CREATE TABLE `suffix` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `name` int DEFAULT NULL,\n"
        "  UNIQUE KEY `name_2` (`name`),\n"
        "  KEY `name` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "validation") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unique validation db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE duplicate_values (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO duplicate_values VALUES (1,10),(2,10)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_values ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'duplicate_values.u_v'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM duplicate_values ORDER BY id",
            .values = preserved_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "rows preserved after failed ADD UNIQUE",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE duplicate_tuple (a INT, b INT, c INT)");
    failures += expect_dml_ok(database, "INSERT INTO duplicate_tuple VALUES (1,2,10),(1,2,20)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE duplicate_tuple ADD UNIQUE u_ab (a,b)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'duplicate_tuple.u_ab'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM duplicate_tuple ORDER BY c",
            .values = composite_preserved_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "rows preserved after failed composite ADD UNIQUE",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE composite_null (a INT, b INT, c INT)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_null VALUES (1,2,10),(1,NULL,11),(1,NULL,12),(NULL,2,13)",
        4
    );
    failures +=
        expect_alter_unique_ok(database, "ALTER TABLE composite_null ADD UNIQUE u_ab (a,b)");
    failures += execute_error(
        database,
        "INSERT INTO composite_null VALUES (1,2,20)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'composite_null.u_ab'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM composite_null ORDER BY c",
            .values = composite_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "composite ADD UNIQUE allows duplicate NULL tuples",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE duplicate_null (id INT, v INT)");
    failures +=
        expect_dml_ok(database, "INSERT INTO duplicate_null VALUES (1,NULL),(2,NULL),(3,10)", 3);
    failures += expect_alter_unique_ok(database, "ALTER TABLE duplicate_null ADD UNIQUE u_v (v)");
    failures += execute_error(
        database,
        "INSERT INTO duplicate_null VALUES (4,10)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'duplicate_null.u_v'",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO duplicate_null VALUES (4,20)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM duplicate_null ORDER BY id",
            .values = duplicate_null_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "duplicate NULL values after ADD UNIQUE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE varchar_case (id INT, name VARCHAR(10))");
    failures += expect_dml_ok(database, "INSERT INTO varchar_case VALUES (1,'a'),(2,'A')", 2);
    failures += execute_error(
        database,
        "ALTER TABLE varchar_case ADD UNIQUE u_name (name)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a' for key 'varchar_case.u_name'",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM varchar_case WHERE id = 2", 1);
    failures +=
        expect_alter_unique_ok(database, "ALTER TABLE varchar_case ADD UNIQUE u_name (name)");
    failures += expect_dml_ok(database, "INSERT INTO varchar_case VALUES (2,'b')", 1);
    failures += execute_error(
        database,
        "UPDATE varchar_case SET name = 'A' WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'A' for key 'varchar_case.u_name'",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE varchar_case DROP INDEX u_name");
    failures += expect_dml_ok(database, "UPDATE varchar_case SET name = 'A' WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, name FROM varchar_case ORDER BY id",
            .values = after_drop_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "duplicates allowed after dropping added unique index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE char_space (id INT, name CHAR(10))");
    failures += expect_dml_ok(database, "INSERT INTO char_space VALUES (1,'a'),(2,'a ')", 2);
    failures += execute_error(
        database,
        "ALTER TABLE char_space ADD UNIQUE u_name (name)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a' for key 'char_space.u_name'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE non_ascii (name VARCHAR(10))");
    failures += expect_dml_ok(database, "INSERT INTO non_ascii VALUES ('é')", 1);
    failures += execute_error(
        database,
        "ALTER TABLE non_ascii ADD UNIQUE u_name (name)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE suffix (id INT, name INT, KEY name (id))");
    failures += expect_alter_unique_ok(database, "ALTER TABLE suffix ADD UNIQUE (name)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE suffix",
            .values = suffix_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ADD UNIQUE omitted-name suffix",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_unique_diagnostics(void) {
    static const char *const named_constraint_row[] = {"u_id", "UNIQUE"};
    static const char *const explicit_index_constraint_row[] = {"visible", "UNIQUE"};
    static const char *const constraint_form_rows[] = {
        "b",
        "UNIQUE",
        "k_c",
        "UNIQUE",
        "uq_d",
        "UNIQUE",
        "visible",
        "UNIQUE",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics db");
    failures += execute_error(
        database,
        "ALTER TABLE no_default ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE diag (id INT, v INT, txt TEXT)");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.diag ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.diag ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += expect_alter_unique_ok(database, "ALTER TABLE diag ADD UNIQUE u_v (v)");
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD UNIQUE u_v (id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'u_v'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD UNIQUE `PRIMARY` (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD UNIQUE u_missing (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += expect_alter_unique_ok(database, "ALTER TABLE diag ADD UNIQUE u_txt (txt)");
    failures += expect_alter_unique_ok(database, "ALTER TABLE diag ADD UNIQUE u_multi (id, v)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE prefix_diag (a VARCHAR(10), b VARCHAR(10))");
    failures += expect_alter_unique_ok(
        database,
        "ALTER TABLE prefix_diag ADD UNIQUE u_prefix (a(2), b(2))"
    );
    failures +=
        expect_alter_unique_ok(database, "ALTER TABLE diag ADD CONSTRAINT u_id UNIQUE (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'diag' AND CONSTRAINT_NAME = 'u_id'",
            .values = named_constraint_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "named ALTER ADD CONSTRAINT UNIQUE metadata",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE explicit_constraint (a INT, b INT)");
    failures += expect_alter_unique_ok(
        database,
        "ALTER TABLE explicit_constraint ADD CONSTRAINT ignored UNIQUE KEY visible (b)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'explicit_constraint' AND CONSTRAINT_NAME = 'visible'",
            .values = explicit_index_constraint_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER ADD CONSTRAINT UNIQUE explicit index metadata",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE constraint_forms (b INT, c INT, d INT, e INT)");
    failures +=
        expect_alter_unique_ok(database, "ALTER TABLE constraint_forms ADD CONSTRAINT UNIQUE (b)");
    failures += expect_alter_unique_ok(
        database,
        "ALTER TABLE constraint_forms ADD CONSTRAINT UNIQUE KEY k_c (c)"
    );
    failures += expect_alter_unique_ok(
        database,
        "ALTER TABLE constraint_forms ADD CONSTRAINT uq_d UNIQUE KEY (d)"
    );
    failures += expect_alter_unique_ok(
        database,
        "ALTER TABLE constraint_forms ADD CONSTRAINT ignored UNIQUE visible (e)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'constraint_forms' ORDER BY CONSTRAINT_NAME",
            .values = constraint_form_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "ALTER ADD CONSTRAINT UNIQUE visible name variants",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD UNIQUE u_qualified (diag.id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE zero_chars (c CHAR(0), v VARCHAR(0))");
    failures += execute_error(
        database,
        "ALTER TABLE zero_chars ADD UNIQUE u_c (c)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'c'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE zero_chars ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_unique_independent_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first db");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second db");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT)");
    failures += expect_alter_unique_ok(first, "ALTER TABLE t ADD UNIQUE u_v (v)");

    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT)");

    failures += expect_physical_index_count(first, 1, "first physical unique index");
    failures += expect_physical_index_count(second, 0, "second physical unique index");

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "%s: expected success, got %d %s %s\n",
            sql,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures += expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_alter_unique_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_physical_index_count(
    mylite_db *database,
    int expected_count,
    const char *context
) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3 *connection = mylite_connection_sqlite_for_test(database);
    sqlite3_stmt *statement = NULL;
    int actual_count = 0;
    int rc = SQLITE_OK;

    if (connection == NULL) {
        fprintf(stderr, "%s: missing SQLite test connection\n", context);
        return 1;
    }

    rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema "
        "WHERE type = 'index' AND name GLOB '_mylite_user_index_*'",
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: prepare physical index query failed: %d\n", context, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        actual_count = sqlite3_column_int(statement, 0);
        rc = SQLITE_OK;
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: physical index query failed: %d\n", context, rc);
        return 1;
    }

    return expect_int(actual_count, expected_count, context);
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
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at row %zu column %zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_alter_add_unique_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
    }
    fclose(file);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing [%s], got [%s]\n",
            context,
            needle,
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
