#include "runtime/mylite_result_metadata.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int test_result_metadata_constructs_and_deinitializes_one_column(void);
static int test_result_metadata_misuse_and_overflow_paths(void);
static int expect_int(int actual, int expected, const char *context);
static int expect_logical_type(
    enum mylite_result_logical_type actual,
    enum mylite_result_logical_type expected,
    const char *context
);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_uint16(uint16_t actual, uint16_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_result_metadata_constructs_and_deinitializes_one_column();
    failures += test_result_metadata_misuse_and_overflow_paths();

    return failures == 0 ? 0 : 1;
}

static int test_result_metadata_constructs_and_deinitializes_one_column(void) {
    enum {
        synthetic_flags = 1,
        synthetic_charset_id = 45,
        synthetic_collation_id = 255,
        synthetic_display_length = 20,
        synthetic_decimals = 0,
    };

    struct mylite_result_metadata zero_metadata = {0};
    struct mylite_result_metadata metadata;
    struct mylite_result_column_descriptor descriptor = {
        .label = "answer",
        .schema_name = "test_schema",
        .table_name = "test_table",
        .origin_schema_name = "origin_schema",
        .origin_table_name = "origin_table",
        .origin_column_name = "answer_column",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
        .flags = synthetic_flags,
        .charset_id = synthetic_charset_id,
        .collation_id = synthetic_collation_id,
        .display_length = synthetic_display_length,
        .decimals = synthetic_decimals,
        .nullable = false,
    };
    const struct mylite_result_column *column = NULL;
    int failures = 0;

    mylite_result_metadata_deinit(&zero_metadata);

    mylite_result_metadata_init(&metadata);
    failures += expect_int(
        mylite_result_metadata_append(&metadata, &descriptor),
        MYLITE_OK,
        "append column"
    );
    failures += expect_size(mylite_result_metadata_column_count(&metadata), 1U, "column count");

    column = mylite_result_metadata_column_at(&metadata, 0U);
    failures += expect_true(column != NULL, "column exists");
    if (column != NULL) {
        failures += expect_text(column->label, "answer", "label");
        failures += expect_text(column->schema_name, "test_schema", "schema name");
        failures += expect_text(column->table_name, "test_table", "table name");
        failures += expect_text(column->origin_schema_name, "origin_schema", "origin schema name");
        failures += expect_text(column->origin_table_name, "origin_table", "origin table name");
        failures += expect_text(column->origin_column_name, "answer_column", "origin column name");
        failures += expect_logical_type(
            column->logical_type,
            MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            "logical type"
        );
        failures += expect_uint32(column->flags, synthetic_flags, "flags");
        failures += expect_uint32(column->charset_id, synthetic_charset_id, "charset id");
        failures += expect_uint32(column->collation_id, synthetic_collation_id, "collation id");
        failures +=
            expect_uint64(column->display_length, synthetic_display_length, "display length");
        failures += expect_uint16(column->decimals, synthetic_decimals, "decimals");
        failures += expect_bool(column->nullable, false, "nullability");
    }

    mylite_result_metadata_deinit(&metadata);
    failures +=
        expect_size(mylite_result_metadata_column_count(&metadata), 0U, "deinit column count");

    return failures;
}

static int test_result_metadata_misuse_and_overflow_paths(void) {
    struct mylite_result_metadata metadata;
    struct mylite_result_column_descriptor descriptor = {
        .label = "overflow",
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };
    int failures = 0;

    failures += expect_int(
        mylite_result_metadata_append(NULL, &descriptor),
        MYLITE_MISUSE,
        "append rejects NULL metadata"
    );

    mylite_result_metadata_init(&metadata);
    failures += expect_int(
        mylite_result_metadata_append(&metadata, NULL),
        MYLITE_MISUSE,
        "append rejects NULL descriptor"
    );
    metadata.column_count = SIZE_MAX;
    failures += expect_int(
        mylite_result_metadata_append(&metadata, &descriptor),
        MYLITE_NOMEM,
        "append rejects count overflow"
    );
    metadata.column_count = 0U;
    mylite_result_metadata_deinit(&metadata);

    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_logical_type(
    enum mylite_result_logical_type actual,
    enum mylite_result_logical_type expected,
    const char *context
) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
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
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
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

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
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
