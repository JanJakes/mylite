#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    repeated_comment_sql_prefix_capacity = 128,
    show_full_columns_comment_column = 8,
    column_comment_max_characters = 1024,
    mysql_error_parse = 1064,
    mysql_error_column_comment_too_long = 1629,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_create_clone_metadata_and_persistence(void);
static int test_alter_column_comment_lifecycle(void);
static int test_temporary_comments_and_sql_modes(void);
static int test_column_comment_diagnostics(void);
static int create_app_schema(mylite_db *database);
static int expect_cell(
    mylite_db *database,
    const char *sql,
    size_t row_index,
    size_t column_index,
    const char *expected,
    const char *context
);
static int expect_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t row_index,
    size_t column_index,
    const char *needle,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
);
static char *make_repeated_comment_create_table_sql(
    const char *table_name,
    const unsigned char *unit,
    size_t unit_size,
    size_t repeat_count,
    char **out_comment
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_clone_metadata_and_persistence();
    failures += test_alter_column_comment_lifecycle();
    failures += test_temporary_comments_and_sql_modes();
    failures += test_column_comment_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_create_clone_metadata_and_persistence(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "persistent") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open persistent file");
    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE commented ("
        "a INT COMMENT 'alpha', "
        "b VARCHAR(5) DEFAULT 'x' COMMENT 'bee', "
        "c INT COMMENT 'first' COMMENT 'second', "
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'identifier', "
        "PRIMARY KEY (id))"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ndb_column_comment ("
        "id INT, "
        "body TEXT COMMENT 'NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE')"
    );
    failures += expect_cell_contains(
        database,
        "SHOW CREATE TABLE commented",
        0U,
        1U,
        "`a` int DEFAULT NULL COMMENT 'alpha'",
        "SHOW CREATE renders int comment"
    );
    failures += expect_cell_contains(
        database,
        "SHOW CREATE TABLE commented",
        0U,
        1U,
        "`b` varchar(5) DEFAULT 'x' COMMENT 'bee'",
        "SHOW CREATE renders varchar comment"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM commented",
        2U,
        show_full_columns_comment_column,
        "second",
        "SHOW FULL COLUMNS final duplicate comment wins"
    );
    failures += expect_cell(
        database,
        "SELECT COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'commented' "
        "AND COLUMN_NAME = 'id'",
        0U,
        0U,
        "identifier",
        "information schema column comment"
    );
    failures += expect_cell_contains(
        database,
        "SHOW CREATE TABLE ndb_column_comment",
        0U,
        1U,
        "`body` text COMMENT 'NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE'",
        "NDB-shaped column comment SHOW CREATE"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM ndb_column_comment",
        1U,
        show_full_columns_comment_column,
        "NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE",
        "NDB-shaped column comment SHOW FULL COLUMNS"
    );
    failures += expect_cell(
        database,
        "SELECT COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'ndb_column_comment' "
        "AND COLUMN_NAME = 'body'",
        0U,
        0U,
        "NDB_COLUMN=BLOB_INLINE_SIZE=4096,MAX_BLOB_PART_SIZE",
        "NDB-shaped column comment information schema"
    );
    failures += execute_statement_ok(database, "CREATE TABLE clone LIKE commented");
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM clone",
        0U,
        show_full_columns_comment_column,
        "alpha",
        "CREATE TABLE LIKE copies column comment"
    );
    failures += execute_statement_ok(database, "CREATE TABLE ctas AS SELECT a, b FROM commented");
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM ctas",
        0U,
        show_full_columns_comment_column,
        "alpha",
        "CREATE TABLE SELECT copies first direct column comment"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM ctas",
        1U,
        show_full_columns_comment_column,
        "bee",
        "CREATE TABLE SELECT copies second direct column comment"
    );
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen persistent file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM commented",
        0U,
        show_full_columns_comment_column,
        "alpha",
        "reopen preserves column comment"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alter_column_comment_lifecycle(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open alter memory"
    );

    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE altered (a INT COMMENT 'alpha', b VARCHAR(5) COMMENT 'bee', "
        "c INT COMMENT 'clear')"
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE altered ADD COLUMN d INT COMMENT 'dee' FIRST");
    failures += execute_statement_ok(
        database,
        "ALTER TABLE altered MODIFY COLUMN a BIGINT COMMENT 'modified'"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE altered CHANGE COLUMN b bb VARCHAR(7) COMMENT 'changed'"
    );
    failures += execute_statement_ok(database, "ALTER TABLE altered MODIFY COLUMN c INT");
    failures += execute_statement_ok(database, "ALTER TABLE altered RENAME COLUMN bb TO bbb");
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM altered",
        0U,
        show_full_columns_comment_column,
        "dee",
        "ADD COLUMN stores comment"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM altered",
        1U,
        show_full_columns_comment_column,
        "modified",
        "MODIFY COLUMN stores replacement comment"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM altered",
        2U,
        show_full_columns_comment_column,
        "changed",
        "CHANGE COLUMN stores and rename preserves comment"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM altered",
        3U,
        show_full_columns_comment_column,
        "",
        "MODIFY COLUMN without COMMENT clears old comment"
    );
    failures += expect_cell_contains(
        database,
        "SHOW CREATE TABLE altered",
        0U,
        1U,
        "`bbb` varchar(7) DEFAULT NULL COMMENT 'changed'",
        "SHOW CREATE after change and rename"
    );

    mylite_close(database);
    return failures;
}

