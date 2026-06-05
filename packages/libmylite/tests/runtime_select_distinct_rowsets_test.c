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

static int test_distinct_rowset_success_and_reopen(void);
static int test_distinct_rowset_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int seed_distinct_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_distinct_rowset_success_and_reopen();
    failures += test_distinct_rowset_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_distinct_rowset_success_and_reopen(void) {
    static const char *const columns_ab[] = {"a", "b"};
    static const char *const values_ab[] = {NULL, "11", "1", "10", "1", "11"};
    static const char *const columns_a[] = {"a"};
    static const char *const values_a[] = {NULL, "1"};
    static const char *const values_a_non_selected_order[] = {"1", NULL};
    static const char *const columns_b[] = {"b"};
    static const char *const values_b_joined_non_selected_order[] = {"10", "11"};
    static const char *const columns_s[] = {"s"};
    static const char *const values_s[] = {NULL, "Alpha", "Beta"};
    static const char *const columns_s_txt[] = {"s", "txt"};
    static const char *const values_s_txt[] = {NULL, NULL, "Alpha", "Text", "Beta", "Text"};
    static const char *const columns_temporal[] = {"y", "d", "tm", "dt"};
    static const char *const values_temporal[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        "2020",
        "2020-01-01",
        "01:02:03",
        "2020-01-01 01:02:03",
        "2021",
        "2020-01-02",
        "-01:02:03",
        "2020-01-02 01:02:03",
    };
    static const char *const columns_xy[] = {"x", "y"};
    static const char *const values_alias[] = {"1", "10", NULL, "11", "1", "11"};
    static const char *const values_limit[] = {"1", "10", "1", "11"};
    static const char *const columns_abs[] = {"a", "b", "s"};
    static const char *const values_wildcard[] =
        {NULL, "11", NULL, "1", "10", "Alpha", "1", "11", "Alpha"};
    static const char *const values_where[] = {NULL, "11", "1", "11"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_status[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "success", path, sizeof(path));
    failures += seed_distinct_tables(database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT a, b FROM t ORDER BY a, b",
            .columns = columns_ab,
            .column_count = 2U,
            .values = values_ab,
            .row_count = 3U,
            .context = "multi-column integer distinct",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT(t.a) FROM t ORDER BY a",
            .columns = columns_a,
            .column_count = 1U,
            .values = values_a,
            .row_count = 2U,
            .context = "parenthesized qualified integer distinct",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT s FROM t ORDER BY s",
            .columns = columns_s,
            .column_count = 1U,
            .values = values_s,
            .row_count = 3U,
            .context = "ASCII string distinct collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT s, txt FROM t ORDER BY s, txt",
            .columns = columns_s_txt,
            .column_count = 2U,
            .values = values_s_txt,
            .row_count = 3U,
            .context = "string row distinct collation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT y, d, tm, dt FROM t ORDER BY y, d, tm, dt",
            .columns = columns_temporal,
            .column_count = 4U,
            .values = values_temporal,
            .row_count = 3U,
            .context = "year and temporal distinct",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT a AS x, b AS y FROM t ORDER BY y, x",
            .columns = columns_xy,
            .column_count = 2U,
            .values = values_alias,
            .row_count = 3U,
            .context = "selected aliases order distinct rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW a, b FROM t ORDER BY a, b LIMIT 10",
            .columns = columns_ab,
            .column_count = 2U,
            .values = values_ab,
            .row_count = 3U,
            .context = "distinctrow rowset synonym",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT a, b FROM t ORDER BY a, b LIMIT 2 OFFSET 1",
            .columns = columns_ab,
            .column_count = 2U,
            .values = values_limit,
            .row_count = 2U,
            .context = "distinct rowset limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT * FROM visible_dupes ORDER BY a, b, s",
            .columns = columns_abs,
            .column_count = 3U,
            .values = values_wildcard,
            .row_count = 3U,
            .context = "wildcard distinct visible rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT vd.* FROM visible_dupes AS vd ORDER BY a, b, s",
            .columns = columns_abs,
            .column_count = 3U,
            .values = values_wildcard,
            .row_count = 3U,
            .context = "qualified wildcard distinct visible rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT a, b FROM t WHERE b = 11 ORDER BY a, b",
            .columns = columns_ab,
            .column_count = 2U,
            .values = values_where,
            .row_count = 2U,
            .context = "filtered distinct rowset",
        }
    );
    failures += execute_ok(database, "SET sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT a FROM t ORDER BY b",
            .columns = columns_a,
            .column_count = 1U,
            .values = values_a_non_selected_order,
            .row_count = 2U,
            .context = "loose mode distinct non-selected order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT l.b FROM t AS l JOIN t AS r ON l.a = r.a ORDER BY l.s, l.b",
            .columns = columns_b,
            .column_count = 1U,
            .values = values_b_joined_non_selected_order,
            .row_count = 2U,
            .context = "joined distinct non-selected order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = 2U,
            .values = values_status,
            .row_count = 1U,
            .context = "row count after distinct rowset",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen distinct database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT s FROM t ORDER BY s",
            .columns = columns_s,
            .column_count = 1U,
            .values = values_s,
            .row_count = 3U,
            .context = "reopened string distinct collation",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_distinct_rowset_diagnostics(void) {
    static const char *const columns_one[] = {"1"};
    static const char *const values_one[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += seed_distinct_tables(database);
    failures += execute_ok(database, "CREATE TABLE decs (d DECIMAL(5,2))", NULL);
    failures += execute_ok(database, "INSERT INTO decs VALUES (1.00)", NULL);

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
        "SELECT DISTINCT a FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports only descriptor-backed table reads",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT 1 FROM t",
            .columns = columns_one,
            .column_count = 1U,
            .values = values_one,
            .row_count = 1U,
            .context = "constant row-scalar distinct",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT d FROM decs",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports only integer, YEAR, DATE, TIME, DATETIME, "
                            "TIMESTAMP, or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT a FROM t ORDER BY b",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT DISTINCT supports ORDER BY only on selected columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT a FROM t ORDER BY FIELD(a, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT ORDER BY supports only descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT DISTINCT SQL_CALC_FOUND_ROWS a FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SQL_CALC_FOUND_ROWS supports only non-distinct descriptor-backed table SELECT",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open distinct file");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    return failures;
}

static int seed_distinct_tables(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE t ("
        "a INT NULL, b INT NULL, s VARCHAR(16) NULL, txt TEXT NULL, y YEAR NULL, "
        "d DATE NULL, tm TIME NULL, dt DATETIME NULL)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,10,'Alpha','Text',2020,'2020-01-01','01:02:03','2020-01-01 01:02:03'),"
        "(1,10,'alpha','text',2020,'2020-01-01','01:02:03','2020-01-01 01:02:03'),"
        "(1,11,'Beta','Text',2021,'2020-01-02','-01:02:03','2020-01-02 01:02:03'),"
        "(NULL,11,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(NULL,11,NULL,NULL,NULL,NULL,NULL,NULL)",
        NULL
    );
    failures +=
        execute_ok(database, "CREATE TABLE visible_dupes (a INT, b INT, s VARCHAR(16))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO visible_dupes VALUES "
        "(1,10,'Alpha'),(1,10,'alpha'),(1,11,'Alpha'),(NULL,11,NULL),(NULL,11,NULL)",
        NULL
    );

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
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
        fprintf(stderr, "execute '%s': expected error, got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_select_distinct_rowsets_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
