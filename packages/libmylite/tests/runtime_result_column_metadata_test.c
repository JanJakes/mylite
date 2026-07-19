#include <mylite/mylite.h>

#include "runtime/mylite_execution_text_internal.h"
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
    mysql_scalar_bigint_display_length = 21,
    mysql_scalar_double_display_length = 23,
    mysql_database_function_display_length = 256,
    mysql_user_function_display_length = 1152,
    mysql_version_function_display_length = 20,
    mysql_json_value_display_length = 2048,
    mysql_json_type_display_length = 68,
    mysql_scalar_var_string_decimals = 31,
    mysql_date_display_length = 10,
    mysql_datetime_display_length = 19,
    mysql_temporal_function_string_display_length = 116,
    mysql_literal_string_abc_display_length = 12,
    mysql_bigint_literal_display_length = 20,
    mysql_large_integer_literal_display_length = 82,
    mysql_timestampadd_string_column_index = 3,
    mysql_session_scalar_column_count = 11,
    mysql_user_function_first_column_index = 2,
    mysql_version_column_index = 6,
    mysql_row_count_column_index = 7,
    mysql_last_insert_id_column_index = 8,
    mysql_unseeded_rand_column_index = 9,
    mysql_seeded_rand_column_index = 10,
    mysql_json_scalar_column_count = 10,
    mysql_json_value_column_index = 2,
    mysql_json_length_column_index = 3,
    mysql_json_contains_column_index = 4,
    mysql_json_contains_path_column_index = 5,
    mysql_json_quote_column_index = 6,
    mysql_json_document_first_column_index = 7,
    mysql_row_scalar_column_count = 11,
    mysql_row_json_type_column_index = 5,
    mysql_row_json_value_column_index = 6,
    mysql_row_json_quote_column_index = 7,
    mysql_row_json_document_first_column_index = 8,
    mysql_show_name_display_length = 256,
    mysql_show_table_type_display_length = 44,
    mysql_show_column_type_display_length = 67108860,
    mysql_show_process_id_display_length = 22,
    mysql_show_warning_code_display_length = 5,
    mysql_show_fallback_display_length = 4096,
};

static const uint64_t mysql_longtext_display_length = 4294967295ULL;
static const uint64_t mysql_json_document_display_length = 4294967292ULL;

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
static int test_show_result_column_metadata(void);
static int test_source_span_copy_bounds(void);
static int setup_metadata_schema(mylite_db *database);
static int expect_show_column_metadata(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    struct expected_column_metadata expected,
    const char *context
);
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
    failures += test_show_result_column_metadata();
    failures += test_source_span_copy_bounds();

    return failures == 0 ? 0 : 1;
}

