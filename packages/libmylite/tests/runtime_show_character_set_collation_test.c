#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    character_set_column_count = 4,
    collation_column_count = 7,
    decimal_base = 10,
    suffix_capacity = 16,
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const character_set_columns[character_set_column_count] = {
    "Charset",
    "Description",
    "Default collation",
    "Maxlen",
};

static const char *const character_set_row[character_set_column_count] = {
    "utf8mb4",
    "UTF-8 Unicode",
    "utf8mb4_0900_ai_ci",
    "4",
};

static const char *const character_set_binary_row[character_set_column_count] = {
    "binary",
    "Binary pseudo charset",
    "binary",
    "1",
};

static const char *const character_set_rows[] = {
    "binary",
    "Binary pseudo charset",
    "binary",
    "1",
    "utf8mb4",
    "UTF-8 Unicode",
    "utf8mb4_0900_ai_ci",
    "4",
};

static const char *const collation_columns[collation_column_count] = {
    "Collation",
    "Charset",
    "Id",
    "Default",
    "Compiled",
    "Sortlen",
    "Pad_attribute",
};

static const char *const collation_rows[] = {
    "binary",
    "binary",
    "63",
    "Yes",
    "Yes",
    "1",
    "NO PAD",
    "utf8mb4_general_ci",
    "utf8mb4",
    "45",
    "",
    "Yes",
    "1",
    "PAD SPACE",
    "utf8mb4_bin",
    "utf8mb4",
    "46",
    "",
    "Yes",
    "1",
    "PAD SPACE",
    "utf8mb4_unicode_ci",
    "utf8mb4",
    "224",
    "",
    "Yes",
    "8",
    "PAD SPACE",
    "utf8mb4_unicode_520_ci",
    "utf8mb4",
    "246",
    "",
    "Yes",
    "8",
    "PAD SPACE",
    "utf8mb4_0900_ai_ci",
    "utf8mb4",
    "255",
    "Yes",
    "Yes",
    "0",
    "NO PAD",
    "utf8mb4_0900_bin",
    "utf8mb4",
    "309",
    "",
    "Yes",
    "1",
    "NO PAD",
};

static const char *const collation_binary_row[collation_column_count] = {
    "binary",
    "binary",
    "63",
    "Yes",
    "Yes",
    "1",
    "NO PAD",
};

static const char *const collation_0900_row[collation_column_count] = {
    "utf8mb4_0900_ai_ci",
    "utf8mb4",
    "255",
    "Yes",
    "Yes",
    "0",
    "NO PAD",
};

static const char *const collation_0900_bin_row[collation_column_count] = {
    "utf8mb4_0900_bin",
    "utf8mb4",
    "309",
    "",
    "Yes",
    "1",
    "NO PAD",
};

static const char *const collation_unicode_row[collation_column_count] = {
    "utf8mb4_unicode_ci",
    "utf8mb4",
    "224",
    "",
    "Yes",
    "8",
    "PAD SPACE",
};

