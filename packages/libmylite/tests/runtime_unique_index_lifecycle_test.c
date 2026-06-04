#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    statistics_field_count = 18,
    unique_table_column_row_count = 5,
    unique_table_index_row_count = 6,
    unique_table_physical_index_count = 6,
    composite_unique_table_column_row_count = 5,
    composite_unique_table_index_row_count = 6,
    composite_unique_metadata_part_row_count = 6,
    composite_unique_constraint_row_count = 3,
    composite_unique_initial_insert_count = 7,
    composite_unique_nullable_row_count = 8,
    mysql_error_parse = 1064,
    mysql_error_duplicate_key = 1062,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_missing = 1072,
    mysql_error_incorrect_prefix_key = 1089,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_unique_index_metadata_dml_and_persistence(void);
static int test_composite_unique_index_metadata_dml_and_persistence(void);
static int test_unique_index_diagnostics(void);
static int test_unique_index_independent_handles(void);
static int create_unique_index_schema(mylite_db *database);
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

    failures += test_unique_index_metadata_dml_and_persistence();
    failures += test_composite_unique_index_metadata_dml_and_persistence();
    failures += test_unique_index_diagnostics();
    failures += test_unique_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_unique_index_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",     "int",          "NO",  "PRI", NULL, "", "v", "int",  "YES", "UNI", NULL, "",
        "amount", "decimal(5,2)", "YES", "UNI", NULL, "", "d", "date", "YES", "UNI", NULL, "",
        "n",      "int",          "NO",  "UNI", NULL, "",
    };
    static const char *const show_index_rows[] = {
        "unique_t", "0", "PRIMARY", "1",   "id",  "A",        "0", NULL,       NULL,  "",
        "BTREE",    "",  "",        "YES", NULL,  "unique_t", "0", "u_n",      "1",   "n",
        "A",        "0", NULL,      NULL,  "",    "BTREE",    "",  "",         "YES", NULL,
        "unique_t", "0", "u_v",     "1",   "v",   "A",        "0", NULL,       NULL,  "YES",
        "BTREE",    "",  "",        "YES", NULL,  "unique_t", "0", "u_amount", "1",   "amount",
        "A",        "0", NULL,      NULL,  "YES", "BTREE",    "",  "",         "YES", NULL,
        "unique_t", "0", "u_date",  "1",   "d",   "A",        "0", NULL,       NULL,  "YES",
        "BTREE",    "",  "",        "YES", NULL,  "unique_t", "1", "k_v",      "1",   "v",
        "A",        "0", NULL,      NULL,  "YES", "BTREE",    "",  "",         "YES", NULL,
    };
    static const char *const statistics_rows[] = {
        "def", "app", "unique_t", "1",   "app",   "k_v",      "1", "v",      "A",
        "0",   NULL,  NULL,       "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "unique_t", "0",   "app",   "PRIMARY",  "1", "id",     "A",
        "0",   NULL,  NULL,       "",    "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "unique_t", "0",   "app",   "u_amount", "1", "amount", "A",
        "0",   NULL,  NULL,       "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "unique_t", "0",   "app",   "u_date",   "1", "d",      "A",
        "0",   NULL,  NULL,       "YES", "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "unique_t", "0",   "app",   "u_n",      "1", "n",      "A",
        "0",   NULL,  NULL,       "",    "BTREE", "",         "",  "YES",    NULL,
        "def", "app", "unique_t", "0",   "app",   "u_v",      "1", "v",      "A",
        "0",   NULL,  NULL,       "YES", "BTREE", "",         "",  "YES",    NULL,
    };
    static const char *const show_create_rows[] = {
        "unique_t",
        "CREATE TABLE `unique_t` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  `amount` decimal(5,2) DEFAULT NULL,\n"
        "  `d` date DEFAULT NULL,\n"
        "  `n` int NOT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `u_n` (`n`),\n"
        "  UNIQUE KEY `u_v` (`v`),\n"
        "  UNIQUE KEY `u_amount` (`amount`),\n"
        "  UNIQUE KEY `u_date` (`d`),\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const inline_show_columns_rows[] = {
        "v",
        "int",
        "YES",
        "UNI",
        NULL,
        "",
        "n",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const inline_show_create_rows[] = {
        "inline_unique",
        "CREATE TABLE `inline_unique` (\n"
        "  `v` int DEFAULT NULL,\n"
        "  `n` int NOT NULL,\n"
        "  UNIQUE KEY `n` (`n`),\n"
        "  UNIQUE KEY `v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const unnamed_show_create_rows[] = {
        "unnamed_unique",
        "CREATE TABLE `unnamed_unique` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  UNIQUE KEY `v` (`v`),\n"
        "  UNIQUE KEY `v_2` (`v`),\n"
        "  UNIQUE KEY `id` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const nullable_unique_rows[] = {
        "1",
        "10",
        "3",
        "20",
        "4",
        NULL,
        "5",
        NULL,
    };
    static const char *const update_null_rows[] = {
        "1",
        "10",
        "2",
        NULL,
        "3",
        NULL,
        "4",
        NULL,
    };
    static const char *const internal_duplicate_rows[] = {
        "1",
        "10",
        "2",
        "20",
    };
    static const char *const clone_show_index_rows[] = {
        "clone", "0", "PRIMARY", "1",   "id",  "A",     "0", NULL,       NULL,  "",
        "BTREE", "",  "",        "YES", NULL,  "clone", "0", "u_n",      "1",   "n",
        "A",     "0", NULL,      NULL,  "",    "BTREE", "",  "",         "YES", NULL,
        "clone", "0", "u_v",     "1",   "v",   "A",     "0", NULL,       NULL,  "YES",
        "BTREE", "",  "",        "YES", NULL,  "clone", "0", "u_amount", "1",   "amount",
        "A",     "0", NULL,      NULL,  "YES", "BTREE", "",  "",         "YES", NULL,
        "clone", "0", "u_date",  "1",   "d",   "A",     "0", NULL,       NULL,  "YES",
        "BTREE", "",  "",        "YES", NULL,  "clone", "1", "k_v",      "1",   "v",
        "A",     "0", NULL,      NULL,  "YES", "BTREE", "",  "",         "YES", NULL,
    };
    static const char *const empty_show_index_rows[] = {0};
    static const char *const persisted_rows[] = {"1", "31", "2", NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unique index file");
    failures += create_unique_index_schema(database);
    failures += expect_physical_index_count(
        database,
        unique_table_physical_index_count,
        "unique index physical SQLite objects"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM unique_t",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = unique_table_column_row_count,
            .context = "unique index SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM unique_t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = unique_table_index_row_count,
            .context = "unique index SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE unique_t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unique index SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'unique_t' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = statistics_field_count,
            .row_count = unique_table_index_row_count,
            .context = "unique index INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE inline_unique (v INT UNIQUE, n INT NOT NULL UNIQUE KEY)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM inline_unique",
            .values = inline_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "inline unique SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE inline_unique",
            .values = inline_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "inline unique SHOW CREATE TABLE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE unnamed_unique (id INT, v INT, UNIQUE (v), UNIQUE KEY (v), UNIQUE (id))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE unnamed_unique",
            .values = unnamed_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "unnamed unique index names",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO unique_t VALUES "
        "(1, 10, 12.30, '2024-01-02', 11),"
        "(2, NULL, NULL, NULL, 12)",
        2
    );
    failures += execute_error(
        database,
        "INSERT INTO unique_t VALUES (3, 10, 13.40, '2024-01-03', 13)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'unique_t.u_v'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE nullable_unique (id INT, v INT, UNIQUE KEY u_v (v))"
    );
    failures += expect_dml_ok(database, "INSERT INTO nullable_unique VALUES (1,10)", 1);
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO nullable_unique VALUES (2,10),(3,20),(4,NULL),(5,NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM nullable_unique ORDER BY id",
            .values = nullable_unique_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "unique index permits duplicate NULL and INSERT IGNORE skips duplicates",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE update_unique (id INT, v INT, UNIQUE KEY u_v (v))"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO update_unique VALUES (1,10),(2,20),(3,NULL),(4,NULL)",
        4
    );
    failures += expect_dml_ok(database, "UPDATE update_unique SET v = NULL WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM update_unique ORDER BY id",
            .values = update_null_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "unique UPDATE permits duplicate NULL",
        }
    );
    failures += execute_error(
        database,
        "UPDATE update_unique SET v = 10 WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 'update_unique.u_v'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE update_internal_duplicate (id INT, v INT, UNIQUE KEY u_v (v))"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO update_internal_duplicate VALUES (1,10),(2,20)", 2);
    failures += execute_error(
        database,
        "UPDATE update_internal_duplicate SET v = 99",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '99' for key 'update_internal_duplicate.u_v'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM update_internal_duplicate ORDER BY id",
            .values = internal_duplicate_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "failed multi-row unique UPDATE leaves rows unchanged",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE unique_t");
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT id, v FROM unique_t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM clone",
            .values = clone_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = unique_table_index_row_count,
            .context = "CREATE TABLE LIKE clones unique indexes",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM copied",
            .values = empty_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "CREATE TABLE SELECT omits unique indexes",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE unique_t TO renamed_unique");
    failures += expect_dml_ok(database, "UPDATE renamed_unique SET v = 31 WHERE id = 1", 1);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after unique index lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen unique index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM renamed_unique ORDER BY id LIMIT 2",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "unique index updates persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_composite_unique_index_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "a",  "int", "YES", "MUL", NULL,  "",    "b",  "int", "YES", "MUL",
        NULL, "",    "c",   "int", "YES", "",    NULL, "",    "d",   "int",
        "NO", "PRI", NULL,  "",    "e",   "int", "NO", "PRI", NULL,  "",
    };
    static const char *const statistics_rows[] = {
        "u_ab", "0", "1", "a", "YES", "u_ab", "0", "2", "b", "YES", "u_ba", "0", "1", "b", "YES",
        "u_ba", "0", "2", "a", "YES", "u_de", "0", "1", "d", "",    "u_de", "0", "2", "e", "",
    };
    static const char *const show_index_rows[] = {
        "cu", "0", "u_de", "1", "d", "A", "0", NULL, NULL, "",    "BTREE", "", "", "YES", NULL,
        "cu", "0", "u_de", "2", "e", "A", "0", NULL, NULL, "",    "BTREE", "", "", "YES", NULL,
        "cu", "0", "u_ab", "1", "a", "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "cu", "0", "u_ab", "2", "b", "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "cu", "0", "u_ba", "1", "b", "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "cu", "0", "u_ba", "2", "a", "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
    };
    static const char *const table_constraints_rows[] = {
        "u_ab",
        "UNIQUE",
        "YES",
        "u_ba",
        "UNIQUE",
        "YES",
        "u_de",
        "UNIQUE",
        "YES",
    };
    static const char *const key_usage_rows[] = {
        "u_ab",
        "a",
        "1",
        "u_ab",
        "b",
        "2",
        "u_ba",
        "b",
        "1",
        "u_ba",
        "a",
        "2",
        "u_de",
        "d",
        "1",
        "u_de",
        "e",
        "2",
    };
    static const char *const show_create_rows[] = {
        "cu",
        "CREATE TABLE `cu` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `c` int DEFAULT NULL,\n"
        "  `d` int NOT NULL,\n"
        "  `e` int NOT NULL,\n"
        "  UNIQUE KEY `u_de` (`d`,`e`),\n"
        "  UNIQUE KEY `u_ab` (`a`,`b`),\n"
        "  UNIQUE KEY `u_ba` (`b`,`a`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const nullable_rows[] = {
        "1",  "2", "10", "1",  NULL, "11", "1",  NULL, "12", NULL, "2", "13",
        NULL, "2", "14", NULL, NULL, "15", NULL, NULL, "16", "3",  "4", "17",
    };
    static const char *const update_null_rows[] = {
        "1",
        "2",
        "10",
        NULL,
        "2",
        "13",
        NULL,
        "4",
        "17",
    };
    static const char *const string_count_rows[] = {"2"};
    static const char *const clone_count_rows[] = {"6"};
    static const char *const persisted_rows[] = {"1", "2", "10", NULL, "4", "17"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "composite") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open composite unique file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cu ("
        "a INT, b INT, c INT, d INT NOT NULL, e INT NOT NULL, "
        "UNIQUE KEY u_ab (a,b), UNIQUE KEY u_ba (b,a), UNIQUE KEY u_de (d,e))"
    );
    failures += expect_physical_index_count(database, 3, "composite unique physical indexes");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM cu",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = composite_unique_table_column_row_count,
            .context = "composite unique SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM cu",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = composite_unique_table_index_row_count,
            .context = "composite unique SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE cu",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite unique SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cu' ORDER BY INDEX_NAME",
            .values = statistics_rows,
            .column_count = composite_unique_table_column_row_count,
            .row_count = composite_unique_metadata_part_row_count,
            .context = "composite unique INFORMATION_SCHEMA.STATISTICS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cu' ORDER BY CONSTRAINT_NAME",
            .values = table_constraints_rows,
            .column_count = 3U,
            .row_count = composite_unique_constraint_row_count,
            .context = "composite unique INFORMATION_SCHEMA.TABLE_CONSTRAINTS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "
                   "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'cu' ORDER BY CONSTRAINT_NAME",
            .values = key_usage_rows,
            .column_count = 3U,
            .row_count = composite_unique_metadata_part_row_count,
            .context = "composite unique INFORMATION_SCHEMA.KEY_COLUMN_USAGE",
        }
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO cu VALUES "
        "(1,2,10,100,200),(1,NULL,11,101,201),(1,NULL,12,102,202),"
        "(NULL,2,13,103,203),(NULL,2,14,104,204),(NULL,NULL,15,105,205),"
        "(NULL,NULL,16,106,206)",
        composite_unique_initial_insert_count
    );
    failures += execute_error(
        database,
        "INSERT INTO cu VALUES (1,2,20,107,207)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'cu.u_ab'",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO cu VALUES (1,2,20,107,207),(3,4,17,108,208)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM cu ORDER BY c",
            .values = nullable_rows,
            .column_count = 3U,
            .row_count = composite_unique_nullable_row_count,
            .context = "composite unique nullable tuples and INSERT IGNORE",
        }
    );
    failures += expect_dml_ok(database, "UPDATE cu SET a = NULL WHERE c IN (13,17)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM cu WHERE c IN (10,13,17) ORDER BY c",
            .values = update_null_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite unique UPDATE allows NULL key part duplicates",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE dup_update (a INT, b INT, c INT, UNIQUE KEY u_ab (a,b))"
    );
    failures += expect_dml_ok(database, "INSERT INTO dup_update VALUES (1,2,10),(3,2,20)", 2);
    failures += execute_error(
        database,
        "UPDATE dup_update SET a = 1 WHERE c = 20",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'dup_update.u_ab'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE dup_update SET a = 9",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '9-2' for key 'dup_update.u_ab'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE string_tuple (a VARCHAR(10), b CHAR(10), UNIQUE KEY u_ab (a,b))"
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO string_tuple VALUES ('abc','x'),('ABC','x'),('abc','y')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM string_tuple",
            .values = string_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "composite string unique collation",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE cu");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone'",
            .values = clone_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones composite unique indexes",
        }
    );
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after composite unique lifecycle"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen composite unique file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c FROM cu WHERE c IN (10,17) ORDER BY c",
            .values = persisted_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "composite unique rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_unique_index_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE unknown_key (id INT, UNIQUE KEY u (missing))",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE duplicate_key_name (id INT, v INT, UNIQUE KEY k (id), KEY k (v))",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE primary_key_name (id INT, UNIQUE KEY `PRIMARY` (id))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ignored_primary_key_name "
        "(id1 INT, id2 INT, PRIMARY KEY idx (id1), INDEX idx (id2))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_unique (body TEXT, UNIQUE KEY u_body (body))"
    );
    failures += expect_dml_ok(database, "INSERT INTO text_unique VALUES ('body')", 1);
    failures += execute_error(
        database,
        "INSERT INTO text_unique VALUES ('body')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry 'body' for key 'text_unique.u_body'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE varchar_zero_unique (name VARCHAR(0), UNIQUE KEY u_name (name))",
        (struct expected_sql_error){
            .code = mysql_error_storage_engine_cant_index_column,
            .sqlstate = "42000",
            .message_part = "The used storage engine can't index column 'name'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_unique (id INT, v INT, UNIQUE KEY u (id, v))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE unique_prefix (id INT, UNIQUE KEY u (id(4)))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_prefix_key,
            .sqlstate = "HY000",
            .message_part = "Incorrect prefix key",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_unique_prefix (a VARCHAR(10), b VARCHAR(10), "
        "UNIQUE KEY u (a(2), b(2)))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE key_bearing (id INT, v INT, UNIQUE KEY u_v (v))"
    );
    failures += expect_dml_ok(database, "INSERT INTO key_bearing SELECT 1, 2", 1);
    failures += expect_dml_ok(database, "REPLACE INTO key_bearing VALUES (3, 4)", 1);
    failures += expect_dml_ok(database, "REPLACE INTO key_bearing SELECT 1, 2", 1);
    failures += execute_error(
        database,
        "ALTER TABLE key_bearing ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ALTER TABLE ORDER BY does not yet support secondary-index tables",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_unique_index_independent_handles(void) {
    static const char *const first_values[] = {"10"};
    static const char *const second_values[] = {"20"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT, UNIQUE KEY u_v (v))");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT, UNIQUE KEY u_v (v))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 20)", 1);
    failures += execute_error(
        first,
        "INSERT INTO t VALUES (2, 10)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 't.u_v'",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = first_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent unique-index table state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT v FROM t WHERE id = 1",
            .values = second_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent unique-index table state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_unique_index_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE unique_t ("
        "id INT NOT NULL, v INT, amount DECIMAL(5,2), d DATE, n INT NOT NULL, "
        "PRIMARY KEY (id), UNIQUE KEY u_v (v), UNIQUE KEY u_amount (amount), "
        "UNIQUE KEY u_date (d), UNIQUE KEY u_n (n), KEY k_v (v))"
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
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
        fprintf(stderr, "expected error for [%s]\n", sql);
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

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
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
        "/tmp/mylite_unique_index_%d_%s.mylite",
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
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
            "%s: expected [%s] to contain [%s]\n",
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
        fprintf(stderr, "%s: byte range did not match\n", context);
        return 1;
    }
    return 0;
}
