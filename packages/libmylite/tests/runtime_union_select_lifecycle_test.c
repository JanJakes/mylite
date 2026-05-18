#include <mylite/mylite.h>

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
    table_union_all_row_count = 6,
    related_file_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_select_reduced = 1222,
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

static int test_scalar_union_values_and_metadata(void);
static int test_table_union_persistence_and_file_safety(void);
static int test_union_diagnostics_and_unsupported_forms(void);
static int test_independent_union_handles(void);
static int seed_union_tables(mylite_db *database);
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
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

    failures += test_scalar_union_values_and_metadata();
    failures += test_table_union_persistence_and_file_safety();
    failures += test_union_diagnostics_and_unsupported_forms();
    failures += test_independent_union_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_union_values_and_metadata(void) {
    static const char *const column_one[] = {"1"};
    static const char *const values_distinct[] = {"1", "2"};
    static const char *const values_all[] = {"1", "1", "2"};
    static const char *const values_null[] = {NULL, "1"};
    static const char *const values_mixed[] = {"1", "1"};
    static const char *const columns_ab[] = {"a", "b"};
    static const char *const values_ab[] = {"1", "2", "3", "4"};
    static const char *const column_row_count[] = {"ROW_COUNT()"};
    static const char *const value_negative_one[] = {"-1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar union memory");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 UNION SELECT 1 UNION SELECT 2",
            .columns = column_one,
            .column_count = 1U,
            .values = values_distinct,
            .row_count = 2U,
            .context = "scalar union distinct",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 UNION ALL SELECT 1 UNION ALL SELECT 2",
            .columns = column_one,
            .column_count = 1U,
            .values = values_all,
            .row_count = 3U,
            .context = "scalar union all",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL UNION DISTINCT SELECT NULL UNION SELECT 1",
            .columns = (const char *const[]){"NULL"},
            .column_count = 1U,
            .values = values_null,
            .row_count = 2U,
            .context = "null duplicate handling",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 UNION ALL SELECT 1 UNION SELECT 1 UNION ALL SELECT 1",
            .columns = column_one,
            .column_count = 1U,
            .values = values_mixed,
            .row_count = 2U,
            .context = "mixed all and distinct chain",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 AS a, 2 AS b UNION SELECT 3 AS c, 4 AS d",
            .columns = columns_ab,
            .column_count = 2U,
            .values = values_ab,
            .row_count = 2U,
            .context = "first branch labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = column_row_count,
            .column_count = 1U,
            .values = value_negative_one,
            .row_count = 1U,
            .context = "row count after union select",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_union_persistence_and_file_safety(void) {
    static const char *const columns_id_v[] = {"id", "v"};
    static const char *const values_distinct[] = {
        "1",
        "a",
        "2",
        "b",
        NULL,
        NULL,
        "3",
        "c",
    };
    static const char *const values_all[] = {
        "1",
        "a",
        "2",
        "b",
        NULL,
        NULL,
        "2",
        "b",
        "3",
        "c",
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table_union") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open table union file");
    failures += seed_union_tables(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t1 UNION SELECT id, v FROM t2",
            .columns = columns_id_v,
            .column_count = 2U,
            .values = values_distinct,
            .row_count = 4U,
            .context = "table union distinct",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t1 UNION ALL SELECT id, v FROM t2",
            .columns = columns_id_v,
            .column_count = 2U,
            .values = values_all,
            .row_count = table_union_all_row_count,
            .context = "table union all",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen table union file");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM app.t1 UNION SELECT id, v FROM app.t2",
            .columns = columns_id_v,
            .column_count = 2U,
            .values = values_distinct,
            .row_count = 4U,
            .context = "schema-qualified union after reopen",
        }
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read union preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "union select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_union_diagnostics_and_unsupported_forms(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open diagnostics memory");
    failures += seed_union_tables(database);
    failures += execute_error(
        database,
        "SELECT 1 UNION SELECT 1, 2",
        (struct expected_sql_error){
            .code = mysql_error_select_reduced,
            .sqlstate = "21000",
            .message_part = "The used SELECT statements have a different number of columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t1 ORDER BY id UNION SELECT id FROM t2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION branch ORDER BY is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 UNION ALL SELECT 2 LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'LIMIT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SQL_NO_CACHE 1 UNION SELECT 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION does not support SELECT options in query blocks",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM missing UNION SELECT id FROM t2",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t1 SELECT id FROM t2 UNION SELECT id FROM t1",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_union_handles(void) {
    static const char *const column_id[] = {"id"};
    static const char *const first_values[] = {"1", "2"};
    static const char *const second_values[] = {"10", "20"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(":memory:", &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(":memory:", &second), MYLITE_OK, "open second handle");
    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "INSERT INTO t VALUES (1)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO t VALUES (10)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t UNION ALL SELECT 2",
            .columns = column_id,
            .column_count = 1U,
            .values = first_values,
            .row_count = 2U,
            .context = "first handle union state",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t UNION ALL SELECT 20",
            .columns = column_id,
            .column_count = 1U,
            .values = second_values,
            .row_count = 2U,
            .context = "second handle union state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static int seed_union_tables(mylite_db *database) {
    mylite_result *result = NULL;
    int rc = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }
    rc = execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }
    rc = execute_ok(database, "CREATE TABLE t1 (id INT, v VARCHAR(10))", &result);
    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }
    rc = execute_ok(database, "CREATE TABLE t2 (id INT, v VARCHAR(10))", &result);
    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }
    rc = execute_ok(database, "INSERT INTO t1 VALUES (1, 'a'), (2, 'b'), (NULL, NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    if (rc != 0) {
        return rc;
    }
    rc = execute_ok(database, "INSERT INTO t2 VALUES (2, 'b'), (3, 'c'), (NULL, NULL)", &result);
    mylite_result_free(result);
    return rc;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
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
    failures += expect_int(result == NULL, 1, "error result is null");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    size_t expected_value_count = expected.row_count * expected.column_count;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t value_index = 0U; value_index < expected_value_count; ++value_index) {
        failures += expect_result_value(
            result,
            value_index / expected.column_count,
            value_index % expected.column_count,
            expected.values[value_index],
            expected.context
        );
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
    int written =
        snprintf(path, path_size, "runtime_union_select_%s_%d.mylite", name, current_process_id());

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to format test path\n");
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
    char related[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);

    return read_count == size ? 0 : 1;
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
        if (actual == expected) {
            return 0;
        }
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing '%s', got '%s'\n",
            context,
            needle,
            actual == NULL ? "NULL" : actual
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }

    return 0;
}
