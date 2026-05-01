#include "types/mylite_column_type.h"

#include <stdio.h>
#include <string.h>

static const unsigned int tinyint_precision = 3U;
static const unsigned int smallint_precision = 5U;
static const unsigned int mediumint_precision = 7U;
static const unsigned int int_precision = 10U;
static const unsigned int bigint_signed_precision = 19U;
static const unsigned int bigint_unsigned_precision = 20U;
static const unsigned int bigint_storage_bytes = 8U;
static const unsigned int display_width_max = 255U;
static const unsigned int display_width_out_of_range = 256U;

static int test_integer_type_metadata(void);
static int test_integer_aliases(void);
static int test_display_width_metadata(void);
static int test_rejected_type_descriptors(void);
static struct mylite_column_type_attributes no_column_type_attributes(void);
static int describe_type(const char *type_name, struct mylite_column_type_attributes attributes,
                         enum mylite_column_type_status expected_status,
                         struct mylite_column_type_descriptor *out_descriptor);
static int expect_string(const char *actual, const char *expected, const char *context);
static int expect_uint(unsigned int actual, unsigned int expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);

int main(void)
{
    int failures = 0;

    failures += test_integer_type_metadata();
    failures += test_integer_aliases();
    failures += test_display_width_metadata();
    failures += test_rejected_type_descriptors();

    return failures == 0 ? 0 : 1;
}

