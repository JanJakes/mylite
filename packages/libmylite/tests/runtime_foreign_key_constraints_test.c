#include <mylite/mylite.h>

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
    mysql_error_duplicate_key_name = 1061,
    mysql_error_key_column_does_not_exist = 1072,
    mysql_error_row_is_referenced = 1451,
    mysql_error_no_referenced_row = 1452,
    mysql_error_failed_to_open_referenced_table = 1824,
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
static int test_alter_table_add_foreign_key_lifecycle(void);
static int test_foreign_key_diagnostics(void);
static int test_foreign_key_persistence(void);
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

int main(void) {
    int failures = 0;

    failures += test_create_table_foreign_key_lifecycle();
    failures += test_alter_table_add_foreign_key_lifecycle();
    failures += test_foreign_key_diagnostics();
    failures += test_foreign_key_persistence();
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
        "ALTER TABLE parent_signed ADD FOREIGN KEY (id) REFERENCES parent_signed (id)",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "FOREIGN",
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
        (struct expected_sql_error){mysql_error_duplicate_key_name, "42000", "Duplicate key name"}
    );

    mylite_close(database);
    return failures;
}

static int test_foreign_key_persistence(void) {
    static const char *const values[] = {"10", "2", "11", "2"};
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
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistence file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (11, 2)", 1);
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

    mylite_close(database);
    remove_related_files(path);
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
