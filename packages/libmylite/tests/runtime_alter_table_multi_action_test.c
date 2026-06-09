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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_multiple_primary_key = 1068,
    mysql_error_key_column_missing = 1072,
    mysql_error_wrong_auto_key = 1075,
    mysql_error_cant_drop_field_or_key = 1091,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_duplicate_key = 1062,
    mysql_error_invalid_use_of_null = 1138,
    mysql_error_unknown_column = 1054,
    mysql_error_field_no_default = 1364,
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

static int test_multi_action_success_metadata_and_persistence(void);
static int test_multi_action_drop_add_and_rollback(void);
static int test_multi_action_primary_key_metadata_and_persistence(void);
static int test_multi_action_primary_key_rollback_and_diagnostics(void);
static int test_multi_action_default_metadata_and_persistence(void);
static int test_multi_action_default_rollback_and_diagnostics(void);
static int test_multi_action_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_ddl_result(const mylite_result *result, const char *context);
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

    failures += test_multi_action_success_metadata_and_persistence();
    failures += test_multi_action_drop_add_and_rollback();
    failures += test_multi_action_primary_key_metadata_and_persistence();
    failures += test_multi_action_primary_key_rollback_and_diagnostics();
    failures += test_multi_action_default_metadata_and_persistence();
    failures += test_multi_action_default_rollback_and_diagnostics();
    failures += test_multi_action_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_multi_action_success_metadata_and_persistence(void) {
    static const char *const rows_after_add[] = {
        "1",
        "10",
        "7",
        "8",
        "2",
        "20",
        "7",
        "8",
    };
    static const char *const statistics_rows[] = {
        "k_a",
        "1",
        "a",
        "PRIMARY",
        "0",
        "id",
        "u_v",
        "0",
        "v",
    };
    static const char *const dependent_statistics_rows[] = {
        "k_c",
        "1",
        "c",
        "PRIMARY",
        "0",
        "id",
    };
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open multi-action file");
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO t VALUES (1,10),(2,20)", 2);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE t ADD COLUMN a INT DEFAULT 7, "
        "ADD COLUMN b BIGINT UNSIGNED NOT NULL DEFAULT 8"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, a, b FROM t ORDER BY id",
            .values = rows_after_add,
            .column_count = 4U,
            .row_count = 2U,
            .context = "rows after multi ADD COLUMN",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE t ADD INDEX k_a (a), ADD UNIQUE u_v (v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "statistics after multi ADD INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dep (id INT PRIMARY KEY, v INT)");
    failures +=
        expect_statement_ok(database, "ALTER TABLE dep ADD COLUMN c INT, ADD INDEX k_c (c)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'dep' "
                   "ORDER BY INDEX_NAME",
            .values = dependent_statistics_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "later multi-action sees earlier added column",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE composed (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE composed ADD COLUMN (a INT DEFAULT 1, b INT DEFAULT 2)"
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE composed ALGORITHM=INSTANT, ADD COLUMN c INT");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE composed ADD COLUMN d INT DEFAULT 4, ADD COLUMN e INT DEFAULT 5, "
        "ALGORITHM=INSTANT, LOCK=DEFAULT"
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after multi-action ALTER"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen multi-action file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, a, b FROM t ORDER BY id",
            .values = rows_after_add,
            .column_count = 4U,
            .row_count = 2U,
            .context = "reopened rows after multi-action ALTER",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "reopened statistics after multi-action ALTER",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_drop_add_and_rollback(void) {
    static const char *const statistics_rows[] = {
        "k_k",
        "1",
        "k",
        "k_v2",
        "1",
        "v",
        "PRIMARY",
        "0",
        "id",
    };
    static const char *const duplicate_rows[] = {"1", "10", "2", "10"};
    static const char *const no_marker_rows[] = {"0"};
    static const char *const retry_index_rows[] = {"k_v", "v"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rollback") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rollback db");
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t (id INT PRIMARY KEY, v INT, k INT, KEY k_v (v), KEY k_k (k))"
    );
    failures += expect_statement_ok(database, "ALTER TABLE t DROP INDEX k_v, ADD INDEX k_v2 (v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "statistics after multi DROP/ADD INDEX",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE dup (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO dup VALUES (1,10),(2,10)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE dup ADD COLUMN marker INT, ADD UNIQUE u_v (v)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'dup.u_v'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'dup' "
                   "AND COLUMN_NAME = 'marker'",
            .values = no_marker_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed multi action rolls back added column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'dup'",
            .values = no_marker_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed multi action rolls back added index",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM dup ORDER BY id",
            .values = duplicate_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "failed multi action preserves rows",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE idx_rollback (id INT, v INT, dup INT)");
    failures += expect_dml_ok(database, "INSERT INTO idx_rollback VALUES (1,10,7),(2,20,7)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE idx_rollback ADD INDEX k_v (v), ADD UNIQUE u_dup (dup)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '7' for key 'idx_rollback.u_dup'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'idx_rollback'",
            .values = no_marker_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed later action rolls back earlier physical index",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE idx_rollback ADD INDEX k_v (v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'idx_rollback'",
            .values = retry_index_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "index name is reusable after failed multi-action rollback",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_primary_key_metadata_and_persistence(void) {
    static const char *const add_pk_show_create_rows[] = {
        "add_pk",
        "CREATE TABLE `add_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const add_pk_statistics_rows[] = {
        "k_v",
        "1",
        "v",
        "PRIMARY",
        "0",
        "id",
    };
    static const char *const swap_pk_show_create_rows[] = {
        "swap_pk",
        "CREATE TABLE `swap_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int NOT NULL,\n"
        "  PRIMARY KEY (`v`),\n"
        "  KEY `k_id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const add_then_pk_show_create_rows[] = {
        "add_then_pk",
        "CREATE TABLE `add_then_pk` (\n"
        "  `v` int DEFAULT NULL,\n"
        "  `id` int NOT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ai_later_key_show_create_rows[] = {
        "ai_later_key",
        "CREATE TABLE `ai_later_key` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `k_id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ai_earlier_key_show_create_rows[] = {
        "ai_earlier_key",
        "CREATE TABLE `ai_earlier_key` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `k_id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "primary_key_success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open pk success db");
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE add_pk (id INT NOT NULL, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO add_pk VALUES (1,10),(2,20)", 2);
    failures +=
        expect_statement_ok(database, "ALTER TABLE add_pk ADD PRIMARY KEY(id), ADD KEY k_v(v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_pk",
            .values = add_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "add primary key multi-action show create",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_pk' "
                   "ORDER BY INDEX_NAME",
            .values = add_pk_statistics_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "add primary key multi-action statistics",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO add_pk VALUES (1, 30)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'add_pk.PRIMARY'",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE swap_pk (id INT PRIMARY KEY, v INT NOT NULL, KEY k_id(id))"
    );
    failures += expect_dml_ok(database, "INSERT INTO swap_pk VALUES (1,10),(2,20)", 2);
    failures +=
        expect_statement_ok(database, "ALTER TABLE swap_pk DROP PRIMARY KEY, ADD PRIMARY KEY(v)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE swap_pk",
            .values = swap_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "swap primary key multi-action show create",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE add_then_pk (v INT)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_then_pk ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_then_pk",
            .values = add_then_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "add column then primary key show create",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_later_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE ai_later_key DROP PRIMARY KEY, ADD KEY k_id(id)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_earlier_key (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE ai_earlier_key ADD KEY k_id(id), DROP PRIMARY KEY"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ai_later_key",
            .values = ai_later_key_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "auto increment drop primary key with later replacement key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ai_earlier_key",
            .values = ai_earlier_key_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "auto increment drop primary key with earlier replacement key",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after primary key multi-action ALTER"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen pk success db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_pk",
            .values = add_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened add primary key show create",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME "
                   "FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_pk' "
                   "ORDER BY INDEX_NAME",
            .values = add_pk_statistics_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "reopened add primary key statistics",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_primary_key_rollback_and_diagnostics(void) {
    static const char *const zero_rows[] = {"0"};
    static const char *const primary_id_rows[] = {"PRIMARY", "id"};
    static const char *const add_dup_columns[] = {"v", "", "YES", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "primary_key_rollback") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open pk rollback db");
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE dup_pk (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO dup_pk VALUES (1,10),(1,20)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE dup_pk ADD KEY k_v(v), ADD PRIMARY KEY(id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'dup_pk.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'dup_pk'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "duplicate primary key rolls back earlier index",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE swap_dup (id INT PRIMARY KEY, v INT NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO swap_dup VALUES (1,10),(2,10)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE swap_dup DROP PRIMARY KEY, ADD PRIMARY KEY(v)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'swap_dup.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'swap_dup' "
                   "ORDER BY INDEX_NAME",
            .values = primary_id_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "failed primary key swap preserves original key",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE null_pk (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO null_pk VALUES (NULL,10),(2,20)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE null_pk ADD KEY k_v(v), ADD PRIMARY KEY(id)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "22004",
            .message_part = "Invalid use of NULL value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'null_pk'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "null primary key rolls back earlier index",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE existing_pk (id INT PRIMARY KEY, v INT)");
    failures += execute_error(
        database,
        "ALTER TABLE existing_pk ADD PRIMARY KEY(v), ADD KEY k_v(v)",
        (struct expected_sql_error){
            .code = mysql_error_multiple_primary_key,
            .sqlstate = "42000",
            .message_part = "Multiple primary key defined",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'existing_pk' "
                   "ORDER BY INDEX_NAME",
            .values = primary_id_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "existing primary key rolls back later index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE no_pk (id INT, v INT)");
    failures += execute_error(
        database,
        "ALTER TABLE no_pk DROP PRIMARY KEY, ADD KEY k_id(id)",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_field_or_key,
            .sqlstate = "42000",
            .message_part = "Can't DROP 'PRIMARY'; check that column/key exists",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'no_pk'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing primary key rolls back later index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE add_dup (v INT)");
    failures += expect_dml_ok(database, "INSERT INTO add_dup VALUES (1),(2)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE add_dup ADD COLUMN id INT NOT NULL, ADD PRIMARY KEY(id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '0' for key 'add_dup.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_dup' "
                   "ORDER BY ORDINAL_POSITION",
            .values = add_dup_columns,
            .column_count = 4U,
            .row_count = 1U,
            .context = "failed add column primary key rolls back column",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_bad (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_bad DROP PRIMARY KEY, ADD KEY k_v(v)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part =
                "Incorrect table definition; there can be only one auto column and it must be "
                "defined as a key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ai_bad' "
                   "ORDER BY INDEX_NAME",
            .values = primary_id_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "failed auto increment final validation preserves primary key",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE hash_pk (id INT NOT NULL, v INT)");
    failures += execute_error(
        database,
        "ALTER TABLE hash_pk ADD PRIMARY KEY USING HASH(id), ADD KEY k_v(v)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not yet support warning-producing "
                            "ADD PRIMARY KEY",
        }
    );
    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE tmp_pk (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE tmp_pk ADD PRIMARY KEY(id), ADD KEY k_id(id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE supports only persistent base tables",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE unknown_col (id INT, v INT)");
    failures += execute_error(
        database,
        "ALTER TABLE unknown_col ADD PRIMARY KEY(missing), ADD KEY k_v(v)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_default_metadata_and_persistence(void) {
    static const char *const default_rows[] = {
        "a",
        "9",
        "b",
        NULL,
        "c",
        NULL,
    };
    static const char *const inserted_rows[] = {"1", "9", "5", "6"};
    static const char *const mixed_rows[] = {"a", "3", "c", "4"};
    static const char *const mixed_show_create_rows[] = {
        "mixed",
        "CREATE TABLE `mixed` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT '3',\n"
        "  `c` int DEFAULT '4',\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "defaults") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open defaults db");
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE defaults_t (id INT PRIMARY KEY, a INT DEFAULT 1, b INT DEFAULT 2, c INT)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE defaults_t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'defaults_t' "
                   "AND COLUMN_NAME IN ('a', 'b', 'c') "
                   "ORDER BY ORDINAL_POSITION",
            .values = default_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "default metadata after multi default alter",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO defaults_t (id, c) VALUES (1, 4)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'b' doesn't have a default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE defaults_t ALTER b SET DEFAULT 5, ALTER c SET DEFAULT 6"
    );
    failures += expect_dml_ok(database, "INSERT INTO defaults_t (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, c FROM defaults_t",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "insert after multi default alter",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE mixed (id INT PRIMARY KEY, a INT)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE mixed ADD COLUMN c INT DEFAULT 4, ALTER a SET DEFAULT 3"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'mixed' "
                   "AND COLUMN_NAME IN ('a', 'c') "
                   "ORDER BY COLUMN_NAME",
            .values = mixed_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "mixed add column and old-column default alter",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE mixed",
            .values = mixed_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "mixed multi default SHOW CREATE TABLE",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after multi default ALTER"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen defaults db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, c FROM defaults_t",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened multi default rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'mixed' "
                   "AND COLUMN_NAME IN ('a', 'c') "
                   "ORDER BY COLUMN_NAME",
            .values = mixed_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "reopened mixed default metadata",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_default_rollback_and_diagnostics(void) {
    static const char *const rollback_rows[] = {"a", "1", "b", "2"};
    static const char *const zero_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *mode_result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "default_rollback") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open default rollback db");
    failures += execute_error(
        database,
        "ALTER TABLE defaults_t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.defaults_t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_t ALTER a SET DEFAULT 9, ALTER b DROP DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_t' doesn't exist",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE defaults_t (id INT PRIMARY KEY, a INT DEFAULT 1, b INT DEFAULT 2, v INT)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE defaults_t ALTER a SET DEFAULT 9, ALTER missing SET DEFAULT 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'defaults_t'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'defaults_t' "
                   "AND COLUMN_NAME IN ('a', 'b') "
                   "ORDER BY COLUMN_NAME",
            .values = rollback_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "failed multi default rolls back earlier default",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE defaults_t ADD INDEX k_v (v), ALTER missing SET DEFAULT 2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'defaults_t'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'defaults_t' "
                   "AND INDEX_NAME = 'k_v'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed multi default rolls back physical index",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE defaults_t ADD COLUMN c INT, ALTER c SET DEFAULT 7",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'c' in 'defaults_t'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'defaults_t' "
                   "AND COLUMN_NAME = 'c'",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed same-statement added default target rolls back column",
        }
    );
    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE tmp (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE tmp ALTER id SET DEFAULT 1, ALTER id DROP DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE supports only persistent base tables",
        }
    );
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE zero_temporal (id INT, d DATETIME DEFAULT '0000-00-00 00:00:00', a INT)"
    );
    failures += execute_ok(database, "SET sql_mode = 'NO_ZERO_DATE'", &mode_result);
    failures +=
        expect_size(mylite_result_warning_count(mode_result), 1U, "NO_ZERO_DATE mode warning");
    mylite_result_free(mode_result);
    mode_result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE zero_temporal ALTER a SET DEFAULT 1, ALTER id SET DEFAULT 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not yet support warning-producing "
                            "SET DEFAULT",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_action_diagnostics(void) {
    static const char *const no_column_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *mode_result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics db");
    failures += execute_error(
        database,
        "ALTER TABLE t ADD COLUMN a INT, ADD COLUMN b INT",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_dml_ok(database, "CREATE DATABASE app", 1);
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t ADD COLUMN a INT, ADD COLUMN b INT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table ADD COLUMN a INT, ADD COLUMN b INT",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += expect_statement_ok(database, "CREATE TEMPORARY TABLE tmp (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE tmp ADD INDEX k_id (id), ADD INDEX k_id2 (id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE supports only persistent base tables",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ADD COLUMN a INT, ADD INDEX k_missing (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND COLUMN_NAME = 'a'",
            .values = no_column_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "unknown key column rolls back earlier ADD COLUMN",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE t ADD INDEX k_id_hash (id) USING HASH, ADD COLUMN b INT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not yet support warning-producing "
                            "ADD INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE ft (body TEXT)");
    failures += execute_error(
        database,
        "ALTER TABLE ft ADD FULLTEXT KEY ft_body (body), ADD COLUMN a INT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "multi-action ALTER TABLE does not yet support warning-producing ADD INDEX",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE sp (p POINT NOT NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE sp ADD INDEX sp_p (p), ADD COLUMN a INT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not yet support warning-producing "
                            "ADD INDEX",
        }
    );
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE z (id INT, d DATETIME DEFAULT '0000-00-00 00:00:00')"
    );
    failures += execute_ok(database, "SET sql_mode = 'NO_ZERO_DATE'", &mode_result);
    failures +=
        expect_size(mylite_result_warning_count(mode_result), 1U, "NO_ZERO_DATE mode warning");
    mylite_result_free(mode_result);
    mode_result = NULL;
    failures += execute_error(
        database,
        "ALTER TABLE z ADD COLUMN e INT, ADD COLUMN f INT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not yet support warning-producing "
                            "ADD COLUMN",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE t ADD COLUMN b INT, ADD COLUMN c INT, ALGORITHM=INPLACE"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got %d/%s/%s\n",
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s]\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_ddl_result(result, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_ddl_result(const mylite_result *result, const char *context) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
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

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (mylite_result_column_count(result) == query.column_count &&
        mylite_result_row_count(result) == query.row_count) {
        size_t row = 0U;

        for (row = 0U; row < query.row_count; ++row) {
            size_t column = 0U;

            for (column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[value_index],
                    query.context
                );
            }
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
        "/tmp/mylite_alter_table_multi_action_%d_%s.mylite",
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
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
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
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
