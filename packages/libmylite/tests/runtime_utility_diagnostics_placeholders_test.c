#include "mylite_test_support.h"

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
    mysql_error_parse = 1064,
    test_path_capacity = 1024,
    test_path_suffix_capacity = 8,
    warning_column_count = 3,
};

struct expected_error {
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

static int test_utility_noop_warning_surface(void);
static int test_utility_noop_preserves_user_transaction(void);
static int test_unsupported_utility_errors(void);
static int expect_utility_noop(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_utility_noop_warning_surface();
    failures += test_utility_noop_preserves_user_transaction();
    failures += test_unsupported_utility_errors();

    return failures == 0 ? 0 : 1;
}

static int test_utility_noop_warning_surface(void) {
    static const char *const utility_statements[] = {
        "ANALYZE TABLE foo UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS",
        "INSTALL COMPONENT 'file://component_validate_password'",
        "UNINSTALL PLUGIN validate_password",
        "CREATE SPATIAL REFERENCE SYSTEM 2004326 NAME 'Copy of WGS 84'",
        "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM 2004326 NAME 'Copy of WGS 84'",
        "DROP SPATIAL REFERENCE SYSTEM IF EXISTS 2004326",
        "CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENGINE=InnoDB",
        "DROP TABLESPACE ts1",
        "CREATE LOGFILE GROUP lg1 ADD UNDOFILE 'undo.dat' ENGINE=InnoDB",
        "ALTER LOGFILE GROUP lg1 ADD UNDOFILE 'undo2.dat' ENGINE=NDB",
        "DROP LOGFILE GROUP lg1 ENGINE=NDB",
        "SET GLOBAL max_allowed_packet=4*1024",
    };
    static const char *const warning_rows[] = {
        "Warning",
        "1105",
        "MyLite accepted this utility statement as an embedded no-op",
    };
    static const char *const row_count_rows[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open utility no-op memory"
    );
    for (size_t index = 0U; index < sizeof(utility_statements) / sizeof(utility_statements[0]);
         ++index) {
        failures += expect_utility_noop(database, utility_statements[index]);
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "utility no-op row count",
        }
    );
    failures += expect_utility_noop(database, "UNINSTALL COMPONENT 'file://component_x'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = warning_column_count,
            .row_count = 1U,
            .context = "utility no-op warning",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_utility_noop_preserves_user_transaction(void) {
    static const char *const count_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "utility_noop_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open transaction no-op");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(database, "START TRANSACTION");
    failures += execute_statement_ok(database, "INSERT INTO t VALUES (1)");
    failures += expect_utility_noop(
        database,
        "CREATE SPATIAL REFERENCE SYSTEM 2004326 NAME 'Copy of WGS 84' DEFINITION 'GEOGCS[]'"
    );
    failures += expect_utility_noop(database, "DROP LOGFILE GROUP lg1 ENGINE=NDB");
    failures += execute_statement_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "utility no-op transaction rollback",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_unsupported_utility_errors(void) {
    static const char *const unsupported_statements[] = {
        "XA START 'xid'",
        "HANDLER t1 OPEN",
        "GET DIAGNOSTICS @n = NUMBER",
        "SHOW PROFILE CPU FOR QUERY 15",
        "SHOW PROFILES",
        "SHOW PROCEDURE CODE p",
        "SHOW FUNCTION CODE f",
        "LOAD DATA INFILE 'tmp.txt' IGNORE INTO TABLE t1 FIELDS TERMINATED BY ','",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open utility errors");
    for (size_t index = 0U;
         index < sizeof(unsupported_statements) / sizeof(unsupported_statements[0]);
         ++index) {
        failures += execute_error(
            database,
            unsupported_statements[index],
            (struct expected_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "not supported",
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int expect_utility_noop(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 1U, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
            failures += expect_result_value(
                result,
                row,
                column,
                query.values[(row * query.column_count) + column],
                query.context
            );
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
    return mylite_test_expect_text(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}
