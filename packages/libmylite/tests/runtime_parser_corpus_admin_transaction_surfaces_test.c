#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_read_only_transaction = 1792,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_chained_transaction_completion(void);
static int test_rename_tables_alias_runtime(void);
static int test_admin_placeholders_runtime(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    int failures = 0;

    failures += test_chained_transaction_completion();
    failures += test_rename_tables_alias_runtime();
    failures += test_admin_placeholders_runtime();

    return failures == 0 ? 0 : 1;
}

static int test_chained_transaction_completion(void) {
    static const char *const after_commit_chain[] = {"1"};
    static const char *const after_rollback_chain[] = {"1,4"};
    static const char *const after_no_active_commit_chain[] = {"1,4"};
    struct expected_sql_error read_only_error = {
        .code = mysql_error_read_only_transaction,
        .sqlstate = "25006",
        .message_part = "Cannot execute statement in a READ ONLY transaction.",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transaction database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE tx (id INT NOT NULL)");
    failures += execute_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO tx VALUES (1)");
    failures += execute_ok(database, "COMMIT AND CHAIN");
    failures += execute_ok(database, "INSERT INTO tx VALUES (2)");
    failures += execute_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(id ORDER BY id) FROM tx",
            .values = after_commit_chain,
            .column_count = 1U,
            .row_count = 1U,
            .context = "commit and chain starts rollbackable transaction",
        }
    );

    failures += execute_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO tx VALUES (3)");
    failures += execute_ok(database, "ROLLBACK AND CHAIN");
    failures += execute_ok(database, "INSERT INTO tx VALUES (4)");
    failures += execute_ok(database, "COMMIT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(id ORDER BY id) FROM tx",
            .values = after_rollback_chain,
            .column_count = 1U,
            .row_count = 1U,
            .context = "rollback and chain starts commit-capable transaction",
        }
    );

    failures += execute_ok(database, "COMMIT AND CHAIN");
    failures += execute_ok(database, "INSERT INTO tx VALUES (5)");
    failures += execute_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(id ORDER BY id) FROM tx",
            .values = after_no_active_commit_chain,
            .column_count = 1U,
            .row_count = 1U,
            .context = "commit and chain without active transaction starts transaction",
        }
    );

    failures += execute_ok(database, "START TRANSACTION READ ONLY");
    failures += execute_ok(database, "COMMIT AND CHAIN");
    failures += execute_error(database, "INSERT INTO tx VALUES (6)", read_only_error);
    failures += execute_ok(database, "ROLLBACK");

    mylite_close(database);
    return failures;
}

static int test_rename_tables_alias_runtime(void) {
    static const char *const renamed_rows[] = {"7", "8"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open rename database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE r1 (id INT)");
    failures += execute_ok(database, "CREATE TABLE r2 (id INT)");
    failures += execute_ok(database, "INSERT INTO r1 VALUES (7)");
    failures += execute_ok(database, "INSERT INTO r2 VALUES (8)");
    failures += execute_ok(database, "RENAME TABLES r1 TO r1a, r2 TO r2a");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM r1a UNION ALL SELECT id FROM r2a ORDER BY id",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "rename tables alias",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_admin_placeholders_runtime(void) {
    static const char *const mysql_servers_count[] = {"0"};
    static const char *const warning_row[] = {
        "Warning",
        "1105",
        "MyLite accepted this server-only statement as an embedded no-op",
    };
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open admin database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE SERVER srv FOREIGN DATA WRAPPER mysql OPTIONS (DATABASE 'test')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "create server warning",
        }
    );
    failures += execute_ok(database, "ALTER SERVER srv OPTIONS (USER 'sally')");
    failures += execute_ok(database, "DROP SERVER IF EXISTS srv");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM mysql.servers",
            .values = mysql_servers_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "server DDL does not persist mysql.servers rows",
        }
    );

    failures += execute_error(database, "ALTER SCHEMA app ENCRYPTION = 'N'", unsupported);
    failures += execute_error(database, "ALTER DATABASE app READ ONLY DEFAULT", unsupported);

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.context, mylite_errmsg(database));
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row_index = 0U; row_index < query.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < query.column_count; ++column_index) {
            const char *actual = mylite_result_value_text(result, row_index, column_index);
            const char *expected = query.values[(row_index * query.column_count) + column_index];

            failures += mylite_test_expect_text(actual, expected, query.context);
        }
    }
    mylite_result_free(result);
    return failures;
}
