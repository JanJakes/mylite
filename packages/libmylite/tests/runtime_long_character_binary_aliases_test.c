#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdint.h>
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
    show_columns_field_count = 6,
    alias_column_count = 5,
    alias_added_column_count = 7,
    alias_metadata_column_count = 3,
    alias_projection_column_count = 5,
    alias_information_schema_column_count = 8,
    long_binary_information_schema_column_count = 5,
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mediumtext_result_display_length = 67108860,
    mediumblob_result_display_length = 16777215,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_blob_text_cant_have_default = 1101,
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

struct expected_bytes {
    const unsigned char *bytes;
    size_t size;
    bool is_null;
};

struct expected_column_metadata {
    const char *name;
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    int nullable;
};

static int test_alias_success_persistence_and_introspection(void);
static int test_alias_diagnostics(void);
static int test_alias_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_alias_rows(mylite_db *database, const char *context);
static int expect_alias_result_metadata(mylite_db *database);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column_index,
    struct expected_column_metadata expected,
    const char *context
);
static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
);
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
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_alias_success_persistence_and_introspection();
    failures += test_alias_diagnostics();
    failures += test_alias_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_alias_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",        "YES", "", NULL, "", "a", "mediumtext", "YES", "", NULL, "",
        "b",  "mediumtext", "YES", "", NULL, "", "c", "mediumblob", "YES", "", NULL, "",
        "nn", "mediumtext", "NO",  "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "aliases",
        "CREATE TABLE `aliases` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `a` mediumtext,\n"
        "  `b` mediumtext,\n"
        "  `c` mediumblob,\n"
        "  `nn` mediumtext NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "a",          "mediumtext",
        "16777215",   "16777215",
        "utf8mb4",    "utf8mb4_0900_ai_ci",
        "mediumtext", "YES",
        "b",          "mediumtext",
        "16777215",   "16777215",
        "utf8mb4",    "utf8mb4_0900_ai_ci",
        "mediumtext", "YES",
        "c",          "mediumblob",
        "16777215",   "16777215",
        NULL,         NULL,
        "mediumblob", "YES",
        "nn",         "mediumtext",
        "16777215",   "16777215",
        "utf8mb4",    "utf8mb4_0900_ai_ci",
        "mediumtext", "NO",
    };
    static const char *const show_columns_after_add_rows[] = {
        "id",         "int",        "YES", "", NULL, "", "a",     "mediumtext", "YES", "", NULL, "",
        "b",          "mediumtext", "YES", "", NULL, "", "c",     "mediumblob", "YES", "", NULL, "",
        "nn",         "mediumtext", "NO",  "", NULL, "", "added", "mediumtext", "NO",  "", NULL, "",
        "added_blob", "mediumblob", "YES", "", NULL, "",
    };
    static const char *const add_column_rows[] = {"1", "", NULL, "2", "", NULL};
    static const char *const like_show_create_rows[] = {
        "like_aliases",
        "CREATE TABLE `like_aliases` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `a` mediumtext,\n"
        "  `b` mediumtext,\n"
        "  `c` mediumblob,\n"
        "  `nn` mediumtext NOT NULL,\n"
        "  `added` mediumtext NOT NULL,\n"
        "  `added_blob` mediumblob\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ctas_show_create_rows[] = {
        "ctas_aliases",
        "CREATE TABLE `ctas_aliases` (\n"
        "  `a` mediumtext,\n"
        "  `b` mediumtext,\n"
        "  `c` mediumblob\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const attrs_show_create_rows[] = {
        "attrs",
        "CREATE TABLE `attrs` (\n"
        "  `a` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,\n"
        "  `b` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci,\n"
        "  `c` mediumblob NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const long_binary_show_create_rows[] = {
        "long_binary",
        "CREATE TABLE `long_binary` (\n"
        "  `a` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const long_binary_information_schema_rows[] = {
        "a",
        "mediumtext",
        "utf8mb4",
        "utf8mb4_bin",
        "mediumtext",
    };
    static const char *const alias_defaults_show_create_rows[] = {
        "alias_defaults",
        "CREATE TABLE `alias_defaults` (\n"
        "  `a` mediumblob DEFAULT (0x4100),\n"
        "  `n` mediumblob DEFAULT (NULL)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const unsigned char alias_default[] = {0x41U, 0x00U};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alias success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE aliases (id INT, a LONG, b LONG VARCHAR, c LONG VARBINARY, "
        "nn LONG NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alias_column_count,
            .context = "alias SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE aliases",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "alias SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "IS_NULLABLE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'aliases' AND COLUMN_NAME <> 'id' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = alias_information_schema_column_count,
            .row_count = 4U,
            .context = "alias INFORMATION_SCHEMA.COLUMNS",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO aliases VALUES "
        "(1, 'text', 'body', X'4142', 'nn'), "
        "(2, NULL, NULL, NULL, '')",
        2
    );
    failures += expect_dml_ok(database, "UPDATE aliases SET b = 'updated' WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE aliases SET b = 'updated' WHERE id = 1", 0);
    failures += expect_dml_ok(database, "UPDATE aliases SET c = X'43' WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE aliases SET c = X'43' WHERE id = 1", 0);
    failures += expect_alias_rows(database, "updated alias rows");
    failures += expect_alias_result_metadata(database);

    failures +=
        expect_statement_ok(database, "ALTER TABLE aliases ADD COLUMN added LONG VARCHAR NOT NULL");
    failures +=
        expect_statement_ok(database, "ALTER TABLE aliases ADD COLUMN added_blob LONG VARBINARY");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases",
            .values = show_columns_after_add_rows,
            .column_count = show_columns_field_count,
            .row_count = alias_added_column_count,
            .context = "alias SHOW COLUMNS after add",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added, added_blob FROM aliases ORDER BY id",
            .values = add_column_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "alias ALTER ADD readback",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE like_aliases LIKE aliases");
    failures +=
        expect_statement_ok(database, "CREATE TABLE ctas_aliases AS SELECT a, b, c FROM aliases");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE like_aliases",
            .values = like_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "alias CREATE TABLE LIKE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ctas_aliases",
            .values = ctas_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "alias CREATE TABLE SELECT",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE attrs (a LONG CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
        "b LONG VARCHAR CHARACTER SET utf8mb4, c LONG VARBINARY NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE attrs",
            .values = attrs_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "alias charset attributes",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE long_binary (a LONG BINARY)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE long_binary",
            .values = long_binary_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "LONG BINARY SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'long_binary' ORDER BY ORDINAL_POSITION",
            .values = long_binary_information_schema_rows,
            .column_count = long_binary_information_schema_column_count,
            .row_count = 1U,
            .context = "LONG BINARY INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE alias_defaults (a LONG VARBINARY DEFAULT (X'4100'), "
        "n LONG VARBINARY DEFAULT (NULL))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE alias_defaults",
            .values = alias_defaults_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "LONG VARBINARY expression default SHOW CREATE TABLE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO alias_defaults () VALUES ()", 1);
    failures += execute_ok(database, "SELECT a, n FROM alias_defaults", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = alias_default, .size = sizeof(alias_default)},
        "LONG VARBINARY expression default materializes"
    );
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.is_null = true},
        "LONG VARBINARY NULL expression default materializes"
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read alias preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "alias file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alias success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_alias_rows(database, "reopened alias rows");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alias_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alias diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_varchar_len (a LONG VARCHAR(10))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_varbinary_len (a LONG VARBINARY(10))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_text (a LONG TEXT)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_default_literal (a LONG DEFAULT 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "can't have a default value",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE long_default_expr (a LONG DEFAULT ('abc'))");
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_varbinary_default_literal (a LONG VARBINARY DEFAULT X'41')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE long_varbinary_default_expr (a LONG VARBINARY DEFAULT (X'41'))"
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_varbinary_charset (a LONG VARBINARY CHARACTER SET utf8mb4)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR, VARCHAR, and TEXT",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_long_varbinary_collate (a LONG VARBINARY COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CHAR, VARCHAR, and TEXT",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_alias_independent_handles(void) {
    static const unsigned char first_blob[] = {0x61U, 0x61U};
    static const unsigned char second_blob[] = {0x62U, 0x62U};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_result *result = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first alias file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second alias file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures +=
        expect_statement_ok(first, "CREATE TABLE t (id INT, body LONG, bin LONG VARBINARY)");
    failures +=
        expect_statement_ok(second, "CREATE TABLE t (id INT, body LONG, bin LONG VARBINARY)");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 'one', X'6161')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 'two', X'6262')", 1);
    failures += execute_ok(first, "SELECT body, bin FROM t WHERE id = 1", &result);
    failures += expect_result_value(result, 0U, 0U, "one", "first independent alias text");
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = first_blob, .size = sizeof(first_blob)},
        "first independent alias binary"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "SELECT body, bin FROM t WHERE id = 1", &result);
    failures += expect_result_value(result, 0U, 0U, "two", "second independent alias text");
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = second_blob, .size = sizeof(second_blob)},
        "second independent alias binary"
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    failures += expect_true(result == NULL, "failed execution result");
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        size_t row = value_index / query.column_count;
        size_t column = value_index % query.column_count;

        failures +=
            expect_result_value(result, row, column, query.values[value_index], query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_alias_rows(mylite_db *database, const char *context) {
    static const char *const expected_values[] = {
        "1",
        "text",
        "updated",
        "nn",
        "2",
        NULL,
        NULL,
        "",
    };
    static const unsigned char updated_blob[] = {0x43U};
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT id, a, b, c, nn FROM aliases ORDER BY id", &result);

    failures +=
        expect_size(mylite_result_column_count(result), alias_projection_column_count, context);
    failures += expect_size(mylite_result_row_count(result), 2U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < 2U; ++row) {
        for (size_t column = 0U; column < 3U; ++column) {
            size_t index = (row * 4U) + column;

            failures += expect_result_value(result, row, column, expected_values[index], context);
        }
        failures += expect_result_value(result, row, 4U, expected_values[(row * 4U) + 3U], context);
    }
    failures += expect_binary_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = updated_blob, .size = sizeof(updated_blob)},
        context
    );
    failures +=
        expect_binary_cell(result, 1U, 3U, (struct expected_bytes){.is_null = true}, context);
    mylite_result_free(result);
    return failures;
}

