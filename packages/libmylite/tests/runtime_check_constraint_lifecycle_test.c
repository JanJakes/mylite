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
    schema_sql_capacity = 128,
    mysql_error_parse = 1064,
    mysql_error_check_constraint_non_boolean = 3812,
    mysql_error_check_constraint_column_ref = 3813,
    mysql_error_check_constraint_function = 3814,
    mysql_error_check_constraint_subquery = 3815,
    mysql_error_check_constraint_variable = 3816,
    mysql_error_check_constraint_auto_increment = 3818,
    mysql_error_check_constraint_violated = 3819,
    mysql_error_check_constraint_unknown_column = 3820,
    mysql_error_duplicate_check_constraint = 3822,
    mysql_error_invalid_json_text = 3140,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_dml_warning {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_check_constraint_create_metadata_and_dml(void);
static int test_check_constraint_lifecycle_persistence_and_file_format(void);
static int test_check_constraint_diagnostics(void);
static int open_seeded_memory(mylite_db **out_database);
static int seed_schema(mylite_db *database, const char *name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_warning(
    mylite_db *database,
    const char *sql,
    struct expected_dml_warning expected
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
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

    failures += test_check_constraint_create_metadata_and_dml();
    failures += test_check_constraint_lifecycle_persistence_and_file_format();
    failures += test_check_constraint_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_check_constraint_create_metadata_and_dml(void) {
    static const char *const check_constraint_rows[] = {
        "checked_chk_1",
        "(`a` > 0)",
        "checked_chk_2",
        "(`c` > 0)",
        "checked_chk_3",
        "(`a` < `b`)",
        "explicit_b_positive",
        "(`b` > 0)",
    };
    static const char *const table_constraint_rows[] = {
        "checked_chk_1",
        "CHECK",
        "YES",
        "checked_chk_2",
        "CHECK",
        "NO",
        "checked_chk_3",
        "CHECK",
        "YES",
        "explicit_b_positive",
        "CHECK",
        "YES",
    };
    static const char *const checked_rows[] = {
        "1",
        "4",
        "-10",
        "2",
        "3",
        NULL,
        "5",
        "6",
        "1",
    };
    static const char *const warning_rows[] = {
        "Warning",
        "3819",
        "Check constraint 'checked_chk_1' is violated.",
        "Warning",
        "3819",
        "Check constraint 'checked_chk_1' is violated.",
    };
    static const char *const text_json_check_rows[] = {
        "checked_text_json_chk_1",
        "(`name` <> _utf8mb4\\'\\')",
        "checked_text_json_chk_2",
        "json_valid(`data`)",
        "checked_text_json_chk_3",
        "((`score` > 0) and (`score` < 100))",
        "length_limit",
        "(length(`data`) < 20)",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE checked ("
        "a INT CHECK (a > 0), "
        "b INT, "
        "c INT CHECK (c > 0) NOT ENFORCED, "
        "CONSTRAINT explicit_b_positive CHECK (b > 0), "
        "CHECK (a < b))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE checked", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CONSTRAINT `checked_chk_1` CHECK ((`a` > 0))",
            "SHOW CREATE generated inline CHECK"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `checked_chk_2` CHECK ((`c` > 0)) /*!80016 NOT ENFORCED */",
            "SHOW CREATE inline NOT ENFORCED CHECK"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `checked_chk_3` CHECK ((`a` < `b`))",
            "SHOW CREATE generated table CHECK"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `explicit_b_positive` CHECK ((`b` > 0))",
            "SHOW CREATE explicit CHECK"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, CHECK_CLAUSE "
            "FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' "
            "ORDER BY CONSTRAINT_NAME",
            check_constraint_rows,
            2U,
            4U,
            "CHECK_CONSTRAINTS descriptor rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "
            "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'checked' "
            "ORDER BY CONSTRAINT_NAME",
            table_constraint_rows,
            3U,
            4U,
            "TABLE_CONSTRAINTS CHECK rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE checked_text_json ("
        "name VARCHAR(255) CHECK (name != ''), "
        "data JSON CHECK (json_valid(data)), "
        "score DOUBLE CHECK (score > 0 AND score < 100), "
        "CONSTRAINT length_limit CHECK (length(data) < 20))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE checked_text_json", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CHECK ((`name` <> _utf8mb4''))",
            "SHOW CREATE CHECK string literal"
        );
        failures += expect_contains(
            show_create,
            "CHECK (json_valid(`data`))",
            "SHOW CREATE CHECK json_valid"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, CHECK_CLAUSE "
            "FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' "
            "AND (CONSTRAINT_NAME = 'checked_text_json_chk_1' "
            "OR CONSTRAINT_NAME = 'checked_text_json_chk_2' "
            "OR CONSTRAINT_NAME = 'checked_text_json_chk_3' "
            "OR CONSTRAINT_NAME = 'length_limit') "
            "ORDER BY CONSTRAINT_NAME",
            text_json_check_rows,
            2U,
            4U,
            "CHECK_CONSTRAINTS text and json rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO checked_text_json (name, data, score) VALUES ('bad', 'invalid JSON', 5)",
        (struct expected_sql_error){
            mysql_error_invalid_json_text,
            "22032",
            "Invalid JSON text: \"Invalid value.\" at position 0 in value for column "
            "'checked_text_json.data'.",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE bare_constraint ("
        "a INT CONSTRAINT CHECK (a > 0), "
        "b INT, "
        "CONSTRAINT CHECK (b > 0))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE bare_constraint", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CONSTRAINT `bare_constraint_chk_1` CHECK ((`a` > 0))",
            "bare CONSTRAINT inline CHECK generated name"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `bare_constraint_chk_2` CHECK ((`b` > 0))",
            "bare CONSTRAINT table CHECK generated name"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (1, 2, -10)", 1);
    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (2, 3, NULL)", 1);
    failures += expect_dml_ok(database, "UPDATE checked SET b = 4 WHERE a = 1", 1);
    failures += execute_error(
        database,
        "INSERT INTO checked VALUES (3, 2, 1)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'checked_chk_3' is violated",
        }
    );
    failures += execute_error(
        database,
        "UPDATE checked SET b = 0 WHERE a = 1",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'explicit_b_positive' is violated",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO checked VALUES (4, 0, 0)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'explicit_b_positive' is violated",
        }
    );
    failures += expect_dml_warning(
        database,
        "INSERT IGNORE INTO checked VALUES (5, 6, 1), (0, 9, 1), (-1, 9, 1)",
        (struct expected_dml_warning){.affected_rows = 1, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SHOW WARNINGS",
            warning_rows,
            3U,
            2U,
            "INSERT IGNORE CHECK warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT a, b, c FROM checked ORDER BY a",
            checked_rows,
            3U,
            3U,
            "CHECK-constrained DML rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE checked_expr ("
        "a INT, b INT, "
        "CHECK (a + 1 < b * 2), "
        "CHECK (a <=> b), "
        "CHECK (a IS NOT NULL), "
        "CHECK (NOT (a < 0)), "
        "CHECK ((a > 0) AND (b > 0)), "
        "CHECK ((a > 0) OR (b > 0)))"
    );
    failures += execute_ok(database, "SHOW CREATE TABLE checked_expr", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CHECK ((`a` <=> `b`))",
            "SHOW CREATE preserves logical null-safe equality"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(database, "INSERT INTO checked_expr VALUES (2, 2)", 1);
    failures += execute_error(
        database,
        "INSERT INTO checked_expr VALUES (NULL, NULL)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_check_constraint_lifecycle_persistence_and_file_format(void) {
    static const char *const cloned_count[] = {"2"};
    static const char *const ctas_check_one_count[] = {"0"};
    static const char *const ctas_check_two_count[] = {"0"};
    static const char *const explicit_count_after_drop[] = {"0"};
    static const char *const cloned_check_one_count[] = {"1"};
    static const char *const cloned_check_two_count[] = {"1"};
    static const char *const cloned_rows[] = {"5", "11"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open CHECK persistence db");
    failures += seed_schema(database, "app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE source ("
        "a INT CHECK (a > 0), "
        "CONSTRAINT explicit_a CHECK (a < 10) NOT ENFORCED)"
    );
    failures += expect_dml_ok(database, "INSERT INTO source VALUES (5)", 1);
    failures += expect_statement_ok(database, "CREATE TABLE cloned LIKE source");
    failures += expect_dml_ok(database, "INSERT INTO cloned VALUES (5), (11)", 2);
    failures += execute_error(
        database,
        "INSERT INTO cloned VALUES (-1)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'cloned_chk_2' is violated",
        }
    );
    failures += execute_ok(database, "SHOW CREATE TABLE cloned", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CONSTRAINT `cloned_chk_1` CHECK ((`a` < 10)) /*!80016 NOT ENFORCED */",
            "LIKE cloned generated not-enforced CHECK"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `cloned_chk_2` CHECK ((`a` > 0))",
            "LIKE cloned generated enforced CHECK"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "CREATE TABLE ctas AS SELECT * FROM source");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'ctas_chk_1'",
            ctas_check_one_count,
            1U,
            1U,
            "CTAS omits first CHECK descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'ctas_chk_2'",
            ctas_check_two_count,
            1U,
            1U,
            "CTAS omits second CHECK descriptor",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE source TO renamed");
    failures += execute_ok(database, "SHOW CREATE TABLE renamed", &result);
    if (failures == 0) {
        const char *show_create = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(
            show_create,
            "CONSTRAINT `renamed_chk_1` CHECK ((`a` > 0))",
            "rename updates generated CHECK name"
        );
        failures += expect_contains(
            show_create,
            "CONSTRAINT `explicit_a` CHECK ((`a` < 10)) /*!80016 NOT ENFORCED */",
            "rename preserves explicit CHECK name"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "DROP TABLE renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'explicit_a'",
            explicit_count_after_drop,
            1U,
            1U,
            "DROP TABLE removes CHECK descriptors",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen CHECK db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'cloned_chk_1'",
            cloned_check_one_count,
            1U,
            1U,
            "reopened first CHECK descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'cloned_chk_2'",
            cloned_check_two_count,
            1U,
            1U,
            "reopened second CHECK descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'cloned' AND CONSTRAINT_TYPE = 'CHECK'",
            cloned_count,
            1U,
            1U,
            "reopened TABLE_CONSTRAINTS CHECK descriptors",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT a FROM cloned ORDER BY a",
            cloned_rows,
            1U,
            2U,
            "reopened CHECK rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO cloned VALUES (-2)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'cloned_chk_2' is violated",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "CHECK lifecycle preserves .mylite preamble"
    );
    remove_related_files(path);

    return failures;
}

static int test_check_constraint_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE dup_owner (a INT, CONSTRAINT c_dup CHECK (a > 0))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE dup_other (a INT, CONSTRAINT c_dup CHECK (a > 0))",
        (struct expected_sql_error){
            mysql_error_duplicate_check_constraint,
            "HY000",
            "Duplicate check constraint name 'c_dup'",
        }
    );
    failures += expect_statement_ok(database, "CREATE DATABASE other");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE other.dup_other (a INT, CONSTRAINT c_dup CHECK (a > 0))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE unknown_col (a INT, CHECK (missing > 0))",
        (struct expected_sql_error){
            mysql_error_check_constraint_unknown_column,
            "HY000",
            "does not exist",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE column_ref_other (a INT CHECK (b > 0), b INT)",
        (struct expected_sql_error){
            mysql_error_check_constraint_column_ref,
            "HY000",
            "references other column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE auto_inc_ref (a INT AUTO_INCREMENT PRIMARY KEY, CHECK (a > 0))",
        (struct expected_sql_error){
            mysql_error_check_constraint_auto_increment,
            "HY000",
            "AUTO_INCREMENT",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE non_boolean (a INT, CHECK (NULL))",
        (struct expected_sql_error){
            mysql_error_check_constraint_non_boolean,
            "HY000",
            "not boolean",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE function_ref (a INT, CHECK (RAND() > 0))",
        (struct expected_sql_error){
            mysql_error_check_constraint_function,
            "HY000",
            "function",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE subquery_ref (a INT, CHECK ((SELECT 1) > 0))",
        (struct expected_sql_error){
            mysql_error_check_constraint_subquery,
            "HY000",
            "subquery",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE variable_ref (a INT, CHECK (@@sql_mode IS NOT NULL))",
        (struct expected_sql_error){
            mysql_error_check_constraint_variable,
            "HY000",
            "variable",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE temp_check (a INT CHECK (a > 0))",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK constraint",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE alter_checked (a INT CHECK (a > 0), b INT)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked DROP COLUMN b",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked RENAME COLUMN b TO c",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked MODIFY COLUMN b BIGINT",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked CHANGE COLUMN b c BIGINT",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked ORDER BY b",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_checked FORCE",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "CHECK-constrained",
        }
    );

    failures += expect_statement_ok(database, "CREATE DATABASE rename_dst");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rename_src (a INT, CONSTRAINT rename_collision CHECK (a > 0))"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rename_dst.rename_owner ("
        "a INT, CONSTRAINT rename_collision CHECK (a > 0))"
    );
    failures += execute_error(
        database,
        "RENAME TABLE rename_src TO rename_dst.rename_src",
        (struct expected_sql_error){
            mysql_error_duplicate_check_constraint,
            "HY000",
            "Duplicate check constraint name 'rename_collision'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE rename_self_collision ("
        "a INT, "
        "CONSTRAINT renamed_self_collision_chk_1 CHECK (a > 0), "
        "CHECK (a < 10))"
    );
    failures += execute_error(
        database,
        "RENAME TABLE rename_self_collision TO renamed_self_collision",
        (struct expected_sql_error){
            mysql_error_duplicate_check_constraint,
            "HY000",
            "Duplicate check constraint name 'renamed_self_collision_chk_1'",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_seeded_memory(mylite_db **out_database) {
    int failures = expect_int(mylite_open(":memory:", out_database), MYLITE_OK, "open memory db");

    if (failures == 0) {
        failures += seed_schema(*out_database, "app");
    }
    if (failures == 0) {
        failures += expect_statement_ok(*out_database, "USE app");
    }
    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    mylite_result *result = NULL;
    char sql[schema_sql_capacity];
    int written = snprintf(sql, sizeof(sql), "CREATE DATABASE %s", name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "schema SQL is too long for %s\n", name);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
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
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
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

static int expect_dml_warning(
    mylite_db *database,
    const char *sql,
    struct expected_dml_warning expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "warning DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "warning DML row count");
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "warning DML affected"
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "warning DML count"
    );
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
        "%s/mylite_check_constraint_lifecycle_%d_%s.mylite",
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
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
