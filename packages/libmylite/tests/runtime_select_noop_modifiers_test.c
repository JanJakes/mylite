#include <mylite/mylite.h>

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
    warning_code_text_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_key_does_not_exist = 1176,
    mysql_error_wrong_usage = 1221,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char sql_no_cache_warning_message[] =
    "'SQL_NO_CACHE' is deprecated and will be removed in a future release.";
static const char found_rows_warning_message[] =
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) "
    "instead.";
static const char sql_calc_warning_message[] =
    "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider using "
    "two separate queries instead.";

static int test_scalar_noop_modifiers(void);
static int test_table_noop_modifiers(void);
static int test_source_select_noop_modifiers(void);
static int test_select_index_hints_noop(void);
static int test_unsupported_modifier_forms(void);
static int prepare_fixture(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t warning_count,
    const char *context
);
static int expect_grid(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t column_count,
    size_t warning_count,
    const char *context
);
static int expect_empty_statement(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context,
    size_t warning_count
);
static int expect_warning_rows(
    mylite_db *database,
    const int *codes,
    const char *const *messages,
    size_t row_count,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_scalar_noop_modifiers();
    failures += test_table_noop_modifiers();
    failures += test_source_select_noop_modifiers();
    failures += test_select_index_hints_noop();
    failures += test_unsupported_modifier_forms();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_noop_modifiers(void) {
    static const char *const noop_values[] = {"7"};
    static const char *const found_rows_values[] = {"1"};
    static const int sql_no_cache_codes[] = {1681};
    static const char *const sql_no_cache_messages[] = {sql_no_cache_warning_message};
    static const int ordered_warning_codes[] = {1681, 1287};
    static const char *const found_rows_warning_messages[] = {
        sql_no_cache_warning_message,
        found_rows_warning_message,
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar modifiers");

    failures += execute_ok(
        database,
        "SELECT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT "
        "SQL_BUFFER_RESULT 7",
        &result
    );
    failures += expect_rows(result, noop_values, 1U, 0U, "scalar no-op modifier row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT SQL_NO_CACHE 7", &result);
    failures += expect_rows(result, noop_values, 1U, 1U, "scalar sql_no_cache row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        sql_no_cache_codes,
        sql_no_cache_messages,
        1U,
        "scalar sql_no_cache warning"
    );

    failures += execute_ok(database, "SELECT SQL_NO_CACHE FOUND_ROWS()", &result);
    failures += expect_rows(result, found_rows_values, 1U, 2U, "sql_no_cache found_rows row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        ordered_warning_codes,
        found_rows_warning_messages,
        2U,
        "sql_no_cache before found_rows warning"
    );

    mylite_close(database);
    return failures;
}

static int test_table_noop_modifiers(void) {
    static const char *const limited_rows[] = {"2", "3"};
    static const char *const distinct_rows[] = {"20", "30"};
    static const char *const count_rows[] = {"4"};
    static const char *const calc_rows[] = {"1"};
    static const char *const grouped_values[] = {"20", "2", "30", "1"};
    static const int ordered_warning_codes[] = {1681, 1287};
    static const char *const calc_warning_messages[] = {
        sql_no_cache_warning_message,
        sql_calc_warning_message,
    };
    static const int sql_no_cache_codes[] = {1681};
    static const char *const sql_no_cache_messages[] = {sql_no_cache_warning_message};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open table modifiers");
    failures += prepare_fixture(database);

    failures += execute_ok(
        database,
        "SELECT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT "
        "id FROM t WHERE n IS NOT NULL ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_rows(result, limited_rows, 2U, 0U, "table no-op modifier rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT DISTINCT SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT n FROM t "
        "WHERE n IS NOT NULL ORDER BY n",
        &result
    );
    failures += expect_rows(result, distinct_rows, 2U, 0U, "distinct no-op modifier rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT SQL_NO_CACHE COUNT(*) FROM t", &result);
    failures += expect_rows(result, count_rows, 1U, 1U, "count sql_no_cache row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        sql_no_cache_codes,
        sql_no_cache_messages,
        1U,
        "count sql_no_cache warning"
    );

    failures += execute_ok(
        database,
        "SELECT SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT n, COUNT(*) FROM t "
        "WHERE n IS NOT NULL GROUP BY n ORDER BY n",
        &result
    );
    failures += expect_grid(result, grouped_values, 2U, 2U, 0U, "grouped no-op modifier rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT SQL_NO_CACHE SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1",
        &result
    );
    failures += expect_rows(result, calc_rows, 1U, 2U, "sql_no_cache sql_calc row");
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        ordered_warning_codes,
        calc_warning_messages,
        2U,
        "sql_no_cache before sql_calc warning"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_source_select_noop_modifiers(void) {
    static const int sql_no_cache_codes[] = {1681};
    static const char *const sql_no_cache_messages[] = {sql_no_cache_warning_message};
    static const char *const copied_rows[] = {"1", "2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "source") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open source modifiers");
    failures += prepare_fixture(database);

    failures += execute_ok(
        database,
        "CREATE TABLE copied AS SELECT SQL_NO_CACHE id FROM t ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_empty_statement(result, 2, "ctas sql_no_cache result", 1U);
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        sql_no_cache_codes,
        sql_no_cache_messages,
        1U,
        "ctas sql_no_cache warning"
    );

    failures += execute_ok(database, "SELECT id FROM copied ORDER BY id", &result);
    failures += expect_rows(result, copied_rows, 2U, 0U, "ctas copied rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE TABLE inserted (id INT NOT NULL)");
    failures += execute_ok(
        database,
        "INSERT INTO inserted (id) SELECT SQL_NO_CACHE id FROM t ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_empty_statement(result, 2, "insert select sql_no_cache result", 1U);
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        sql_no_cache_codes,
        sql_no_cache_messages,
        1U,
        "insert select sql_no_cache warning"
    );

    failures += execute_ok(database, "SELECT id FROM inserted ORDER BY id", &result);
    failures += expect_rows(result, copied_rows, 2U, 0U, "insert select copied rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE TABLE replaced (id INT NOT NULL)");
    failures += execute_ok(
        database,
        "REPLACE INTO replaced (id) SELECT SQL_NO_CACHE id FROM t ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_empty_statement(result, 2, "replace select sql_no_cache result", 1U);
    mylite_result_free(result);
    result = NULL;
    failures += expect_warning_rows(
        database,
        sql_no_cache_codes,
        sql_no_cache_messages,
        1U,
        "replace select sql_no_cache warning"
    );

    failures += execute_ok(database, "SELECT id FROM replaced ORDER BY id", &result);
    failures += expect_rows(result, copied_rows, 2U, 0U, "replace select copied rows");
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_select_index_hints_noop(void) {
    static const char *const use_rows[] = {"2", "3"};
    static const char *const primary_rows[] = {"1", "2"};
    static const char *const grouped_values[] = {"20", "2", "30", "1"};
    static const char *const join_values[] = {"2", "7", "3", "7"};
    static const char *const status_values[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "index-hints") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open index hints");
    failures += prepare_fixture(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE r (id INT NOT NULL, n INT NULL, PRIMARY KEY(id), KEY r_n (n))"
    );
    failures += execute_statement_ok(database, "INSERT INTO r VALUES (7, 20), (8, 30), (9, 40)");

    failures +=
        execute_ok(database, "SELECT id FROM t USE INDEX (k_n) WHERE n = 20 ORDER BY id", &result);
    failures += expect_rows(result, use_rows, 2U, 0U, "use index no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t USE INDEX (zet) WHERE n = 20 ORDER BY id", &result);
    failures += expect_rows(result, use_rows, 2U, 0U, "unambiguous prefix index no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT ROW_COUNT(), @@warning_count", &result);
    failures += expect_grid(result, status_values, 1U, 2U, 0U, "use index select status");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t USE KEY () WHERE n = 20 ORDER BY id", &result);
    failures += expect_rows(result, use_rows, 2U, 0U, "empty use key no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT id FROM t FORCE KEY FOR ORDER BY (PRIMARY) ORDER BY id LIMIT 2",
        &result
    );
    failures += expect_rows(result, primary_rows, 2U, 0U, "force primary no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM t USE KEY (PRI) WHERE id = 1", &result);
    failures += expect_rows(result, primary_rows, 1U, 0U, "primary prefix no-op row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT n, COUNT(*) FROM t USE INDEX FOR GROUP BY (k_n) "
        "WHERE n IS NOT NULL GROUP BY n ORDER BY n",
        &result
    );
    failures += expect_grid(result, grouped_values, 2U, 2U, 0U, "group index no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT x.id, y.id FROM t AS x USE INDEX (k_n) "
        "JOIN r AS y FORCE KEY FOR JOIN (r_n) ON x.n = y.n "
        "WHERE x.n = 20 ORDER BY x.id",
        &result
    );
    failures += expect_grid(result, join_values, 2U, 2U, 0U, "join index hints no-op rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT id FROM t USE INDEX (missing) WHERE n = 20",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'missing' doesn't exist in table 't'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t USE INDEX (k) WHERE n = 20",
        (struct expected_sql_error){
            .code = mysql_error_key_does_not_exist,
            .sqlstate = "42000",
            .message_part = "Key 'k' doesn't exist in table 't'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t USE INDEX FOR JOIN (k_n) FORCE INDEX FOR ORDER BY (PRIMARY) "
        "WHERE n = 20 ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of USE INDEX and FORCE INDEX",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t FORCE INDEX () WHERE n = 20",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t IGNORE INDEX () WHERE n = 20",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM t USE INDEX(k_n) WHERE n = 999",
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

static int test_unsupported_modifier_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported modifiers");
    failures += prepare_fixture(database);

    failures += execute_error(
        database,
        "SELECT SQL_NO_CACHE SQL_BUFFER_RESULT id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SQL_CACHE id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'SQL_CACHE' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT HIGH_PRIORITY HIGH_PRIORITY id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT SQL_CALC_FOUND_ROWS 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL_CALC_FOUND_ROWS supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT SQL_CALC_FOUND_ROWS id FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL_CALC_FOUND_ROWS supports only",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT id FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL_CALC_FOUND_ROWS supports only non-distinct",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE calc_copy AS SELECT SQL_CALC_FOUND_ROWS id FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE TABLE ... SELECT does not support SQL_CALC_FOUND_ROWS",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int prepare_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE t (id INT NOT NULL, n INT NULL, PRIMARY KEY(id), "
        "KEY k_n (n), KEY k_n_copy (n), KEY zeta_n (n))"
    );
    failures +=
        execute_statement_ok(database, "INSERT INTO t VALUES (1, NULL), (2, 20), (3, 20), (4, 30)");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': %s (%s)\n",
            sql,
            mylite_errmsg(database),
            mylite_sqlstate(database)
        );
        return 1;
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got ok\n", sql);
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures +=
        expect_text_contains(mylite_errmsg(database), expected.message_part, "error message");
    return failures;
}

static int expect_rows(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t warning_count,
    const char *context
) {
    return expect_grid(result, values, row_count, 1U, warning_count, context);
}

static int expect_grid(
    const mylite_result *result,
    const char *const *values,
    size_t row_count,
    size_t column_count,
    size_t warning_count,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), column_count, context);
    failures += expect_size(mylite_result_row_count(result), row_count, context);
    failures += expect_size(mylite_result_warning_count(result), warning_count, context);
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            size_t value_index = (row_index * column_count) + column_index;

            failures += expect_text(
                mylite_result_value_text(result, row_index, column_index),
                values[value_index],
                context
            );
        }
    }
    return failures;
}

static int expect_empty_statement(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context,
    size_t warning_count
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), warning_count, context);
    return failures;
}

static int expect_warning_rows(
    mylite_db *database,
    const int *codes,
    const char *const *messages,
    size_t row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 3U, context);
        failures += expect_size(mylite_result_row_count(result), row_count, context);
        for (size_t row_index = 0U; row_index < row_count; ++row_index) {
            char code_text[warning_code_text_capacity];

            snprintf(code_text, sizeof(code_text), "%d", codes[row_index]);
            failures +=
                expect_text(mylite_result_value_text(result, row_index, 0U), "Warning", context);
            failures +=
                expect_text(mylite_result_value_text(result, row_index, 1U), code_text, context);
            failures += expect_text(
                mylite_result_value_text(result, row_index, 2U),
                messages[row_index],
                context
            );
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
    fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_select_noop_modifiers_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char suffixed[test_path_capacity + path_suffix_capacity];
    int written = snprintf(suffixed, sizeof(suffixed), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(suffixed)) {
        return;
    }
    remove(suffixed);
}
