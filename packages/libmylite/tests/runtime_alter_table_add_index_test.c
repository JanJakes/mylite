#include "mylite_test_support.h"

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
    added_index_statistics_row_count = 9,
    added_index_physical_index_count = 10,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key_name = 1061,
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

static int test_alter_add_index_success_metadata_and_persistence(void);
static int test_alter_add_index_result_metadata_cache_invalidation(void);
static int test_alter_add_index_name_generation_and_auto_increment(void);
static int test_alter_add_index_diagnostics(void);
static int test_alter_add_index_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_alter_index_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_select_column_key_flags(
    mylite_db *database,
    const char *sql,
    uint32_t expected_key_flags,
    const char *context
);
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

    failures += test_alter_add_index_success_metadata_and_persistence();
    failures += test_alter_add_index_result_metadata_cache_invalidation();
    failures += test_alter_add_index_name_generation_and_auto_increment();
    failures += test_alter_add_index_diagnostics();
    failures += test_alter_add_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_add_index_success_metadata_and_persistence(void) {
    static const char *const statistics_rows[] = {
        "k_amount", "1",   "1",    "amount", "YES",    "k_c", "1",    "1",    "c",
        "YES",      "k_d", "1",    "1",      "d",      "YES", "k_dt", "1",    "1",
        "dt",       "YES", "k_ts", "1",      "1",      "ts",  "YES",  "k_u",  "1",
        "1",        "u",   "YES",  "k_v",    "1",      "1",   "v",    "YES",  "name",
        "1",        "1",   "name", "YES",    "name_2", "1",   "1",    "name", "YES",
    };
    static const char *const show_create_rows[] = {
        "add_idx",
        "CREATE TABLE `add_idx` (\n"
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
        "  KEY `k_v` (`v`),\n"
        "  KEY `k_u` (`u`),\n"
        "  KEY `name` (`name`),\n"
        "  KEY `name_2` (`name`),\n"
        "  KEY `k_c` (`c`),\n"
        "  KEY `k_amount` (`amount`),\n"
        "  KEY `k_d` (`d`),\n"
        "  KEY `k_dt` (`dt`),\n"
        "  KEY `k_ts` (`ts`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const updated_rows[] = {"1", "15"};
    static const char *const clone_index_count_rows[] = {"10"};
    static const char *const copied_index_count_rows[] = {"0"};
    static const char *const renamed_index_count_rows[] = {"10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open add index file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE add_idx ("
        "id INT PRIMARY KEY, v INT, u BIGINT UNSIGNED, name VARCHAR(10), c CHAR(3), "
        "amount DECIMAL(5,2), d DATE, dt DATETIME, ts TIMESTAMP NULL, txt TEXT)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO add_idx VALUES "
        "(1,10,100,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03',"
        "'2024-01-01 01:02:03','hello'),"
        "(2,NULL,200,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL,'body')",
        2
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_v (v)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD KEY k_u (u)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD KEY (name)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX (name)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_c (c)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_amount (amount)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_d (d)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_dt (dt)");
    failures += expect_alter_index_ok(database, "ALTER TABLE add_idx ADD INDEX k_ts (ts)");
    failures += expect_physical_index_count(
        database,
        added_index_physical_index_count,
        "physical indexes after ADD INDEX"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_idx",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after ADD INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_idx' AND INDEX_NAME <> 'PRIMARY' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = added_index_statistics_row_count,
            .context = "I_S STATISTICS after ADD INDEX",
        }
    );
    failures += expect_dml_ok(database, "UPDATE add_idx SET v = 15 WHERE id = 1", 1);
    failures += expect_dml_ok(database, "DELETE FROM add_idx WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM add_idx",
            .values = updated_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "DML after ADD INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE add_idx");
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT * FROM add_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone'",
            .values = clone_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE copies added indexes",
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
            .context = "CREATE TABLE SELECT omits indexes",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE add_idx TO renamed_idx");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_idx'",
            .values = renamed_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed table keeps added indexes",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after ADD INDEX"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen add index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'renamed_idx'",
            .values = renamed_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "reopened added indexes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_index_result_metadata_cache_invalidation(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "metadata_cache") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata cache file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cached_flags (id INT PRIMARY KEY, v INT, u INT)"
    );
    failures += expect_select_column_key_flags(
        database,
        "SELECT v FROM cached_flags",
        0U,
        "metadata cache before ADD INDEX"
    );
    failures += expect_select_column_key_flags(
        database,
        "SELECT v FROM cached_flags",
        0U,
        "metadata cache repeat before ADD INDEX"
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE cached_flags ADD INDEX k_v (v)");
    failures += expect_select_column_key_flags(
        database,
        "SELECT v FROM cached_flags",
        MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY | MYLITE_RESULT_COLUMN_FLAG_PART_KEY,
        "metadata cache after ADD INDEX"
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE cached_flags ADD UNIQUE KEY u_u (u)");
    failures += expect_select_column_key_flags(
        database,
        "SELECT u FROM cached_flags",
        MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY | MYLITE_RESULT_COLUMN_FLAG_PART_KEY,
        "metadata cache after ADD UNIQUE"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_index_name_generation_and_auto_increment(void) {
    static const char *const unnamed_show_create_rows[] = {
        "unnamed_idx",
        "CREATE TABLE `unnamed_idx` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `v` (`v`),\n"
        "  KEY `v_2` (`v`),\n"
        "  KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const auto_rows[] = {"1", "10", "2", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "names_ai") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open names file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE unnamed_idx (id INT, v INT)");
    failures += expect_alter_index_ok(database, "ALTER TABLE unnamed_idx ADD INDEX (v)");
    failures += expect_alter_index_ok(database, "ALTER TABLE unnamed_idx ADD KEY (v)");
    failures += expect_alter_index_ok(database, "ALTER TABLE unnamed_idx ADD INDEX (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE unnamed_idx",
            .values = unnamed_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unnamed ADD INDEX names",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE ai (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures += expect_alter_index_ok(database, "ALTER TABLE ai ADD KEY id_idx (id)");
    failures += expect_dml_ok(database, "ALTER TABLE ai DROP PRIMARY KEY", 0);
    failures += expect_dml_ok(database, "INSERT INTO ai (v) VALUES (10),(20)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM ai ORDER BY id",
            .values = auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "ADD INDEX supports later AUTO_INCREMENT primary drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_index_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "ALTER TABLE no_default ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE diag (id INT, v INT, txt TEXT)");
    failures += expect_alter_index_ok(database, "ALTER TABLE diag ADD INDEX k_v (v)");
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD INDEX k_v (id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k_v'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD INDEX k_v (missing)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k_v'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD INDEX `PRIMARY` (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD INDEX k_missing (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE diag ADD INDEX k_txt (txt)");
    failures += expect_statement_ok(database, "CREATE TABLE zero_chars (c CHAR(0), v VARCHAR(0))");
    failures += execute_error(
        database,
        "ALTER TABLE zero_chars ADD INDEX k_c (c)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'c'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE zero_chars ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'v'",
        }
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE diag ADD KEY k_multi (id, v)");
    failures += execute_error(
        database,
        "ALTER TABLE diag ADD INDEX k_qualified (diag.v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_alter_index_ok(database, "ALTER TABLE diag ADD INDEX k_v2 USING BTREE (v)");
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.diag ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ADD INDEX k_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_index_independent_handles(void) {
    static const char *const first_index_count_rows[] = {"1"};
    static const char *const second_index_count_rows[] = {"0"};
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

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT)");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT)");
    failures += expect_alter_index_ok(first, "ALTER TABLE t ADD INDEX k_v (v)");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .values = first_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle added index",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't'",
            .values = second_index_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle remains independent",
        }
    );
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
    failures += mylite_test_expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures +=
        mylite_test_expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(
        mylite_diagnostics_errmsg(diagnostics),
        expected.message_part,
        sql
    );
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_alter_index_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
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

static int expect_select_column_key_flags(
    mylite_db *database,
    const char *sql,
    uint32_t expected_key_flags,
    const char *context
) {
    enum {
        key_flags_mask = MYLITE_RESULT_COLUMN_FLAG_PRI_KEY | MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY |
                         MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY |
                         MYLITE_RESULT_COLUMN_FLAG_PART_KEY,
    };

    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        uint32_t actual_key_flags = mylite_result_column_flags(result, 0U) & key_flags_mask;

        failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, context);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, context);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);
        if (actual_key_flags != expected_key_flags) {
            fprintf(
                stderr,
                "%s: expected key flags %u, got %u\n",
                context,
                expected_key_flags,
                actual_key_flags
            );
            ++failures;
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

    return mylite_test_expect_int(actual_count, expected_count, context);
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

    return mylite_test_expect_text(actual, expected, context);
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