static int test_show_character_set_and_collation_values_filters_and_state(void);
static int test_show_character_set_and_collation_schema_independence_and_persistence(void);
static int test_show_character_set_and_collation_diagnostics(void);
static int test_independent_show_character_set_and_collation_handles(void);
static int expect_result(
    mylite_db *database,
    const char *sql,
    const char *const *expected_columns,
    size_t expected_column_count,
    const char *const *expected_row,
    size_t expected_row_count,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_character_set_and_collation_values_filters_and_state();
    failures += test_show_character_set_and_collation_schema_independence_and_persistence();
    failures += test_show_character_set_and_collation_diagnostics();
    failures += test_independent_show_character_set_and_collation_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_character_set_and_collation_values_filters_and_state(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open values memory");
    failures += expect_result(
        database,
        "SHOW CHARACTER SET",
        character_set_columns,
        character_set_column_count,
        character_set_rows,
        sizeof(character_set_rows) / sizeof(character_set_rows[0]) / character_set_column_count,
        "show character set"
    );
    failures += expect_result(
        database,
        "SHOW CHARACTER SET LIKE 'BINARY'",
        character_set_columns,
        character_set_column_count,
        character_set_binary_row,
        1U,
        "show binary character set uppercase like"
    );
    failures += expect_result(
        database,
        "SHOW CHARSET LIKE 'UTF8MB4'",
        character_set_columns,
        character_set_column_count,
        character_set_row,
        1U,
        "show charset uppercase like"
    );
    failures += expect_result(
        database,
        "SHOW CHARACTER SET LIKE 'utf%mb4'",
        character_set_columns,
        character_set_column_count,
        character_set_row,
        1U,
        "show character set wildcard like"
    );
    failures += expect_result(
        database,
        "SHOW CHARACTER SET LIKE 'utf_mb4'",
        character_set_columns,
        character_set_column_count,
        character_set_row,
        1U,
        "show character set underscore like"
    );
    failures += expect_result(
        database,
        "SHOW CHARACTER SET LIKE 'missing%'",
        character_set_columns,
        character_set_column_count,
        NULL,
        0U,
        "show character set no match"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION",
        collation_columns,
        collation_column_count,
        collation_rows,
        sizeof(collation_rows) / sizeof(collation_rows[0]) / collation_column_count,
        "show collation"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'BINARY'",
        collation_columns,
        collation_column_count,
        collation_binary_row,
        1U,
        "show binary collation uppercase like"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'UTF8MB4_0900_AI_CI'",
        collation_columns,
        collation_column_count,
        collation_0900_row,
        1U,
        "show collation uppercase like"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'utf8mb4\\_0900\\_ai\\_ci'",
        collation_columns,
        collation_column_count,
        collation_0900_row,
        1U,
        "show collation escaped underscore"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'UTF8MB4_0900_BIN'",
        collation_columns,
        collation_column_count,
        collation_0900_bin_row,
        1U,
        "show binary 0900 collation uppercase like"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'utf8mb4_unicode_ci'",
        collation_columns,
        collation_column_count,
        collation_unicode_row,
        1U,
        "show legacy collation like"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'missing%'",
        collation_columns,
        collation_column_count,
        NULL,
        0U,
        "show collation no match"
    );

    mylite_close(database);
    return failures;
}

static int test_show_character_set_and_collation_schema_independence_and_persistence(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open persistence file");
    failures += expect_result(
        database,
        "SHOW CHARACTER SET",
        character_set_columns,
        character_set_column_count,
        character_set_rows,
        sizeof(character_set_rows) / sizeof(character_set_rows[0]) / character_set_column_count,
        "show character set without schema"
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE configured (id INT) CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
    );
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_result(
        database,
        "SHOW CHARACTER SET",
        character_set_columns,
        character_set_column_count,
        character_set_rows,
        sizeof(character_set_rows) / sizeof(character_set_rows[0]) / character_set_column_count,
        "show character set with selected schema"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION",
        collation_columns,
        collation_column_count,
        collation_rows,
        sizeof(collation_rows) / sizeof(collation_rows[0]) / collation_column_count,
        "show collation with selected schema"
    );
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after static show"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after static show"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after static show"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistence file");
    failures += expect_result(
        database,
        "SHOW CHARACTER SET LIKE 'utf8mb4'",
        character_set_columns,
        character_set_column_count,
        character_set_row,
        1U,
        "reopened show character set"
    );
    failures += expect_result(
        database,
        "SHOW COLLATION LIKE 'utf8mb4_0900_bin'",
        collation_columns,
        collation_column_count,
        collation_0900_bin_row,
        1U,
        "reopened show collation"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_character_set_and_collation_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");
    failures += execute_error(
        database,
        "SHOW CHARACTER SETS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARSETS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATIONS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET WHERE Charset = 'utf8mb4'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET LIKE N'utf8mb4'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET LIKE _utf8mb4'utf8mb4'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW CHARACTER SET LIKE 'utf\\0%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW LIKE does not support NUL bytes in patterns",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION WHERE Collation = 'utf8mb4_0900_ai_ci'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION LIKE N'utf8mb4_0900_ai_ci'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION LIKE _utf8mb4'utf8mb4_0900_ai_ci'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW COLLATION LIKE 'utf8mb4%' FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_show_character_set_and_collation_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += execute_statement_ok(first, "CREATE DATABASE firstapp");
    failures += execute_statement_ok(first, "USE firstapp");
    failures += execute_statement_ok(second, "CREATE DATABASE secondapp");
    failures += execute_statement_ok(second, "USE secondapp");
    failures += expect_result(
        first,
        "SHOW CHARACTER SET LIKE 'utf8mb4'",
        character_set_columns,
        character_set_column_count,
        character_set_row,
        1U,
        "first handle character set"
    );
    failures += expect_result(
        second,
        "SHOW COLLATION LIKE 'utf8mb4_0900_bin'",
        collation_columns,
        collation_column_count,
        collation_0900_bin_row,
        1U,
        "second handle collation"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int expect_result(
    mylite_db *database,
    const char *sql,
    const char *const *expected_columns,
    size_t expected_column_count,
    const char *const *expected_row,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), expected_column_count, context);
    for (size_t column_index = 0U; column_index < expected_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected_columns[column_index],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    if (expected_row != NULL) {
        for (size_t row_index = 0U; row_index < expected_row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected_column_count; ++column_index) {
                failures += expect_text_or_null(
                    mylite_result_value_text(result, row_index, column_index),
                    expected_row[(row_index * expected_column_count) + column_index],
                    context
                );
            }
        }
    }
    failures += expect_row_count(database, -1, context);

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_row_count(result) == 1U && mylite_result_column_count(result) == 1U) {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        (void)fprintf(stderr, "%s: missing result\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_show_character_set_collation_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    char buffer[test_path_capacity + suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek\n", path);
        (void)fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        (void)fprintf(stderr, "%s: failed to close file\n", path);
        return 1;
    }
    if (read_size != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, read_size);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(
            stderr,
            "%s: expected %" PRId64 ", got %" PRId64 "\n",
            context,
            expected,
            actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        (void)fprintf(
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

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        (void)fprintf(
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        (void)fprintf(stderr, "%s: byte ranges differ\n", context);
        return 1;
    }

    return 0;
}
