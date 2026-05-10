#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "storage/mylite_file_format.h"

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
    sql_buffer_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *column;
    const char *const *values;
    size_t value_count;
    const char *context;
};

struct expected_row_count {
    const char *expected;
    const char *context;
};

static int test_select_qualified_columns_values_reopen_rename_and_drop(void);
static int test_select_qualified_columns_diagnostics(void);
static int test_independent_qualified_column_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_numbers_table(mylite_db *database, const char *insert_rows);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_row_count(mylite_db *database, struct expected_row_count expected);
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_select_qualified_columns_values_reopen_rename_and_drop();
    failures += test_select_qualified_columns_diagnostics();
    failures += test_independent_qualified_column_handles();

    return failures == 0 ? 0 : 1;
}

static int test_select_qualified_columns_values_reopen_rename_and_drop(void) {
    static const char *const ordered_n[] = {"10", NULL, "10"};
    static const char *const alias_desc_non_null[] = {"10"};
    static const char *const alias_all_desc_non_null[] = {"10", "10"};
    static const char *const distinct_n[] = {NULL, "10"};
    static const char *const count_n[] = {"2"};
    static const char *const count_distinct_n[] = {"1"};
    static const char *const min_n[] = {"10"};
    static const char *const renamed_values[] = {"10", NULL, "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "(1, 10, 5), (2, NULL, 6), (3, 10, 7)");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT numbers.n FROM numbers ORDER BY numbers.id",
            .column = "n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "table-qualified selected and ordered columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT app.numbers.n FROM app.numbers ORDER BY app.numbers.id",
            .column = "n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "schema-qualified selected and ordered columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT app.numbers.n FROM numbers ORDER BY app.numbers.id",
            .column = "n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "schema-qualified columns against unqualified source",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT nums.n FROM numbers AS nums WHERE nums.n IS NOT NULL "
                   "ORDER BY nums.id DESC LIMIT 1",
            .column = "n",
            .values = alias_desc_non_null,
            .value_count = sizeof(alias_desc_non_null) / sizeof(alias_desc_non_null[0]),
            .context = "alias-qualified where order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ALL nums.n FROM numbers nums WHERE nums.n IS NOT NULL "
                   "ORDER BY nums.id DESC LIMIT 2",
            .column = "n",
            .values = alias_all_desc_non_null,
            .value_count = sizeof(alias_all_desc_non_null) / sizeof(alias_all_desc_non_null[0]),
            .context = "all alias-qualified where order limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCT nums.n FROM numbers AS nums ORDER BY nums.n",
            .column = "n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "alias-qualified distinct",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT DISTINCTROW nums.n FROM numbers nums ORDER BY nums.n",
            .column = "n",
            .values = distinct_n,
            .value_count = sizeof(distinct_n) / sizeof(distinct_n[0]),
            .context = "alias-qualified distinctrow",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(nums.n) FROM numbers AS nums",
            .column = "COUNT(nums.n)",
            .values = count_n,
            .value_count = sizeof(count_n) / sizeof(count_n[0]),
            .context = "alias-qualified count column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(DISTINCT nums.n) FROM numbers AS nums",
            .column = "COUNT(DISTINCT nums.n)",
            .values = count_distinct_n,
            .value_count = sizeof(count_distinct_n) / sizeof(count_distinct_n[0]),
            .context = "alias-qualified count distinct column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT MIN(nums.n) FROM numbers AS nums WHERE nums.n IS NOT NULL",
            .column = "MIN(nums.n)",
            .values = min_n,
            .value_count = sizeof(min_n) / sizeof(min_n[0]),
            .context = "alias-qualified min where",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT MAX(app.numbers.n) FROM app.numbers "
                   "WHERE app.numbers.n IS NOT NULL",
            .column = "MAX(app.numbers.n)",
            .values = min_n,
            .value_count = sizeof(min_n) / sizeof(min_n[0]),
            .context = "schema-qualified max where",
        }
    );
    failures += expect_row_count(
        database,
        (struct expected_row_count){
            .expected = "-1",
            .context = "row count after qualified select",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "qualified selects preserve preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT nums.n FROM numbers AS nums ORDER BY nums.id",
            .column = "n",
            .values = ordered_n,
            .value_count = sizeof(ordered_n) / sizeof(ordered_n[0]),
            .context = "reopened alias-qualified selected column",
        }
    );

    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT r.n FROM renamed_numbers AS r ORDER BY r.id",
            .column = "n",
            .values = renamed_values,
            .value_count = sizeof(renamed_values) / sizeof(renamed_values[0]),
            .context = "alias-qualified renamed table",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT r.n FROM renamed_numbers AS r",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_select_qualified_columns_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "(1, 10, 5), (2, NULL, 6), (3, 10, 7)");

    failures += execute_error(
        database,
        "SELECT numbers.n FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'numbers.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT app.numbers.n FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'app.numbers.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT wrong.n FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT nums.missing FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nums.missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT nums.n FROM numbers AS nums WHERE wrong.n = 10",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT nums.n FROM numbers AS nums ORDER BY wrong.n",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(wrong.n) FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MIN(wrong.n) FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'wrong.n' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT nums.* FROM numbers AS nums",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_qualified_column_handles(void) {
    static const char *const first_expected[] = {"10"};
    static const char *const second_expected[] = {"20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(first, "(1, 10, 5)");
    failures += create_numbers_table(second, "(1, 20, 5)");

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT n.n FROM numbers AS n ORDER BY n.id",
            .column = "n",
            .values = first_expected,
            .value_count = sizeof(first_expected) / sizeof(first_expected[0]),
            .context = "first handle qualified row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT n.n FROM numbers AS n ORDER BY n.id",
            .column = "n",
            .values = second_expected,
            .value_count = sizeof(second_expected) / sizeof(second_expected[0]),
            .context = "second handle qualified row",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    struct mylite_catalog_schema_descriptor schema = {0};

    return expect_int(
        mylite_catalog_create_schema(database, name, &schema),
        MYLITE_OK,
        "seed schema"
    );
}

static int create_numbers_table(mylite_db *database, const char *insert_rows) {
    char sql[sql_buffer_capacity];
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, n INT NULL, nn INT NOT NULL)",
        &result
    );
    int written = 0;

    mylite_result_free(result);
    result = NULL;
    written = snprintf(sql, sizeof(sql), "INSERT INTO numbers VALUES %s", insert_rows);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "insert SQL is too long\n");
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected MYLITE_OK, got %d (%d %s %s)\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, query.context);
    failures += expect_text(mylite_result_column_name(result, 0U), query.column, query.context);
    failures += expect_size(mylite_result_row_count(result), query.value_count, query.context);
    for (size_t index = 0U; index < query.value_count; ++index) {
        failures += expect_result_value(result, index, 0U, query.values[index], query.context);
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_row_count(mylite_db *database, struct expected_row_count expected) {
    static const char *const row_count_column = "ROW_COUNT()";
    const char *const values[] = {expected.expected};

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column = row_count_column,
            .values = values,
            .value_count = sizeof(values) / sizeof(values[0]),
            .context = expected.context,
        }
    );
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
        return expect_true(actual == NULL, context);
    }

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
        "%s/mylite_select_qualified_columns_%d_%s.mylite",
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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);

    return 0;
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte range mismatch\n", context);
        return 1;
    }

    return 0;
}
