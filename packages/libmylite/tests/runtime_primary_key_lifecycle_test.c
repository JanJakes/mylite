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
    show_columns_field_count = 6,
    show_index_field_count = 15,
    mysql_error_bad_null = 1048,
    mysql_error_no_database_selected = 1046,
    mysql_error_invalid_default = 1067,
    mysql_error_duplicate_column = 1060,
    mysql_error_duplicate_key = 1062,
    mysql_error_multiple_primary_key = 1068,
    mysql_error_parse = 1064,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_key_column_missing = 1072,
    mysql_error_primary_key_part_null = 1171,
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

static int test_primary_key_metadata_dml_and_persistence(void);
static int test_composite_primary_key_lifecycle(void);
static int test_primary_key_type_and_mutation_coverage(void);
static int test_primary_key_diagnostics(void);
static int test_primary_key_independent_handles(void);
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

    failures += test_primary_key_metadata_dml_and_persistence();
    failures += test_composite_primary_key_lifecycle();
    failures += test_primary_key_type_and_mutation_coverage();
    failures += test_primary_key_diagnostics();
    failures += test_primary_key_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_primary_key_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
        "n",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "inline_pk",
        "0",
        "PRIMARY",
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
    static const char *const show_create_rows[] = {
        "inline_pk",
        "CREATE TABLE `inline_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `n` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const table_pk_show_create_rows[] = {
        "table_pk",
        "CREATE TABLE `table_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const unsigned_show_columns_rows[] = {
        "u",
        "int unsigned",
        "NO",
        "PRI",
        NULL,
        "",
        "b",
        "bigint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const unsigned_show_create_rows[] = {
        "unsigned_pk",
        "CREATE TABLE `unsigned_pk` (\n"
        "  `u` int unsigned NOT NULL,\n"
        "  `b` bigint unsigned DEFAULT NULL,\n"
        "  PRIMARY KEY (`u`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const default_pk_rows[] = {"7", "10"};
    static const char *const inserted_rows[] = {"1", "10", NULL, "2", "20", "5"};
    static const char *const ignore_rows[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const like_show_create_rows[] = {
        "like_pk",
        "CREATE TABLE `like_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `n` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const like_show_index_rows[] = {
        "like_pk",
        "0",
        "PRIMARY",
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
    static const char *const ctas_show_create_rows[] = {
        "ctas_pk",
        "CREATE TABLE `ctas_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `n` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const renamed_index_rows[] = {
        "renamed_pk",
        "0",
        "PRIMARY",
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
    static const char *const reopened_rows[] = {"1", "10", "2", "20", "3", "30"};
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

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE inline_pk (id INT PRIMARY KEY, v INT, n INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM inline_pk",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "inline primary key SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM inline_pk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "inline primary key SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE inline_pk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "inline primary key SHOW CREATE TABLE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE table_pk (id INT, v INT, PRIMARY KEY (id))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE table_pk",
            .values = table_pk_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "table primary key SHOW CREATE TABLE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE unsigned_pk (u INT UNSIGNED PRIMARY KEY, b BIGINT UNSIGNED)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM unsigned_pk",
            .values = unsigned_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "unsigned primary key SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE unsigned_pk",
            .values = unsigned_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unsigned primary key SHOW CREATE TABLE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE default_pk (id INT DEFAULT 7 PRIMARY KEY, v INT)"
    );
    failures += expect_dml_ok(database, "INSERT INTO default_pk (v) VALUES (10)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM default_pk ORDER BY id",
            .values = default_pk_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "primary key integer default",
        }
    );

    failures +=
        expect_dml_ok(database, "INSERT INTO inline_pk VALUES (1, 10, NULL), (2, 20, 5)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, n FROM inline_pk ORDER BY id",
            .values = inserted_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "primary key inserted rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO inline_pk VALUES (1, 99, 9)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'inline_pk.PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE inline_pk SET id = 1 WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'inline_pk.PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO inline_pk VALUES (NULL, 99, 9)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'id' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE inline_pk SET id = NULL WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'id' cannot be null",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO inline_pk VALUES (1, 11, 1), (3, 30, 3)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM inline_pk ORDER BY id",
            .values = ignore_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "insert ignore duplicate primary key",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE like_pk LIKE inline_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE like_pk",
            .values = like_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE primary key SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM like_pk",
            .values = like_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE primary key SHOW INDEX",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO like_pk VALUES (1, 1, NULL)", 1);
    failures += execute_error(
        database,
        "INSERT INTO like_pk VALUES (1, 2, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'like_pk.PRIMARY'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE ctas_pk AS SELECT * FROM inline_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ctas_pk",
            .values = ctas_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE SELECT does not copy primary key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM ctas_pk",
            .values = NULL,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "CREATE TABLE SELECT primary key SHOW INDEX",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO ctas_pk VALUES (1, 90, 9)", 1);

    failures += expect_statement_ok(database, "RENAME TABLE inline_pk TO renamed_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM renamed_pk",
            .values = renamed_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "renamed primary key SHOW INDEX",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO renamed_pk VALUES (1, 99, 9)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'renamed_pk.PRIMARY'",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "primary key preserves preamble"
    );
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen metadata file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_pk ORDER BY id",
            .values = reopened_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "primary key persists after reopen",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_pk");
    failures += execute_error(
        database,
        "SHOW INDEX FROM renamed_pk",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_pk' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_composite_primary_key_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "a",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "b",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "v",
        "int",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "cpk", "0", "PRIMARY", "1", "a", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
        "cpk", "0", "PRIMARY", "2", "b", "A", "0", NULL, NULL, "", "BTREE", "", "", "YES", NULL,
    };
    static const char *const show_create_rows[] = {
        "cpk",
        "CREATE TABLE `cpk` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_column_rows[] = {
        "a",
        "PRI",
        "NO",
        "b",
        "PRI",
        "NO",
        "v",
        "",
        "YES",
    };
    static const char *const key_usage_rows[] = {
        "PRIMARY",
        "a",
        "1",
        "PRIMARY",
        "b",
        "2",
    };
    static const char *const statistics_rows[] = {
        "PRIMARY",
        "1",
        "a",
        "PRIMARY",
        "2",
        "b",
    };
    static const char *const rows_after_insert[] = {"1", "2", "10", "1", "3", "30"};
    static const char *const rows_after_update[] = {"1", "2", "10", "2", "4", "30"};
    static const char *const rows_after_ignore[] = {
        "1",
        "2",
        "10",
        "2",
        "2",
        "22",
        "2",
        "4",
        "30",
    };
    static const char *const default_rows[] = {"7", "8", "10"};
    static const char *const like_show_create_rows[] = {
        "cpk_like",
        "CREATE TABLE `cpk_like` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const like_show_index_rows[] = {
        "cpk_like", "0", "PRIMARY", "1",   "a",  "A",        "0", NULL,      NULL,  "",
        "BTREE",    "",  "",        "YES", NULL, "cpk_like", "0", "PRIMARY", "2",   "b",
        "A",        "0", NULL,      NULL,  "",   "BTREE",    "",  "",        "YES", NULL,
    };
    static const char *const ctas_show_create_rows[] = {
        "cpk_ctas",
        "CREATE TABLE `cpk_ctas` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const renamed_index_rows[] = {
        "cpk_renamed", "0", "PRIMARY", "1",   "a",  "A",           "0", NULL,      NULL,  "",
        "BTREE",       "",  "",        "YES", NULL, "cpk_renamed", "0", "PRIMARY", "2",   "b",
        "A",           "0", NULL,      NULL,  "",   "BTREE",       "",  "",        "YES", NULL,
    };
    static const char *const reopened_rows[] = {
        "1",
        "2",
        "10",
        "2",
        "2",
        "22",
        "2",
        "4",
        "30",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "composite") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open composite file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE cpk (a INT, b INT, v INT, PRIMARY KEY (a,b))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM cpk",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "composite primary key SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cpk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "composite primary key SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cpk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite primary key SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_KEY, IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'cpk' ORDER BY ORDINAL_POSITION",
            .values = information_schema_column_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite primary key INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'cpk' ORDER BY ORDINAL_POSITION",
            .values = key_usage_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite primary key KEY_COLUMN_USAGE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql =
                "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
                "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'cpk' ORDER BY SEQ_IN_INDEX",
            .values = statistics_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite primary key STATISTICS",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO cpk VALUES (1, 2, 10), (1, 3, 30)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk ORDER BY v",
            .values = rows_after_insert,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite primary key inserted rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO cpk VALUES (1, 2, 99)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'cpk.PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE cpk SET b = 2 WHERE b = 3",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'cpk.PRIMARY'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpk_update_diag (a INT, b INT, v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO cpk_update_diag VALUES (1, 2, 10), (2, 2, 20), (2, 3, 30)",
        3
    );
    failures += execute_error(
        database,
        "UPDATE cpk_update_diag SET b = 2 WHERE b >= 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2-2' for key 'cpk_update_diag.PRIMARY'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpk_update_limit_diag (a INT, b INT, v INT, PRIMARY KEY (a,b))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO cpk_update_limit_diag VALUES "
        "(1, 2, 10), (1, 3, 30), (2, 2, 20), (2, 3, 40)",
        4
    );
    failures += execute_error(
        database,
        "UPDATE cpk_update_limit_diag SET b = 2 WHERE b >= 2 ORDER BY v DESC LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2-2' for key 'cpk_update_limit_diag.PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO cpk VALUES (NULL, 5, 50)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'a' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE cpk SET b = NULL WHERE a = 1 AND b = 2",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'b' cannot be null",
        }
    );
    failures += expect_dml_ok(database, "UPDATE cpk SET b = 4 WHERE a = 1 AND b = 3", 1);
    failures += expect_dml_ok(database, "UPDATE cpk SET a = 2 WHERE a = 1 AND b = 4", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk ORDER BY v",
            .values = rows_after_update,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite primary key updated rows",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO cpk VALUES (1, 2, 11), (2, 2, 22)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk ORDER BY v",
            .values = rows_after_ignore,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite primary key INSERT IGNORE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpk_defaults (a INT DEFAULT 7, b INT DEFAULT 8, v INT, PRIMARY KEY(a,b))"
    );
    failures += expect_dml_ok(database, "INSERT INTO cpk_defaults (v) VALUES (10)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk_defaults",
            .values = default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "composite primary key defaults",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpk_types (i INT, j INTEGER, b BIGINT, u INT UNSIGNED, "
        "ui INTEGER UNSIGNED, ub BIGINT UNSIGNED, PRIMARY KEY(i,j,b,u,ui,ub))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO cpk_types VALUES (-2147483648, 2147483647, 9223372036854775807, "
        "4294967295, 4294967295, 9223372036854775807)",
        1
    );
    failures += execute_error(
        database,
        "INSERT INTO cpk_types VALUES (-2147483648, 2147483647, 9223372036854775807, "
        "4294967295, 4294967295, 9223372036854775807)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part =
                "Duplicate entry '-2147483648-2147483647-9223372036854775807-4294967295-",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE cpk_like LIKE cpk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cpk_like",
            .values = like_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE composite primary key SHOW CREATE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cpk_like",
            .values = like_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "CREATE TABLE LIKE composite primary key SHOW INDEX",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO cpk_like VALUES (1, 2, 10)", 1);
    failures += execute_error(
        database,
        "INSERT INTO cpk_like VALUES (1, 2, 11)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'cpk_like.PRIMARY'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE cpk_ctas AS SELECT * FROM cpk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cpk_ctas",
            .values = ctas_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE SELECT omits composite primary key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cpk_ctas",
            .values = NULL,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "CREATE TABLE SELECT composite primary key SHOW INDEX",
        }
    );

    failures += expect_statement_ok(database, "RENAME TABLE cpk TO cpk_renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cpk_renamed",
            .values = renamed_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "renamed composite primary key SHOW INDEX",
        }
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen composite file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM cpk_renamed ORDER BY v",
            .values = reopened_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite primary key persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_primary_key_type_and_mutation_coverage(void) {
    static const char *const set_rows[] = {"1", "10"};
    static const char *const delete_rows[] = {"1", "15", "2", "20"};
    static const char *const truncate_rows[] = {"1", "30"};
    static const char *const truncate_index_rows[] = {
        "delete_pk",
        "0",
        "PRIMARY",
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
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "types") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open type coverage file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE pk_int (id INT PRIMARY KEY, v INT)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE pk_integer (id INTEGER PRIMARY KEY, v INT)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE pk_bigint (id BIGINT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_int_unsigned (id INT UNSIGNED PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_integer_unsigned (id INTEGER UNSIGNED PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE pk_bigint_unsigned (id BIGINT UNSIGNED PRIMARY KEY, v INT)"
    );

    failures += expect_dml_ok(database, "INSERT INTO pk_int SET id = 1, v = 10", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_int SET id = 1, v = 11",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'pk_int.PRIMARY'",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO pk_int SET id = 1, v = 12",
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk_int",
            .values = set_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "INSERT SET duplicate primary key",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO pk_integer VALUES (-2147483648, 20)", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_integer VALUES (-2147483648, 21)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '-2147483648' for key 'pk_integer.PRIMARY'",
        }
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO pk_bigint VALUES (9223372036854775807, 30)", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_bigint VALUES (9223372036854775807, 31)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '9223372036854775807' for key 'pk_bigint.PRIMARY'",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO pk_int_unsigned VALUES (4294967295, 40)", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_int_unsigned VALUES (4294967295, 41)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '4294967295' for key 'pk_int_unsigned.PRIMARY'",
        }
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO pk_integer_unsigned VALUES (4294967295, 50)", 1);
    failures += execute_error(
        database,
        "INSERT INTO pk_integer_unsigned VALUES (4294967295, 51)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '4294967295' for key 'pk_integer_unsigned.PRIMARY'",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO pk_bigint_unsigned VALUES (9223372036854775807, 60)",
        1
    );
    failures += execute_error(
        database,
        "INSERT INTO pk_bigint_unsigned VALUES (9223372036854775807, 61)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part =
                "Duplicate entry '9223372036854775807' for key 'pk_bigint_unsigned.PRIMARY'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE delete_pk (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO delete_pk VALUES (1, 10), (2, 20)", 2);
    failures += expect_dml_ok(database, "DELETE FROM delete_pk WHERE id = 1", 1);
    failures += expect_dml_ok(database, "INSERT INTO delete_pk VALUES (1, 15)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM delete_pk ORDER BY id",
            .values = delete_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "primary key after delete",
        }
    );

    failures += expect_statement_ok(database, "TRUNCATE TABLE delete_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM delete_pk",
            .values = truncate_index_rows,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "primary key metadata after truncate",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO delete_pk VALUES (1, 30)", 1);
    failures += execute_error(
        database,
        "INSERT INTO delete_pk VALUES (1, 31)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'delete_pk.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM delete_pk",
            .values = truncate_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "primary key after truncate",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_primary_key_diagnostics(void) {
    static const char *const keyed_rows_after_drop[] = {"10"};
    static const char *const keyed_rows_after_replace[] = {"10", "11"};
    static const char *const keyed_negative_rows_after_replace_select[] = {"1", "10"};
    static const char *const no_show_index_rows[] = {NULL};
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
        "CREATE TABLE no_schema (id INT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE missing_schema.pk (id INT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE _mylite_pk (id INT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE explicit_null_pk (id INT NULL PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE explicit_null_table_pk (id INT NULL, PRIMARY KEY (id))",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE cpk_explicit_null (a INT NULL, b INT, PRIMARY KEY (a,b))",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE cpk_default_null (a INT DEFAULT NULL, b INT, PRIMARY KEY (a,b))",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "All parts of a PRIMARY KEY must be NOT NULL",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE cpk_duplicate_part (a INT, b INT, PRIMARY KEY (a,a))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE cpk_missing_part (a INT, b INT, PRIMARY KEY (a,missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE cpk_qualified_part (a INT, b INT, PRIMARY KEY (app.a,b))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '.'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpk_string_part (a INT, b VARCHAR(10), PRIMARY KEY (a,b))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_null_pk (id INT DEFAULT NULL PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'id'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_pk (id INT PRIMARY KEY, v INT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_multiple_primary_key,
            .sqlstate = "42000",
            .message_part = "Multiple primary key defined",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unknown_pk (id INT, PRIMARY KEY (missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE text_pk (v TEXT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE qualified_pk (id INT, PRIMARY KEY (qualified_pk.id))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '.'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE constraint_pk (id INT, CONSTRAINT c PRIMARY KEY (id))"
    );

    failures += expect_statement_ok(database, "CREATE TABLE keyed (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO keyed VALUES (1, 10)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE keyed ADD COLUMN extra INT PRIMARY KEY",
        (struct expected_sql_error){
            .code = mysql_error_multiple_primary_key,
            .sqlstate = "42000",
            .message_part = "Multiple primary key defined",
        }
    );
    failures += expect_dml_ok(database, "ALTER TABLE keyed DROP COLUMN id", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM keyed",
            .values = no_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "primary-key table after DROP COLUMN id SHOW INDEX",
        }
    );
    failures += expect_dml_ok(database, "ALTER TABLE keyed MODIFY COLUMN v BIGINT", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM keyed",
            .values = keyed_rows_after_drop,
            .column_count = 1U,
            .row_count = 1U,
            .context = "primary-key table after MODIFY COLUMN",
        }
    );
    failures += expect_statement_ok(database, "ALTER TABLE keyed ORDER BY v");
    failures += expect_statement_ok(database, "ALTER TABLE keyed FORCE");
    failures += expect_dml_ok(database, "REPLACE INTO keyed VALUES (11)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT v FROM keyed ORDER BY v",
            .values = keyed_rows_after_replace,
            .column_count = 1U,
            .row_count = 2U,
            .context = "primary-key table after DROP COLUMN id REPLACE",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE keyed_negative (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO keyed_negative VALUES (1, 10)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE keyed_negative ORDER BY v",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE ORDER BY does not yet support primary-key tables",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE keyed_negative FORCE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE FORCE does not yet support primary-key tables",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO keyed_negative SELECT id, v FROM keyed_negative",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'keyed_negative.PRIMARY'",
        }
    );
    failures +=
        expect_dml_ok(database, "REPLACE INTO keyed_negative SELECT id, v FROM keyed_negative", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM keyed_negative",
            .values = keyed_negative_rows_after_replace_select,
            .column_count = 2U,
            .row_count = 1U,
            .context = "primary key table REPLACE SELECT exact replacement",
        }
    );
    failures += execute_error(
        database,
        "UPDATE missing_table SET id = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_primary_key_independent_handles(void) {
    static const char *const first_expected[] = {"10"};
    static const char *const second_expected[] = {"20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 20)", 1);
    failures += execute_error(
        first,
        "INSERT INTO t VALUES (1, 11)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 't.PRIMARY'",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent primary-key state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent primary-key state",
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
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
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

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "DML affected"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "DML warnings"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return mylite_test_expect_true(actual == NULL, context);
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
