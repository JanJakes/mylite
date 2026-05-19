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
    path_suffix_capacity = 16,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_no_source_dual_and_do_greatest_least(void);
static int test_table_backed_greatest_least(void);
static int test_greatest_least_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_greatest_least();
    failures += test_table_backed_greatest_least();
    failures += test_greatest_least_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_greatest_least(void) {
    static const char *const columns_no_source[] = {
        "GREATEST(2, 0)",
        "LEAST(2, 0)",
        "GREATEST(-2, +3, 1)",
        "LEAST(TRUE, FALSE, 2)",
        "GREATEST(NULL, 1)",
        "LEAST(1, NULL)",
        "GREATEST('a', 'A')",
        "LEAST('a', 'A')",
        "GREATEST('', 'a')",
        "LEAST('a', '')",
        "GREATEST(-9223372036854775808, 9223372036854775807)",
        "LEAST(-9223372036854775808, 9223372036854775807)",
        "@@warning_count",
    };
    static const char *const values_no_source[] = {
        "2",
        "0",
        "3",
        "0",
        NULL,
        NULL,
        "A",
        "a",
        "a",
        "",
        "9223372036854775807",
        "-9223372036854775808",
        "0",
    };
    static const char *const columns_dual[] = {"GREATEST (2,1)", "least_alias"};
    static const char *const values_dual[] = {"2", "a"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GREATEST(2, 0), LEAST(2, 0), GREATEST(-2, +3, 1), "
                   "LEAST(TRUE, FALSE, 2), GREATEST(NULL, 1), LEAST(1, NULL), "
                   "GREATEST('a', 'A'), LEAST('a', 'A'), GREATEST('', 'a'), "
                   "LEAST('a', ''), "
                   "GREATEST(-9223372036854775808, 9223372036854775807), "
                   "LEAST(-9223372036854775808, 9223372036854775807), @@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source greatest least",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GREATEST (2,1), LEAST('b','a') AS least_alias FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual greatest least",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after greatest least select",
        }
    );

    failures += execute_ok(database, "DO GREATEST('b','a'), LEAST(3,1), GREATEST(NULL,1)", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "greatest least do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "greatest least do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "greatest least do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "greatest least do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after greatest least do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_greatest_least(void) {
    static const char *const columns_table[] =
        {"id", "g_n", "l_n", "g_code", "l_code", "g_c", "l_body"};
    static const char *const values_table[] = {
        "1", "3",  "2",  "m",  "alpha", "m",  "first", "2", "5",  "3", "m",     "Beta", "zz", "m",
        "3", NULL, NULL, NULL, NULL,    NULL, NULL,    "4", "20", "3", "Other", "m",    "m",  "m",
    };
    static const char *const columns_limited[] = {"id", "g_code", "l_n"};
    static const char *const values_limited[] = {"4", "Other", "4", "2", "m", "4"};
    static const char *const columns_ties[] = {"id", "g_tie", "l_tie"};
    static const char *const values_ties[] = {"1", "A", "a", "2", "b", "B"};
    static const char *const columns_integer_family[] = {
        "g_int",
        "l_integer",
        "g_big",
        "l_big_min",
        "l_unsigned",
        "g_unsigned_big",
    };
    static const char *const values_integer_family[] = {
        "2",
        "2",
        "9223372036854775807",
        "-9223372036854775808",
        "4",
        "9223372036854775807",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, code VARCHAR(20), n INT, c CHAR(4), body TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'alpha', 2, 'Aa', 'first'), "
        "(2, 'Beta', 5, 'zz', 'second'), "
        "(3, NULL, NULL, NULL, NULL), "
        "(4, 'Other', 20, 'aa', 'third')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, GREATEST(n, 3) AS g_n, LEAST(n, 3) AS l_n, "
                   "GREATEST(code, 'm') AS g_code, LEAST(code, 'm') AS l_code, "
                   "GREATEST(c, 'm') AS g_c, LEAST(body, 'm') AS l_body "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table greatest least projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, GREATEST(code, 'm') AS g_code, LEAST(n, 4) AS l_n "
                   "FROM t WHERE id <> 3 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table greatest least where order limit",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE ties(id INT, left_s VARCHAR(5), right_s VARCHAR(5))",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO ties VALUES (1, 'a', 'A'), (2, 'B', 'b')", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, GREATEST(left_s, right_s) AS g_tie, "
                   "LEAST(left_s, right_s) AS l_tie FROM ties ORDER BY id",
            .columns = columns_ties,
            .column_count = sizeof(columns_ties) / sizeof(columns_ties[0]),
            .values = values_ties,
            .row_count = 2U,
            .context = "table greatest least string ties",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE nums(i INT, j INTEGER, b BIGINT, b_min BIGINT, "
        "u INT UNSIGNED, ub BIGINT UNSIGNED)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO nums VALUES "
        "(1, 2, 9223372036854775807, -9223372036854775808, 4, 9223372036854775807)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GREATEST(i, 2) AS g_int, LEAST(j, 3) AS l_integer, "
                   "GREATEST(b, 0) AS g_big, LEAST(b_min, 0) AS l_big_min, "
                   "LEAST(u, 10) AS l_unsigned, GREATEST(ub, 0) AS g_unsigned_big "
                   "FROM nums",
            .columns = columns_integer_family,
            .column_count = sizeof(columns_integer_family) / sizeof(columns_integer_family[0]),
            .values = values_integer_family,
            .row_count = 1U,
            .context = "table greatest least integer families",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_greatest_least_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), n INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1)", NULL);
    failures += execute_error(
        database,
        "SELECT GREATEST()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'GREATEST'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEAST('x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LEAST'",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(v, n) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() do not support mixed string and numeric arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEAST('x', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() do not support mixed string and numeric arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(v + 1, 2) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(9223372036854775808, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEAST('\xC3\xA9', 'e')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GREATEST() and LEAST() string literals support only ASCII values",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(1.5, 1.25)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(CAST('a' AS BINARY), 'a')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST((SELECT 1), 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT GREATEST(GREATEST(1, 2), 3)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONCAT(GREATEST(n, 2), 'x') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT GREATEST() and LEAST() are supported only as top-level",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(TRUE, GREATEST(n, 2), 0) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "row-scalar SELECT GREATEST() and LEAST() are supported only as top-level",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM t WHERE GREATEST(n, 2) = 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT n FROM t ORDER BY GREATEST(n, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE t SET n = GREATEST(n, 2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

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

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-greatest-least-functions-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
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

    return expect_text(actual, expected, context);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
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
