#include <mylite/mylite.h>

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
    path_suffix_capacity = 16,
    mysql_collation_binary_id = 63,
    mysql_collation_utf8mb4_bin_id = 46,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_int_display_length = 11,
    mysql_varchar_20_display_length = 80,
    mysql_varchar_10_display_length = 40,
};

struct expected_column_metadata {
    const char *label;
    const char *schema_name;
    const char *table_name;
    const char *origin_schema_name;
    const char *origin_table_name;
    const char *origin_column_name;
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
};

static int test_descriptor_result_column_metadata(void);
static int test_result_column_metadata_scalar_defaults_and_misuse(void);
static int setup_metadata_schema(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_warning_count(mylite_db *database, const char *sql, size_t warnings);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column_index,
    struct expected_column_metadata expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_descriptor_result_column_metadata();
    failures += test_result_column_metadata_scalar_defaults_and_misuse();

    return failures == 0 ? 0 : 1;
}

static int test_descriptor_result_column_metadata(void) {
    enum {
        id_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_PRI_KEY |
                   MYLITE_RESULT_COLUMN_FLAG_AUTO_INCREMENT | MYLITE_RESULT_COLUMN_FLAG_PART_KEY |
                   MYLITE_RESULT_COLUMN_FLAG_NUM,
        unique_int_flags = MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY |
                           MYLITE_RESULT_COLUMN_FLAG_PART_KEY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        indexed_varchar_flags =
            MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY |
            MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT | MYLITE_RESULT_COLUMN_FLAG_PART_KEY,
        year_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_ZEROFILL |
                     MYLITE_RESULT_COLUMN_FLAG_NUM,
        unsigned_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_NUM,
    };

    static const struct expected_column_metadata selected_columns[] = {
        {
            .label = "ident",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "id",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
            .flags = id_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 11U,
            .decimals = 0U,
            .nullable = 0,
        },
        {
            .label = "i",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "i",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
            .flags = unique_int_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 11U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "label_v",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "v",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = indexed_varchar_flags,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = 80U,
            .decimals = 0U,
            .nullable = 0,
        },
        {
            .label = "ti",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "ti",
            .type = MYLITE_RESULT_COLUMN_TYPE_TINY,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 4U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "tiu",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "tiu",
            .type = MYLITE_RESULT_COLUMN_TYPE_TINY,
            .flags = unsigned_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 3U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "si",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "si",
            .type = MYLITE_RESULT_COLUMN_TYPE_SHORT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 6U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "mi",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "mi",
            .type = MYLITE_RESULT_COLUMN_TYPE_INT24,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 9U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "biu",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "biu",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = unsigned_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 20U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "d",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "d",
            .type = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 8U,
            .decimals = 2U,
            .nullable = 1,
        },
        {
            .label = "du",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "du",
            .type = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
            .flags = unsigned_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 7U,
            .decimals = 2U,
            .nullable = 1,
        },
        {
            .label = "f",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "f",
            .type = MYLITE_RESULT_COLUMN_TYPE_FLOAT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 12U,
            .decimals = 31U,
            .nullable = 1,
        },
        {
            .label = "x",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "x",
            .type = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 22U,
            .decimals = 31U,
            .nullable = 1,
        },
        {
            .label = "y",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "y",
            .type = MYLITE_RESULT_COLUMN_TYPE_YEAR,
            .flags = year_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 4U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "dt",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "dt",
            .type = MYLITE_RESULT_COLUMN_TYPE_DATE,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 10U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "tm",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "tm",
            .type = MYLITE_RESULT_COLUMN_TYPE_TIME,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 10U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "ts",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "ts",
            .type = MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 19U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "dttm",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "dttm",
            .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 19U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "c",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "c",
            .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
            .flags = 0U,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = 20U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "txt",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "txt",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = 262140U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "b",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "b",
            .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 3U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "vb",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "vb",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 4U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "blob_col",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "blob_col",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 65535U,
            .decimals = 0U,
            .nullable = 1,
        },
        {
            .label = "bitcol",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "bitcol",
            .type = MYLITE_RESULT_COLUMN_TYPE_BIT,
            .flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 5U,
            .decimals = 0U,
            .nullable = 1,
        },
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "descriptor") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open descriptor metadata");
    failures += setup_metadata_schema(database);
    failures += execute_ok(
        database,
        "SELECT id AS ident, i, v AS label_v, ti, tiu, si, mi, biu, d, du, f, x, y, "
        "dt, tm, ts, dttm, c, txt, b, vb, blob_col, bitcol FROM meta AS m LIMIT 0",
        &result
    );
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            sizeof(selected_columns) / sizeof(selected_columns[0]),
            "selected column count"
        );
        failures += expect_size(mylite_result_row_count(result), 0U, "metadata row count");
        failures += expect_size(mylite_result_warning_count(result), 0U, "metadata warnings");
        for (size_t index = 0U; index < sizeof(selected_columns) / sizeof(selected_columns[0]);
             ++index) {
            failures += expect_column_metadata(result, index, selected_columns[index], "selected");
        }
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT * FROM meta LIMIT 0", &result);
    if (failures == 0) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "id",
                .schema_name = "app",
                .table_name = "meta",
                .origin_schema_name = "app",
                .origin_table_name = "meta",
                .origin_column_name = "id",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
                .flags = id_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_int_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "wildcard first column"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "SET NAMES utf8mb4 COLLATE utf8mb4_bin");
    failures += execute_ok(database, "SELECT v FROM meta LIMIT 0", &result);
    if (failures == 0) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "v",
                .schema_name = "app",
                .table_name = "meta",
                .origin_schema_name = "app",
                .origin_table_name = "meta",
                .origin_column_name = "v",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = indexed_varchar_flags,
                .charset_id = mysql_collation_utf8mb4_bin_id,
                .collation_id = mysql_collation_utf8mb4_bin_id,
                .display_length = mysql_varchar_20_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "connection collation metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "SET NAMES utf8mb4");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE bin_default(v VARCHAR(10)) COLLATE=utf8mb4_bin"
    );
    failures += execute_ok(database, "SELECT v FROM bin_default LIMIT 0", &result);
    if (failures == 0) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "v",
                .schema_name = "app",
                .table_name = "bin_default",
                .origin_schema_name = "app",
                .origin_table_name = "bin_default",
                .origin_column_name = "v",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_varchar_10_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "binary table collation metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "metadata file preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen descriptor metadata");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_ok(database, "SELECT id AS ident FROM meta LIMIT 0", &result);
    if (failures == 0) {
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "ident",
                .schema_name = "app",
                .table_name = "meta",
                .origin_schema_name = "app",
                .origin_table_name = "meta",
                .origin_column_name = "id",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
                .flags = id_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_int_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "reopened descriptor metadata"
        );
    }
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_result_column_metadata_scalar_defaults_and_misuse(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open scalar metadata");
    failures += execute_ok(database, "SELECT 1 AS one", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, "scalar column count");
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "one",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
                .flags = 0U,
                .charset_id = 0U,
                .collation_id = 0U,
                .display_length = 0U,
                .decimals = 0U,
                .nullable = 1,
            },
            "scalar default metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DO 1", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "do column count");
        failures +=
            expect_text(mylite_result_column_schema_name(result, 0U), NULL, "invalid schema name");
        failures +=
            expect_text(mylite_result_column_table_name(result, 0U), NULL, "invalid table name");
        failures += expect_text(
            mylite_result_column_origin_schema_name(result, 0U),
            NULL,
            "invalid origin schema"
        );
        failures += expect_text(
            mylite_result_column_origin_table_name(result, 0U),
            NULL,
            "invalid origin table"
        );
        failures +=
            expect_text(mylite_result_column_origin_name(result, 0U), NULL, "invalid origin name");
        failures += expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
            "invalid column type"
        );
        failures += expect_uint32(mylite_result_column_flags(result, 0U), 0U, "invalid flags");
        failures +=
            expect_uint32(mylite_result_column_charset_id(result, 0U), 0U, "invalid charset");
        failures +=
            expect_uint32(mylite_result_column_collation_id(result, 0U), 0U, "invalid collation");
        failures += expect_uint64(
            mylite_result_column_display_length(result, 0U),
            0U,
            "invalid display length"
        );
        failures +=
            expect_uint16(mylite_result_column_decimals(result, 0U), 0U, "invalid decimals");
        failures += expect_int(mylite_result_column_nullable(result, 0U), 0, "invalid nullable");
    }
    mylite_result_free(result);
    mylite_close(database);

    failures += expect_text(mylite_result_column_schema_name(NULL, 0U), NULL, "NULL schema");
    failures += expect_text(mylite_result_column_table_name(NULL, 0U), NULL, "NULL table");
    failures +=
        expect_text(mylite_result_column_origin_schema_name(NULL, 0U), NULL, "NULL origin schema");
    failures +=
        expect_text(mylite_result_column_origin_table_name(NULL, 0U), NULL, "NULL origin table");
    failures += expect_text(mylite_result_column_origin_name(NULL, 0U), NULL, "NULL origin name");
    failures += expect_int(
        (int)mylite_result_column_type(NULL, 0U),
        MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
        "NULL type"
    );
    failures += expect_uint32(mylite_result_column_flags(NULL, 0U), 0U, "NULL flags");
    failures += expect_uint32(mylite_result_column_charset_id(NULL, 0U), 0U, "NULL charset");
    failures += expect_uint32(mylite_result_column_collation_id(NULL, 0U), 0U, "NULL collation");
    failures +=
        expect_uint64(mylite_result_column_display_length(NULL, 0U), 0U, "NULL display length");
    failures += expect_uint16(mylite_result_column_decimals(NULL, 0U), 0U, "NULL decimals");
    failures += expect_int(mylite_result_column_nullable(NULL, 0U), 0, "NULL nullable");
    return failures;
}

