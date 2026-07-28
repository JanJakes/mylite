#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    sql_capacity = 512,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    statistics_probe_field_count = 5,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_parse = 1064,
    mysql_error_duplicate_column = 1060,
    mysql_error_duplicate_key = 1062,
    mysql_error_multiple_primary_key = 1068,
    mysql_error_key_column_missing = 1072,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_invalid_use_of_null = 1138,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_storage_engine_cant_index_column = 1167,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

struct primary_key_type_case {
    const char *table_name;
    const char *type_name;
    const char *first_value;
    const char *second_value;
};

static int test_alter_add_primary_key_success_metadata_and_persistence(void);
static int test_alter_add_composite_primary_key_lifecycle(void);
static int test_alter_add_primary_key_integer_types_and_defaults(void);
static int test_alter_add_primary_key_diagnostics(void);
static int test_alter_add_primary_key_independent_handles(void);
static int expect_type_alter_primary_key(
    mylite_db *database,
    struct primary_key_type_case test_case
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_alter_primary_key_ok(mylite_db *database, const char *sql);
static int expect_truncate_ok(mylite_db *database, const char *sql);
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
static int expect_physical_index_count(
    mylite_db *database,
    int64_t expected_count,
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

    failures += test_alter_add_primary_key_success_metadata_and_persistence();
    failures += test_alter_add_composite_primary_key_lifecycle();
    failures += test_alter_add_primary_key_integer_types_and_defaults();
    failures += test_alter_add_primary_key_diagnostics();
    failures += test_alter_add_primary_key_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alter_add_primary_key_success_metadata_and_persistence(void) {
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
        "MUL",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "add_pk", "0", "PRIMARY", "1",   "id",  "A",      "0", NULL,  NULL,  "",
        "BTREE",  "",  "",        "YES", NULL,  "add_pk", "1", "k_v", "1",   "v",
        "A",      "0", NULL,      NULL,  "YES", "BTREE",  "",  "",    "YES", NULL,
    };
    static const char *const show_create_rows[] = {
        "add_pk",
        "CREATE TABLE `add_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_columns_rows[] = {
        "id",
        "NO",
        "PRI",
        NULL,
        "v",
        "YES",
        "MUL",
        NULL,
    };
    static const char *const information_schema_constraints_rows[] =
        {"PRIMARY", "PRIMARY KEY", "YES"};
    static const char *const information_schema_key_column_usage_rows[] =
        {"PRIMARY", "id", "1", NULL};
    static const char *const information_schema_statistics_rows[] = {
        "k_v",
        "1",
        "1",
        "v",
        "YES",
        "PRIMARY",
        "0",
        "1",
        "id",
        "",
    };
    static const char *const table_rows[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const post_key_dml_rows[] = {
        "1",
        "10",
        "2",
        "20",
        "4",
        "40",
        "5",
        "30",
    };
    static const char *const post_truncate_index_rows[] = {
        "add_pk", "0", "PRIMARY", "1",   "id",  "A",      "0", NULL,  NULL,  "",
        "BTREE",  "",  "",        "YES", NULL,  "add_pk", "1", "k_v", "1",   "v",
        "A",      "0", NULL,      NULL,  "YES", "BTREE",  "",  "",    "YES", NULL,
    };
    static const char *const clone_show_create_rows[] = {
        "clone_pk",
        "CREATE TABLE `clone_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const copied_show_index_rows[] = {0};
    static const char *const renamed_show_index_rows[] = {
        "renamed_pk", "0", "PRIMARY", "1",   "id",  "A",          "0", NULL,  NULL,  "",
        "BTREE",      "",  "",        "YES", NULL,  "renamed_pk", "1", "k_v", "1",   "v",
        "A",          "0", NULL,      NULL,  "YES", "BTREE",      "",  "",    "YES", NULL,
    };
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
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE add_pk (id INT, v INT, KEY k_v (v))");
    failures += expect_dml_ok(database, "INSERT INTO add_pk VALUES (2, 20), (1, 10)", 2);
    failures += expect_alter_primary_key_ok(database, "ALTER TABLE add_pk ADD PRIMARY KEY (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_pk",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "SHOW COLUMNS after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM add_pk",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "SHOW INDEX after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_pk",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_pk' ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "I_S COLUMNS after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
                   "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_pk' ORDER BY CONSTRAINT_NAME",
            .values = information_schema_constraints_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "I_S TABLE_CONSTRAINTS after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "REFERENCED_TABLE_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_pk' "
                   "ORDER BY CONSTRAINT_NAME",
            .values = information_schema_key_column_usage_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "I_S KEY_COLUMN_USAGE after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_pk' ORDER BY INDEX_NAME",
            .values = information_schema_statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 2U,
            .context = "I_S STATISTICS after ADD PRIMARY KEY",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO add_pk (v) VALUES (30)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO add_pk VALUES (3, 30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM add_pk ORDER BY id",
            .values = table_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "rows after ADD PRIMARY KEY",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO add_pk VALUES (2, 22), (4, 40)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_dml_ok(database, "UPDATE add_pk SET id = 5 WHERE id = 3", 1);
    failures += execute_error(
        database,
        "UPDATE add_pk SET id = 4 WHERE id = 5",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '4' for key 'add_pk.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM add_pk ORDER BY id",
            .values = post_key_dml_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "DML after ADD PRIMARY KEY",
        }
    );
    failures += expect_truncate_ok(database, "TRUNCATE TABLE add_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM add_pk",
            .values = post_truncate_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "ADD PRIMARY KEY metadata after truncate",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO add_pk VALUES (1, 10)", 1);
    failures += execute_error(
        database,
        "INSERT INTO add_pk VALUES (1, 11)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'add_pk.PRIMARY'",
        }
    );
    failures += expect_physical_index_count(database, 2, "physical primary plus secondary indexes");

    failures += expect_statement_ok(database, "CREATE TABLE clone_pk LIKE add_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone_pk",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE clones added primary key",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE copied AS SELECT id, v FROM add_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM copied",
            .values = copied_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 0U,
            .context = "CREATE TABLE SELECT omits added primary key",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE add_pk TO renamed_pk");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM renamed_pk",
            .values = renamed_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "renamed table keeps added primary key",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_pk");
    failures += execute_error(
        database,
        "ALTER TABLE renamed_pk ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_pk' doesn't exist",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after ADD PRIMARY KEY"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone_pk",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened clone keeps added primary key",
        }
    );
    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_alter_add_composite_primary_key_lifecycle(void) {
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
        "MUL",
        NULL,
        "",
    };
    static const char *const show_index_rows[] = {
        "add_comp", "0",     "PRIMARY", "1",        "a",     "A",   "0",        NULL,    NULL,
        "",         "BTREE", "",        "",         "YES",   NULL,  "add_comp", "0",     "PRIMARY",
        "2",        "b",     "A",       "0",        NULL,    NULL,  "",         "BTREE", "",
        "",         "YES",   NULL,      "add_comp", "1",     "k_v", "1",        "v",     "A",
        "0",        NULL,    NULL,      "YES",      "BTREE", "",    "",         "YES",   NULL,
    };
    static const char *const show_create_rows[] = {
        "add_comp",
        "CREATE TABLE `add_comp` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`),\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_columns_rows[] = {
        "a",
        "NO",
        "PRI",
        NULL,
        "b",
        "NO",
        "PRI",
        NULL,
        "v",
        "YES",
        "MUL",
        NULL,
    };
    static const char *const information_schema_key_column_usage_rows[] = {
        "PRIMARY",
        "a",
        "1",
        NULL,
        "PRIMARY",
        "b",
        "2",
        NULL,
    };
    static const char *const information_schema_statistics_rows[] = {
        "k_v",
        "1",
        "1",
        "v",
        "YES",
        "PRIMARY",
        "0",
        "1",
        "a",
        "",
        "PRIMARY",
        "0",
        "2",
        "b",
        "",
    };
    static const char *const rows_after_dml[] = {
        "1",
        "2",
        "10",
        "2",
        "1",
        "20",
        "2",
        "2",
        "22",
        "3",
        "3",
        "30",
    };
    static const char *const default_show_create_rows[] = {
        "default_comp",
        "CREATE TABLE `default_comp` (\n"
        "  `a` int NOT NULL DEFAULT '7',\n"
        "  `b` int NOT NULL DEFAULT '8',\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const explicit_null_show_create_rows[] = {
        "explicit_null_comp",
        "CREATE TABLE `explicit_null_comp` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const qualified_show_create_rows[] = {
        "qualified_comp",
        "CREATE TABLE `qualified_comp` (\n"
        "  `a` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  PRIMARY KEY (`a`,`b`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const renamed_show_index_rows[] = {
        "renamed_comp",
        "0",
        "PRIMARY",
        "1",
        "a",
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
        "renamed_comp",
        "0",
        "PRIMARY",
        "2",
        "b",
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
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "composite_alter") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open composite alter file"
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE add_comp (a INT, b INT, v INT, KEY k_v (v))");
    failures += expect_dml_ok(database, "INSERT INTO add_comp VALUES (2,1,20),(1,2,10)", 2);
    failures += expect_alter_primary_key_ok(database, "ALTER TABLE add_comp ADD PRIMARY KEY (a,b)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_comp",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "composite ADD PRIMARY KEY SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM add_comp",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 3U,
            .context = "composite ADD PRIMARY KEY SHOW INDEX",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_comp",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite ADD PRIMARY KEY SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, IS_NULLABLE, COLUMN_KEY, COLUMN_DEFAULT "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_comp' ORDER BY ORDINAL_POSITION",
            .values = information_schema_columns_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "composite I_S COLUMNS after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
                   "REFERENCED_TABLE_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'add_comp' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_key_column_usage_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "composite I_S KEY_COLUMN_USAGE after ADD PRIMARY KEY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "
                   "FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'add_comp' ORDER BY INDEX_NAME",
            .values = information_schema_statistics_rows,
            .column_count = statistics_probe_field_count,
            .row_count = 3U,
            .context = "composite I_S STATISTICS after ADD PRIMARY KEY",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO add_comp VALUES (2, 1, 21)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2-1' for key 'add_comp.PRIMARY'",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO add_comp VALUES (2,2,22),(3,3,30)", 2);
    failures += execute_error(
        database,
        "UPDATE add_comp SET a = 2 WHERE b = 2",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2-2' for key 'add_comp.PRIMARY'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM add_comp ORDER BY v",
            .values = rows_after_dml,
            .column_count = 3U,
            .row_count = 4U,
            .context = "rows after composite ADD PRIMARY KEY DML",
        }
    );
    failures +=
        expect_physical_index_count(database, 2, "composite primary plus secondary indexes");

    failures += expect_statement_ok(
        database,
        "CREATE TABLE default_comp (a INT DEFAULT 7, b INT DEFAULT 8, v INT)"
    );
    failures += expect_dml_ok(database, "INSERT INTO default_comp (v) VALUES (10)", 1);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE default_comp ADD PRIMARY KEY (a,b)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE default_comp",
            .values = default_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite non-NULL defaults preserved",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO default_comp (v) VALUES (20)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '7-8' for key 'default_comp.PRIMARY'",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE explicit_null_comp (a INT DEFAULT NULL, b INT DEFAULT NULL, v INT)"
    );
    failures += expect_dml_ok(database, "INSERT INTO explicit_null_comp VALUES (1,2,10)", 1);
    failures += expect_alter_primary_key_ok(
        database,
        "ALTER TABLE explicit_null_comp ADD PRIMARY KEY (a,b)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE explicit_null_comp",
            .values = explicit_null_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "composite DEFAULT NULL normalized away",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO explicit_null_comp (v) VALUES (20)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'a' doesn't have a default value",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE qualified_comp (a INT, b INT)");
    failures += expect_dml_ok(database, "INSERT INTO qualified_comp VALUES (1,2)", 1);
    failures += expect_alter_primary_key_ok(
        database,
        "ALTER TABLE app.qualified_comp ADD PRIMARY KEY (a,b)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE qualified_comp",
            .values = qualified_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified composite ADD PRIMARY KEY",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE rename_comp (a INT, b INT)");
    failures += expect_dml_ok(database, "INSERT INTO rename_comp VALUES (1,2)", 1);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE rename_comp ADD PRIMARY KEY (a,b)");
    failures += expect_statement_ok(database, "RENAME TABLE rename_comp TO renamed_comp");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM renamed_comp",
            .values = renamed_show_index_rows,
            .column_count = show_index_field_count,
            .row_count = 2U,
            .context = "renamed composite primary key metadata",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE renamed_comp");
    failures += execute_error(
        database,
        "SHOW INDEX FROM renamed_comp",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_comp' doesn't exist",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after composite ADD PRIMARY KEY"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen composite alter file"
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE add_comp",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "reopened composite added primary key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_primary_key_integer_types_and_defaults(void) {
    static const char *const default_show_create_rows[] = {
        "default_pk",
        "CREATE TABLE `default_pk` (\n"
        "  `id` int NOT NULL DEFAULT '7',\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const explicit_null_show_create_rows[] = {
        "explicit_null_pk",
        "CREATE TABLE `explicit_null_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const qualified_show_create_rows[] = {
        "qualified_pk",
        "CREATE TABLE `qualified_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "types_defaults") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open type/default file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_tiny",
            .type_name = "TINYINT",
            .first_value = "-128",
            .second_value = "127",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_tiny_u",
            .type_name = "TINYINT UNSIGNED",
            .first_value = "0",
            .second_value = "255",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_small",
            .type_name = "SMALLINT",
            .first_value = "-32768",
            .second_value = "32767",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_small_u",
            .type_name = "SMALLINT UNSIGNED",
            .first_value = "0",
            .second_value = "65535",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_medium",
            .type_name = "MEDIUMINT",
            .first_value = "-8388608",
            .second_value = "8388607",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_medium_u",
            .type_name = "MEDIUMINT UNSIGNED",
            .first_value = "0",
            .second_value = "16777215",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_int",
            .type_name = "INT",
            .first_value = "-2147483648",
            .second_value = "2147483647",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_integer",
            .type_name = "INTEGER",
            .first_value = "-2",
            .second_value = "2",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_int_u",
            .type_name = "INT UNSIGNED",
            .first_value = "0",
            .second_value = "4294967295",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_big",
            .type_name = "BIGINT",
            .first_value = "-9223372036854775808",
            .second_value = "9223372036854775807",
        }
    );
    failures += expect_type_alter_primary_key(
        database,
        (struct primary_key_type_case){
            .table_name = "pk_big_u",
            .type_name = "BIGINT UNSIGNED",
            .first_value = "0",
            .second_value = "9223372036854775807",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE default_pk (id INT DEFAULT 7, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO default_pk VALUES (7, 10)", 1);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE default_pk ADD PRIMARY KEY (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE default_pk",
            .values = default_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "non-NULL default preserved",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO default_pk (v) VALUES (20)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '7' for key 'default_pk.PRIMARY'",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE explicit_null_pk (id INT DEFAULT NULL, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO explicit_null_pk VALUES (1, 10)", 1);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE explicit_null_pk ADD PRIMARY KEY (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE explicit_null_pk",
            .values = explicit_null_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "DEFAULT NULL normalized away",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO explicit_null_pk (v) VALUES (20)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE qualified_pk (id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO qualified_pk VALUES (1, 10)", 1);
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE app.qualified_pk ADD PRIMARY KEY (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE qualified_pk",
            .values = qualified_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified ADD PRIMARY KEY",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_primary_key_diagnostics(void) {
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
        "ALTER TABLE no_default ADD PRIMARY KEY (id)",
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
        "ALTER TABLE missing_table ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.missing ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE dup (id INT)");
    failures += expect_dml_ok(database, "INSERT INTO dup VALUES (1), (1)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE dup ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1' for key 'dup.PRIMARY'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE has_null (id INT)");
    failures += expect_dml_ok(database, "INSERT INTO has_null VALUES (NULL)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE has_null ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "22004",
            .message_part = "Invalid use of NULL value",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE missing_col (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE missing_col ADD PRIMARY KEY (missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE existing_pk (id INT PRIMARY KEY)");
    failures += execute_error(
        database,
        "ALTER TABLE existing_pk ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_multiple_primary_key,
            .sqlstate = "42000",
            .message_part = "Multiple primary key defined",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE text_pk (id TEXT)");
    failures += execute_error(
        database,
        "ALTER TABLE text_pk ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY supports only integer columns",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dup_comp (a INT, b INT)");
    failures += expect_dml_ok(database, "INSERT INTO dup_comp VALUES (1,2),(1,2)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE dup_comp ADD PRIMARY KEY (a, b)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '1-2' for key 'dup_comp.PRIMARY'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dup_comp_order (a INT, b INT)");
    failures +=
        expect_dml_ok(database, "INSERT INTO dup_comp_order VALUES (2,2),(2,2),(1,9),(1,9)", 4);
    failures += execute_error(
        database,
        "ALTER TABLE dup_comp_order ADD PRIMARY KEY (a, b)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '2-2' for key 'dup_comp_order.PRIMARY'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE has_null_comp (a INT, b INT)");
    failures += expect_dml_ok(database, "INSERT INTO has_null_comp VALUES (1,NULL)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE has_null_comp ADD PRIMARY KEY (a, b)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "22004",
            .message_part = "Invalid use of NULL value",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE dup_part (a INT, b INT)");
    failures += execute_error(
        database,
        "ALTER TABLE dup_part ADD PRIMARY KEY (a, a)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_column,
            .sqlstate = "42S21",
            .message_part = "Duplicate column name 'a'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE missing_comp_col (a INT, b INT)");
    failures += execute_error(
        database,
        "ALTER TABLE missing_comp_col ADD PRIMARY KEY (a, missing)",
        (struct expected_sql_error){
            .code = mysql_error_key_column_missing,
            .sqlstate = "42000",
            .message_part = "Key column 'missing' doesn't exist in table",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE text_comp_pk (a INT, b VARCHAR(10))");
    failures +=
        expect_alter_primary_key_ok(database, "ALTER TABLE text_comp_pk ADD PRIMARY KEY (a, b)");
    failures += expect_statement_ok(database, "CREATE TABLE qualified_pk (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE qualified_pk ADD PRIMARY KEY (qualified_pk.id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near '.id)'",
        }
    );
    failures += expect_alter_primary_key_ok(
        database,
        "ALTER TABLE qualified_pk ADD CONSTRAINT named_pk PRIMARY KEY (id)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE using_pk (id INT)");
    failures += expect_alter_primary_key_ok(
        database,
        "ALTER TABLE using_pk ADD PRIMARY KEY USING BTREE (id)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE multi_action_pk (id INT)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE multi_action_pk ADD PRIMARY KEY (id), ADD KEY k_id (id)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE existing_pk DROP INDEX PRIMARY",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.add_pk ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ADD PRIMARY KEY (id)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE private_column_pk (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE private_column_pk ADD PRIMARY KEY (_mylite_private)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_name,
            .sqlstate = "42000",
            .message_part = "Incorrect column name '_mylite_private'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_add_primary_key_independent_handles(void) {
    static const char *const first_expected[] = {"1", "10"};
    static const char *const second_expected[] = {"2", "20"};
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
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT)");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT)");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_alter_primary_key_ok(first, "ALTER TABLE t ADD PRIMARY KEY (id)");
    failures += expect_alter_primary_key_ok(second, "ALTER TABLE t ADD PRIMARY KEY (id)");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_expected,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent added primary key",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_expected,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent added primary key",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_type_alter_primary_key(
    mylite_db *database,
    struct primary_key_type_case test_case
) {
    char sql[sql_capacity];
    const char *expected_rows[2] = {test_case.first_value, test_case.second_value};
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id %s, v INT)",
        test_case.table_name,
        test_case.type_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "type test CREATE SQL too long for %s\n", test_case.table_name);
        return 1;
    }
    failures += expect_statement_ok(database, sql);
    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES (%s, 10), (%s, 20)",
        test_case.table_name,
        test_case.first_value,
        test_case.second_value
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "type test INSERT SQL too long for %s\n", test_case.table_name);
        return failures + 1;
    }
    failures += expect_dml_ok(database, sql, 2);
    written =
        snprintf(sql, sizeof(sql), "ALTER TABLE %s ADD PRIMARY KEY (id)", test_case.table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "type test ALTER SQL too long for %s\n", test_case.table_name);
        return failures + 1;
    }
    failures += expect_alter_primary_key_ok(database, sql);
    written = snprintf(sql, sizeof(sql), "SELECT id FROM %s ORDER BY id", test_case.table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "type test SELECT SQL too long for %s\n", test_case.table_name);
        return failures + 1;
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = expected_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = test_case.table_name,
        }
    );

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

static int expect_alter_primary_key_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        0U,
        "ADD PRIMARY KEY column count"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 0U, "ADD PRIMARY KEY row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        0,
        "ADD PRIMARY KEY affected rows"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "ADD PRIMARY KEY warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_truncate_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "truncate column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "truncate row count");
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "truncate affected rows");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "truncate warning count");
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

static int expect_physical_index_count(
    mylite_db *database,
    int64_t expected_count,
    const char *context
) {
    static const char *const sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND "
                                   "name LIKE '_mylite_user_index_%'";
    sqlite3_stmt *statement = NULL;
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    int failures = 0;
    int sqlite_rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (sqlite_rc != SQLITE_OK) {
        fprintf(stderr, "%s: failed to prepare physical index count\n", context);
        return 1;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        failures += mylite_test_expect_int64(
            (int64_t)sqlite3_column_int64(statement, 0),
            expected_count,
            context
        );
    } else {
        fprintf(stderr, "%s: physical index count did not return a row\n", context);
        failures += 1;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        fprintf(stderr, "%s: failed to finalize physical index count\n", context);
        failures += 1;
    }

    return failures;
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
