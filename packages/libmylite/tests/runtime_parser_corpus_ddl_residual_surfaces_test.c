#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    alias_column_count = 1,
    alias_row_count = 8,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_value {
    const char *sql;
    const char *value;
    const char *context;
};

static int test_executable_ddl_aliases(void);
static int test_ddl_residual_runtime(void);
static int test_duplicate_column_defaults(void);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_alias_column_types(mylite_db *database);
static int expect_duplicate_default_metadata(mylite_db *database);
static int expect_duplicate_default_insert_values(mylite_db *database);
static int expect_query_single_value(
    mylite_db *database,
    const struct expected_single_value *expected
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_executable_ddl_aliases();
    failures += test_ddl_residual_runtime();
    failures += test_duplicate_column_defaults();

    return failures == 0 ? 0 : 1;
}

static int test_executable_ddl_aliases(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open alias database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t1(e INT, m INT)");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE type_aliases ("
        "d DOUBLE PRECISION(42,12), r REAL(42,12), f FLOAT(58,0) SIGNED, "
        "fw FLOAT(10.3), "
        "y YEAR UNSIGNED, y4 YEAR(4) UNSIGNED, vb VARCHAR(10) BYTE, lb LONG BYTE)"
    );
    failures += expect_alias_column_types(database);
    failures += execute_statement_ok(database, "CREATE INDEX e_index TYPE btree ON t1(e)");
    failures += execute_statement_ok(database, "CREATE INDEX m_index ON t1(m) TYPE btree");
    failures += execute_statement_ok(database, "ALTER TABLE t1 CHARACTER SET binary");
    failures += execute_statement_ok(
        database,
        "ALTER TABLE t1 CONVERT TO CHARACTER SET DEFAULT COLLATE utf8mb4_bin"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE generated_residual ("
        "pk INT NOT NULL AUTO_INCREMENT, c INT NOT NULL, "
        "g INT GENERATED ALWAYS AS ((c + c)) VIRTUAL NOT NULL, "
        "PRIMARY KEY (pk)) ENGINE=InnoDB AUTO_INCREMENT=30 DEFAULT CHARSET=utf8mb4"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE repeated_nullability (a INT NOT NULL NULL, b INT NULL NOT NULL)"
    );
    failures += expect_query_single_value(
        database,
        &(const struct expected_single_value){
            .sql = "SELECT IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'repeated_nullability' "
                   "AND COLUMN_NAME = 'a'",
            .value = "YES",
            .context = "last repeated NULL makes column nullable",
        }
    );
    failures += expect_query_single_value(
        database,
        &(const struct expected_single_value){
            .sql = "SELECT IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'repeated_nullability' "
                   "AND COLUMN_NAME = 'b'",
            .value = "NO",
            .context = "last repeated NOT NULL makes column not nullable",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_ddl_residual_runtime(void) {
    mylite_db *database = NULL;
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open residual database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE ft_parser (a TEXT)");
    failures += execute_statement_ok(database, "CREATE TABLE parent (a INT PRIMARY KEY)");
    failures += execute_statement_ok(database, "CREATE TABLE reorder_t (payoutid INT, bandid INT)");
    failures += execute_error(
        database,
        "CREATE TABLE ft_parser (a TEXT, FULLTEXT(a) WITH PARSER simple_parser)",
        unsupported
    );
    failures += execute_error(
        database,
        "ALTER TABLE ft_parser ADD FULLTEXT(a) WITH PARSER simple_parser",
        unsupported
    );
    failures += execute_error(
        database,
        "CREATE FULLTEXT INDEX ft_a ON ft_parser(a) WITH PARSER simple_parser",
        unsupported
    );
    failures += execute_error(
        database,
        "CREATE TABLE fk_default ("
        "a INT, FOREIGN KEY (a) REFERENCES parent(a) ON DELETE SET DEFAULT)",
        unsupported
    );
    failures += execute_error(
        database,
        "ALTER TABLE reorder_t ADD COLUMN new_col INT, ORDER BY payoutid,bandid",
        unsupported
    );
    failures += expect_query_single_value(
        database,
        &(const struct expected_single_value){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'reorder_t' AND COLUMN_NAME = 'new_col'",
            .value = "0",
            .context = "alter order placeholder does not add column",
        }
    );
    failures +=
        execute_error(database, "CREATE TABLE binlog_tail (f1 INT) START TRANSACTION", unsupported);
    failures += expect_query_single_value(
        database,
        &(const struct expected_single_value){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'binlog_tail'",
            .value = "0",
            .context = "create table start transaction placeholder does not create table",
        }
    );
    failures += execute_error(database, "ALTER TABLE reorder_t REORGANIZE PARTITION", unsupported);

    mylite_close(database);
    return failures;
}

static int test_duplicate_column_defaults(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open defaults database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE duplicate_defaults ("
        "a INT DEFAULT 1 DEFAULT 2, b INT DEFAULT NULL DEFAULT 5, "
        "c INT DEFAULT 6 DEFAULT NULL, d CHAR(4) DEFAULT 'a' DEFAULT 'b')"
    );
    failures += expect_duplicate_default_metadata(database);
    failures += execute_statement_ok(database, "INSERT INTO duplicate_defaults () VALUES ()");
    failures += expect_duplicate_default_insert_values(database);

    mylite_close(database);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "diagnostic: %d %s %s\n",
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement rows");
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

static int expect_alias_column_types(mylite_db *database) {
    static const char *const expected[] = {
        "double(42,12)",
        "double(42,12)",
        "float(58,0)",
        "float",
        "year",
        "year",
        "varbinary(10)",
        "mediumblob",
    };
    static const char query[] = "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                                "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'type_aliases' "
                                "ORDER BY ORDINAL_POSITION";
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query, sizeof(query) - 1U, &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, "alias column type query");
    failures +=
        expect_size(mylite_result_column_count(result), alias_column_count, "alias columns");
    failures += expect_size(mylite_result_row_count(result), alias_row_count, "alias rows");
    for (size_t row = 0U; row < sizeof(expected) / sizeof(expected[0]); ++row) {
        failures += expect_result_value(result, row, 0U, expected[row], "alias column type");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_duplicate_default_metadata(mylite_db *database) {
    static const char *const expected_names[] = {"a", "b", "c", "d"};
    static const char *const expected_defaults[] = {"2", "5", NULL, "b"};
    static const char query[] =
        "SELECT COLUMN_NAME, COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'duplicate_defaults' "
        "ORDER BY ORDINAL_POSITION";
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query, sizeof(query) - 1U, &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, "duplicate default metadata query");
    failures += expect_size(mylite_result_column_count(result), 2U, "duplicate default columns");
    failures += expect_size(mylite_result_row_count(result), 4U, "duplicate default rows");
    for (size_t row = 0U; row < sizeof(expected_names) / sizeof(expected_names[0]); ++row) {
        failures +=
            expect_result_value(result, row, 0U, expected_names[row], "duplicate default name");
        failures +=
            expect_result_value(result, row, 1U, expected_defaults[row], "duplicate default value");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_duplicate_default_insert_values(mylite_db *database) {
    return expect_query_single_value(
        database,
        &(const struct expected_single_value){
            .sql = "SELECT CONCAT(IFNULL(a,'N'), ':', IFNULL(b,'N'), ':', IFNULL(c,'N'), ':', d) "
                   "FROM duplicate_defaults",
            .value = "2:5:N:b",
            .context = "duplicate default inserted row",
        }
    );
}

static int expect_query_single_value(
    mylite_db *database,
    const struct expected_single_value *expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected->sql, strlen(expected->sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, expected->context);
    failures += expect_size(mylite_result_column_count(result), 1U, expected->context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected->context);
    failures += expect_result_value(result, 0U, 0U, expected->value, expected->context);
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
    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
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
