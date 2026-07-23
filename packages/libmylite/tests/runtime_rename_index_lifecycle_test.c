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
    test_path_suffix_capacity = 16,
    show_columns_field_count = 6,
    show_index_field_count = 15,
    renamed_metadata_physical_index_count = 5,
    renamed_show_index_row_count = 6,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_key_does_not_exist = 1176,
    mysql_error_incorrect_index_name = 1280,
    mysql_error_no_referenced_row = 1452,
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

static int test_rename_index_success_metadata_and_persistence(void);
static int test_rename_index_diagnostics(void);
static int test_rename_index_foreign_key_rename_and_drop_interaction(void);
static int test_rename_index_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_rename_index_ok(mylite_db *database, const char *sql);
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

    failures += test_rename_index_success_metadata_and_persistence();
    failures += test_rename_index_diagnostics();
    failures += test_rename_index_foreign_key_rename_and_drop_interaction();
    failures += test_rename_index_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_rename_index_success_metadata_and_persistence(void) {
    static const char *const show_create_rows[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `v` varchar(20) DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `renamed_u` (`a`),\n"
        "  KEY `K_B` (`b`),\n"
        "  KEY `renamed_v` (`v`(5) DESC),\n"
        "  KEY `k_mix` (`a`,`b` DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const clone_show_create_rows[] = {
        "clone",
        "CREATE TABLE `clone` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` int DEFAULT NULL,\n"
        "  `v` varchar(20) DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  UNIQUE KEY `renamed_u` (`a`),\n"
        "  KEY `K_B` (`b`),\n"
        "  KEY `renamed_v` (`v`(5) DESC),\n"
        "  KEY `k_mix` (`a`,`b` DESC)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const show_index_rows[] = {
        "t", "0", "PRIMARY",   "1", "id", "A", "0", NULL, NULL, "",    "BTREE", "", "", "YES", NULL,
        "t", "0", "renamed_u", "1", "a",  "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "K_B",       "1", "b",  "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "renamed_v", "1", "v",  "D", "0", "5",  NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_mix",     "1", "a",  "A", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
        "t", "1", "k_mix",     "2", "b",  "D", "0", NULL, NULL, "YES", "BTREE", "", "", "YES", NULL,
    };
    static const char *const show_columns_rows[] = {
        "id", "int", "NO",  "PRI", NULL, "", "a", "int",         "YES", "UNI", NULL, "",
        "b",  "int", "YES", "MUL", NULL, "", "v", "varchar(20)", "YES", "MUL", NULL, "",
    };
    static const char *const single_count_rows[] = {"1"};
    static const char *const composite_count_rows[] = {"2"};
    static const char *const zero_count_rows[] = {"0"};
    static const char *const unique_constraint_rows[] = {"renamed_u", "UNIQUE"};
    static const char *const key_column_rows[] = {"renamed_u", "a"};
    static const char *const rows_after_rename[] =
        {"1", "10", "100", "abc", "2", "20", "200", "def"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open rename index file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE t ("
        "id INT PRIMARY KEY, a INT, b INT, v VARCHAR(20), "
        "UNIQUE KEY u_a (a), KEY k_b (b), KEY k_v (v(5) DESC), KEY k_mix (a,b DESC))"
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO t VALUES (1,10,100,'abc'),(2,20,200,'def')", 2);
    failures += expect_physical_index_count(
        database,
        renamed_metadata_physical_index_count,
        "physical indexes before rename"
    );
    session = mylite_connection_session_state(database);
    sqlite_schema_generation = session == NULL ? 0U : session->sqlite_schema_generation;

    failures += expect_rename_index_ok(database, "ALTER TABLE t RENAME INDEX k_b TO K_B");
    failures += expect_rename_index_ok(database, "ALTER TABLE t RENAME KEY k_v TO renamed_v");
    failures += expect_rename_index_ok(database, "ALTER TABLE t RENAME INDEX u_a TO renamed_u");
    failures +=
        expect_rename_index_ok(database, "ALTER TABLE t RENAME INDEX renamed_u TO renamed_u");
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_uint64(
        session == NULL ? 0U : session->sqlite_schema_generation,
        sqlite_schema_generation,
        "rename index keeps sqlite schema generation"
    );
    failures += expect_physical_index_count(
        database,
        renamed_metadata_physical_index_count,
        "physical indexes after rename"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE after rename index",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM t",
            .values = show_index_rows,
            .column_count = show_index_field_count,
            .row_count = renamed_show_index_row_count,
            .context = "SHOW INDEX after rename index",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM t",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "SHOW COLUMNS after rename index",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'K_B'",
            .values = single_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed K_B INFORMATION_SCHEMA.STATISTICS row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND INDEX_NAME = 'renamed_u'",
            .values = single_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed unique INFORMATION_SCHEMA.STATISTICS row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND INDEX_NAME = 'renamed_v'",
            .values = single_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed prefix INFORMATION_SCHEMA.STATISTICS row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_mix'",
            .values = composite_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "unchanged composite INFORMATION_SCHEMA.STATISTICS rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_v'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "old k_v INFORMATION_SCHEMA.STATISTICS row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'u_a'",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "old u_a INFORMATION_SCHEMA.STATISTICS row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM "
                   "INFORMATION_SCHEMA.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' AND CONSTRAINT_TYPE = 'UNIQUE'",
            .values = unique_constraint_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed unique TABLE_CONSTRAINTS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT CONSTRAINT_NAME, COLUMN_NAME FROM "
                   "INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 't' AND CONSTRAINT_NAME = 'renamed_u'",
            .values = key_column_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "renamed unique KEY_COLUMN_USAGE",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (3,10,300,'dup')",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '10' for key 't.renamed_u'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, v FROM t ORDER BY id",
            .values = rows_after_rename,
            .column_count = 4U,
            .row_count = 2U,
            .context = "rows after rename index",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE clone",
            .values = clone_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE after rename index",
        }
    );
    failures += expect_bytes(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)) == 0 ? actual_preamble
                                                                              : NULL,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after rename index"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen rename index file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'K_B'",
            .values = (const char *const[]){"1"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "reopened renamed index metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b, v FROM t ORDER BY id",
            .values = rows_after_rename,
            .column_count = 4U,
            .row_count = 2U,
            .context = "reopened rows after rename index",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_index_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open rename diagnostics db"
    );
    failures += execute_error(
        database,
        "ALTER TABLE t RENAME INDEX k_v TO k_new",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing.t RENAME INDEX k_v TO k_new",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing'",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE diag (id INT PRIMARY KEY, v INT, u INT, KEY k_v (v), KEY k_u (u))"
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_table RENAME INDEX k_v TO k_new",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private RENAME INDEX k_v TO k_new",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag RENAME INDEX missing_idx TO k_new",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'missing_idx' doesn't exist in table 'diag'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag RENAME INDEX k_v TO k_u",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key_name,
            .sqlstate = "42000",
            .message_part = "Duplicate key name 'k_u'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag RENAME INDEX `PRIMARY` TO renamed_primary",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag RENAME INDEX k_v TO `PRIMARY`",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_index_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name 'PRIMARY'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag RENAME INDEX k_v TO _mylite_private",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect index name '_mylite_private'",
        }
    );
    failures += expect_rename_index_ok(database, "ALTER TABLE app.diag RENAME INDEX k_v TO K_V");
    failures += expect_rename_index_ok(database, "ALTER TABLE diag RENAME INDEX K_V TO K_V");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_index_foreign_key_rename_and_drop_interaction(void) {
    static const char *const child_show_create_rows[] = {
        "c",
        "CREATE TABLE `c` (\n"
        "  `id` int NOT NULL,\n"
        "  `p_id` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `renamed_pid` (`p_id`),\n"
        "  CONSTRAINT `fk_c_p` FOREIGN KEY (`p_id`) REFERENCES `p` (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const renamed_table_stat_rows[] = {"1"};
    static const char *const dropped_table_stat_rows[] = {"0"};
    static const char *const referential_constraint_rows[] = {"1"};
    static const char *const renamed_parent_stat_rows[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "foreign") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open rename fk db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE p (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE c ("
        "id INT PRIMARY KEY, p_id INT, KEY k_pid (p_id), "
        "CONSTRAINT fk_c_p FOREIGN KEY (p_id) REFERENCES p(id))"
    );
    failures += expect_rename_index_ok(database, "ALTER TABLE c RENAME INDEX k_pid TO renamed_pid");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE c",
            .values = child_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "foreign key SHOW CREATE after index rename",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
                   "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'c' "
                   "AND CONSTRAINT_NAME = 'fk_c_p'",
            .values = referential_constraint_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "foreign key REFERENTIAL_CONSTRAINTS after index rename",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO p VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO c VALUES (1,1)", 1);
    failures += execute_error(
        database,
        "INSERT INTO c VALUES (2,999)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "Cannot add or update a child row",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE c TO c2");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'c2' "
                   "AND INDEX_NAME = 'renamed_pid'",
            .values = renamed_table_stat_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed table keeps renamed index",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE c2");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'c2'",
            .values = dropped_table_stat_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "dropped renamed index table metadata",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE p_unique (id INT PRIMARY KEY, code INT, UNIQUE KEY u_code (code))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE c_unique ("
        "id INT PRIMARY KEY, code INT, "
        "CONSTRAINT fk_c_unique_p FOREIGN KEY (code) REFERENCES p_unique(code))"
    );
    failures += expect_rename_index_ok(
        database,
        "ALTER TABLE p_unique RENAME INDEX u_code TO renamed_u_code"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'p_unique' "
                   "AND INDEX_NAME = 'renamed_u_code'",
            .values = renamed_parent_stat_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "renamed parent unique index metadata",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO p_unique VALUES (1,10)", 1);
    failures += expect_dml_ok(database, "INSERT INTO c_unique VALUES (1,10)", 1);
    failures += execute_error(
        database,
        "INSERT INTO c_unique VALUES (2,999)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "Cannot add or update a child row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_rename_index_independent_handles(void) {
    static const char *const first_renamed_rows[] = {"1"};
    static const char *const first_old_rows[] = {"0"};
    static const char *const second_old_rows[] = {"1"};
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
        "open first rename handle"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second rename handle"
    );
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, v INT, KEY k_v (v))");
    failures += expect_rename_index_ok(first, "ALTER TABLE t RENAME INDEX k_v TO renamed_v");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' "
                   "AND INDEX_NAME = 'renamed_v'",
            .values = first_renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle renamed index",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_v'",
            .values = first_old_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle old index gone",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 't' AND INDEX_NAME = 'k_v'",
            .values = second_old_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle old index remains",
        }
    );
    failures += expect_physical_index_count(first, 1, "first physical index count after rename");
    failures += expect_physical_index_count(second, 1, "second physical index count unchanged");

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

static int expect_rename_index_ok(mylite_db *database, const char *sql) {
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return -1;
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
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes did not match\n", context);
        return 1;
    }
    return 0;
}
