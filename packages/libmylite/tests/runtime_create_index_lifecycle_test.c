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
    created_index_statistics_row_count = 8,
    created_index_physical_index_count = 9,
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

static int test_create_index_success_metadata_and_persistence(void);
static int test_create_unique_index_validation_and_dml(void);
static int test_create_index_diagnostics(void);
static int test_create_index_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_create_index_ok(mylite_db *database, const char *sql);
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

    failures += test_create_index_success_metadata_and_persistence();
    failures += test_create_unique_index_validation_and_dml();
    failures += test_create_index_diagnostics();
    failures += test_create_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_index_success_metadata_and_persistence(void) {
    static const char *const statistics_rows[] = {
        "k_amount", "1", "1", "amount", "YES", "k_c",    "1", "1", "c",    "YES",
        "k_d",      "1", "1", "d",      "YES", "k_dt",   "1", "1", "dt",   "YES",
        "k_ts",     "1", "1", "ts",     "YES", "k_u",    "1", "1", "u",    "YES",
        "k_v",      "1", "1", "v",      "YES", "u_name", "0", "1", "name", "YES",
    };
    static const char *const show_create_rows[] = {
        "create_idx",
        "CREATE TABLE `create_idx` (\n"
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
        "  UNIQUE KEY `u_name` (`name`),\n"
        "  KEY `k_v` (`v`),\n"
        "  KEY `k_u` (`u`),\n"
        "  KEY `k_c` (`c`),\n"
        "  KEY `k_amount` (`amount`),\n"
        "  KEY `k_d` (`d`),\n"
        "  KEY `k_dt` (`dt`),\n"
        "  KEY `k_ts` (`ts`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const constraint_rows[] = {"u_name", "UNIQUE", "u_name", "name"};
    static const char *const clone_index_count_rows[] = {"9"};
    static const char *const copied_index_count_rows[] = {"0"};
    static const char *const renamed_index_count_rows[] = {"9"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const row_values[] = {"1", "15", "aa"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create index file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE create_idx ("
        "id INT PRIMARY KEY, v INT, u BIGINT UNSIGNED, name VARCHAR(10), c CHAR(3), "
        "amount DECIMAL(5,2), d DATE, dt DATETIME, ts TIMESTAMP NULL, txt TEXT)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO create_idx VALUES "
        "(1,10,100,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03',"
        "'2024-01-01 01:02:03','hello'),"
        "(2,NULL,200,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL,'body')",
        2
    );
    failures += expect_create_index_ok(database, "CREATE INDEX k_v ON create_idx (v)");
    failures += expect_create_index_ok(database, "CREATE UNIQUE INDEX u_name ON create_idx (name)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_u ON create_idx (u)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_c ON create_idx (c)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_amount ON create_idx (amount)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_d ON create_idx (d)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_dt ON create_idx (dt)");
    failures += expect_create_index_ok(database, "CREATE INDEX k_ts ON create_idx (ts)");
    failures += expect_physical_index_count(
        database,
        created_index_physical_index_count,
        "physical indexes after CREATE INDEX"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE create_idx",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after CREATE INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'create_idx' AND INDEX_NAME <> 'PRIMARY' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = created_index_statistics_row_count,
            .context = "I_S STATISTICS after CREATE INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'create_idx' AND CONSTRAINT_TYPE = 'UNIQUE' "
                   "ORDER BY CONSTRAINT_NAME",
            .values = constraint_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unique constraints after CREATE UNIQUE INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME FROM "
                   "INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'create_idx' AND CONSTRAINT_NAME = 'u_name'",
            .values = &constraint_rows[2],
            .column_count = 2U,
            .row_count = 1U,
            .context = "key-column usage after CREATE UNIQUE INDEX",
        }
    );
    failures += expect_dml_ok(database, "UPDATE create_idx SET v = 15 WHERE id = 1", 1);
    failures += expect_dml_ok(database, "DELETE FROM create_idx WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, name FROM create_idx",
            .values = row_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "DML after CREATE INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE create_idx");
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT * FROM create_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone'",
            .values = clone_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones CREATE INDEX descriptors",
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
            .context = "CREATE TABLE SELECT omits CREATE INDEX descriptors",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after CREATE INDEX"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen create index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE create_idx",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after CREATE INDEX reopen",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE create_idx TO renamed_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_idx'",
            .values = renamed_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statistics after CREATE INDEX table rename",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_idx'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statistics after CREATE INDEX table drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_create_unique_index_validation_and_dml(void) {
    static const char *const preserved_rows[] = {"1", "10", "2", "10"};
    static const char *const duplicate_null_rows[] = {"1", NULL, "2", NULL, "3", "10", "4", "20"};
    static const char *const after_drop_rows[] = {"1", "a", "2", "A"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unique_validation") != 0) {
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
        "CREATE UNIQUE INDEX u_v ON duplicate_values (v)",
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
            .context = "rows preserved after failed CREATE UNIQUE INDEX",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE duplicate_null (id INT, v INT)");
    failures +=
        expect_dml_ok(database, "INSERT INTO duplicate_null VALUES (1,NULL),(2,NULL),(3,10)", 3);
    failures += expect_create_index_ok(database, "CREATE UNIQUE INDEX u_v ON duplicate_null (v)");
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
            .context = "duplicate NULL values after CREATE UNIQUE INDEX",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE varchar_case (id INT, name VARCHAR(10))");
    failures += expect_dml_ok(database, "INSERT INTO varchar_case VALUES (1,'a'),(2,'A')", 2);
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_name ON varchar_case (name)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'a' for key 'varchar_case.u_name'",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM varchar_case WHERE id = 2", 1);
    failures +=
        expect_create_index_ok(database, "CREATE UNIQUE INDEX u_name ON varchar_case (name)");
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
            .context = "duplicates allowed after dropping standalone unique index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE non_ascii (name VARCHAR(10))");
    failures += expect_dml_ok(database, "INSERT INTO non_ascii VALUES ('é')", 1);
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_name ON non_ascii (name)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "non-ASCII string key values are not supported",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_create_index_diagnostics(void) {
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
        "CREATE INDEX k_v ON no_default (v)",
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
        "CREATE INDEX k_v ON missing_schema.diag (v)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_v ON missing_table (v)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_v ON _mylite_private.diag (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_v ON _mylite_private (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += expect_create_index_ok(database, "CREATE INDEX k_v ON diag (v)");
    failures += execute_error(
        database,
        "CREATE INDEX k_v ON diag (id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k_v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX `PRIMARY` ON diag (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_missing ON diag (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_txt ON diag (txt)",
        (struct expected_sql_error){
            .code = mysql_error_blob_key_without_length,
            .sqlstate = "42000",
            .message_part = "BLOB/TEXT column 'txt' used in key specification without a key length",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_multi ON diag (id, v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE INDEX supports exactly one key column",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE zero_chars (c CHAR(0), v VARCHAR(0))");
    failures += execute_error(
        database,
        "CREATE INDEX k_c ON zero_chars (c)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'c'",
        }
    );
    failures += execute_error(
        database,
        "CREATE UNIQUE INDEX u_v ON zero_chars (v)",
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

static int test_create_index_independent_handles(void) {
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
    failures += expect_create_index_ok(first, "CREATE INDEX k_v ON t (v)");

    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT)");

    failures += expect_physical_index_count(first, 1, "first physical index");
    failures += expect_physical_index_count(second, 0, "second physical index");

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

static int expect_create_index_ok(mylite_db *database, const char *sql) {
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
        "/tmp/mylite_create_index_%d_%s.mylite",
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
