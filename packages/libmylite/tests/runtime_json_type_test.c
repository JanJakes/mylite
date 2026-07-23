#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    information_schema_column_count = 9,
    json_docs_row_count = 6,
    mysql_collation_binary_id = 63,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_blob_text_cant_have_default = 1101,
    mysql_error_invalid_json_text = 3140,
    mysql_error_json_used_as_key = 3152,
};

static const uint64_t longtext_max_length = 4294967295ULL;

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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_json_success_metadata_dml_and_persistence(void);
static int test_json_diagnostics(void);
static int test_independent_json_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_json_result_metadata(mylite_db *database);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
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

    failures += test_json_success_metadata_dml_and_persistence();
    failures += test_json_diagnostics();
    failures += test_independent_json_handles();

    return failures == 0 ? 0 : 1;
}

static int test_json_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "payload",
        "json",
        "YES",
        "",
        NULL,
        "",
        "required",
        "json",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_create_rows[] = {
        "json_docs",
        "CREATE TABLE `json_docs` (\n"
        "  `id` int DEFAULT NULL,\n"
        "  `payload` json DEFAULT NULL,\n"
        "  `required` json NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id",       "int",  "int",  NULL, NULL, "10", "0",  "YES", NULL,
        "payload",  "json", "json", NULL, NULL, NULL, NULL, "YES", NULL,
        "required", "json", "json", NULL, NULL, NULL, NULL, "NO",  NULL,
    };
    static const char *const inserted_rows[] = {
        "1",
        "{\"a\": 1, \"b\": 2}",
        "{\"x\": 2}",
        "2",
        "null",
        "true",
        "3",
        NULL,
        "false",
        "4",
        "[1, 2, 3]",
        "123",
        "5",
        "\"hello\"",
        "\"world\"",
        "6",
        "1.5",
        "1e2",
    };
    static const char *const null_predicate_rows[] = {"2", "3"};
    static const char *const not_null_predicate_rows[] = {"1", "4", "5", "6"};
    static const char *const altered_rows[] = {"1", NULL, "null"};
    static const char *const ignored_rows[] = {"1", "null"};
    static const char *const copied_rows[] = {"{\"source\": 1}"};
    static const char *const escaped_rows[] = {
        "{\"q\": \"a\\\"b\", \"u\": \"A\", \"nl\": \"line\\n\"}"
    };
    static const char *const persisted_rows[] = {
        "1",
        "{\"updated\": 1}",
        "2",
        NULL,
        "3",
        NULL,
        "4",
        "{\"source\": 1}",
        "5",
        "\"hello\"",
        "6",
        "1.5",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open JSON database");
    failures += expect_statement_ok(database, "CREATE SCHEMA app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE json_docs (id INT, payload JSON, required JSON NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM json_docs",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "SHOW COLUMNS JSON metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE json_docs",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "SHOW CREATE TABLE JSON metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, IS_NULLABLE, "
                   "COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'json_docs' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = 3U,
            .context = "INFORMATION_SCHEMA JSON metadata",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO json_docs VALUES "
        "(1, '{\"b\":2,\"a\":1}', '{\"x\":1,\"x\":2}'),"
        "(2, 'null', 'true'),"
        "(3, NULL, 'false'),"
        "(4, '[1,2,3]', '123'),"
        "(5, '\"hello\"', '\"world\"'),"
        "(6, '1.5', '1e2')",
        (struct expected_dml_result){.affected_rows = json_docs_row_count, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, payload, required FROM json_docs ORDER BY id",
            .values = inserted_rows,
            .column_count = 3U,
            .row_count = json_docs_row_count,
            .context = "canonical JSON readback",
        }
    );
    failures += expect_json_result_metadata(database);
    failures += expect_dml_ok(
        database,
        "UPDATE json_docs SET payload = '{\"updated\":1}' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE json_docs SET payload = '{\"updated\":1}' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE json_docs SET payload = NULL WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM json_docs WHERE payload IS NULL ORDER BY id",
            .values = null_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "JSON IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM json_docs WHERE payload IS NOT NULL ORDER BY id",
            .values = not_null_predicate_rows,
            .column_count = 1U,
            .row_count = 4U,
            .context = "JSON IS NOT NULL predicate",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_json (id JSON)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO alter_json VALUES ('1')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(database, "ALTER TABLE alter_json ADD COLUMN j JSON");
    failures += expect_statement_ok(database, "ALTER TABLE alter_json ADD COLUMN nn JSON NOT NULL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, j, nn FROM alter_json",
            .values = altered_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "ALTER ADD JSON backfill",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE ignore_json (id INT, j JSON NOT NULL)");
    failures += expect_dml_ok(
        database,
        "INSERT IGNORE INTO ignore_json(id) VALUES(1)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, j FROM ignore_json",
            .values = ignored_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "INSERT IGNORE JSON NOT NULL implicit value",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE source_json (id INT, j JSON)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO source_json VALUES (1, '{\"source\":1}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE json_docs SET payload = (SELECT j FROM source_json WHERE id = 1) WHERE id = 4",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE copied_json AS SELECT payload FROM json_docs WHERE id = 4"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT payload FROM copied_json",
            .values = copied_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "CREATE TABLE SELECT JSON copy",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE escape_json (j JSON)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO escape_json VALUES "
        "('{\"q\":\"a\\\\\"b\",\"nl\":\"line\\\\n\",\"u\":\"\\\\u0041\"}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT j FROM escape_json",
            .values = escaped_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "JSON string escape normalization",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "JSON writes preserve MyLite preamble"
    );
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen JSON database");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, payload FROM json_docs ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = json_docs_row_count,
            .context = "JSON persistence after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open JSON diagnostic db");
    failures += expect_statement_ok(database, "CREATE SCHEMA app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "CREATE TABLE t (id INT, j JSON, nn JSON NOT NULL)");

    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, '{bad}', 'null')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 1, 'null')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON values support only string",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 'null', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default_null (j JSON NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'j'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_bare_default (j JSON DEFAULT '{}')",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "can't have a default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expr_default (j JSON DEFAULT ('{}'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON expression defaults are not yet supported",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_pk (j JSON PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_json_used_as_key,
            .sqlstate = "42000",
            .message_part = "JSON column 'j' supports indexing",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_key (j JSON, KEY k(j))",
        (struct expected_sql_error){
            .code = mysql_error_json_used_as_key,
            .sqlstate = "42000",
            .message_part = "JSON column 'j' supports indexing",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_unique (j JSON, UNIQUE KEY k(j))",
        (struct expected_sql_error){
            .code = mysql_error_json_used_as_key,
            .sqlstate = "42000",
            .message_part = "JSON column 'j' supports indexing",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_json_handles(void) {
    static const char *const first_rows[] = {"{\"a\": 1}"};
    static const char *const second_rows[] = {"{\"b\": 2}"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first JSON handle"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second JSON handle"
    );
    failures += expect_statement_ok(first, "CREATE SCHEMA app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "CREATE SCHEMA app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (j JSON)");
    failures += expect_statement_ok(second, "CREATE TABLE t (j JSON)");
    failures += expect_dml_ok(
        first,
        "INSERT INTO t VALUES ('{\"a\":1}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO t VALUES ('{\"b\":2}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT j FROM t",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first JSON handle state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT j FROM t",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second JSON handle state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error %d\n", sql, expected.code);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            sql
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            sql
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t index = (row * query.column_count) + column;

                failures +=
                    expect_result_value(result, row, column, query.values[index], query.context);
            }
        }
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_json_result_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT payload FROM json_docs WHERE id = 1", &result);

    if (failures == 0) {
        uint32_t flags = mylite_result_column_flags(result, 0U);

        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            1U,
            "JSON metadata columns"
        );
        failures += mylite_test_expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "JSON result type"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_charset_id(result, 0U),
            mysql_collation_binary_id,
            "JSON result charset"
        );
        failures += mylite_test_expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_collation_binary_id,
            "JSON result collation"
        );
        failures += mylite_test_expect_uint64(
            mylite_result_column_display_length(result, 0U),
            longtext_max_length,
            "JSON display length"
        );
        failures += mylite_test_expect_true(
            (flags & MYLITE_RESULT_COLUMN_FLAG_BLOB) != 0U,
            "JSON result BLOB flag"
        );
        failures += mylite_test_expect_true(
            (flags & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
            "JSON result binary flag"
        );
        failures += mylite_test_expect_true(
            mylite_result_column_nullable(result, 0U) != 0,
            "JSON nullable"
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected NULL, got %s\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }

    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s row %zu column %zu: expected %s, got %s\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
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
        fprintf(stderr, "failed to seek in %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