static int test_temporary_comments_and_sql_modes(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open temp memory"
    );

    failures += create_app_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE temp_comment (a INT COMMENT 'temp')"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM temp_comment",
        0U,
        show_full_columns_comment_column,
        "temp",
        "temporary column comment"
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures +=
        execute_statement_ok(database, "CREATE TABLE no_backslash (a INT COMMENT 'a\\\\b')");
    failures += expect_cell(
        database,
        "SELECT COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'no_backslash' AND COLUMN_NAME = 'a'",
        0U,
        0U,
        "a\\\\b",
        "NO_BACKSLASH_ESCAPES stores literal backslashes"
    );
    failures += expect_cell_contains(
        database,
        "SHOW CREATE TABLE no_backslash",
        0U,
        1U,
        "COMMENT 'a\\\\\\\\b'",
        "NO_BACKSLASH_ESCAPES SHOW CREATE quoting"
    );

    mylite_close(database);
    return failures;
}

static int test_column_comment_diagnostics(void) {
    enum {
        prefix_length = sizeof("CREATE TABLE too_long (a INT COMMENT '") - 1U,
        comment_length = column_comment_max_characters + 1U,
        suffix_length = sizeof("')") - 1U,
    };

    static const unsigned char e_acute_utf8[] = {0xc3U, 0xa9U};
    char *multibyte_comment = NULL;
    char *multibyte_sql = NULL;
    char *sql = NULL;
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open diagnostics memory"
    );

    failures += create_app_schema(database);
    failures += execute_error(
        database,
        "CREATE TABLE equal_comment (a INT COMMENT='x')",
        strlen("CREATE TABLE equal_comment (a INT COMMENT='x')"),
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE numeric_comment (a INT COMMENT 123)",
        strlen("CREATE TABLE numeric_comment (a INT COMMENT 123)"),
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_comment (a INT COMMENT 'a\\0b')",
        strlen("CREATE TABLE nul_comment (a INT COMMENT 'a\\0b')"),
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "column comments do not support NUL bytes",
        }
    );

    multibyte_sql = make_repeated_comment_create_table_sql(
        "multibyte_ok",
        e_acute_utf8,
        sizeof(e_acute_utf8),
        column_comment_max_characters,
        &multibyte_comment
    );
    if (multibyte_sql == NULL || multibyte_comment == NULL) {
        fprintf(stderr, "failed to allocate multibyte column comment SQL\n");
        failures += 1;
    } else {
        failures += execute_statement_ok(database, multibyte_sql);
        failures += expect_cell(
            database,
            "SHOW FULL COLUMNS FROM multibyte_ok",
            0U,
            show_full_columns_comment_column,
            multibyte_comment,
            "1024 multibyte character column comment accepted"
        );
    }
    free(multibyte_sql);
    free(multibyte_comment);

    failures += execute_statement_ok(
        database,
        "CREATE TABLE four_byte_comment (a INT COMMENT '\360\237\231\202')"
    );
    failures += expect_cell(
        database,
        "SHOW FULL COLUMNS FROM four_byte_comment",
        0U,
        show_full_columns_comment_column,
        "?",
        "four-byte column comment replaced"
    );

    sql = malloc(prefix_length + comment_length + suffix_length + 1U);
    if (sql == NULL) {
        fprintf(stderr, "failed to allocate overlength column comment SQL\n");
        mylite_close(database);
        return failures + 1;
    }
    memcpy(sql, "CREATE TABLE too_long (a INT COMMENT '", prefix_length);
    memset(sql + prefix_length, 'a', comment_length);
    memcpy(sql + prefix_length + comment_length, "')", suffix_length);
    sql[prefix_length + comment_length + suffix_length] = '\0';
    failures += execute_error(
        database,
        sql,
        prefix_length + comment_length + suffix_length,
        (struct expected_sql_error){
            .code = mysql_error_column_comment_too_long,
            .sqlstate = "HY000",
            .message_part = "Comment for field 'a' is too long (max = 1024)",
        }
    );

    free(sql);
    mylite_close(database);
    return failures;
}

