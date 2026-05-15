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
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_duplicate_column = 1060,
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_does_not_exist = 1072,
    mysql_error_cant_drop_field_or_key = 1091,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_foreign_key_definition = 1239,
    mysql_error_incorrect_index_name = 1280,
    mysql_error_row_is_referenced = 1451,
    mysql_error_no_referenced_row = 1452,
    mysql_error_foreign_key_cascade_duplicate = 1761,
    mysql_error_failed_to_open_referenced_table = 1824,
    mysql_error_duplicate_foreign_key = 1826,
    mysql_error_foreign_key_column_incompatible = 3780,
    mysql_error_foreign_key_missing_unique = 6125,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t row_count;
    size_t column_count;
    const char *context;
};

struct expected_dml_status {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_create_table_foreign_key_lifecycle(void);
static int test_composite_foreign_key_lifecycle(void);
static int test_alter_table_add_foreign_key_lifecycle(void);
static int test_foreign_key_symbol_action_metadata(void);
static int test_foreign_key_cascade_actions(void);
static int test_alter_table_drop_foreign_key_lifecycle(void);
static int test_foreign_key_diagnostics(void);
static int test_drop_foreign_key_diagnostics(void);
static int test_foreign_key_persistence(void);
static int test_drop_foreign_key_persistence_and_file_format(void);
static int test_drop_foreign_key_independent_handles(void);
static int test_drop_database_cleans_foreign_key_descriptors(void);
static int open_seeded_memory(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_warning(
    mylite_db *database,
    const char *sql,
    struct expected_dml_status expected
);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_not_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const unsigned char *expected,
    size_t size,
    const char *context
);
static int read_preamble(const char *path, unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]);

int main(void) {
    int failures = 0;

    failures += test_create_table_foreign_key_lifecycle();
    failures += test_composite_foreign_key_lifecycle();
    failures += test_alter_table_add_foreign_key_lifecycle();
    failures += test_foreign_key_symbol_action_metadata();
    failures += test_foreign_key_cascade_actions();
    failures += test_alter_table_drop_foreign_key_lifecycle();
    failures += test_foreign_key_diagnostics();
    failures += test_drop_foreign_key_diagnostics();
    failures += test_foreign_key_persistence();
    failures += test_drop_foreign_key_persistence_and_file_format();
    failures += test_drop_foreign_key_independent_handles();
    failures += test_drop_database_cleans_foreign_key_descriptors();

    return failures == 0 ? 0 : 1;
}

