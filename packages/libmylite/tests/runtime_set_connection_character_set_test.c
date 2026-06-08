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
    charset_value_column_count = 7,
    set_names_tail_column_count = 6,
    mysql_error_parse = 1064,
    mysql_error_unknown_character_set = 1115,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_unknown_collation = 1273,
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

static int test_set_connection_character_set_success_and_persistence(void);
static int test_set_connection_character_set_diagnostics(void);
static int test_set_connection_character_set_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_set_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
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
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_set_connection_character_set_success_and_persistence();
    failures += test_set_connection_character_set_diagnostics();
    failures += test_set_connection_character_set_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_set_connection_character_set_success_and_persistence(void) {
    static const char *const charset_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "0",
        "0",
        "0",
    };
    static const char *const unicode_charset_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "0",
        "0",
        "0",
    };
    static const char *const bin_collation_value[] = {"utf8mb4_bin"};
    static const char *const bin_0900_charset_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_bin",
        "0",
        "0",
        "0",
    };
    static const char *const binary_charset_values[] = {
        "binary",
        "binary",
        "binary",
        "binary",
        "0",
        "0",
        "0",
    };
    static const char *const set_names_tail_values[] = {
        "utf8mb4",
        "utf8mb4_bin",
        "ok",
        "0",
        "0",
        "0",
    };
    static const char *const reopened_charset_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "0",
        "0",
        "-1",
    };
    static const char *const warning_count_values[] = {"0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t sqlite_generation_before_set = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open set charset file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_set = session->sqlite_schema_generation;
    }

    failures += expect_set_ok(database, "SET NAMES utf8mb4");
    failures += expect_set_ok(database, "SET NAMES 'utf8mb4' COLLATE `utf8mb4_0900_ai_ci`");
    failures += expect_set_ok(database, "SET NAMES DEFAULT");
    failures += expect_set_ok(database, "SET CHARACTER SET UTF8MB4");
    failures += expect_set_ok(database, "SET CHARSET DEFAULT");
    failures += expect_set_ok(database, "SET NAMES `utf8mb4`");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "set charset keeps fixed connection values",
        }
    );
    failures += expect_set_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = unicode_charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "set names legacy collation values",
        }
    );
    failures += expect_set_ok(database, "SET NAMES UTF8MB4 COLLATE UTF8MB4_BIN");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@collation_connection",
            .values = bin_collation_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set names canonicalizes legacy collation",
        }
    );
    failures += expect_set_ok(database, "SET NAMES binary");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = binary_charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "set names binary values",
        }
    );
    failures += expect_set_ok(
        database,
        "SET NAMES utf8mb4, collation_connection = utf8mb4_bin, @set_names_tail = "
        "_latin1 X'6F6B' COLLATE latin1_swedish_ci"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@collation_connection, @set_names_tail, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = set_names_tail_values,
            .column_count = set_names_tail_column_count,
            .row_count = 1U,
            .context = "set names tail assignment values",
        }
    );
    failures += expect_set_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_0900_bin");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = bin_0900_charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "set names 0900 binary collation values",
        }
    );
    failures += expect_set_ok(database, "SET NAMES utf8mb4");

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_set,
            "set charset leaves SQLite schema generation"
        );
    }

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += expect_set_ok(database, "SET NAMES utf8mb4");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COUNT(*) WARNINGS",
            .values = warning_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set charset clears previous diagnostics",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "set charset preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen set charset file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = reopened_charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "reopened set charset values",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_set_connection_character_set_diagnostics(void) {
    static const char *const charset_values[] = {
        "utf8mb4",
        "latin1",
        "latin1",
        "latin1_swedish_ci",
        "0",
        "0",
        "0",
    };
    static const char *const rollback_values[] = {
        "latin1",
        "latin1_swedish_ci",
        "before",
        "1",
        "1",
        "-1",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_set_ok(database, "SET NAMES latin1");
    failures += execute_error(
        database,
        "SET NAMES bogus",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'bogus'",
        }
    );
    failures += execute_error(
        database,
        "SET NAMES names",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'names'",
        }
    );
    failures += execute_error(
        database,
        "SET CHARACTER SET ucs2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'ucs2'",
        }
    );
    failures += execute_error(
        database,
        "SET NAMES utf8mb4 COLLATE latin1_swedish_ci",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET "
                            "'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "SET NAMES",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SET NAMES NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SET NAMES utf8mb4, latin1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += expect_set_ok(database, "SET character_set_client = utf8mb4");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@character_set_connection, "
                   "@@character_set_results, @@collation_connection, @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .values = charset_values,
            .column_count = charset_value_column_count,
            .row_count = 1U,
            .context = "set character_set_client keeps fixed values",
        }
    );
    failures += expect_set_ok(database, "SET NAMES latin1");
    failures += expect_set_ok(database, "SET @set_names_tail_rollback = 'before'");
    failures += execute_error(
        database,
        "SET NAMES binary, @set_names_tail_rollback = 'after', no_such_system_var = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@collation_connection, "
                   "@set_names_tail_rollback, @@warning_count, @@error_count, ROW_COUNT()",
            .values = rollback_values,
            .column_count = set_names_tail_column_count,
            .row_count = 1U,
            .context = "failed set names tail restores session state",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_set_connection_character_set_independent_handles(void) {
    static const char *const values[] = {
        "utf8mb4",
        "utf8mb4_bin",
        "0",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "0",
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_set_ok(first, "SET NAMES utf8mb4 COLLATE utf8mb4_bin");
    failures += expect_set_ok(second, "SET CHARACTER SET DEFAULT");
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@collation_connection, ROW_COUNT()",
            .values = values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first independent set charset handle",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT @@character_set_client, @@collation_connection, ROW_COUNT()",
            .values = values + 3,
            .column_count = 3U,
            .row_count = 1U,
            .context = "second independent set charset handle",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
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

static int expect_set_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "set charset column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "set charset row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "set charset affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "set charset warning count");
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
        "%s/mylite_set_connection_charset_%d_%s.mylite",
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
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
