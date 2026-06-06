#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_check_constraint_non_boolean = 3812,
    mysql_error_check_constraint_function = 3814,
    mysql_error_check_constraint_auto_increment = 3818,
    mysql_error_check_constraint_violated = 3819,
    mysql_error_check_constraint_not_found = 3821,
    mysql_error_duplicate_check_constraint = 3822,
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

static int test_alter_check_metadata_dml_and_rebuild(void);
static int test_alter_check_persistence_and_file_format(void);
static int test_alter_check_diagnostics(void);
static int open_seeded_memory(mylite_db **out_database);
static int seed_schema(mylite_db *database, const char *name);
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
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_alter_check_metadata_dml_and_rebuild();
    failures += test_alter_check_persistence_and_file_format();
    failures += test_alter_check_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_alter_check_metadata_dml_and_rebuild(void) {
    static const char *const check_rows_after_add[] = {
        "checked_chk_1",
        "(`a` > 0)",
        "old_non",
        "(`a` < 10)",
    };
    static const char *const enforced_rows_after_add[] = {
        "checked_chk_1",
        "YES",
        "old_non",
        "NO",
    };
    static const char *const index_count_rows[] = {"2"};
    static const char *const remaining_rows[] = {
        "1",
        "1",
        "2",
        "2",
        "3",
        NULL,
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = open_seeded_memory(&database);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE checked ("
        "id INT PRIMARY KEY, "
        "a INT, "
        "KEY a_key (a), "
        "CONSTRAINT old_non CHECK (a < 10) NOT ENFORCED)"
    );
    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (1,1),(2,2),(3,NULL)", 3);
    failures += expect_dml_ok(database, "ALTER TABLE checked ADD CHECK (a > 0)", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, CHECK_CLAUSE "
            "FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' "
            "ORDER BY CONSTRAINT_NAME",
            check_rows_after_add,
            2U,
            2U,
            "ALTER ADD CHECK metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME, ENFORCED "
            "FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'checked' "
            "AND CONSTRAINT_TYPE = 'CHECK' "
            "ORDER BY CONSTRAINT_NAME",
            enforced_rows_after_add,
            2U,
            2U,
            "ALTER ADD CHECK enforcement metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'checked'",
            index_count_rows,
            1U,
            1U,
            "ALTER CHECK rebuild preserves indexes",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO checked VALUES (4,-1)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'checked_chk_1' is violated",
        }
    );

    failures +=
        expect_dml_ok(database, "ALTER TABLE checked ALTER CHECK checked_chk_1 NOT ENFORCED", 0);
    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (4,-1)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'checked_chk_1' is violated",
        }
    );
    failures += execute_ok(database, "SHOW CREATE TABLE checked", &result);
    if (failures == 0) {
        failures += expect_contains(
            mylite_result_value_text(result, 0U, 1U),
            "CONSTRAINT `checked_chk_1` CHECK ((`a` > 0)) /*!80016 NOT ENFORCED */",
            "failed enforce leaves CHECK not enforced"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(database, "DELETE FROM checked WHERE id = 4", 1);
    failures +=
        expect_dml_ok(database, "ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED", 3);
    failures +=
        expect_dml_ok(database, "ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED", 0);
    failures += execute_error(
        database,
        "INSERT INTO checked VALUES (4,-1)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'checked_chk_1' is violated",
        }
    );
    failures += expect_dml_ok(database, "ALTER TABLE checked DROP CHECK checked_chk_1", 0);
    failures += expect_dml_ok(database, "INSERT INTO checked VALUES (4,-1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT id, a FROM checked WHERE id < 4 ORDER BY id",
            remaining_rows,
            2U,
            3U,
            "ALTER CHECK preserves rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_alter_check_persistence_and_file_format(void) {
    static const char *const persisted_rows[] = {"1", "2"};
    static const char *const check_count_rows[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open ALTER CHECK db");
    failures += seed_schema(database, "app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE persisted (id INT PRIMARY KEY, a INT)");
    failures += expect_dml_ok(database, "INSERT INTO persisted VALUES (1,1),(2,2)", 2);
    failures +=
        expect_dml_ok(database, "ALTER TABLE persisted ADD CONSTRAINT positive_a CHECK (a > 0)", 2);
    failures +=
        expect_dml_ok(database, "ALTER TABLE persisted ALTER CHECK positive_a NOT ENFORCED", 0);
    failures += expect_dml_ok(database, "ALTER TABLE persisted ALTER CHECK positive_a ENFORCED", 2);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen ALTER CHECK db");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'positive_a'",
            check_count_rows,
            1U,
            1U,
            "reopened ALTER CHECK descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT a FROM persisted ORDER BY id",
            persisted_rows,
            1U,
            2U,
            "reopened ALTER CHECK rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO persisted VALUES (3,-1)",
        (struct expected_sql_error){
            mysql_error_check_constraint_violated,
            "HY000",
            "Check constraint 'positive_a' is violated",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "ALTER CHECK preserves .mylite preamble"
    );
    remove_related_files(path);

    return failures;
}

static int test_alter_check_diagnostics(void) {
    static const char *const reused_generated_name[] = {"reuse_owner_chk_1"};
    static const char *const qualified_check_count[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open diagnostics db");
    failures += execute_error(
        database,
        "ALTER TABLE missing_default ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_no_database_selected,
            "3D000",
            "No database selected",
        }
    );
    failures += seed_schema(database, "app");
    failures += seed_schema(database, "other");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE checked (a INT)");
    failures += expect_statement_ok(database, "CREATE TABLE other.qualified_checked (a INT)");
    failures += expect_dml_ok(database, "ALTER TABLE other.qualified_checked ADD CHECK (a > 0)", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'other' AND TABLE_NAME = 'qualified_checked' "
            "AND CONSTRAINT_TYPE = 'CHECK'",
            qualified_check_count,
            1U,
            1U,
            "schema-qualified ALTER ADD CHECK target",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE dup_owner (a INT, CONSTRAINT dup_check CHECK (a > 0))"
    );

    failures += execute_error(
        database,
        "ALTER TABLE missing_schema.checked ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_unknown_database,
            "42000",
            "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE missing ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_table_does_not_exist,
            "42S02",
            "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private.checked ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_incorrect_database_name,
            "42000",
            "Incorrect database name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE _mylite_private ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_incorrect_table_name,
            "42000",
            "Incorrect table name '_mylite_private'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked DROP CHECK missing_check",
        (struct expected_sql_error){
            mysql_error_check_constraint_not_found,
            "HY000",
            "Check constraint 'missing_check' is not found in the table",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ALTER CHECK missing_check ENFORCED",
        (struct expected_sql_error){
            mysql_error_check_constraint_not_found,
            "HY000",
            "Check constraint 'missing_check' is not found in the table",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ADD CONSTRAINT dup_check CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_duplicate_check_constraint,
            "HY000",
            "Duplicate check constraint name 'dup_check'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ADD CONSTRAINT bad CHECK (missing > 0)",
        (struct expected_sql_error){
            mysql_error_unknown_column,
            "42S22",
            "Unknown column 'missing' in 'check constraint bad expression'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ADD CHECK (NULL)",
        (struct expected_sql_error){
            mysql_error_check_constraint_non_boolean,
            "HY000",
            "not boolean",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ADD CHECK (RAND() > 0)",
        (struct expected_sql_error){
            mysql_error_check_constraint_function,
            "HY000",
            "function",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE auto_ref (a INT AUTO_INCREMENT PRIMARY KEY)");
    failures += execute_error(
        database,
        "ALTER TABLE auto_ref ADD CHECK (a > 0)",
        (struct expected_sql_error){
            mysql_error_check_constraint_auto_increment,
            "HY000",
            "AUTO_INCREMENT",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE checked ADD CHECK (a > 0), ADD CHECK (a < 10)",
        (struct expected_sql_error){
            mysql_error_parse,
            "42000",
            "syntax",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE reuse_owner (a INT)");
    failures += expect_dml_ok(database, "ALTER TABLE reuse_owner ADD CHECK (a > 0)", 0);
    failures += expect_dml_ok(database, "ALTER TABLE reuse_owner DROP CHECK reuse_owner_chk_1", 0);
    failures +=
        expect_dml_ok(database, "ALTER TABLE reuse_owner ADD CHECK (a < 10) NOT ENFORCED", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            "SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "
            "WHERE CONSTRAINT_SCHEMA = 'app' AND CONSTRAINT_NAME = 'reuse_owner_chk_1'",
            reused_generated_name,
            1U,
            1U,
            "ALTER ADD CHECK reuses dropped generated name",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE collision_owner ("
        "a INT, CONSTRAINT reuse_owner_chk_2 CHECK (a > 0))"
    );
    failures += execute_error(
        database,
        "ALTER TABLE reuse_owner ADD CHECK (a IS NULL)",
        (struct expected_sql_error){
            mysql_error_duplicate_check_constraint,
            "HY000",
            "Duplicate check constraint name 'reuse_owner_chk_2'",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_seeded_memory(mylite_db **out_database) {
    int failures =
        expect_int(mylite_test_open_temporary(out_database), MYLITE_OK, "open memory db");

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
        "%s/mylite_alter_check_constraint_lifecycle_%d_%s.mylite",
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
