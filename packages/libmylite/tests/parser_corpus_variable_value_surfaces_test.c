#include "parser_test_support.h"

#include <stddef.h>

static int test_set_variable_values(void);
static int test_dml_variable_values(void);
static int test_predicate_variable_values(void);
static int parse_ok(const char *sql);

int main(void) {
    int failures = 0;

    failures += test_set_variable_values();
    failures += test_dml_variable_values();
    failures += test_predicate_variable_values();

    return failures == 0 ? 0 : 1;
}

static int test_set_variable_values(void) {
    static const char *const forms[] = {
        "SET @step3 = @step * 3",
        "SET @unix_time = @unix_time - @unix_time % @step6",
        "SET @now = UNIX_TIMESTAMP()",
        "SET TIMESTAMP = @@TIMESTAMP + 1",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_dml_variable_values(void) {
    static const char *const forms[] = {
        "INSERT INTO t1 VALUES (@A), (@B)",
        "INSERT INTO t1 VALUES (@tzid, @now + 3 * @step, 1)",
        "INSERT INTO t1 VALUES (@@time_zone)",
        "INSERT INTO t1 SET id = @id, name = CONCAT(@prefix, '-x')",
        "REPLACE INTO t1 VALUES (@id + 1, @step * 3)",
        "UPDATE t1 SET i = @next WHERE id = 1",
        "UPDATE t1 SET s = @@time_zone WHERE id = 1",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int test_predicate_variable_values(void) {
    static const char *const forms[] = {
        "SELECT * FROM t1 WHERE a < @maxint",
        "SELECT * FROM t1 WHERE 0 < @maxint",
        "SELECT i8 FROM t1 WHERE i8 IN (@int_one, @int_two, @int_five)",
        "SELECT 1 FROM t1 WHERE a LIKE @pattern",
        "SELECT * FROM t1 WHERE created_at BETWEEN @start_ts AND @@timestamp",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        failures += parse_ok(forms[index]);
    }

    return failures;
}

static int parse_ok(const char *sql) {
    struct mylite_sql_parse_result result;
    int failures = parser_test_parse_sql(sql, MYLITE_SQL_PARSE_OK, &result);

    if (failures != 0) {
        (void)parser_test_expect_true(0, sql);
    }
    mylite_sql_parse_result_deinit(&result);
    return failures;
}
