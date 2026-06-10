#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    show_columns_column_count = 6,
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

static int test_constraint_syntax_runtime(void);
static int test_extended_option_placeholder_runtime(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_constraint_syntax_runtime();
    failures += test_extended_option_placeholder_runtime();

    return failures == 0 ? 0 : 1;
}

static int test_constraint_syntax_runtime(void) {
    static const char *const primary_column_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const foreign_key_rows[] = {"1"};
    static const char *const check_enforced_rows[] = {"NO"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open constraint database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE bare_pk (id INT, CONSTRAINT PRIMARY KEY (id))");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bare_pk LIKE 'id'",
            .values = primary_column_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "bare CONSTRAINT PRIMARY KEY metadata",
        }
    );
    failures += execute_ok(database, "CREATE TABLE alter_pk (id INT)");
    failures += execute_ok(database, "ALTER TABLE alter_pk ADD CONSTRAINT PRIMARY KEY (id)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_pk LIKE 'id'",
            .values = primary_column_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "bare ADD CONSTRAINT PRIMARY KEY metadata",
        }
    );

    failures += execute_ok(database, "CREATE TABLE fk_parent (id INT PRIMARY KEY)");
    failures += execute_ok(
        database,
        "CREATE TABLE fk_child (pid INT, CONSTRAINT FOREIGN KEY (pid) REFERENCES fk_parent(id))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'fk_child' "
                   "AND CONSTRAINT_TYPE = 'FOREIGN KEY'",
            .values = foreign_key_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "bare CONSTRAINT FOREIGN KEY metadata",
        }
    );
    failures += execute_ok(database, "CREATE TABLE fk_alter_child (pid INT)");
    failures += execute_ok(
        database,
        "ALTER TABLE fk_alter_child ADD CONSTRAINT FOREIGN KEY (pid) REFERENCES fk_parent(id)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'fk_alter_child' "
                   "AND CONSTRAINT_TYPE = 'FOREIGN KEY'",
            .values = foreign_key_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "bare ADD CONSTRAINT FOREIGN KEY metadata",
        }
    );

    failures += execute_ok(database, "CREATE TABLE checked (id INT, CONSTRAINT c CHECK (id > 0))");
    failures += execute_ok(database, "ALTER TABLE checked ALTER CONSTRAINT c NOT ENFORCED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ENFORCED FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'checked' "
                   "AND CONSTRAINT_NAME = 'c'",
            .values = check_enforced_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "ALTER CONSTRAINT CHECK enforcement metadata",
        }
    );
    failures += execute_ok(database, "INSERT INTO checked VALUES (-1)");

    mylite_close(database);
    return failures;
}

static int test_extended_option_placeholder_runtime(void) {
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open placeholder database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures +=
        execute_error(database, "CREATE TABLE encrypted_t (id INT) ENCRYPTION='N'", unsupported);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'encrypted_t'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "encryption placeholder no table",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE column_storage_t (a INT STORAGE DISK, b INT STORAGE MEMORY)",
        unsupported
    );
    failures += execute_error(
        database,
        "CREATE TABLE column_format_t (a INT COLUMN_FORMAT DYNAMIC)",
        unsupported
    );
    failures += execute_error(
        database,
        "CREATE TABLE secondary_t (id INT) SECONDARY_ENGINE=myisam",
        unsupported
    );
    failures += execute_error(
        database,
        "CREATE TABLE check_in_t (f1 INT CHECK (f1 IN (10, 20)))",
        unsupported
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'check_in_t'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "check IN placeholder no table",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "failed result columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = expect_int(
        mylite_execute(database, query.sql, strlen(query.sql), &result),
        MYLITE_OK,
        query.context
    );

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;

            failures += expect_text(
                mylite_result_value_text(result, row, column),
                query.values == NULL ? NULL : query.values[index],
                query.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
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