static int setup_metadata_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET NAMES utf8mb4");
    failures += expect_statement_warning_count(
        database,
        "CREATE TABLE meta("
        "id INT NOT NULL AUTO_INCREMENT, "
        "ti TINYINT, "
        "tiu TINYINT UNSIGNED, "
        "si SMALLINT, "
        "mi MEDIUMINT, "
        "i INT, "
        "biu BIGINT UNSIGNED, "
        "d DECIMAL(6,2), "
        "du DECIMAL(6,2) UNSIGNED, "
        "f FLOAT, "
        "x DOUBLE, "
        "y YEAR, "
        "dt DATE, "
        "tm TIME, "
        "ts TIMESTAMP NULL, "
        "dttm DATETIME, "
        "c CHAR(5), "
        "v VARCHAR(20) NOT NULL, "
        "txt TEXT, "
        "b BINARY(3), "
        "vb VARBINARY(4), "
        "blob_col BLOB, "
        "bitcol BIT(5), "
        "PRIMARY KEY(id), "
        "UNIQUE KEY u_i(i), "
        "KEY k_v(v(3))"
        ")",
        1U
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_warning_count(database, sql, 0U);
}

static int expect_statement_warning_count(mylite_db *database, const char *sql, size_t warnings) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_size(mylite_result_warning_count(result), warnings, sql);
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
        expect_text(mylite_result_column_name(result, column_index), expected.label, context);
    failures += expect_text(
        mylite_result_column_schema_name(result, column_index),
        expected.schema_name,
        context
    );
    failures += expect_text(
        mylite_result_column_table_name(result, column_index),
        expected.table_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_schema_name(result, column_index),
        expected.origin_schema_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_table_name(result, column_index),
        expected.origin_table_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_name(result, column_index),
        expected.origin_column_name,
        context
    );
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
    failures += expect_uint16(
        mylite_result_column_decimals(result, column_index),
        expected.decimals,
        context
    );
    failures +=
        expect_int(mylite_result_column_nullable(result, column_index), expected.nullable, context);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-result-column-metadata-%s-%d.mylite",
        name,
        current_process_id()
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    if (read_count != size) {
        fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, read_count);
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

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint16(uint16_t actual, uint16_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu16 ", got %" PRIu16 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu32 ", got %" PRIu32 "\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
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