static int test_integer_type_metadata(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures +=
        describe_type("TINYINT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyint", "tinyint data type");
    failures += expect_string(descriptor.column_type, "tinyint", "tinyint column type");
    failures += expect_uint(descriptor.numeric_precision, tinyint_precision, "tinyint precision");
    failures += expect_uint(descriptor.numeric_scale, 0U, "tinyint scale");
    failures += expect_uint(descriptor.storage_bytes, 1U, "tinyint storage");
    failures += expect_string(descriptor.range_min, "-128", "tinyint signed min");
    failures += expect_string(descriptor.range_max, "127", "tinyint signed max");

    failures +=
        describe_type("SMALLINT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_uint(descriptor.numeric_precision, smallint_precision, "smallint precision");
    failures += expect_uint(descriptor.storage_bytes, 2U, "smallint storage");

    failures +=
        describe_type("MEDIUMINT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_uint(descriptor.numeric_precision, mediumint_precision, "mediumint precision");
    failures += expect_uint(descriptor.storage_bytes, 3U, "mediumint storage");

    failures +=
        describe_type("INT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_uint(descriptor.numeric_precision, int_precision, "int precision");
    failures += expect_uint(descriptor.storage_bytes, 4U, "int storage");
    failures += expect_string(descriptor.range_min, "-2147483648", "int signed min");
    failures += expect_string(descriptor.range_max, "2147483647", "int signed max");

    failures +=
        describe_type("BIGINT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_uint(descriptor.numeric_precision, bigint_signed_precision,
                            "bigint signed precision");
    failures += expect_uint(descriptor.storage_bytes, bigint_storage_bytes, "bigint storage");
    failures += expect_string(descriptor.range_min, "-9223372036854775808", "bigint signed min");
    failures += expect_string(descriptor.range_max, "9223372036854775807", "bigint signed max");

    failures += describe_type("BIGINT",
                              (struct mylite_column_type_attributes){
                                  .has_unsigned = true,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_bool(descriptor.is_unsigned, true, "bigint unsigned flag");
    failures += expect_uint(descriptor.numeric_precision, bigint_unsigned_precision,
                            "bigint unsigned precision");
    failures += expect_string(descriptor.column_type, "bigint unsigned", "bigint unsigned type");
    failures += expect_string(descriptor.range_min, "0", "bigint unsigned min");
    failures += expect_string(descriptor.range_max, "18446744073709551615", "bigint unsigned max");

    return failures;
}

static int test_integer_aliases(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures +=
        describe_type("INTEGER", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "int", "integer alias data type");
    failures += expect_string(descriptor.column_type, "int", "integer alias column type");

    failures +=
        describe_type("INT1", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyint", "int1 alias");

    failures +=
        describe_type("INT2", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "smallint", "int2 alias");

    failures +=
        describe_type("INT3", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "mediumint", "int3 alias");

    failures +=
        describe_type("INT4", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "int", "int4 alias");

    failures +=
        describe_type("INT8", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "bigint", "int8 alias");

    failures +=
        describe_type("MIDDLEINT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "mediumint", "middleint alias");

    failures +=
        describe_type("BOOL", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyint", "bool data type");
    failures += expect_string(descriptor.column_type, "tinyint(1)", "bool column type");
    failures += expect_bool(descriptor.is_boolean_alias, true, "bool alias flag");
    failures += expect_bool(descriptor.has_display_width, true, "bool display width flag");
    failures += expect_uint(descriptor.display_width, 1U, "bool display width");

    failures +=
        describe_type("BOOLEAN", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyint", "boolean data type");
    failures += expect_string(descriptor.column_type, "tinyint(1)", "boolean column type");

    return failures;
}

static int test_display_width_metadata(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures += describe_type("INT",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = 0U,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_bool(descriptor.has_display_width, true, "int zero width flag");
    failures += expect_uint(descriptor.display_width, 0U, "int zero width");
    failures += expect_string(descriptor.column_type, "int", "int zero width column type");

    failures += describe_type("INT",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = display_width_max,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "int", "int width 255 column type");

    failures += describe_type("TINYINT",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = 1U,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "tinyint(1)", "tinyint one column type");

    failures += describe_type("TINYINT",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = 1U,
                                  .has_unsigned = true,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.column_type, "tinyint unsigned", "tinyint unsigned width one");

    failures += describe_type("INT",
                              (struct mylite_column_type_attributes){
                                  .has_signed = true,
                                  .has_unsigned = true,
                              },
                              MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_bool(descriptor.is_unsigned, true, "mixed signedness unsigned wins");
    failures += expect_string(descriptor.column_type, "int unsigned", "mixed signedness type");

    return failures;
}

static int test_rejected_type_descriptors(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures +=
        describe_type("INT5", no_column_type_attributes(), MYLITE_COLUMN_TYPE_UNKNOWN, &descriptor);
    failures +=
        describe_type("INT0", no_column_type_attributes(), MYLITE_COLUMN_TYPE_UNKNOWN, &descriptor);
    failures +=
        describe_type("INT6", no_column_type_attributes(), MYLITE_COLUMN_TYPE_UNKNOWN, &descriptor);
    failures +=
        describe_type("INT7", no_column_type_attributes(), MYLITE_COLUMN_TYPE_UNKNOWN, &descriptor);
    failures +=
        describe_type("INT9", no_column_type_attributes(), MYLITE_COLUMN_TYPE_UNKNOWN, &descriptor);
    failures += describe_type("INT",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = display_width_out_of_range,
                              },
                              MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_type("BOOL",
                              (struct mylite_column_type_attributes){
                                  .has_unsigned = true,
                              },
                              MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_type("BOOL",
                              (struct mylite_column_type_attributes){
                                  .has_signed = true,
                              },
                              MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_type("BOOLEAN",
                              (struct mylite_column_type_attributes){
                                  .has_display_width = true,
                                  .display_width = 1U,
                              },
                              MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_type("BOOLEAN",
                              (struct mylite_column_type_attributes){
                                  .has_unsigned = true,
                              },
                              MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);

    return failures;
}

static struct mylite_column_type_attributes no_column_type_attributes(void)
{
    return (struct mylite_column_type_attributes){
        .has_display_width = false,
        .display_width = 0U,
        .has_signed = false,
        .has_unsigned = false,
    };
}

static int describe_type(const char *type_name, struct mylite_column_type_attributes attributes,
                         enum mylite_column_type_status expected_status,
                         struct mylite_column_type_descriptor *out_descriptor)
{
    enum mylite_column_type_status actual = mylite_column_type_describe_integer(
        type_name, strlen(type_name), attributes, out_descriptor);

    if (actual != expected_status) {
        fprintf(stderr, "%s: expected %s, got %s\n", type_name,
                mylite_column_type_status_name(expected_status),
                mylite_column_type_status_name(actual));
        return 1;
    }
    return 0;
}

static int expect_string(const char *actual, const char *expected, const char *context)
{
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected,
                actual == NULL ? "(null)" : actual);
        return 1;
    }
    return 0;
}

static int expect_uint(unsigned int actual, unsigned int expected, const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_bool(bool actual, bool expected, const char *context)
{
    const char *actual_text = "false";
    const char *expected_text = "false";

    if (actual) {
        actual_text = "true";
    }
    if (expected) {
        expected_text = "true";
    }

    if (actual != expected) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected_text, actual_text);
        return 1;
    }
    return 0;
}