static int test_create_table_foreign_key_lifecycle(void) {
    enum {
        metadata_key_column_usage_offset = 3,
        metadata_referential_constraints_offset = 7,
        metadata_referential_constraints_column_count = 5,
    };

    static const char *const child_values[] = {"11", "2", "12", NULL};
    static const char *const missing_insert_values[] = {"0"};
    static const char *const statistic_values[] = {"fk_child_parent", "1", "parent_id"};
    static const char *const metadata_values[] = {
        "fk_child_parent",
        "FOREIGN KEY",
        "YES",
        "fk_child_parent",
        "parent_id",
        "parent",
        "id",
        "fk_child_parent",
        "PRIMARY",
        "NONE",
        "NO ACTION",
        "NO ACTION",
    };
    mylite_db *database = NULL;
    mylite_result *show_create_result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child", &show_create_result);
    if (failures == 0) {
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "KEY `fk_child_parent` (`parent_id`)",
            "SHOW CREATE named foreign-key child index"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "INDEX_NAME = 'fk_child_parent'",
            statistic_values,
            1U,
            3U,
            "foreign key child index statistics",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10, 1), (11, 2), (12, NULL)", 3);
    failures += execute_error(
        database,
        "INSERT INTO child VALUES (13, 99)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_dml_warning(
        database,
        "INSERT IGNORE INTO child VALUES (13, 99)",
        (struct expected_dml_status){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM child WHERE id = 13",
            missing_insert_values,
            1U,
            1U,
            "INSERT IGNORE foreign-key violation skips row",
        }
    );
    failures += execute_error(
        database,
        "UPDATE child SET parent_id = 99 WHERE id = 10",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += execute_error(
        database,
        "DELETE FROM parent WHERE id = 1",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += execute_error(
        database,
        "UPDATE parent SET id = 9 WHERE id = 1",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += expect_dml_ok(database, "DELETE FROM child WHERE id = 10", 1);
    failures += expect_dml_ok(database, "DELETE FROM parent WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM child ORDER BY id",
            child_values,
            2U,
            2U,
            "child rows after FK enforcement",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
            "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "CONSTRAINT_TYPE = 'FOREIGN KEY'",
            metadata_values,
            1U,
            3U,
            "foreign key table constraints metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, COLUMN_NAME, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
            "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "CONSTRAINT_NAME = 'fk_child_parent'",
            metadata_values + metadata_key_column_usage_offset,
            1U,
            4U,
            "foreign key key-column metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UNIQUE_CONSTRAINT_NAME, MATCH_OPTION, UPDATE_RULE, "
            "DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child'",
            metadata_values + metadata_referential_constraints_offset,
            1U,
            metadata_referential_constraints_column_count,
            "foreign key referential metadata",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE parent",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += expect_statement_ok(database, "CREATE TABLE select_source (id INT, parent_id INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE select_child (id INT, parent_id INT, "
        "CONSTRAINT fk_select_parent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += execute_error(
        database,
        "INSERT INTO select_child SELECT id, parent_id FROM select_source",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "foreign-key child tables",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO select_child SELECT id, parent_id FROM select_source",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "foreign-key child tables",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_composite_foreign_key_lifecycle(void) {
    enum {
        composite_key_usage_column_count = 6,
    };

    static const char *const child_values[] = {
        "1",
        "1",
        "2",
        "2",
        NULL,
        "2",
        "3",
        "1",
        NULL,
        "4",
        NULL,
        NULL,
    };
    static const char *const key_usage_values[] = {
        "fk_child_parent",
        "a",
        "1",
        "1",
        "parent_pk",
        "a",
        "fk_child_parent",
        "b",
        "2",
        "2",
        "parent_pk",
        "b",
    };
    static const char *const statistic_values[] = {
        "fk_child_parent",
        "1",
        "a",
        "fk_child_parent",
        "2",
        "b",
    };
    static const char *const missing_count[] = {"0"};
    mylite_db *database = NULL;
    mylite_result *show_create_result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent_pk (a INT NOT NULL, b INT NOT NULL, c INT, "
        "PRIMARY KEY (a,b), UNIQUE KEY u_ba (b,a))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_fk (id INT NOT NULL, a INT NULL, b INT NULL, PRIMARY KEY (id), "
        "CONSTRAINT fk_child_parent FOREIGN KEY (a,b) REFERENCES parent_pk (a,b))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_fk", &show_create_result);
    if (failures == 0) {
        const char *create_sql = mylite_result_value_text(show_create_result, 0U, 1U);

        failures += expect_contains(
            create_sql,
            "KEY `fk_child_parent` (`a`,`b`)",
            "composite FK child index in SHOW CREATE"
        );
        failures += expect_contains(
            create_sql,
            "CONSTRAINT `fk_child_parent` FOREIGN KEY (`a`, `b`) REFERENCES `parent_pk` (`a`, `b`)",
            "composite FK constraint in SHOW CREATE"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;

    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_fk' AND "
            "INDEX_NAME = 'fk_child_parent' ORDER BY SEQ_IN_INDEX",
            statistic_values,
            2U,
            3U,
            "composite FK child index statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION, "
            "POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
            "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_fk' AND "
            "CONSTRAINT_NAME = 'fk_child_parent' ORDER BY ORDINAL_POSITION",
            key_usage_values,
            2U,
            composite_key_usage_column_count,
            "composite FK key-column metadata",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_existing (id INT, a INT, b INT, c INT, KEY existing_ab (a,b,c), "
        "CONSTRAINT fk_existing FOREIGN KEY (a,b) REFERENCES parent_pk (a,b))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_existing", &show_create_result);
    if (failures == 0) {
        const char *create_sql = mylite_result_value_text(show_create_result, 0U, 1U);

        failures += expect_contains(
            create_sql,
            "KEY `existing_ab` (`a`,`b`,`c`)",
            "composite FK reuses existing child index"
        );
        failures += expect_not_contains(
            create_sql,
            "KEY `fk_existing`",
            "composite FK does not duplicate reused child index"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;

    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_unnamed (id INT, a INT, b INT, "
        "FOREIGN KEY (a,b) REFERENCES parent_pk (a,b))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_unnamed", &show_create_result);
    if (failures == 0) {
        const char *create_sql = mylite_result_value_text(show_create_result, 0U, 1U);

        failures +=
            expect_contains(create_sql, "KEY `a` (`a`,`b`)", "unnamed composite FK child index");
        failures += expect_contains(
            create_sql,
            "CONSTRAINT `child_unnamed_ibfk_1` FOREIGN KEY",
            "unnamed composite FK constraint name"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_ref_ba (id INT, a INT, b INT, "
        "CONSTRAINT fk_ba FOREIGN KEY (b,a) REFERENCES parent_pk (b,a))"
    );

    failures += expect_dml_ok(database, "INSERT INTO parent_pk VALUES (1,2,10), (3,4,20)", 2);
    failures += expect_dml_ok(
        database,
        "INSERT INTO child_fk VALUES (1,1,2), (2,NULL,2), (3,1,NULL), (4,NULL,NULL)",
        4
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, a, b FROM child_fk ORDER BY id",
            child_values,
            4U,
            3U,
            "composite FK MATCH SIMPLE rows",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO child_ref_ba VALUES (1,1,2)", 1);
    failures += execute_error(
        database,
        "INSERT INTO child_ref_ba VALUES (2,1,4)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += execute_error(
        database,
        "INSERT INTO child_fk VALUES (5,1,99)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_dml_warning(
        database,
        "INSERT IGNORE INTO child_fk VALUES (5,1,99)",
        (struct expected_dml_status){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM child_fk WHERE id = 5",
            missing_count,
            1U,
            1U,
            "composite FK INSERT IGNORE skips missing parent",
        }
    );
    failures += execute_error(
        database,
        "UPDATE child_fk SET b = 99 WHERE id = 1",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += execute_error(
        database,
        "DELETE FROM parent_pk WHERE a = 1 AND b = 2",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += execute_error(
        database,
        "UPDATE parent_pk SET a = 9 WHERE a = 1 AND b = 2",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent_alter (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY (a,b))"
    );
    failures += expect_statement_ok(database, "CREATE TABLE child_alter (id INT, a INT, b INT)");
    failures += expect_dml_ok(database, "INSERT INTO parent_alter VALUES (7,8)", 1);
    failures += expect_dml_ok(database, "INSERT INTO child_alter VALUES (1,7,8), (2,7,9)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE child_alter ADD CONSTRAINT fk_alter_composite FOREIGN KEY (a,b) "
        "REFERENCES parent_alter (a,b)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_dml_ok(database, "DELETE FROM child_alter WHERE id = 2", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE child_alter ADD CONSTRAINT fk_alter_composite FOREIGN KEY (a,b) "
        "REFERENCES parent_alter (a,b)"
    );
    failures += execute_error(
        database,
        "DROP INDEX fk_alter_composite ON child_alter",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE child_alter ADD CONSTRAINT fk_alter_dup_child FOREIGN KEY (a,a) "
        "REFERENCES parent_alter (a,b)",
        (struct expected_sql_error){
            mysql_error_duplicate_column,
            "42S21",
            "Duplicate column name 'a'",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE child_mismatch (a INT, b INT, FOREIGN KEY (a,b) REFERENCES parent_pk (a))",
        (struct expected_sql_error){
            mysql_error_incorrect_foreign_key_definition,
            "42000",
            "Key reference",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE parent_no_unique (a INT, b INT)");
    failures += execute_error(
        database,
        "CREATE TABLE child_missing_unique (a INT, b INT, "
        "FOREIGN KEY (a,b) REFERENCES parent_no_unique (a,b))",
        (struct expected_sql_error){
            mysql_error_foreign_key_missing_unique,
            "HY000",
            "Missing unique key",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent_unsigned (a INT UNSIGNED NOT NULL, b INT UNSIGNED NOT NULL, "
        "PRIMARY KEY (a,b))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_incompatible_composite (a INT UNSIGNED, b INT, "
        "FOREIGN KEY (a,b) REFERENCES parent_unsigned (a,b))",
        (struct expected_sql_error){
            mysql_error_foreign_key_column_incompatible,
            "HY000",
            "incompatible",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_duplicate_composite (a INT, b INT, "
        "CONSTRAINT fk_dup FOREIGN KEY (a,b) REFERENCES parent_pk (a,b), "
        "CONSTRAINT fk_dup FOREIGN KEY (b,a) REFERENCES parent_pk (b,a))",
        (struct expected_sql_error){mysql_error_duplicate_foreign_key, "HY000", "Duplicate"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_duplicate_child_part (a INT, b INT, "
        "CONSTRAINT fk_dup_child_part FOREIGN KEY (a,a) REFERENCES parent_pk (a,b))",
        (struct expected_sql_error){
            mysql_error_duplicate_column,
            "42S21",
            "Duplicate column name 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_duplicate_parent_part (a INT, b INT, "
        "CONSTRAINT fk_dup_parent_part FOREIGN KEY (a,b) REFERENCES parent_pk (a,a))",
        (struct expected_sql_error){
            mysql_error_foreign_key_missing_unique,
            "HY000",
            "Missing unique key",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_alter_table_add_foreign_key_lifecycle(void) {
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT)");
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10, 1), (11, 99)", 2);
    failures += execute_error(
        database,
        "ALTER TABLE child ADD CONSTRAINT fk_alter_parent FOREIGN KEY (parent_id) "
        "REFERENCES parent (id)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_dml_ok(database, "DELETE FROM child WHERE id = 11", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE child ADD CONSTRAINT fk_alter_parent FOREIGN KEY (parent_id) "
        "REFERENCES parent (id)"
    );
    failures += execute_error(
        database,
        "DROP INDEX fk_alter_parent ON child",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );

    mylite_close(database);
    return failures;
}

static int test_foreign_key_symbol_action_metadata(void) {
    static const char *const create_symbol_statistics[] = {"fk_idx", "1", "parent_id"};
    static const char *const create_symbol_referential[] = {
        "child_symbol_ibfk_1",
        "NO ACTION",
        "NO ACTION",
    };
    static const char *const create_action_statistics[] = {"fk_action", "1", "parent_id"};
    static const char *const create_action_referential[] = {
        "fk_action",
        "CASCADE",
        "RESTRICT",
    };
    static const char *const create_reverse_action_referential[] = {
        "fk_reverse_action",
        "RESTRICT",
        "CASCADE",
    };
    static const char *const create_no_action_referential[] = {
        "fk_no_action",
        "NO ACTION",
        "NO ACTION",
    };
    static const char *const alter_symbol_statistics[] = {"idx_pid", "1", "parent_id"};
    static const char *const alter_symbol_referential[] = {
        "child_alter_symbol_ibfk_1",
        "NO ACTION",
        "CASCADE",
    };
    mylite_db *database = NULL;
    mylite_result *show_create_result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_symbol (id INT PRIMARY KEY, parent_id INT, "
        "FOREIGN KEY fk_idx (parent_id) REFERENCES parent (id))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_symbol", &show_create_result);
    if (failures == 0) {
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "KEY `fk_idx` (`parent_id`)",
            "SHOW CREATE unnamed foreign-key index symbol"
        );
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "CONSTRAINT `child_symbol_ibfk_1` FOREIGN KEY",
            "SHOW CREATE unnamed foreign-key generated constraint"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_symbol' AND "
            "INDEX_NAME = 'fk_idx'",
            create_symbol_statistics,
            1U,
            3U,
            "unnamed foreign-key index symbol statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_symbol'",
            create_symbol_referential,
            1U,
            3U,
            "unnamed foreign-key default referential actions",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_action (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_action FOREIGN KEY ignored_idx (parent_id) REFERENCES parent (id) "
        "ON UPDATE CASCADE ON DELETE RESTRICT)"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_action", &show_create_result);
    if (failures == 0) {
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "KEY `fk_action` (`parent_id`)",
            "SHOW CREATE explicit constraint names generated child index"
        );
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "ON DELETE RESTRICT ON UPDATE CASCADE",
            "SHOW CREATE renders non-default foreign-key actions"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_action' AND "
            "INDEX_NAME = 'fk_action'",
            create_action_statistics,
            1U,
            3U,
            "explicit foreign-key generated child index statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_action'",
            create_action_referential,
            1U,
            3U,
            "explicit foreign-key action metadata",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_reverse_action (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_reverse_action FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE CASCADE ON UPDATE RESTRICT)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_reverse_action'",
            create_reverse_action_referential,
            1U,
            3U,
            "reverse-order foreign-key action metadata",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_no_action (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_no_action FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON UPDATE NO ACTION ON DELETE NO ACTION)"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_no_action", &show_create_result);
    if (failures == 0) {
        failures += expect_not_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "NO ACTION",
            "SHOW CREATE omits explicit NO ACTION"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_no_action'",
            create_no_action_referential,
            1U,
            3U,
            "explicit NO ACTION metadata",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE child_alter_symbol (id INT, parent_id INT)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE child_alter_symbol ADD FOREIGN KEY idx_pid (parent_id) "
        "REFERENCES parent (id) ON DELETE CASCADE"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE child_alter_symbol", &show_create_result);
    if (failures == 0) {
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "KEY `idx_pid` (`parent_id`)",
            "SHOW CREATE alter foreign-key index symbol"
        );
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "CONSTRAINT `child_alter_symbol_ibfk_1` FOREIGN KEY",
            "SHOW CREATE alter generated foreign-key constraint"
        );
        failures += expect_contains(
            mylite_result_value_text(show_create_result, 0U, 1U),
            "ON DELETE CASCADE",
            "SHOW CREATE alter delete cascade action"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child_alter_symbol' AND "
            "INDEX_NAME = 'idx_pid'",
            alter_symbol_statistics,
            1U,
            3U,
            "alter foreign-key index symbol statistics",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_alter_symbol'",
            alter_symbol_referential,
            1U,
            3U,
            "alter foreign-key action metadata",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_foreign_key_cascade_actions(void) {
    static const char *const delete_limited_values[] = {
        "10",
        "1",
        "12",
        NULL,
        "13",
        "3",
    };
    static const char *const delete_final_values[] = {"10", "1", "12", NULL};
    static const char *const update_values[] = {"20", "11", "21", "2", "22", NULL};
    static const char *const composite_update_values[] = {"30", "5", "2", "31", NULL, "2"};
    static const char *const child_validation_values[] = {"40", "1"};
    static const char *const unique_conflict_values[] = {"50", "1", "51", "2"};
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE pdel (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cdel (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_cdel_parent FOREIGN KEY (parent_id) REFERENCES pdel (id) "
        "ON DELETE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO pdel VALUES (1), (2), (3)", 3);
    failures +=
        expect_dml_ok(database, "INSERT INTO cdel VALUES (10,1), (11,2), (12,NULL), (13,3)", 4);
    failures += expect_dml_ok(database, "DELETE FROM pdel WHERE id >= 2 ORDER BY id LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM cdel ORDER BY id",
            delete_limited_values,
            3U,
            2U,
            "limited parent DELETE cascades matching child rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM pdel WHERE id = 3", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM cdel ORDER BY id",
            delete_final_values,
            2U,
            2U,
            "parent DELETE cascade leaves nullable child rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE pupd (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cupd (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_cupd_parent FOREIGN KEY (parent_id) REFERENCES pupd (id) "
        "ON UPDATE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO pupd VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO cupd VALUES (20,1), (21,2), (22,NULL)", 3);
    failures += expect_dml_ok(database, "UPDATE pupd SET id = 11 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM cupd ORDER BY id",
            update_values,
            3U,
            2U,
            "parent UPDATE cascades matching child rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE ppair (a INT, b INT, PRIMARY KEY (a,b))");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cpair (id INT PRIMARY KEY, a INT NULL, b INT NULL, "
        "CONSTRAINT fk_cpair_parent FOREIGN KEY (a,b) REFERENCES ppair (a,b) "
        "ON UPDATE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO ppair VALUES (1,2), (3,4)", 2);
    failures += expect_dml_ok(database, "INSERT INTO cpair VALUES (30,1,2), (31,NULL,2)", 2);
    failures += expect_dml_ok(database, "UPDATE ppair SET a = 5 WHERE a = 1 AND b = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, a, b FROM cpair ORDER BY id",
            composite_update_values,
            2U,
            3U,
            "parent UPDATE cascades composite child rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE pvalidate (id INT PRIMARY KEY)");
    failures += expect_statement_ok(database, "CREATE TABLE qvalidate (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cvalidate (id INT PRIMARY KEY, parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES pvalidate (id) ON UPDATE CASCADE, "
        "FOREIGN KEY (parent_id) REFERENCES qvalidate (id))"
    );
    failures += expect_dml_ok(database, "INSERT INTO pvalidate VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO qvalidate VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO cvalidate VALUES (40,1)", 1);
    failures += execute_error(
        database,
        "UPDATE pvalidate SET id = 2 WHERE id = 1",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM cvalidate ORDER BY id",
            child_validation_values,
            1U,
            2U,
            "failed cascade rolls back child-side foreign-key violation",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE puniq (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE cuniq (id INT PRIMARY KEY, parent_id INT, UNIQUE KEY u_parent(parent_id), "
        "FOREIGN KEY (parent_id) REFERENCES puniq (id) ON UPDATE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO puniq VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO cuniq VALUES (50,1), (51,2)", 2);
    failures += execute_error(
        database,
        "UPDATE puniq SET id = 2 WHERE id = 1",
        (struct expected_sql_error){
            mysql_error_foreign_key_cascade_duplicate,
            "23000",
            "would lead to a duplicate entry",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM cuniq ORDER BY id",
            unique_conflict_values,
            2U,
            2U,
            "failed cascade unique conflict rolls back child rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE prestrict (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE crestrict (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_restrict FOREIGN KEY (parent_id) REFERENCES prestrict (id) "
        "ON DELETE RESTRICT ON UPDATE RESTRICT)"
    );
    failures += expect_dml_ok(database, "INSERT INTO prestrict VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO crestrict VALUES (1,1)", 1);
    failures += execute_error(
        database,
        "DELETE FROM prestrict WHERE id = 1",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );
    failures += execute_error(
        database,
        "UPDATE prestrict SET id = 2 WHERE id = 1",
        (struct expected_sql_error){mysql_error_row_is_referenced, "23000", "parent row"}
    );

    mylite_close(database);
    return failures;
}

static int test_alter_table_drop_foreign_key_lifecycle(void) {
    static const char *const zero_rows[] = {"0"};
    static const char *const one_row[] = {"1"};
    static const char *const child_values[] = {"10", "1", "11", "2", "12", NULL, "13", "99"};
    mylite_db *database = NULL;
    mylite_result *show_create_result = NULL;
    mylite_result *show_index_result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT NULL, "
        "CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10, 1), (11, 2), (12, NULL)", 3);
    failures += execute_error(
        database,
        "INSERT INTO child VALUES (13, 99)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );
    failures += expect_dml_ok(database, "ALTER TABLE child DROP FOREIGN KEY fk_child_parent", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "CONSTRAINT_TYPE = 'FOREIGN KEY'",
            zero_rows,
            1U,
            1U,
            "DROP FOREIGN KEY removes table-constraint metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "REFERENCED_TABLE_NAME IS NOT NULL",
            zero_rows,
            1U,
            1U,
            "DROP FOREIGN KEY removes key-column metadata",
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
            "DROP FOREIGN KEY removes referential metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "INDEX_NAME = 'fk_child_parent'",
            one_row,
            1U,
            1U,
            "DROP FOREIGN KEY preserves child index",
        }
    );
    failures += execute_ok(database, "SHOW INDEX FROM child", &show_index_result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_row_count(show_index_result), 2U, "SHOW INDEX after drop FK");
    }
    mylite_result_free(show_index_result);
    show_index_result = NULL;
    failures += execute_ok(database, "SHOW CREATE TABLE child", &show_create_result);
    if (failures == 0) {
        const char *create_sql = mylite_result_value_text(show_create_result, 0U, 1U);

        failures += expect_contains(
            create_sql,
            "KEY `fk_child_parent` (`parent_id`)",
            "SHOW CREATE after DROP FOREIGN KEY preserves child index"
        );
        failures += expect_not_contains(
            create_sql,
            "CONSTRAINT `fk_child_parent`",
            "SHOW CREATE after DROP FOREIGN KEY omits constraint"
        );
    }
    mylite_result_free(show_create_result);
    show_create_result = NULL;
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (13, 99)", 1);
    failures += expect_dml_ok(database, "UPDATE parent SET id = 7 WHERE id = 1", 1);
    failures += expect_dml_ok(database, "DELETE FROM parent WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM child ORDER BY id",
            child_values,
            4U,
            2U,
            "rows after dropped foreign-key enforcement",
        }
    );
    failures += expect_statement_ok(database, "DROP INDEX fk_child_parent ON child");
    failures += execute_ok(database, "SHOW INDEX FROM child", &show_index_result);
    if (failures == 0) {
        failures += expect_size(
            mylite_result_row_count(show_index_result),
            1U,
            "SHOW INDEX after dropping preserved index"
        );
    }
    mylite_result_free(show_index_result);
    show_index_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'child' AND "
            "INDEX_NAME = 'fk_child_parent'",
            zero_rows,
            1U,
            1U,
            "DROP INDEX removes preserved child index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE unnamed_parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE unnamed_child (id INT PRIMARY KEY, parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES unnamed_parent (id))"
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE unnamed_child DROP FOREIGN KEY unnamed_child_ibfk_1",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'unnamed_child' AND "
            "INDEX_NAME = 'parent_id'",
            one_row,
            1U,
            1U,
            "DROP generated foreign-key name preserves generated child index",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE qualified_parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE qualified_child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_qualified_parent FOREIGN KEY (parent_id) REFERENCES qualified_parent (id))"
    );
    failures += expect_dml_ok(
        database,
        "ALTER TABLE app.qualified_child DROP FOREIGN KEY fk_qualified_parent",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'qualified_child'",
            zero_rows,
            1U,
            1U,
            "schema-qualified DROP FOREIGN KEY removes metadata",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO qualified_child VALUES (1, 99)", 1);

    failures += expect_statement_ok(database, "CREATE TABLE case_parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE case_child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT MiXeD_FK FOREIGN KEY (parent_id) REFERENCES case_parent (id))"
    );
    failures += expect_dml_ok(database, "ALTER TABLE case_child DROP FOREIGN KEY `mixed_fk`", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'case_child'",
            zero_rows,
            1U,
            1U,
            "DROP FOREIGN KEY matches names case-insensitively",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE rename_parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rename_child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_rename_parent FOREIGN KEY (parent_id) REFERENCES rename_parent (id))"
    );
    failures += expect_statement_ok(database, "RENAME TABLE rename_child TO renamed_child");
    failures +=
        expect_dml_ok(database, "ALTER TABLE renamed_child DROP FOREIGN KEY fk_rename_parent", 0);
    failures += expect_statement_ok(database, "DROP TABLE rename_parent");

    mylite_close(database);
    return failures;
}

static int test_foreign_key_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent_no_key (id INT)");
    failures += expect_statement_ok(database, "CREATE TABLE parent_signed (id INT PRIMARY KEY)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE parent_unsigned (id INT UNSIGNED PRIMARY KEY)");
    failures += execute_error(
        database,
        "CREATE TABLE child_missing_parent (id INT, parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES missing_parent (id))",
        (struct expected_sql_error){
            mysql_error_failed_to_open_referenced_table,
            "HY000",
            "referenced table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_missing_column (id INT, parent_id INT, "
        "FOREIGN KEY (missing_col) REFERENCES parent_signed (id))",
        (struct expected_sql_error){
            mysql_error_key_column_does_not_exist,
            "42000",
            "missing_col",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_missing_unique (id INT, parent_id INT, "
        "FOREIGN KEY (parent_id) REFERENCES parent_no_key (id))",
        (struct expected_sql_error){
            mysql_error_foreign_key_missing_unique,
            "HY000",
            "Missing unique key",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_incompatible (id INT, parent_id INT UNSIGNED, "
        "FOREIGN KEY (parent_id) REFERENCES parent_signed (id))",
        (struct expected_sql_error){
            mysql_error_foreign_key_column_incompatible,
            "HY000",
            "incompatible",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_duplicate_fk_index_symbol (a INT, b INT, "
        "FOREIGN KEY fk_idx (a) REFERENCES parent_signed (id), "
        "FOREIGN KEY fk_idx (b) REFERENCES parent_signed (id))",
        (struct expected_sql_error){mysql_error_duplicate_key_name, "42000", "Duplicate key"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE child_primary_fk_index_symbol (a INT, "
        "FOREIGN KEY `PRIMARY` (a) REFERENCES parent_signed (id))",
        (struct expected_sql_error){
            mysql_error_incorrect_index_name,
            "42000",
            "Incorrect index name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE parent_signed ADD FOREIGN KEY (id) REFERENCES parent_signed (id) "
        "ON UPDATE CASCADE ON UPDATE RESTRICT",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "ON UPDATE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_duplicate_fk (id INT, parent_id INT, "
        "CONSTRAINT fk_dup FOREIGN KEY (parent_id) REFERENCES parent_signed (id))"
    );
    failures += execute_error(
        database,
        "ALTER TABLE child_duplicate_fk ADD CONSTRAINT fk_dup FOREIGN KEY (parent_id) "
        "REFERENCES parent_signed (id)",
        (struct expected_sql_error){mysql_error_duplicate_foreign_key, "HY000", "Duplicate"}
    );

    mylite_close(database);
    return failures;
}

static int test_drop_foreign_key_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics");

    failures += execute_error(
        database,
        "ALTER TABLE child DROP FOREIGN KEY fk_child_parent",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += open_seeded_memory(&database);
    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += execute_error(
        database,
        "ALTER TABLE missing.child DROP FOREIGN KEY fk_child_parent",
        (struct expected_sql_error){mysql_error_unknown_database, "42000", "Unknown database"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing_child DROP FOREIGN KEY fk_child_parent",
        (struct expected_sql_error){mysql_error_table_does_not_exist, "42S02", "doesn't exist"}
    );
    failures += execute_error(
        database,
        "ALTER TABLE parent DROP FOREIGN KEY fk_missing",
        (struct expected_sql_error){
            mysql_error_cant_drop_field_or_key,
            "42000",
            "fk_missing",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_catalog_tables DROP FOREIGN KEY fk_missing",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE child DROP FOREIGN KEY IF EXISTS fk_child_parent",
        (struct expected_sql_error){mysql_error_parse, "42000", "IF"}
    );

    mylite_close(database);
    return failures;
}

static int test_foreign_key_persistence(void) {
    static const char *const values[] = {"10", "2", "11", "2"};
    static const char *const composite_values[] = {"20", "1", "2", "21", "3", "4"};
    static const char *const action_values[] = {"30", "9"};
    static const char *const action_rules[] = {"fk_action_persist", "CASCADE", "CASCADE"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open persistence file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_persist FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1), (2)", 2);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10, 1)", 1);
    failures += expect_dml_ok(database, "UPDATE child SET parent_id = 2 WHERE id = 10", 1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE parent_pair (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY (a,b))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_pair (id INT PRIMARY KEY, a INT, b INT, "
        "CONSTRAINT fk_pair FOREIGN KEY (a,b) REFERENCES parent_pair (a,b))"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent_pair VALUES (1,2), (3,4)", 2);
    failures += expect_dml_ok(database, "INSERT INTO child_pair VALUES (20,1,2)", 1);
    failures += expect_statement_ok(database, "CREATE TABLE parent_action (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child_action_persist (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_action_persist FOREIGN KEY (parent_id) REFERENCES parent_action (id) "
        "ON UPDATE CASCADE ON DELETE CASCADE)"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent_action VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO child_action_persist VALUES (30,1)", 1);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistence file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (11, 2)", 1);
    failures += expect_dml_ok(database, "INSERT INTO child_pair VALUES (21,3,4)", 1);
    failures += expect_dml_ok(database, "UPDATE parent_action SET id = 9 WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM child ORDER BY id",
            values,
            2U,
            2U,
            "foreign key rows after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, a, b FROM child_pair ORDER BY id",
            composite_values,
            2U,
            3U,
            "composite foreign key rows after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM child_action_persist ORDER BY id",
            action_values,
            1U,
            2U,
            "foreign-key actions persist after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE "
            "FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child_action_persist'",
            action_rules,
            1U,
            3U,
            "foreign-key action metadata persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_foreign_key_persistence_and_file_format(void) {
    static const char *const zero_rows[] = {"0"};
    static const char *const values[] = {"10", "1", "11", "99"};
    unsigned char before_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char after_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "drop_persistence") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop persistence file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_persist_drop FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10, 1)", 1);
    failures += read_preamble(path, before_preamble);
    failures += expect_int(
        mylite_file_preamble_validate(before_preamble),
        1,
        "pre-drop preamble validates"
    );
    failures += expect_dml_ok(database, "ALTER TABLE child DROP FOREIGN KEY fk_persist_drop", 0);
    mylite_close(database);
    database = NULL;

    failures += read_preamble(path, after_preamble);
    failures += expect_int(
        mylite_file_preamble_validate(after_preamble),
        1,
        "post-drop preamble validates"
    );
    failures += expect_bytes(
        after_preamble,
        before_preamble,
        sizeof(after_preamble),
        "DROP FOREIGN KEY preserves MyLite preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen drop persistence file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'child'",
            zero_rows,
            1U,
            1U,
            "dropped foreign key absent after reopen",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (11, 99)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, parent_id FROM child ORDER BY id",
            values,
            2U,
            2U,
            "rows after reopened dropped foreign key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_drop_foreign_key_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "drop_independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "drop_independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first FK file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second FK file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(second, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        first,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_independent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_statement_ok(
        second,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_independent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_dml_ok(first, "ALTER TABLE child DROP FOREIGN KEY fk_independent", 0);
    failures += expect_dml_ok(first, "INSERT INTO child VALUES (1, 99)", 1);
    failures += execute_error(
        second,
        "INSERT INTO child VALUES (1, 99)",
        (struct expected_sql_error){mysql_error_no_referenced_row, "23000", "child row"}
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_drop_database_cleans_foreign_key_descriptors(void) {
    static const char *const zero_rows[] = {"0"};
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_drop_parent FOREIGN KEY (parent_id) REFERENCES parent (id))"
    );
    failures += expect_statement_ok(database, "DROP DATABASE app");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT)");
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (1, 99)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app'",
            zero_rows,
            1U,
            1U,
            "DROP DATABASE removes foreign-key descriptors",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_seeded_memory(mylite_db **out_database) {
    int failures = expect_int(mylite_open(":memory:", out_database), MYLITE_OK, "open memory");

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
            "%s: expected success, got %d %s %s\n",
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
        fprintf(stderr, "%s: expected error, got success\n", sql);
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
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
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

static int expect_dml_warning(
    mylite_db *database,
    const char *sql,
    struct expected_dml_status expected
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
        "/tmp/mylite_foreign_key_constraints_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
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

static int expect_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) != NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] not to contain [%s]\n",
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
    const unsigned char *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte arrays differ\n", context);
        return 1;
    }
    return 0;
}

static int read_preamble(const char *path, unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE]) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file for preamble read\n", path);
        return 1;
    }
    read_count = fread(preamble, 1U, MYLITE_FILE_PREAMBLE_SIZE, file);
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file after preamble read\n", path);
        return 1;
    }
    if (read_count != MYLITE_FILE_PREAMBLE_SIZE) {
        fprintf(stderr, "%s: failed to read complete preamble\n", path);
        return 1;
    }
    return 0;
}
