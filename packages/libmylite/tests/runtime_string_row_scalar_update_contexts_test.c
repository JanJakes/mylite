#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_string_row_scalar_update_contexts(void);
static int test_string_row_scalar_update_does_not_widen_joined_update(void);
static int open_string_update_context_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_string_row_scalar_update_contexts();
    failures += test_string_row_scalar_update_does_not_widen_joined_update();

    return failures == 0 ? 0 : 1;
}

static int test_string_row_scalar_update_contexts(void) {
    static const char *const columns[] = {
        "id",
        "out_concat",
        "out_concat_ws",
        "out_field",
        "out_hex",
        "out_to_base64",
        "out_from_base64",
        "out_left",
        "out_right",
        "out_substring",
        "out_lpad",
        "out_rpad",
        "out_repeat",
        "out_space",
        "out_locate",
        "out_instr",
        "out_replace",
        "out_insert",
        "out_substring_index",
        "out_find_in_set",
        "out_strcmp",
        "out_regexp_like",
        "out_regexp_instr",
        "out_regexp_substr",
        "out_regexp_replace",
        "out_export_set",
        "out_make_set",
    };
    static const char *const values[] = {
        "1",
        "Alpha Beta-2",
        "Alpha Beta:Beta:2",
        "1",
        "416C7068612042657461",
        "QWxwaGEgQmV0YQ==",
        "Alpha",
        "Al",
        "ta",
        "lp",
        "..Beta",
        "Beta..",
        "BetaBeta",
        "[  ]",
        "7",
        "7",
        "Alpha X",
        "AZha Beta",
        "Alpha Beta",
        "2",
        "-1",
        "0",
        "1",
        "Alpha",
        "_lph_ B_t_",
        "N,Y,N,N",
        "b",
        "2",
        "One,Two,Three-3",
        "One,Two,Three:Two:3",
        "2",
        "4F6E652C54776F2C5468726565",
        "T25lLFR3byxUaHJlZQ==",
        "Two",
        "One",
        "ree",
        "ne,",
        "...Two",
        "Two...",
        "TwoTwoTwo",
        "[   ]",
        "5",
        "5",
        "One,X,Three",
        "OZTwo,Three",
        "One,Two",
        "2",
        "-1",
        "1",
        "1",
        "One",
        "_n_,Tw_,Thr__",
        "Y,Y,N,N",
        "a,b",
        "3",
        NULL,
        "",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const status_values[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_string_update_context_database(&database, "assignment", path, sizeof(path));
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_concat = CONCAT(s, '-', n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_concat_ws = CONCAT_WS(':', s, needle, n)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_field = FIELD(needle, 'Beta', 'Two', 'Nope')",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_hex = HEX(s)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_to_base64 = TO_BASE64(s)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_from_base64 = FROM_BASE64(b64)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_left = LEFT(s, n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_right = RIGHT(s, n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_substring = SUBSTRING(s, 2, n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_lpad = LPAD(needle, 6, '.')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_rpad = RPAD(needle, 6, '.')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_repeat = REPEAT(needle, n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_space = SPACE(n)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_locate = LOCATE(needle, s)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_instr = INSTR(s, needle)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_replace = REPLACE(s, needle, 'X')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_insert = INSERT(s, 2, n, 'Z')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_substring_index = SUBSTRING_INDEX(s, ',', 2)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_find_in_set = FIND_IN_SET(needle, csv)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_strcmp = STRCMP(s, needle)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_regexp_like = REGEXP_LIKE(s, '^One')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_regexp_instr = REGEXP_INSTR(s, '[A-Z][a-z]+')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_regexp_substr = REGEXP_SUBSTR(s, '[A-Z][a-z]+')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_regexp_replace = REGEXP_REPLACE(s, '[aeiou]', '_')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_export_set = EXPORT_SET(n, 'Y', 'N', ',', 4)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_make_set = MAKE_SET(n, 'a', 'b', 'c')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, out_concat, out_concat_ws, out_field, out_hex, "
                   "out_to_base64, out_from_base64, out_left, out_right, out_substring, "
                   "out_lpad, out_rpad, out_repeat, CONCAT('[', out_space, ']') AS out_space, "
                   "out_locate, out_instr, out_replace, out_insert, out_substring_index, "
                   "out_find_in_set, out_strcmp, out_regexp_like, out_regexp_instr, "
                   "out_regexp_substr, out_regexp_replace, out_export_set, out_make_set "
                   "FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 3U,
            .context = "string row-scalar update assignments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = "string row-scalar status after select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_string_row_scalar_update_does_not_widen_joined_update(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_string_update_context_database(&database, "joined", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE joined_source(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO joined_source VALUES (1)", NULL);
    failures += execute_error(
        database,
        "UPDATE t JOIN joined_source ON t.id = joined_source.id "
        "SET t.out_concat = CONCAT(t.s, '-', t.n)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined UPDATE supports only constant assignment values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_string_update_context_database(
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
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            *out_database,
            "CREATE TABLE t("
            "id INT, s VARCHAR(64), needle VARCHAR(32), csv VARCHAR(64), "
            "b64 VARCHAR(64), n INT, "
            "out_concat VARCHAR(128), out_concat_ws VARCHAR(128), out_field INT, "
            "out_hex VARCHAR(128), out_to_base64 VARCHAR(128), out_from_base64 VARCHAR(128), "
            "out_left VARCHAR(128), out_right VARCHAR(128), out_substring VARCHAR(128), "
            "out_lpad VARCHAR(128), out_rpad VARCHAR(128), out_repeat VARCHAR(128), "
            "out_space VARCHAR(128), out_locate INT, out_instr INT, out_replace VARCHAR(128), "
            "out_insert VARCHAR(128), out_substring_index VARCHAR(128), out_find_in_set INT, "
            "out_strcmp INT, out_regexp_like INT, out_regexp_instr INT, "
            "out_regexp_substr VARCHAR(128), out_regexp_replace VARCHAR(128), "
            "out_export_set VARCHAR(128), out_make_set VARCHAR(128))",
            (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
        );
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            *out_database,
            "INSERT INTO t(id, s, needle, csv, b64, n) VALUES "
            "(1, 'Alpha Beta', 'Beta', 'Alpha,Beta,Gamma', 'QWxwaGE=', 2), "
            "(2, 'One,Two,Three', 'Two', 'One,Two,Three', 'VHdv', 3), "
            "(3, NULL, NULL, NULL, NULL, NULL)",
            (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
        );
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-string-row-scalar-update-contexts-%s-%d.mylite",
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char full_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(full_path, sizeof(full_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(full_path)) {
        (void)remove(full_path);
    }
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &local);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got %d: %s\n",
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(local);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = local;
    } else {
        mylite_result_free(local);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "dml affected rows"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "dml warnings"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
