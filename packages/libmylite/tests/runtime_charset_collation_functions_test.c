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
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_illegal_mix_of_collations = 1267,
    mysql_error_unknown_collation = 1273,
    mysql_error_unknown_character_set = 1115,
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
    size_t warning_count;
    const char *context;
};

static int test_no_source_dual_and_do_charset_collation(void);
static int test_table_backed_charset_collation_and_reopen(void);
static int test_charset_collation_diagnostics(void);
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

    failures += test_no_source_dual_and_do_charset_collation();
    failures += test_table_backed_charset_collation_and_reopen();
    failures += test_charset_collation_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_charset_collation(void) {
    static const char *const columns_scalar[] = {
        "lit_charset",
        "lit_collation",
        "cast_charset",
        "convert_collation",
        "using_charset",
        "null_collation",
        "int_charset",
        "rand_collation",
        "db_charset",
        "db_collation",
        "utf8_convert_charset",
        "concat_collation",
        "null_concat_charset",
        "null_concat_collation",
        "binary_concat_charset",
        "hex_concat_collation",
        "collated_concat_charset",
        "collated_concat_collation",
        "binary_collated_concat_charset",
        "binary_collated_concat_collation",
    };
    static const char *const values_scalar[] = {
        "utf8mb4", "utf8mb4_0900_ai_ci",
        "binary",  "binary",
        "binary",  "binary",
        "binary",  "binary",
        "utf8mb3", "utf8mb3_general_ci",
        "utf8mb4", "utf8mb4_0900_ai_ci",
        "utf8mb4", "utf8mb4_0900_ai_ci",
        "binary",  "binary",
        "utf8mb4", "utf8mb4_bin",
        "utf8mb4", "utf8mb4_bin",
    };
    static const char *const columns_dual[] = {"cs", "co"};
    static const char *const values_dual[] = {"utf8mb4", "binary"};
    static const char *const columns_nondefault_collation[] = {
        "literal_collation",
        "concat_collation",
        "convert_collation",
        "convert_charset",
    };
    static const char *const values_nondefault_collation[] = {
        "utf8mb4_unicode_ci",
        "utf8mb4_unicode_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
    };
    static const char *const columns_convert_charset[] = {
        "utf8_charset",
        "utf8_collation",
        "utf8mb3_charset",
        "utf8mb3_collation",
        "latin1_charset",
        "latin1_collation",
        "collated_charset",
        "collated_collation",
        "latin1_collated_charset",
        "latin1_collated_collation",
    };
    static const char *const values_convert_charset[] = {
        "utf8mb3",
        "utf8mb3_general_ci",
        "utf8mb3",
        "utf8mb3_general_ci",
        "latin1",
        "latin1_swedish_ci",
        "utf8mb4",
        "utf8mb4_bin",
        "latin1",
        "latin1_bin",
    };
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "SET NAMES utf8mb4", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHARSET('abc') AS lit_charset, COLLATION('abc') AS lit_collation, "
                   "CHARSET(CAST('ABC' AS BINARY)) AS cast_charset, "
                   "COLLATION(CONVERT('ABC', BINARY)) AS convert_collation, "
                   "CHARSET(CONVERT('ABC' USING BINARY)) AS using_charset, "
                   "COLLATION(NULL) AS null_collation, CHARSET(123) AS int_charset, "
                   "COLLATION(RAND(0)) AS rand_collation, CHARSET(DATABASE()) AS db_charset, "
                   "COLLATION(DATABASE()) AS db_collation, "
                   "CHARSET(CONVERT('ABC' USING utf8mb4)) AS utf8_convert_charset, "
                   "COLLATION(CONCAT(1, 'a')) AS concat_collation, "
                   "CHARSET(CONCAT(NULL, 1)) AS null_concat_charset, "
                   "COLLATION(CONCAT(NULL, 1)) AS null_concat_collation, "
                   "CHARSET(CONCAT(CAST('a' AS BINARY), 'b')) AS binary_concat_charset, "
                   "COLLATION(CONCAT(X'41', 'b')) AS hex_concat_collation, "
                   "CHARSET(CONCAT('a' COLLATE utf8mb4_bin, 'b')) "
                   "AS collated_concat_charset, "
                   "COLLATION(CONCAT('a' COLLATE utf8mb4_bin, 'b')) "
                   "AS collated_concat_collation, "
                   "CHARSET(CONCAT(CAST('a' AS BINARY), 'b' COLLATE utf8mb4_bin)) "
                   "AS binary_collated_concat_charset, "
                   "COLLATION(CONCAT(X'41', 'b' COLLATE utf8mb4_bin)) "
                   "AS binary_collated_concat_collation",
            .columns = columns_scalar,
            .column_count = sizeof(columns_scalar) / sizeof(columns_scalar[0]),
            .values = values_scalar,
            .row_count = 1U,
            .context = "no-source charset/collation values",
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
            .context = "row count after charset select",
        }
    );
    failures += execute_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLLATION('abc') AS literal_collation, "
                   "COLLATION(CONCAT('a', 'b')) AS concat_collation, "
                   "COLLATION(CONVERT('ABC' USING utf8mb4)) AS convert_collation, "
                   "CHARSET(CONVERT('ABC' USING utf8mb4)) AS convert_charset",
            .columns = columns_nondefault_collation,
            .column_count =
                sizeof(columns_nondefault_collation) / sizeof(columns_nondefault_collation[0]),
            .values = values_nondefault_collation,
            .row_count = 1U,
            .context = "nondefault connection collation",
        }
    );
    failures += execute_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHARSET(CONVERT('ABC' USING utf8)) AS utf8_charset, "
                   "COLLATION(CONVERT('ABC' USING utf8)) AS utf8_collation, "
                   "CHARSET(CONVERT('ABC' USING utf8mb3)) AS utf8mb3_charset, "
                   "COLLATION(CONVERT('ABC' USING utf8mb3)) AS utf8mb3_collation, "
                   "CHARSET(CONVERT('ABC' USING latin1)) AS latin1_charset, "
                   "COLLATION(CONVERT('ABC' USING latin1)) AS latin1_collation, "
                   "CHARSET(CONVERT('ABC' USING utf8mb4) COLLATE utf8mb4_bin) "
                   "AS collated_charset, "
                   "COLLATION(CONVERT('ABC' USING utf8mb4) COLLATE utf8mb4_bin) "
                   "AS collated_collation, "
                   "CHARSET(CONVERT('ABC' USING latin1) COLLATE latin1_bin) "
                   "AS latin1_collated_charset, "
                   "COLLATION(CONVERT('ABC' USING latin1) COLLATE latin1_bin) "
                   "AS latin1_collated_collation",
            .columns = columns_convert_charset,
            .column_count = sizeof(columns_convert_charset) / sizeof(columns_convert_charset[0]),
            .values = values_convert_charset,
            .row_count = 1U,
            .warning_count = 4U,
            .context = "convert charset/collate metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHARSET ('a') AS cs, COLLATION(CONVERT('A' USING BINARY)) AS co "
                   "FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual charset/collation values",
        }
    );
    failures += execute_ok(
        database,
        "DO CHARSET('abc'), COLLATION(NULL), CHARSET(CAST('A' AS BINARY))",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "charset do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "charset do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "charset do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "charset do warnings");
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
            .context = "row count after charset do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_charset_collation_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "CHARSET(v)",
        "COLLATION(v)",
        "CHARSET(c)",
        "COLLATION(txt)",
        "CHARSET(b)",
        "COLLATION(bl)",
        "CHARSET(e)",
        "COLLATION(s)",
        "CHARSET(i)",
        "COLLATION(d)",
    };
    static const char *const values_table[] = {
        "1",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "binary",
        "binary",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "binary",
        "binary",
        "2",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "binary",
        "binary",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "binary",
        "binary",
    };
    static const char *const columns_envelope[] = {"id", "cs"};
    static const char *const values_envelope[] = {"2", "utf8mb4", "1", "utf8mb4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-backed", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t (id INT, v VARCHAR(10), c CHAR(5), txt TEXT, b VARBINARY(5), "
        "bl BLOB, e ENUM('a','b'), s SET('a','b'), i INT, d DATE) "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 'x', 'y', 'z', X'41', X'42', 'a', 'a,b', 7, '2024-01-02'), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CHARSET(v), COLLATION(v), CHARSET(c), COLLATION(txt), "
                   "CHARSET(b), COLLATION(bl), CHARSET(e), COLLATION(s), CHARSET(i), "
                   "COLLATION(d) FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table-backed charset/collation values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CHARSET(v) AS cs FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_envelope,
            .column_count = sizeof(columns_envelope) / sizeof(columns_envelope[0]),
            .values = values_envelope,
            .row_count = 2U,
            .context = "table-backed row envelope",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen charset file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CHARSET(v) AS cs FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_envelope,
            .column_count = sizeof(columns_envelope) / sizeof(columns_envelope[0]),
            .values = values_envelope,
            .row_count = 2U,
            .context = "reopened table-backed charset/collation values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_charset_collation_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'abc')", NULL);
    failures += execute_error(
        database,
        "SELECT CHARSET()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near ')'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COLLATION('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near ','",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARSET(missing)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COLLATION(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARSET(CONCAT(v, 'x')) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHARSET() and COLLATION() support only scalar metadata arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHARSET(NULL COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COLLATION(X'41' COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT COLLATION(CONCAT('a' COLLATE utf8mb4_bin, "
        "'b' COLLATE utf8mb4_0900_ai_ci))",
        (struct expected_sql_error){
            .code = mysql_error_illegal_mix_of_collations,
            .sqlstate = "HY000",
            .message_part = "Illegal mix of collations (utf8mb4_bin,EXPLICIT) and "
                            "(utf8mb4_0900_ai_ci,EXPLICIT) for operation 'concat'",
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
        fprintf(stderr, "%s: expected success: %s\n", sql, mylite_errmsg(database));
        mylite_result_free(result);
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
        }
    }
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
        "runtime_charset_collation_functions_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "%s: failed to build path\n", name);
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
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
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
            fprintf(stderr, "%s: expected NULL, got %s\n", context, actual);
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
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle
        );
        return 1;
    }
    return 0;
}