static int create_app_schema(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    failures += execute_statement_ok(database, "USE app");
    return failures;
}

static int expect_cell(
    mylite_db *database,
    const char *sql,
    size_t row_index,
    size_t column_index,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result) > row_index ? 1U : 0U, 1U, context);
    if (mylite_result_row_count(result) <= row_index ||
        mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected cell %zu/%zu\n", context, row_index, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, row_index, column_index);
        failures += mylite_test_expect_text(actual, expected, context);
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_cell_contains(
    mylite_db *database,
    const char *sql,
    size_t row_index,
    size_t column_index,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result) > row_index ? 1U : 0U, 1U, context);
    if (mylite_result_row_count(result) <= row_index ||
        mylite_result_column_count(result) <= column_index) {
        fprintf(stderr, "%s: expected cell %zu/%zu\n", context, row_index, column_index);
        failures += 1;
    } else {
        actual = mylite_result_value_text(result, row_index, column_index);
        failures += mylite_test_expect_contains(actual, needle, context);
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
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
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static char *make_repeated_comment_create_table_sql(
    const char *table_name,
    const unsigned char *unit,
    size_t unit_size,
    size_t repeat_count,
    char **out_comment
) {
    char prefix[repeated_comment_sql_prefix_capacity];
    char *comment = NULL;
    char *sql = NULL;
    size_t comment_length = 0U;
    size_t prefix_length = 0U;
    size_t sql_length = 0U;
    int written = 0;

    if (out_comment != NULL) {
        *out_comment = NULL;
    }
    if (table_name == NULL || unit == NULL || unit_size == 0U) {
        return NULL;
    }
    if (repeat_count > SIZE_MAX / unit_size) {
        return NULL;
    }
    comment_length = unit_size * repeat_count;
    written = snprintf(prefix, sizeof(prefix), "CREATE TABLE %s (a INT COMMENT '", table_name);
    if (written < 0 || (size_t)written >= sizeof(prefix)) {
        return NULL;
    }
    prefix_length = (size_t)written;
    if (prefix_length > SIZE_MAX - comment_length ||
        prefix_length + comment_length > SIZE_MAX - 2U) {
        return NULL;
    }
    sql_length = prefix_length + comment_length + 2U;

    if (out_comment != NULL) {
        comment = malloc(comment_length + 1U);
        if (comment == NULL) {
            return NULL;
        }
    }
    sql = malloc(sql_length + 1U);
    if (sql == NULL) {
        free(comment);
        return NULL;
    }

    memcpy(sql, prefix, prefix_length);
    for (size_t index = 0U; index < repeat_count; ++index) {
        memcpy(sql + prefix_length + (index * unit_size), unit, unit_size);
        if (comment != NULL) {
            memcpy(comment + (index * unit_size), unit, unit_size);
        }
    }
    sql[prefix_length + comment_length] = '\'';
    sql[prefix_length + comment_length + 1U] = ')';
    sql[prefix_length + comment_length + 2U] = '\0';
    if (comment != NULL) {
        comment[comment_length] = '\0';
        *out_comment = comment;
    }
    return sql;
}

static void remove_related_files(const char *path) {
    (void)remove(path);
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
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
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
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte sequence mismatch\n", context);
    return 1;
}
