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
    show_index_field_count = 15,
    mysql_error_no_database_selected = 1046,
    mysql_error_duplicate_entry = 1062,
    mysql_error_parse = 1064,
    mysql_error_wrong_auto_key = 1075,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_row_is_referenced = 1553,
    mysql_error_drop_constraint_ambiguous = 3939,
    mysql_error_constraint_does_not_exist = 3940,
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

static int test_drop_constraint_successes(void);
static int test_drop_constraint_diagnostics(void);
static int test_drop_constraint_persistence_and_independent_handles(void);
static int open_seeded_memory(mylite_db **out_database);
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
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_drop_constraint_successes();
    failures += test_drop_constraint_diagnostics();
    failures += test_drop_constraint_persistence_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_drop_constraint_successes(void) {
    static const char *const checked_show_create_rows[] = {
        "checked",
        "CREATE TABLE `checked` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const child_show_create_rows[] = {
        "child",
        "CREATE TABLE `child` (\n"
        "  `id` int NOT NULL,\n"
        "  `pid` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`),\n"
        "  KEY `fk_child_parent` (`pid`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const child_show_index_rows[] = {
        "child", "0",     "PRIMARY", "1", "id",  "A",  "0",     NULL,    NULL,
        "",      "BTREE", "",        "",  "YES", NULL, "child", "1",     "fk_child_parent",
        "1",     "pid",   "A",       "0", NULL,  NULL, "YES",   "BTREE", "",
        "",      "YES",   NULL,
    };
    static const char *const drop_pk_show_create_rows[] = {
        "drop_pk",
        "CREATE TABLE `drop_pk` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const empty_rows[] = {0};
    static const char *const unique_show_create_rows[] = {
        "unique_c",
        "CREATE TABLE `unique_c` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  KEY `k_v` (`v`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const unique_show_index_rows[] = {
        "unique_c",
        "1",
        "k_v",
        "1",
        "v",
        "A",
        "0",
        NULL,
        NULL,
        "YES",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const zero_rows[] = {"0"};
    static const char *const zero_zero_row[] = {"0", "0"};
    static const char *const one_row[] = {"1"};
    static const char *const two_zero_row[] = {"2", "0"};
    static const char *const duplicate_unique_rows[] = {"1", "10", "1", "20"};
    static const char *const duplicate_primary_rows[] = {"1", "10", "2", "20", "1", "30"};
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE unique_c (id INT, v INT, CONSTRAINT uq_id UNIQUE (id), KEY k_v (v))"
    );
    failures += expect_dml_ok(database, "INSERT INTO unique_c VALUES (1,10)", 1);
    failures += expect_dml_ok(database, "ALTER TABLE unique_c DROP CONSTRAINT uq_id", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT ROW_COUNT(), @@warning_count",
            zero_zero_row,
            2U,
            1U,
            "ROW_COUNT after unique DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c' "
            "AND CONSTRAINT_NAME = 'uq_id'",
            zero_rows,
            1U,
            1U,
            "unique constraint metadata removed",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c' "
            "AND CONSTRAINT_NAME = 'uq_id'",
            zero_rows,
            1U,
            1U,
            "unique key-column metadata removed",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c' "
            "AND INDEX_NAME = 'uq_id'",
            zero_rows,
            1U,
            1U,
            "unique index metadata removed",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c' "
            "AND INDEX_NAME = 'k_v'",
            one_row,
            1U,
            1U,
            "nonmatching secondary index remains",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW CREATE TABLE unique_c",
            unique_show_create_rows,
            2U,
            1U,
            "SHOW CREATE after unique DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW INDEX FROM unique_c",
            unique_show_index_rows,
            show_index_field_count,
            1U,
            "SHOW INDEX after unique DROP CONSTRAINT",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE unique_c_clone LIKE unique_c");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c_clone' "
            "AND INDEX_NAME = 'k_v'",
            one_row,
            1U,
            1U,
            "CREATE TABLE LIKE keeps remaining index after DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_c_clone' "
            "AND INDEX_NAME = 'uq_id'",
            zero_rows,
            1U,
            1U,
            "CREATE TABLE LIKE omits dropped unique constraint",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO unique_c VALUES (1,20)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, v FROM unique_c ORDER BY v",
            duplicate_unique_rows,
            2U,
            2U,
            "duplicates after unique DROP CONSTRAINT",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE unique_idx (id INT)");
    failures += expect_statement_ok(database, "CREATE UNIQUE INDEX uq_idx_id ON unique_idx (id)");
    failures += expect_dml_ok(database, "ALTER TABLE unique_idx DROP CONSTRAINT uq_idx_id", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unique_idx' "
            "AND INDEX_NAME = 'uq_idx_id'",
            zero_rows,
            1U,
            1U,
            "standalone unique index removed through DROP CONSTRAINT",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE qualified_c (id INT, CONSTRAINT uq_q UNIQUE (id))"
    );
    failures += expect_dml_ok(database, "ALTER TABLE app.qualified_c DROP CONSTRAINT uq_q", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'qualified_c' "
            "AND CONSTRAINT_NAME = 'uq_q'",
            zero_rows,
            1U,
            1U,
            "schema-qualified DROP CONSTRAINT removes metadata",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE option_c (id INT, CONSTRAINT uq_option UNIQUE (id))"
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE option_c DROP CONSTRAINT uq_option, ALGORITHM=INPLACE, LOCK=NONE",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT ROW_COUNT(), @@warning_count",
            zero_zero_row,
            2U,
            1U,
            "ROW_COUNT after option-tail DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'option_c' "
            "AND INDEX_NAME = 'uq_option'",
            zero_rows,
            1U,
            1U,
            "option-tail DROP CONSTRAINT removes unique index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE drop_pk (id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO drop_pk VALUES (1,10),(2,20)", 2);
    failures += expect_dml_ok(database, "ALTER TABLE drop_pk DROP CONSTRAINT `PRIMARY`", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT ROW_COUNT(), @@warning_count",
            two_zero_row,
            2U,
            1U,
            "ROW_COUNT after primary DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_pk' "
            "AND CONSTRAINT_NAME = 'PRIMARY'",
            zero_rows,
            1U,
            1U,
            "primary metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'drop_pk'",
            zero_rows,
            1U,
            1U,
            "primary key-column metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW CREATE TABLE drop_pk",
            drop_pk_show_create_rows,
            2U,
            1U,
            "SHOW CREATE after primary DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW INDEX FROM drop_pk",
            empty_rows,
            show_index_field_count,
            0U,
            "SHOW INDEX after primary DROP CONSTRAINT",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO drop_pk VALUES (1,30)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, v FROM drop_pk ORDER BY v",
            duplicate_primary_rows,
            2U,
            3U,
            "duplicates after primary DROP CONSTRAINT",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, pid INT, "
        "CONSTRAINT fk_child_parent FOREIGN KEY (pid) REFERENCES parent(id))"
    );
    failures += expect_dml_ok(database, "ALTER TABLE child DROP CONSTRAINT fk_child_parent", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' "
            "AND CONSTRAINT_TYPE = 'FOREIGN KEY'",
            zero_rows,
            1U,
            1U,
            "foreign key metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' "
            "AND REFERENCED_TABLE_NAME IS NOT NULL",
            zero_rows,
            1U,
            1U,
            "foreign key key-column metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child'",
            zero_rows,
            1U,
            1U,
            "foreign key referential metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' "
            "AND INDEX_NAME = 'fk_child_parent'",
            one_row,
            1U,
            1U,
            "foreign key child index remains",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW CREATE TABLE child",
            child_show_create_rows,
            2U,
            1U,
            "SHOW CREATE after foreign-key DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW INDEX FROM child",
            child_show_index_rows,
            show_index_field_count,
            2U,
            "SHOW INDEX after foreign-key DROP CONSTRAINT",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (1,999)", 1);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE checked (id INT PRIMARY KEY, v INT, CONSTRAINT chk_v CHECK (v > 0))"
    );
    failures += expect_dml_ok(database, "ALTER TABLE checked DROP CONSTRAINT chk_v", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'chk_v'",
            zero_rows,
            1U,
            1U,
            "check metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'checked' "
            "AND CONSTRAINT_TYPE = 'CHECK'",
            zero_rows,
            1U,
            1U,
            "check table-constraint metadata removed through DROP CONSTRAINT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW CREATE TABLE checked",
            checked_show_create_rows,
            2U,
            1U,
            "SHOW CREATE after CHECK DROP CONSTRAINT",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (1,-1)", 1);

    mylite_close(database);
    return failures;
}

static int test_drop_constraint_diagnostics(void) {
    static const char *const one_row[] = {"1"};
    static const char *const two_rows[] = {"2"};
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE nonunique_c (id INT, KEY k_id (id))");
    failures += execute_error(
        database,
        "ALTER TABLE nonunique_c DROP CONSTRAINT k_id",
        (struct expected_sql_error){
            mysql_error_constraint_does_not_exist,
            "HY000",
            "Constraint 'k_id' does not exist",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'nonunique_c' "
            "AND INDEX_NAME = 'k_id'",
            one_row,
            1U,
            1U,
            "nonunique index preserved after failed DROP CONSTRAINT",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE unknown_c (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE unknown_c DROP CONSTRAINT no_such_name",
        (struct expected_sql_error){
            mysql_error_constraint_does_not_exist,
            "HY000",
            "Constraint 'no_such_name' does not exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE unknown_c DROP CONSTRAINT PRIMARY",
        (struct expected_sql_error){mysql_error_parse, "42000", "syntax"}
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ambiguous_c (id INT, CONSTRAINT c UNIQUE (id), CONSTRAINT c CHECK (id > 0))"
    );
    failures += execute_error(
        database,
        "ALTER TABLE ambiguous_c DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_drop_constraint_ambiguous,
            "HY000",
            "multiple constraints with the name 'c'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ambiguous_c' "
            "AND CONSTRAINT_NAME = 'c'",
            two_rows,
            1U,
            1U,
            "ambiguous DROP CONSTRAINT preserves both constraints",
        }
    );

    failures += execute_error(
        database,
        "ALTER TABLE missing_default DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.t DROP CONSTRAINT c",
        (struct expected_sql_error){mysql_error_unknown_database, "42000", "Unknown database"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.t DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_incorrect_database_name,
            "42000",
            "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE no_table DROP CONSTRAINT c, ALGORITHM=BOGUS",
        (struct expected_sql_error){mysql_error_parse, "42000", "syntax"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE no_table DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "doesn't exist",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_bad (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_error(
        database,
        "ALTER TABLE ai_bad DROP CONSTRAINT `PRIMARY`",
        (struct expected_sql_error){
            mysql_error_wrong_auto_key,
            "42000",
            "there can be only one auto column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ai_bad' "
            "AND CONSTRAINT_NAME = 'PRIMARY'",
            one_row,
            1U,
            1U,
            "auto-increment failed DROP CONSTRAINT preserves primary",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE parent_ref (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_ref (id INT PRIMARY KEY, pid INT, "
        "CONSTRAINT fk_ref FOREIGN KEY (pid) REFERENCES parent_ref(id))"
    );
    failures += execute_error(
        database,
        "ALTER TABLE parent_ref DROP CONSTRAINT `PRIMARY`",
        (struct expected_sql_error){
            mysql_error_row_is_referenced,
            "HY000",
            "needed in a foreign key constraint",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'parent_ref' "
            "AND CONSTRAINT_NAME = 'PRIMARY'",
            one_row,
            1U,
            1U,
            "referenced failed DROP CONSTRAINT preserves primary",
        }
    );

    mylite_close(database);

    failures += expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open no schema db");
    failures += execute_error(
        database,
        "ALTER TABLE missing_default DROP CONSTRAINT c",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    mylite_close(database);
    return failures;
}

static int test_drop_constraint_persistence_and_independent_handles(void) {
    static const char *const zero_rows[] = {"0"};
    static const char *const one_row[] = {"1"};
    static const char *const persisted_rows[] = {"1", "10", "1", "20"};
    char path[test_path_capacity];
    char other_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *other = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persisted") != 0 ||
        make_test_path(other_path, sizeof(other_path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(other_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open persisted db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE persisted ("
        "id INT PRIMARY KEY, v INT, CONSTRAINT uq_v UNIQUE (v), "
        "CONSTRAINT chk_v CHECK (v > 0))"
    );
    failures += expect_dml_ok(database, "INSERT INTO persisted VALUES (1,10)", 1);
    failures += expect_dml_ok(database, "ALTER TABLE persisted DROP CONSTRAINT `PRIMARY`", 1);
    failures += expect_dml_ok(database, "ALTER TABLE persisted DROP CONSTRAINT uq_v", 0);
    failures += expect_dml_ok(database, "ALTER TABLE persisted DROP CONSTRAINT chk_v", 0);
    failures += expect_dml_ok(database, "INSERT INTO persisted VALUES (1,20)", 1);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "preamble after DROP CONSTRAINT"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persisted db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'persisted'",
            zero_rows,
            1U,
            1U,
            "reopened dropped constraints",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, v FROM persisted ORDER BY v",
            persisted_rows,
            2U,
            2U,
            "reopened rows after DROP CONSTRAINT",
        }
    );

    failures += expect_int(mylite_open(other_path, &other), MYLITE_OK, "open independent db");
    failures += expect_statement_ok(other, "CREATE DATABASE app");
    failures += expect_statement_ok(other, "USE app");
    failures += expect_statement_ok(other, "CREATE TABLE persisted (id INT, UNIQUE KEY uq_v (id))");
    failures += expect_dml_ok(other, "INSERT INTO persisted VALUES (1)", 1);
    failures += execute_error(
        other,
        "INSERT INTO persisted VALUES (1)",
        (struct expected_sql_error){mysql_error_duplicate_entry, "23000", "Duplicate entry"}
    );
    failures += expect_dml_ok(database, "INSERT INTO persisted VALUES (1,30)", 1);
    failures += expect_query_values(
        other,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'persisted' "
            "AND CONSTRAINT_NAME = 'uq_v'",
            one_row,
            1U,
            1U,
            "independent handle keeps unique constraint",
        }
    );

    mylite_close(other);
    mylite_close(database);
    remove_related_files(other_path);
    remove_related_files(path);
    return failures;
}

static int open_seeded_memory(mylite_db **out_database) {
    int failures = expect_int(mylite_open(":memory:", out_database), MYLITE_OK, "open memory db");

    if (failures == 0) {
        failures += expect_statement_ok(*out_database, "CREATE DATABASE app");
        failures += expect_statement_ok(*out_database, "USE app");
    }
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    if (expected.message_part != NULL &&
        strstr(mylite_errmsg(database), expected.message_part) == NULL) {
        fprintf(
            stderr,
            "error message: expected '%s' to contain '%s'\n",
            mylite_errmsg(database),
            expected.message_part
        );
        failures += 1;
    }
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

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, "DML affected");
    failures += expect_size(mylite_result_warning_count(result), 0U, "DML warning count");
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
        "%s/mylite_drop_constraint_lifecycle_%d_%s.mylite",
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
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
