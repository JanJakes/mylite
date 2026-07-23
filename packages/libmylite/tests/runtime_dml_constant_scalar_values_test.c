#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    scalar_row_column_count = 9,
    scalar_auto_increment_column_count = 2,
    mysql_error_truncated_wrong_value = 1366,
};

struct expected_dml_result {
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

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_constant_scalar_insert_replace_update(void);
static int test_constant_scalar_auto_increment_and_diagnostics(void);
static int open_app_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_constant_scalar_insert_replace_update();
    failures += test_constant_scalar_auto_increment_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_constant_scalar_insert_replace_update(void) {
    static const char *const inserted_rows[] = {
        "1",
        "abb",
        "ABC",
        "42",
        "12.34",
        "25",
        "{\"a\": 1}",
        "2024-05-06 07:08:09",
        "01:01:01",
    };
    static const char *const duplicate_rows[] = {
        "1",
        "abbZ",
        "ABC",
        "7",
        "7.89",
        "25",
        "{\"a\": 1}",
        "2024-05-06 07:08:09",
        "01:01:01",
    };
    static const char *const replace_rows[] = {
        "1",
        "abbZ",
        "ABC",
        "7",
        "7.89",
        "25",
        "{\"a\": 1}",
        "2024-05-06 07:08:09",
        "01:01:01",
        "2",
        "AB",
        "xy",
        "5",
        "1.50",
        "2",
        "{\"b\": 2}",
        "2024-01-02 00:00:00",
        "00:00:59",
    };
    static const char *const updated_rows[] = {
        "2",
        "ABQ",
        "DE",
        "8",
        "1.50",
        "2",
        "{\"b\": 2}",
        "2024-05-07 08:09:10",
        "00:00:59",
    };
    static const char *const row_scalar_update_rows[] = {
        "2",
        "abq-8",
        "20",
        "2024-05-08 08:09:10",
    };
    static const char *const row_scalar_duplicate_rows[] = {
        "2",
        "abq-8-20",
        "40",
        "2024-05-08 08:09:10",
    };
    static const char *const string_literal_rows[] = {"3", "Cote d'Ivoire", "ab"};
    static const char *const values_literal_rows[] = {"ab", "c"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE scalars("
        "id INT PRIMARY KEY, "
        "v VARCHAR(16), "
        "b VARBINARY(8), "
        "i INT, "
        "d DECIMAL(5,2), "
        "f DOUBLE, "
        "js JSON, "
        "dt DATETIME, "
        "tm TIME"
        ")"
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO scalars(id, v, b, i, d, f, js, dt, tm) VALUES ("
        "1, "
        "CONCAT(_utf8mb4'a', REPEAT('b', 2)), "
        "CONVERT('ABC' USING BINARY), "
        "CONVERT('42' USING utf8mb4), "
        "CONVERT('12.34' USING utf8mb4), "
        "CONVERT('2.5e1' USING utf8mb4), "
        "CONVERT('{\"a\":1}' USING utf8mb4), "
        "STR_TO_DATE('2024-05-06 07:08:09', '%Y-%m-%d %H:%i:%s'), "
        "SEC_TO_TIME(3661)"
        ")",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, b, i, d, f, js, dt, tm FROM scalars WHERE id = 1",
            .values = inserted_rows,
            .column_count = scalar_row_column_count,
            .row_count = 1U,
            .context = "insert scalar row",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO scalars(id, v, i, d) VALUES (1, 'ignored', 0, 0) "
        "ON DUPLICATE KEY UPDATE "
        "v = CONCAT('abb', 'Z'), "
        "i = CONVERT('7' USING utf8mb4), "
        "d = CONVERT('7.89' USING utf8mb4)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, b, i, d, f, js, dt, tm FROM scalars WHERE id = 1",
            .values = duplicate_rows,
            .column_count = scalar_row_column_count,
            .row_count = 1U,
            .context = "duplicate scalar row",
        }
    );
    failures += expect_dml_result(
        database,
        "REPLACE INTO scalars(id, v, b, i, d, f, js, dt, tm) VALUES ("
        "2, _latin1 0x4142, _utf8mb4'xy', "
        "CONVERT('5' USING utf8mb4), "
        "CONVERT('1.50' USING utf8mb4), "
        "CONVERT('2' USING utf8mb4), "
        "CONVERT('{\"b\":2}' USING utf8mb4), "
        "DATE '2024-01-02', "
        "SEC_TO_TIME(59)"
        ")",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, b, i, d, f, js, dt, tm FROM scalars ORDER BY id",
            .values = replace_rows,
            .column_count = scalar_row_column_count,
            .row_count = 2U,
            .context = "replace scalar rows",
        }
    );
    failures += expect_dml_result(
        database,
        "UPDATE scalars SET "
        "v = CONCAT('AB', 'Q'), "
        "b = CONVERT('DE' USING BINARY), "
        "i = CONVERT('8' USING utf8mb4), "
        "dt = STR_TO_DATE('2024-05-07 08:09:10', '%Y-%m-%d %H:%i:%s'), "
        "tm = SEC_TO_TIME(59) "
        "WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, b, i, d, f, js, dt, tm FROM scalars WHERE id = 2",
            .values = updated_rows,
            .column_count = scalar_row_column_count,
            .row_count = 1U,
            .context = "update scalar row",
        }
    );
    failures += expect_dml_result(
        database,
        "UPDATE scalars SET "
        "v = CONCAT(LOWER(v), '-', COALESCE(i, 0)), "
        "i = GREATEST(i, 20), "
        "dt = DATE_ADD(dt, INTERVAL 1 DAY) "
        "WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, i, dt FROM scalars WHERE id = 2",
            .values = row_scalar_update_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "row-dependent update scalar row",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO scalars(id, v, i, d) VALUES (2, 'ignored', 0, 0) "
        "ON DUPLICATE KEY UPDATE "
        "v = CONCAT(LOWER(v), '-', COALESCE(i, 0)), "
        "i = GREATEST(i, 40)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, i, dt FROM scalars WHERE id = 2",
            .values = row_scalar_duplicate_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "row-dependent duplicate scalar row",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO scalars(id, v, b, i, d, f, js, dt, tm) VALUES ("
        "3, 'ab' 'cd', N'xy', 0, 0, 0, NULL, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE scalars SET v = N'Cote d\\'Ivoire', b = 'a' 'b' WHERE id = 3",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, b FROM scalars WHERE id = 3",
            .values = string_literal_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "adjacent and national string DML values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "VALUES ROW('a' 'b', N'c')",
            .values = values_literal_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "adjacent and national VALUES literals",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_constant_scalar_auto_increment_and_diagnostics(void) {
    static const char *const auto_increment_rows[] = {"1", "10", "5", "20", "6", "30"};
    static const char *const ignore_rows[] = {"7", "bad", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_generated("
        "id INT AUTO_INCREMENT PRIMARY KEY, v INT, label VARCHAR(16))"
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO auto_generated(id, v) VALUES (CONVERT(NULL USING utf8mb4), 10)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO auto_generated(id, v) VALUES (CONVERT('5' USING utf8mb4), 20)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO auto_generated(v) VALUES (30)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_generated ORDER BY id",
            .values = auto_increment_rows,
            .column_count = scalar_auto_increment_column_count,
            .row_count = 3U,
            .context = "scalar auto increment rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO auto_generated(label, v) VALUES "
        "(CONVERT('bad' USING utf8mb4), CONVERT('abc' USING utf8mb4))",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO auto_generated(label, v) VALUES "
        "(CONVERT('bad' USING utf8mb4), CONVERT('abc' USING utf8mb4))",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, label, v FROM auto_generated WHERE label = 'bad'",
            .values = ignore_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "scalar ignore warning row",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_app_database(mylite_db **out_database) {
    int rc = mylite_test_open_temporary(out_database);

    if (rc != MYLITE_OK) {
        return mylite_test_expect_int(rc, MYLITE_OK, "open temporary database");
    }
    return expect_statement_ok(*out_database, "CREATE DATABASE app") +
           expect_statement_ok(*out_database, "USE app");
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected SQL to succeed: %s\nerror %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected SQL to fail: %s\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "affected rows"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "warning count"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t expected_value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        for (size_t index = 0U; index < expected_value_count; ++index) {
            failures += expect_result_value(
                result,
                index / query.column_count,
                index % query.column_count,
                query.values[index],
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu/%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return mylite_test_expect_text(actual, expected, context);
}
