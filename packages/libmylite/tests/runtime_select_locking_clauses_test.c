#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_table_exists = 1050,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_cannot_update_table_while_creating = 1746,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_locking_select_paths(void);
static int test_locking_source_dml(void);
static int test_create_table_select_locking_rejected(void);
static int test_locking_wait_options(void);
static int test_locking_file_reopen(void);
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

    failures += test_locking_select_paths();
    failures += test_locking_source_dml();
    failures += test_create_table_select_locking_rejected();
    failures += test_locking_wait_options();
    failures += test_locking_file_reopen();

    return failures == 0 ? 0 : 1;
}

static int test_locking_select_paths(void) {
    static const char *const scalar_row[] = {"7"};
    static const char *const dual_row[] = {"8"};
    static const char *const update_rows[] = {"1", "3"};
    static const char *const share_row[] = {"1"};
    static const char *const count_row[] = {"4"};
    static const char *const grouped_rows[] = {NULL, "1", "10", "2", "30", "1"};
    static const char *const distinct_rows[] = {NULL, "10", "30"};
    static const char *const calc_row[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "paths") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locking paths");
    failures += prepare_fixture(database);

    failures += execute_ok(database, "SELECT 7 FOR UPDATE", &result);
    failures += expect_rows(result, scalar_row, 1U, 0U, "scalar for update row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT 8 FROM DUAL FOR SHARE", &result);
    failures += expect_rows(result, dual_row, 1U, 0U, "dual for share row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT id FROM t WHERE n <=> 10 ORDER BY id LIMIT 2 FOR UPDATE",
        &result
    );
    failures += expect_rows(result, update_rows, 2U, 0U, "table for update rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 1 FOR SHARE", &result);
    failures += expect_rows(result, share_row, 1U, 0U, "table for share row");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t ORDER BY id LIMIT 1 LOCK IN SHARE MODE", &result);
    failures += expect_rows(result, share_row, 1U, 0U, "table lock in share mode row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT COUNT(*) FROM t FOR UPDATE", &result);
    failures += expect_rows(result, count_row, 1U, 0U, "count for update row");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT n, COUNT(*) FROM t GROUP BY n ORDER BY n FOR SHARE", &result);
    failures += expect_grid(result, grouped_rows, 3U, 2U, 0U, "grouped for share rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT DISTINCT n FROM t ORDER BY n FOR UPDATE", &result);
    failures += expect_rows(result, distinct_rows, 3U, 0U, "distinct for update rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1 FOR UPDATE",
        &result
    );
    failures += expect_rows(result, calc_row, 1U, 1U, "sql calc for update row");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_locking_source_dml(void) {
    static const char *const inserted_rows[] = {"1"};
    static const char *const replaced_rows[] = {"2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "dml") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locking dml");
    failures += prepare_fixture(database);

    failures += execute_statement_ok(database, "CREATE TABLE inserted (id INT NOT NULL)");
    failures += execute_ok(
        database,
        "INSERT INTO inserted (id) SELECT id FROM t WHERE id = 1 FOR UPDATE",
        &result
    );
    failures += expect_empty_statement(result, 1, "insert select for update result", 0U);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM inserted ORDER BY id", &result);
    failures += expect_rows(result, inserted_rows, 1U, 0U, "insert select for update rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE TABLE replaced (id INT NOT NULL)");
    failures += execute_ok(
        database,
        "REPLACE INTO replaced (id) SELECT id FROM t WHERE id = 2 FOR SHARE",
        &result
    );
    failures += expect_empty_statement(result, 1, "replace select for share result", 0U);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM replaced ORDER BY id", &result);
    failures += expect_rows(result, replaced_rows, 1U, 0U, "replace select for share rows");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_create_table_select_locking_rejected(void) {
    static const char *const empty_copy_count[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "ctas") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locking ctas");
    failures += prepare_fixture(database);

    failures += execute_error(
        database,
        "CREATE TABLE copy AS SELECT id FROM t FOR UPDATE",
        (struct expected_sql_error){
            .code = mysql_error_cannot_update_table_while_creating,
            .sqlstate = "HY000",
            .message_part = "Can't update table 't' while 'copy' is being created.",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM copy",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.copy' doesn't exist",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE copy (id INT NOT NULL)");
    failures += execute_error(
        database,
        "CREATE TABLE copy AS SELECT id FROM t FOR UPDATE",
        (struct expected_sql_error){
            .code = mysql_error_table_exists,
            .sqlstate = "42S01",
            .message_part = "Table 'copy' already exists",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE IF NOT EXISTS copy AS SELECT id FROM t FOR UPDATE",
        &result
    );
    failures += expect_empty_statement(result, 0, "ctas locking if-not-exists existing", 1U);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT COUNT(*) FROM copy", &result);
    failures += expect_rows(result, empty_copy_count, 1U, 0U, "ctas if-not-exists skips copy");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_locking_wait_options(void) {
    static const char *const all_rows[] = {"1", "2", "3", "4"};
    static const char *const one_row[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "wait") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locking wait options");
    failures += prepare_fixture(database);

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id FOR UPDATE NOWAIT", &result);
    failures += expect_rows(result, all_rows, 4U, 0U, "for update nowait rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id FOR SHARE SKIP LOCKED", &result);
    failures += expect_rows(result, all_rows, 4U, 0U, "for share skip locked rows");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t WHERE id = 1 FOR UPDATE SKIP LOCKED", &result);
    failures += expect_rows(result, one_row, 1U, 0U, "for update skip locked filtered row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM t WHERE id = 1 FOR SHARE NOWAIT", &result);
    failures += expect_rows(result, one_row, 1U, 0U, "for share nowait filtered row");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT id FROM t ORDER BY id FOR UPDATE OF t", &result);
    failures += expect_rows(result, all_rows, 4U, 0U, "for update of table rows");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t AS lk ORDER BY id FOR SHARE OF lk NOWAIT", &result);
    failures += expect_rows(result, all_rows, 4U, 0U, "for share of alias nowait rows");
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id FROM t ORDER BY id FOR UPDATE OF t SKIP LOCKED", &result);
    failures += expect_rows(result, all_rows, 4U, 0U, "for update of table skip locked rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT id FROM t FOR UPDATE FOR SHARE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t FOR UPDATE ORDER BY id",
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

static int test_locking_file_reopen(void) {
    static const char *const rows[] = {"1", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locking reopen");
    failures += prepare_fixture(database);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen locking file");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(
        database,
        "SELECT id FROM t WHERE n <=> 10 ORDER BY id LIMIT 2 FOR UPDATE",
        &result
    );
    failures += expect_rows(result, rows, 2U, 0U, "reopen for update rows");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int prepare_fixture(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE t (id INT NOT NULL, n INT NULL)");
    failures +=
        execute_statement_ok(database, "INSERT INTO t VALUES (1, 10), (2, NULL), (3, 10), (4, 30)");
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
        "%s/mylite_select_locking_clauses_%d_%s.mylite",
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
