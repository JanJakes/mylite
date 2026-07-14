#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    mysql_error_bad_null = 1048,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_wrong_usage = 1221,
    mysql_error_data_out_of_range = 1264,
    mysql_error_generated_column_value = 3105,
    show_columns_column_count = 6,
    generated_values_column_count = 7,
    generated_summary_column_count = 5,
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
    const char *info;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_contains_query {
    const char *sql;
    const char *needle;
    const char *context;
};

static int test_create_dml_metadata_and_persistence(void);
static int test_generated_column_diagnostics(void);
static int test_independent_generated_handles(void);
static int seed_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query);
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

    failures += test_create_dml_metadata_and_persistence();
    failures += test_generated_column_diagnostics();
    failures += test_independent_generated_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_dml_metadata_and_persistence(void) {
    static const char *const initial_rows[] = {
        "1", "4", "5", "8",  NULL, "1", "1",  "2", NULL, NULL, NULL, NULL, "1", NULL,
        "3", "7", "8", "14", NULL, "1", "-2", "4", "8",  "9",  "16", NULL, "1", "-3",
    };
    static const char *const updated_row[] = {"1", "10", "11", "20", "-5"};
    static const char *const where_order_row[] = {"1"};
    static const char *const show_bonus[] = {
        "bonus",
        "int",
        "YES",
        "",
        NULL,
        "VIRTUAL GENERATED",
    };
    static const char *const info_schema_rows[] = {
        "bonus",
        "VIRTUAL GENERATED",
        "(`base` + 1)",
        "doubled",
        "STORED GENERATED",
        "(`base` * 2)",
    };
    static const char *const clone_row[] = {"5", "3", "4", "6"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle file");
    failures += seed_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE generated_values ("
        "id INT NOT NULL, "
        "base INT NULL, "
        "bonus INT GENERATED ALWAYS AS (base + 1) VIRTUAL, "
        "doubled INT AS (base * 2) STORED, "
        "nullable_value INT GENERATED ALWAYS AS (NULL) VIRTUAL, "
        "truth INT AS (TRUE + FALSE), "
        "signed_value BIGINT GENERATED ALWAYS AS (-base + 5) STORED)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO generated_values (id, base) VALUES (1, 4), (2, NULL)",
        (struct expected_statement){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO generated_values (id, base, bonus) VALUES (3, 7, DEFAULT)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO generated_values VALUES (4, 8, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, base, bonus, doubled, nullable_value, truth, signed_value "
                   "FROM generated_values ORDER BY id",
            .values = initial_rows,
            .column_count = generated_values_column_count,
            .row_count = 4U,
            .context = "generated columns after inserts",
        }
    );

    failures += expect_statement_result(
        database,
        "UPDATE generated_values SET base = 10 WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE generated_values SET bonus = DEFAULT WHERE id = 1",
        (struct expected_statement){
            .affected_rows = 0,
            .warning_count = 0U,
            .info = "Rows matched: 1  Changed: 0  Warnings: 0",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, base, bonus, doubled, signed_value "
                   "FROM generated_values WHERE id = 1",
            .values = updated_row,
            .column_count = generated_summary_column_count,
            .row_count = 1U,
            .context = "generated columns after base update",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM generated_values WHERE bonus <=> 11 ORDER BY doubled DESC "
                   "LIMIT 1",
            .values = where_order_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "generated column predicate and ordering",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM generated_values LIKE 'bonus'",
            .values = show_bonus,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "SHOW COLUMNS generated extra",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE generated_values",
            .needle = "`bonus` int GENERATED ALWAYS AS ((`base` + 1)) VIRTUAL",
            .context = "SHOW CREATE virtual generated column",
        }
    );
    failures += expect_single_value_contains(
        database,
        (struct expected_contains_query){
            .sql = "SHOW CREATE TABLE generated_values",
            .needle = "`doubled` int GENERATED ALWAYS AS ((`base` * 2)) STORED",
            .context = "SHOW CREATE stored generated column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, EXTRA, GENERATION_EXPRESSION "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'generated_values' "
                   "AND COLUMN_NAME IN ('bonus', 'doubled') ORDER BY COLUMN_NAME",
            .values = info_schema_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "information_schema generated columns",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE cloned LIKE generated_values");
    failures += expect_statement_result(
        database,
        "INSERT INTO cloned (id, base) VALUES (5, 3)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, base, bonus, doubled FROM cloned",
            .values = clone_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE preserves generated descriptor",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen lifecycle file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, base, bonus, doubled, signed_value "
                   "FROM generated_values WHERE id = 1",
            .values = updated_row,
            .column_count = generated_summary_column_count,
            .row_count = 1U,
            .context = "generated columns persist after reopen",
        }
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read lifecycle preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "lifecycle preamble"
    );

    if (database != NULL) {
        mylite_close(database);
    }
    remove_related_files(path);
    return failures;
}

static int test_generated_column_diagnostics(void) {
    static const char *const multi_default_values[] = {"20", "21", "40"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database);
    failures += execute_error(
        database,
        "CREATE TABLE bad_unknown (a INT, b INT AS (missing + 1))",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_future (b INT AS (a + 1), a INT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "previously declared base columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_string (a INT, b INT AS ('x'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "integer, boolean, and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (a INT, b INT AS (a + 1) DEFAULT 0)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "DEFAULT",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_auto (a INT, b INT AS (a + 1) AUTO_INCREMENT)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "AUTO_INCREMENT",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_key (a INT, b INT AS (a + 1), PRIMARY KEY (b))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "generated columns are not yet supported in keys",
        }
    );

    failures += execute_statement_ok(database, "CREATE TABLE writable (a INT, b INT AS (a + 1))");
    failures += execute_error(
        database,
        "INSERT INTO writable (a, b) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_generated_column_value,
            .sqlstate = "HY000",
            .message_part = "generated column 'b'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE writable SET b = 2",
        (struct expected_sql_error){
            .code = mysql_error_generated_column_value,
            .sqlstate = "HY000",
            .message_part = "generated column 'b'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO writable (a, b) SELECT 1, 2",
        (struct expected_sql_error){
            .code = mysql_error_generated_column_value,
            .sqlstate = "HY000",
            .message_part = "generated column 'b'",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE not_null_generated (a INT, b INT AS (NULL) NOT NULL)"
    );
    failures += execute_error(
        database,
        "INSERT INTO not_null_generated (a) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'b' cannot be null",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE not_null_names ("
        "a INT, "
        "b INT AS (a + 1) NOT NULL, "
        "c INT AS (NULL) NOT NULL)"
    );
    failures += execute_error(
        database,
        "INSERT INTO not_null_names (a) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'c' cannot be null",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE range_guard (a INT, b TINYINT AS (a + 1000))");
    failures += execute_error(
        database,
        "INSERT INTO range_guard (a) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE unsigned_guard (a INT, b BIGINT UNSIGNED AS (-a))"
    );
    failures += execute_error(
        database,
        "INSERT INTO unsigned_guard (a) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE multi_default (id INT, a INT, b INT AS (a + 1), c INT AS (a * 2))"
    );
    failures += execute_statement_ok(database, "INSERT INTO multi_default (id, a) VALUES (1, 10)");
    failures += expect_statement_result(
        database,
        "UPDATE multi_default SET a = 20, b = DEFAULT WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE multi_default SET b = DEFAULT, c = DEFAULT WHERE id = 1",
        (struct expected_statement){
            .affected_rows = 0,
            .warning_count = 0U,
            .info = "Rows matched: 1  Changed: 0  Warnings: 0",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a,b,c FROM multi_default WHERE id = 1",
            .values = multi_default_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "generated DEFAULT in multiple assignments",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE dupe (id INT NOT NULL PRIMARY KEY, a INT, b INT AS (a + 1))"
    );
    failures += execute_statement_ok(database, "INSERT INTO dupe (id, a) VALUES (1, 10)");
    failures += expect_statement_result(
        database,
        "INSERT INTO dupe (id, a) VALUES (1, 20) ON DUPLICATE KEY UPDATE b = DEFAULT",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO dupe (id, a) VALUES (1, 20) ON DUPLICATE KEY UPDATE a = VALUES(a), "
        "b = DEFAULT",
        (struct expected_statement){.affected_rows = 2, .warning_count = 1U}
    );
    failures += execute_error(
        database,
        "INSERT INTO dupe (id, a) VALUES (1, 30) ON DUPLICATE KEY UPDATE b = 31",
        (struct expected_sql_error){
            .code = mysql_error_generated_column_value,
            .sqlstate = "HY000",
            .message_part = "generated column 'b'",
        }
    );

    if (database != NULL) {
        mylite_close(database);
    }
    remove_related_files(path);
    return failures;
}

static int test_independent_generated_handles(void) {
    static const char *const first_row[] = {"5", "6"};
    static const char *const second_row[] = {"9", "10"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += execute_statement_ok(first, "CREATE TABLE t (a INT, b INT AS (a + 1))");
    failures += execute_statement_ok(second, "CREATE TABLE t (a INT, b INT AS (a + 1))");
    failures += execute_statement_ok(first, "INSERT INTO t (a) VALUES (5)");
    failures += execute_statement_ok(second, "INSERT INTO t (a) VALUES (9)");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT a, b FROM t",
            .values = first_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first generated handle",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT a, b FROM t",
            .values = second_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second generated handle",
        }
    );

    if (first != NULL) {
        mylite_close(first);
    }
    if (second != NULL) {
        mylite_close(second);
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = execute_statement_ok(database, "CREATE DATABASE app");

    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "warning count");
    if (expected.info != NULL) {
        failures += expect_text(mylite_result_info(result), expected.info, "statement info");
    }
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_single_value_contains(mylite_db *database, struct expected_contains_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), 2U, query.context);
    failures += expect_size(mylite_result_row_count(result), 1U, query.context);
    failures +=
        expect_contains(mylite_result_value_text(result, 0U, 1U), query.needle, query.context);
    mylite_result_free(result);

    return failures;
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
        "%s/mylite_generated_column_lifecycle_%d_%s.mylite",
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
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