static int expect_alias_result_metadata(mylite_db *database) {
    static const struct expected_column_metadata expected[] = {
        {
            .name = "a",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mediumtext_result_display_length,
            .nullable = 1,
        },
        {
            .name = "b",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mediumtext_result_display_length,
            .nullable = 1,
        },
        {
            .name = "c",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mediumblob_result_display_length,
            .nullable = 1,
        },
    };
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT a, b, c FROM aliases ORDER BY id LIMIT 1", &result);

    failures += expect_size(
        mylite_result_column_count(result),
        alias_metadata_column_count,
        "alias metadata column count"
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "alias metadata row count");
    for (size_t column = 0U; column < alias_metadata_column_count; ++column) {
        failures +=
            expect_column_metadata(result, column, expected[column], "alias result metadata");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column_index,
    struct expected_column_metadata expected,
    const char *context
) {
    int failures = 0;

    failures +=
        expect_text(mylite_result_column_name(result, column_index), expected.name, context);
    failures += expect_text(mylite_result_column_schema_name(result, column_index), "app", context);
    failures +=
        expect_text(mylite_result_column_table_name(result, column_index), "aliases", context);
    failures +=
        expect_text(mylite_result_column_origin_schema_name(result, column_index), "app", context);
    failures += expect_text(
        mylite_result_column_origin_table_name(result, column_index),
        "aliases",
        context
    );
    failures +=
        expect_text(mylite_result_column_origin_name(result, column_index), expected.name, context);
    failures += expect_int(
        (int)mylite_result_column_type(result, column_index),
        (int)expected.type,
        context
    );
    failures +=
        expect_uint32(mylite_result_column_flags(result, column_index), expected.flags, context);
    failures += expect_uint32(
        mylite_result_column_charset_id(result, column_index),
        expected.charset_id,
        context
    );
    failures += expect_uint32(
        mylite_result_column_collation_id(result, column_index),
        expected.collation_id,
        context
    );
    failures += expect_uint64(
        mylite_result_column_display_length(result, column_index),
        expected.display_length,
        context
    );
    failures += expect_uint16(mylite_result_column_decimals(result, column_index), 0U, context);
    failures +=
        expect_int(mylite_result_column_nullable(result, column_index), expected.nullable, context);
    return failures;
}

static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
) {
    const unsigned char *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = 0;

    if (expected.is_null) {
        failures += expect_true(actual == NULL, context);
        failures += expect_size(actual_size, 0U, context);
        return failures;
    }
    failures += expect_true(actual != NULL, context);
    failures += expect_size(actual_size, expected.size, context);
    if (actual != NULL && actual_size == expected.size) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_long_character_binary_aliases_%d_%s.mylite",
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    if (ferror(file) != 0) {
        fclose(file);
        return 1;
    }
    fclose(file);
    return bytes_read == size ? 0 : 1;
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

static int expect_uint16(uint16_t actual, uint16_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
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
            "%s: expected '%s', got '%s'\n",
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
    if (actual == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
