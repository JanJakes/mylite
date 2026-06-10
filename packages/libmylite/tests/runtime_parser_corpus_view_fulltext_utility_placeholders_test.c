#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
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

static int test_unsupported_placeholder_diagnostics(void);
static int test_noop_placeholder_warning_surface(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_noop(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_unsupported_placeholder_diagnostics();
    failures += test_noop_placeholder_warning_surface();

    return failures == 0 ? 0 : 1;
}

static int test_unsupported_placeholder_diagnostics(void) {
    static const char *const unsupported_statements[] = {
        "CREATE VIEW v AS SELECT LPAD('x', 1 NOT IN (0), 1) AS c",
        "SELECT * FROM articles WHERE MATCH title, body AGAINST ('needle' IN BOOLEAN MODE)",
        "SELECT * FROM articles WHERE MATCH name AGAINST ('needle' IN BOOLEAN MODE)",
        "UPDATE articles SET id = 2 WHERE MATCH title AGAINST ('needle')",
        "DELETE FROM articles WHERE MATCH title AGAINST ('needle')",
        "SELECT * INTO OUTFILE 'tmp1.txt' FROM articles",
        "SELECT title INTO DUMPFILE 'tmp1.bin' FROM articles LIMIT 1",
        "LOAD XML INFILE 'tmp.xml' INTO TABLE articles ROWS IDENTIFIED BY '<row>'",
        "IMPORT TABLE FROM 'articles_*.sdi'",
        "ALTER TABLE articles DISCARD TABLESPACE",
        "ALTER TABLE articles DISCARD PARTITION p0 TABLESPACE",
        "ALTER TABLE articles IMPORT PARTITION p0 TABLESPACE",
        "HELP no_such_topic",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures +=
        execute_ok(database, "CREATE TABLE articles (id INT, name TEXT, title TEXT, body TEXT)");

    for (size_t index = 0U;
         index < sizeof(unsupported_statements) / sizeof(unsupported_statements[0]);
         ++index) {
        failures += execute_error(
            database,
            unsupported_statements[index],
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "utility statement is not supported",
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int test_noop_placeholder_warning_surface(void) {
    static const char *const noop_statements[] = {
        "LOCK INSTANCE FOR BACKUP",
        "UNLOCK INSTANCE",
        ("CHANGE REPLICATION SOURCE TO SOURCE_USER='plug_user', SOURCE_PASSWORD='plug_user', "
         "SOURCE_RETRY_COUNT=0"),
        "CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu' ENGINE InnoDB",
        "ALTER UNDO TABLESPACE undo_003 SET ACTIVE ENGINE InnoDB",
        "DROP UNDO TABLESPACE undo_003 ENGINE InnoDB",
    };
    static const char *const row_count_rows[] = {"0"};
    static const char *const count_rows[] = {"0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT)");
    for (size_t index = 0U; index < sizeof(noop_statements) / sizeof(noop_statements[0]); ++index) {
        failures += expect_noop(database, noop_statements[index]);
    }
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "no-op row count",
        }
    );

    failures += execute_ok(database, "START TRANSACTION");
    failures += execute_ok(database, "INSERT INTO t VALUES (1)");
    failures += expect_noop(database, "LOCK INSTANCE FOR BACKUP");
    failures += execute_ok(database, "ROLLBACK");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "no-op preserves transaction",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "failed result columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_noop(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
    failures += expect_size(mylite_result_warning_count(result), 1U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0 && query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