static int test_source_span_copy_bounds(void) {
    static const char source[] = "x";
    mylite_db *database = NULL;
    char *text = (char *)source;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open span bounds");

    if (database != NULL) {
        failures += expect_int(
            mylite_execution_copy_source_span_text(
                database,
                &(struct mylite_sql_source_span){
                    .text = source,
                    .length = 2U,
                    .offset = 0U,
                    .source_length = sizeof(source) - 1U,
                },
                &text
            ),
            MYLITE_ERROR,
            "reject out-of-bounds source span copy"
        );
        failures += expect_int(text == NULL, 1, "clear rejected source span output");
    }
    mylite_close(database);
    return failures;
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
            .display_length = mysql_datetime_display_length,
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
            .display_length = mysql_datetime_display_length,
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
            .label = "ltxt",
            .schema_name = "app",
            .table_name = "m",
            .origin_schema_name = "app",
            .origin_table_name = "meta",
            .origin_column_name = "ltxt",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_longtext_display_length,
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
        "dt, tm, ts, dttm, c, txt, ltxt, b, vb, blob_col, bitcol FROM meta AS m LIMIT 0",
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
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_varchar_20_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "descriptor collation metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT RAND(), RAND(0) FROM meta LIMIT 0", &result);
    if (failures == 0) {
        enum {
            not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                            MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                            MYLITE_RESULT_COLUMN_FLAG_NUM,
            table_rand_column_count = 2,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            table_rand_column_count,
            "table RAND metadata column count"
        );
        failures += expect_size(mylite_result_row_count(result), 0U, "table RAND metadata rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "table RAND metadata warnings");
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "RAND()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "table RAND metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "RAND(0)",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "table seeded RAND metadata"
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
                .charset_id = mysql_collation_utf8mb4_bin_id,
                .collation_id = mysql_collation_utf8mb4_bin_id,
                .display_length = mysql_varchar_10_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "binary table collation metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT sys.format_bytes(1024)", &result);
    if (failures == 0) {
        failures += expect_text(
            mylite_result_column_name(result, 0U),
            "sys.format_bytes(1024)",
            "qualified sys function keeps source label"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT /*!80000 1 */ + 2", &result);
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            1U,
            "executable comment expression column count"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, 0U),
            "3",
            "executable comment expression value"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT DATE_ADD(dt, INTERVAL 1 DAY) AS d_day, "
        "DATE_ADD(dt, INTERVAL 1 MINUTE) AS d_minute, "
        "DATE_ADD(dttm, INTERVAL 1 MONTH) AS dttm_month, "
        "DATE_SUB(ts, INTERVAL 1 WEEK) AS ts_week, "
        "DATE_ADD(v, INTERVAL 1 DAY) AS v_day FROM meta LIMIT 0",
        &result
    );
    if (failures == 0) {
        enum {
            date_interval_row_column_count = 5,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            date_interval_row_column_count,
            "row DATE interval metadata column count"
        );
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "d_day",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATE,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_date_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row DATE_ADD DATE unit metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "d_minute",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row DATE_ADD DATE time-unit metadata"
        );
        failures += expect_column_metadata(
            result,
            2U,
            (struct expected_column_metadata){
                .label = "dttm_month",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row DATE_ADD DATETIME metadata"
        );
        failures += expect_column_metadata(
            result,
            3U,
            (struct expected_column_metadata){
                .label = "ts_week",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row DATE_SUB TIMESTAMP metadata"
        );
        failures += expect_column_metadata(
            result,
            4U,
            (struct expected_column_metadata){
                .label = "v_day",
                .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_temporal_function_string_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "row DATE_ADD string metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, dt) AS d_shift, "
        "TIMESTAMPADD(SECOND, 1, dttm) AS dttm_shift, "
        "TIMESTAMPADD(SECOND, 1, ts) AS ts_shift, "
        "TIMESTAMPADD(SECOND, 1, v) AS v_shift FROM meta LIMIT 0",
        &result
    );
    if (failures == 0) {
        enum {
            timestampadd_row_column_count = 4,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            timestampadd_row_column_count,
            "row TIMESTAMPADD metadata column count"
        );
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "d_shift",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row TIMESTAMPADD DATE metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "dttm_shift",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row TIMESTAMPADD DATETIME metadata"
        );
        failures += expect_column_metadata(
            result,
            2U,
            (struct expected_column_metadata){
                .label = "ts_shift",
                .type = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_datetime_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "row TIMESTAMPADD TIMESTAMP metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_timestampadd_string_column_index,
            (struct expected_column_metadata){
                .label = "v_shift",
                .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_temporal_function_string_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "row TIMESTAMPADD string metadata"
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
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "scalar") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open scalar metadata");
    failures += execute_ok(
        database,
        "SELECT 1 AS one, TRUE AS truthy, NULL AS nil, 'abc' AS str FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                            MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                            MYLITE_RESULT_COLUMN_FLAG_NUM,
            null_binary_numeric_flags =
                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        failures += expect_size(mylite_result_column_count(result), 4U, "scalar column count");
        failures += expect_size(mylite_result_row_count(result), 1U, "scalar row count");
        failures += expect_size(mylite_result_warning_count(result), 0U, "scalar warnings");
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
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = 2U,
                .decimals = 0U,
                .nullable = 0,
            },
            "integer scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "truthy",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = 1U,
                .decimals = 0U,
                .nullable = 0,
            },
            "boolean scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            2U,
            (struct expected_column_metadata){
                .label = "nil",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_NULL,
                .flags = null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = 0U,
                .decimals = 0U,
                .nullable = 1,
            },
            "NULL scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_json_length_column_index,
            (struct expected_column_metadata){
                .label = "str",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_literal_string_abc_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "string scalar metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT TIMESTAMPADD(SECOND, 1, '2008-01-02') AS shifted, "
        "DATE_ADD('2008-01-02', INTERVAL 1 SECOND) AS added FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            date_interval_scalar_column_count = 2,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            date_interval_scalar_column_count,
            "date interval scalar column count"
        );
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "shifted",
                .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_temporal_function_string_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "TIMESTAMPADD scalar string metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "added",
                .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_temporal_function_string_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "DATE_ADD scalar string metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT 18446744073709551615 AS umax, -9223372036854775808 AS nmin, "
        "-9223372036854775809 AS below_min FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                            MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                            MYLITE_RESULT_COLUMN_FLAG_NUM,
            not_null_unsigned_binary_numeric_flags =
                MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        failures += expect_size(mylite_result_column_count(result), 3U, "integer boundary columns");
        failures += expect_size(mylite_result_row_count(result), 1U, "integer boundary rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "integer boundary warnings");
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "umax",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_unsigned_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_bigint_literal_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "unsigned max integer scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "nmin",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_bigint_literal_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "signed min integer scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            2U,
            (struct expected_column_metadata){
                .label = "below_min",
                .type = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_bigint_literal_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "below signed min integer scalar metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT "
        "999999999999999999999999999"
        "999999999999999999999999999"
        "999999999999999999999999999 AS big, -"
        "999999999999999999999999999"
        "999999999999999999999999999"
        "999999999999999999999999999 AS neg_big FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                            MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                            MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        static const char *const labels[] = {"big", "neg_big"};

        failures += expect_size(mylite_result_column_count(result), 2U, "large integer columns");
        failures += expect_size(mylite_result_row_count(result), 1U, "large integer rows");
        failures += expect_size(mylite_result_warning_count(result), 0U, "large integer warnings");
        for (size_t index = 0U; index < 2U; ++index) {
            failures += expect_column_metadata(
                result,
                index,
                (struct expected_column_metadata){
                    .label = labels[index],
                    .schema_name = "",
                    .table_name = "",
                    .origin_schema_name = "",
                    .origin_table_name = "",
                    .origin_column_name = "",
                    .type = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
                    .flags = not_null_binary_numeric_flags,
                    .charset_id = mysql_collation_binary_id,
                    .collation_id = mysql_collation_binary_id,
                    .display_length = mysql_large_integer_literal_display_length,
                    .decimals = 0U,
                    .nullable = 0,
                },
                "large integer scalar metadata"
            );
        }
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT DATABASE(), SCHEMA(), USER(), SESSION_USER(), SYSTEM_USER(), CURRENT_USER(), "
        "VERSION(), ROW_COUNT(), LAST_INSERT_ID(), RAND(), RAND(0) FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                            MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                            MYLITE_RESULT_COLUMN_FLAG_NUM,
            not_null_unsigned_binary_numeric_flags =
                MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            mysql_session_scalar_column_count,
            "session column count"
        );
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "DATABASE()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_database_function_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "DATABASE scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "SCHEMA()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = 0U,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_database_function_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "SCHEMA scalar metadata"
        );
        {
            static const char *const user_function_labels[] = {
                "USER()",
                "SESSION_USER()",
                "SYSTEM_USER()",
                "CURRENT_USER()",
            };
            static const char *const user_function_contexts[] = {
                "USER scalar metadata",
                "SESSION_USER scalar metadata",
                "SYSTEM_USER scalar metadata",
                "CURRENT_USER scalar metadata",
            };

            for (size_t index = 0U;
                 index < sizeof(user_function_labels) / sizeof(user_function_labels[0]);
                 ++index) {
                failures += expect_column_metadata(
                    result,
                    mysql_user_function_first_column_index + index,
                    (struct expected_column_metadata){
                        .label = user_function_labels[index],
                        .schema_name = "",
                        .table_name = "",
                        .origin_schema_name = "",
                        .origin_table_name = "",
                        .origin_column_name = "",
                        .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                        .flags = 0U,
                        .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                        .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                        .display_length = mysql_user_function_display_length,
                        .decimals = mysql_scalar_var_string_decimals,
                        .nullable = 1,
                    },
                    user_function_contexts[index]
                );
            }
        }
        failures += expect_column_metadata(
            result,
            mysql_version_column_index,
            (struct expected_column_metadata){
                .label = "VERSION()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_version_function_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "VERSION scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_row_count_column_index,
            (struct expected_column_metadata){
                .label = "ROW_COUNT()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "ROW_COUNT scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_last_insert_id_column_index,
            (struct expected_column_metadata){
                .label = "LAST_INSERT_ID()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = not_null_unsigned_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "LAST_INSERT_ID scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_unseeded_rand_column_index,
            (struct expected_column_metadata){
                .label = "RAND()",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "RAND scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_seeded_rand_column_index,
            (struct expected_column_metadata){
                .label = "RAND(0)",
                .schema_name = "",
                .table_name = "",
                .origin_schema_name = "",
                .origin_table_name = "",
                .origin_column_name = "",
                .type = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
                .flags = not_null_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 0,
            },
            "seeded RAND scalar metadata"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT JSON_VALID('{}') AS valid_json, JSON_TYPE('{}') AS json_type, "
        "JSON_VALUE('{\"a\":1}','$.a') AS json_value, "
        "JSON_LENGTH('{}') AS json_length, JSON_CONTAINS('{}','{}') AS contains_json, "
        "JSON_CONTAINS_PATH('{}','one','$') AS contains_path, "
        "JSON_QUOTE('abc') AS quoted_json, "
        "JSON_EXTRACT('{\"a\":1}','$.a') AS extracted_json, "
        "JSON_ARRAY(1,'a') AS array_json, JSON_OBJECT('a',1) AS object_json FROM DUAL",
        &result
    );
    if (failures == 0) {
        enum {
            nullable_binary_numeric_flags =
                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            mysql_json_scalar_column_count,
            "json scalar columns"
        );
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "valid_json",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = nullable_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "JSON_VALID scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            1U,
            (struct expected_column_metadata){
                .label = "json_type",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_type_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "JSON_TYPE scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_json_value_column_index,
            (struct expected_column_metadata){
                .label = "json_value",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_value_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "JSON_VALUE scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            3U,
            (struct expected_column_metadata){
                .label = "json_length",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = nullable_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "JSON_LENGTH scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_json_contains_column_index,
            (struct expected_column_metadata){
                .label = "contains_json",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = nullable_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "JSON_CONTAINS scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_json_contains_path_column_index,
            (struct expected_column_metadata){
                .label = "contains_path",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                .flags = nullable_binary_numeric_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .nullable = 1,
            },
            "JSON_CONTAINS_PATH scalar metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_json_quote_column_index,
            (struct expected_column_metadata){
                .label = "quoted_json",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_document_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "JSON_QUOTE scalar metadata"
        );
        for (size_t index = mysql_json_document_first_column_index;
             index < mysql_json_scalar_column_count;
             ++index) {
            static const char *const json_contexts[] = {
                "JSON_EXTRACT scalar metadata",
                "JSON_ARRAY scalar metadata",
                "JSON_OBJECT scalar metadata",
            };
            static const char *const json_labels[] = {
                "extracted_json",
                "array_json",
                "object_json",
            };

            failures += expect_column_metadata(
                result,
                index,
                (struct expected_column_metadata){
                    .label = json_labels[index - mysql_json_document_first_column_index],
                    .type = MYLITE_RESULT_COLUMN_TYPE_JSON,
                    .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                    .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                    .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                    .display_length = mysql_json_document_display_length,
                    .decimals = mysql_scalar_var_string_decimals,
                    .nullable = 1,
                },
                json_contexts[index - mysql_json_document_first_column_index]
            );
        }
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE js(id INT NOT NULL, j JSON, s VARCHAR(20), PRIMARY KEY(id))"
    );
    failures +=
        expect_statement_ok(database, "INSERT INTO js VALUES (1, '{\"a\":1}', '{\"a\":1}')");
    failures += execute_ok(
        database,
        "SELECT id, JSON_VALID(j) AS valid_j, JSON_LENGTH(j) AS len_j, "
        "JSON_CONTAINS(j,'1','$.a') AS c, JSON_CONTAINS_PATH(j,'one','$.a') AS cp, "
        "JSON_TYPE(j) AS jt, JSON_VALUE(j,'$.a') AS jv, JSON_QUOTE(s) AS jq, "
        "JSON_EXTRACT(j,'$.a') AS je, JSON_ARRAY(id,s) AS ja, "
        "JSON_OBJECT('a',id) AS jo FROM js AS src LIMIT 0",
        &result
    );
    if (failures == 0) {
        enum {
            primary_id_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                               MYLITE_RESULT_COLUMN_FLAG_PRI_KEY |
                               MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT |
                               MYLITE_RESULT_COLUMN_FLAG_PART_KEY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            nullable_binary_numeric_flags =
                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
        };

        failures += expect_size(
            mylite_result_column_count(result),
            mysql_row_scalar_column_count,
            "row-scalar columns"
        );
        failures += expect_size(mylite_result_row_count(result), 0U, "row-scalar metadata rows");
        failures += expect_column_metadata(
            result,
            0U,
            (struct expected_column_metadata){
                .label = "id",
                .schema_name = "app",
                .table_name = "src",
                .origin_schema_name = "app",
                .origin_table_name = "js",
                .origin_column_name = "id",
                .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
                .flags = primary_id_flags,
                .charset_id = mysql_collation_binary_id,
                .collation_id = mysql_collation_binary_id,
                .display_length = mysql_int_display_length,
                .decimals = 0U,
                .nullable = 0,
            },
            "row-scalar descriptor column metadata"
        );
        {
            static const char *const json_numeric_labels[] = {
                "valid_j",
                "len_j",
                "c",
                "cp",
            };
            static const char *const json_numeric_contexts[] = {
                "row-scalar JSON_VALID metadata",
                "row-scalar JSON_LENGTH metadata",
                "row-scalar JSON_CONTAINS metadata",
                "row-scalar JSON_CONTAINS_PATH metadata",
            };

            for (size_t index = 0U;
                 index < sizeof(json_numeric_labels) / sizeof(json_numeric_labels[0]);
                 ++index) {
                failures += expect_column_metadata(
                    result,
                    1U + index,
                    (struct expected_column_metadata){
                        .label = json_numeric_labels[index],
                        .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
                        .flags = nullable_binary_numeric_flags,
                        .charset_id = mysql_collation_binary_id,
                        .collation_id = mysql_collation_binary_id,
                        .display_length = mysql_scalar_bigint_display_length,
                        .decimals = 0U,
                        .nullable = 1,
                    },
                    json_numeric_contexts[index]
                );
            }
        }
        failures += expect_column_metadata(
            result,
            mysql_row_json_type_column_index,
            (struct expected_column_metadata){
                .label = "jt",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_type_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "row-scalar JSON_TYPE metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_row_json_quote_column_index,
            (struct expected_column_metadata){
                .label = "jq",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_document_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "row-scalar JSON_QUOTE metadata"
        );
        failures += expect_column_metadata(
            result,
            mysql_row_json_value_column_index,
            (struct expected_column_metadata){
                .label = "jv",
                .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                .display_length = mysql_json_value_display_length,
                .decimals = mysql_scalar_var_string_decimals,
                .nullable = 1,
            },
            "row-scalar JSON_VALUE metadata"
        );
        {
            static const char *const json_document_labels[] = {"je", "ja", "jo"};
            static const char *const json_document_contexts[] = {
                "row-scalar JSON_EXTRACT metadata",
                "row-scalar JSON_ARRAY metadata",
                "row-scalar JSON_OBJECT metadata",
            };

            for (size_t index = 0U;
                 index < sizeof(json_document_labels) / sizeof(json_document_labels[0]);
                 ++index) {
                failures += expect_column_metadata(
                    result,
                    mysql_row_json_document_first_column_index + index,
                    (struct expected_column_metadata){
                        .label = json_document_labels[index],
                        .type = MYLITE_RESULT_COLUMN_TYPE_JSON,
                        .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                        .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                        .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
                        .display_length = mysql_json_document_display_length,
                        .decimals = mysql_scalar_var_string_decimals,
                        .nullable = 1,
                    },
                    json_document_contexts[index]
                );
            }
        }
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
    remove_related_files(path);

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

static int test_show_result_column_metadata(void) {
    enum {
        not_null_binary_no_default_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                           MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                           MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
        not_null_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NUM,
        not_null_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                        MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                        MYLITE_RESULT_COLUMN_FLAG_NUM,
        unsigned_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_NUM,
        unsigned_binary_numeric_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                                        MYLITE_RESULT_COLUMN_FLAG_BINARY |
                                        MYLITE_RESULT_COLUMN_FLAG_NUM,
    };

    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "show") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open SHOW metadata");
    failures += setup_metadata_schema(database);
    failures += expect_show_column_metadata(
        database,
        "SHOW TABLES",
        0U,
        (struct expected_column_metadata){
            .label = "Tables_in_app",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = not_null_binary_no_default_flags,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_name_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW TABLES metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW FULL TABLES",
        1U,
        (struct expected_column_metadata){
            .label = "Table_type",
            .type = MYLITE_RESULT_COLUMN_TYPE_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                     MYLITE_RESULT_COLUMN_FLAG_ENUM | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_table_type_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW FULL TABLES metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW COLUMNS FROM meta",
        1U,
        (struct expected_column_metadata){
            .label = "Type",
            .type = MYLITE_RESULT_COLUMN_TYPE_BLOB,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BLOB |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_column_type_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW COLUMNS metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW INDEX FROM meta",
        1U,
        (struct expected_column_metadata){
            .label = "Non_unique",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
            .flags = not_null_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = 2U,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW INDEX metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW TABLE STATUS LIKE 'meta'",
        4U,
        (struct expected_column_metadata){
            .label = "Rows",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = unsigned_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_scalar_bigint_display_length,
            .decimals = 0U,
            .nullable = 1,
        },
        "SHOW TABLE STATUS metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW PROCESSLIST",
        0U,
        (struct expected_column_metadata){
            .label = "Id",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = not_null_binary_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_show_process_id_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW PROCESSLIST metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW WARNINGS",
        1U,
        (struct expected_column_metadata){
            .label = "Code",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONG,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                     MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_show_warning_code_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW WARNINGS metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW COUNT(*) WARNINGS",
        0U,
        (struct expected_column_metadata){
            .label = "@@session.warning_count",
            .type = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            .flags = unsigned_binary_numeric_flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = mysql_scalar_bigint_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW COUNT WARNINGS metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW VARIABLES LIKE 'autocommit'",
        0U,
        (struct expected_column_metadata){
            .label = "Variable_name",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_name_display_length,
            .decimals = 0U,
            .nullable = 0,
        },
        "SHOW VARIABLES metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW CREATE TABLE meta",
        1U,
        (struct expected_column_metadata){
            .label = "Create Table",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_fallback_display_length,
            .decimals = mysql_scalar_var_string_decimals,
            .nullable = 0,
        },
        "SHOW CREATE TABLE metadata"
    );
    failures += expect_show_column_metadata(
        database,
        "SHOW ENGINES",
        0U,
        (struct expected_column_metadata){
            .label = "Engine",
            .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            .flags = 0U,
            .charset_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .collation_id = mysql_collation_utf8mb4_0900_ai_ci_id,
            .display_length = mysql_show_fallback_display_length,
            .decimals = 0U,
            .nullable = 1,
        },
        "SHOW fallback metadata"
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_show_column_metadata(
    mylite_db *database,
    const char *sql,
    size_t column_index,
    struct expected_column_metadata expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_column_metadata(result, column_index, expected, context);
    }
    mylite_result_free(result);
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
        "ltxt LONGTEXT, "
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
        expected.schema_name == NULL ? "" : expected.schema_name,
        context
    );
    failures += expect_text(
        mylite_result_column_table_name(result, column_index),
        expected.table_name == NULL ? "" : expected.table_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_schema_name(result, column_index),
        expected.origin_schema_name == NULL ? "" : expected.origin_schema_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_table_name(result, column_index),
        expected.origin_table_name == NULL ? "" : expected.origin_table_name,
        context
    );
    failures += expect_text(
        mylite_result_column_origin_name(result, column_index),
        expected.origin_column_name == NULL ? "" : expected.origin_column_name,
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
