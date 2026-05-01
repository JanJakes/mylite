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
static const unsigned long long tiny_string_capacity = 255ULL;
static const unsigned long long regular_string_capacity = 65535ULL;
static const unsigned long long long_string_capacity = 4294967295ULL;
static const unsigned long long char_default_length = 1ULL;
static const unsigned long long char_four_length = 4ULL;
static const unsigned long long utf8mb4_char_four_octets = 16ULL;
static const unsigned long long utf8mb3_char_four_octets = 12ULL;
static const unsigned long long text_utf8mb4_tiny_boundary = 63ULL;
static const unsigned long long text_utf8mb4_regular_boundary = 64ULL;
static const unsigned long long text_latin1_tiny_boundary = 255ULL;
static const unsigned long long regular_overflow_length = 65536ULL;
static const unsigned long long medium_overflow_length = 16777216ULL;
static const unsigned long long char_binary_overflow_length = 256ULL;
static const unsigned long long varchar_utf8mb4_overflow_length = 16384ULL;
static const unsigned long long blob_text_overflow_length = 4294967296ULL;
static const unsigned int decimal_default_precision = 10U;
static const unsigned int decimal_max_precision = 65U;
static const unsigned int decimal_max_scale = 30U;
static const unsigned long long decimal_ten_precision = 10ULL;
static const unsigned long long decimal_excess_scale = 11ULL;
static const unsigned int float_default_precision = 12U;
static const unsigned int double_default_precision = 22U;
static const unsigned long long float_precision_single_max = 24ULL;
static const unsigned long long float_precision_double_min = 25ULL;
static const unsigned long long float_precision_out_of_range = 54ULL;
static const unsigned int approximate_display_width_max = 255U;
static const unsigned int approximate_display_width_out_of_range = 256U;
static const unsigned long long numeric_alias_precision = 8ULL;
static const unsigned long long fixed_alias_precision = 7ULL;
static const unsigned int temporal_fsp_max = 6U;
static const unsigned int temporal_fsp_out_of_range = 7U;
static const unsigned long long year_invalid_width = 5ULL;
static const unsigned int time_base_storage_bytes = 3U;
static const unsigned int datetime_base_storage_bytes = 5U;
static const unsigned int timestamp_base_storage_bytes = 4U;
static const unsigned long long year_display_width = 4ULL;

static int test_integer_type_metadata(void);
static int test_integer_aliases(void);
static int test_display_width_metadata(void);
static int test_string_binary_type_metadata(void);
static int test_string_binary_aliases_and_charsets(void);
static int test_text_blob_length_mapping(void);
static int test_numeric_type_metadata(void);
static int test_numeric_aliases_and_attributes(void);
static int test_temporal_type_metadata(void);
static int test_rejected_type_descriptors(void);
static struct mylite_column_type_attributes no_column_type_attributes(void);
static struct mylite_column_type_attributes length_attribute(unsigned long long length);
static struct mylite_column_type_attributes character_set_attribute(const char *character_set);
static struct mylite_column_type_attributes collation_attribute(const char *collation);
static int describe_type(const char *type_name, struct mylite_column_type_attributes attributes,
                         enum mylite_column_type_status expected_status,
                         struct mylite_column_type_descriptor *out_descriptor);
static int describe_string_binary_type(const char *type_name,
                                       struct mylite_column_type_attributes attributes,
                                       enum mylite_column_type_status expected_status,
                                       struct mylite_column_type_descriptor *out_descriptor);
static int describe_numeric_type(const char *type_name,
                                 struct mylite_column_type_attributes attributes,
                                 enum mylite_column_type_status expected_status,
                                 struct mylite_column_type_descriptor *out_descriptor);
static int describe_temporal_type(const char *type_name,
                                  struct mylite_column_type_attributes attributes,
                                  enum mylite_column_type_status expected_status,
                                  struct mylite_column_type_descriptor *out_descriptor);
static int expect_string(const char *actual, const char *expected, const char *context);
static int expect_uint(unsigned int actual, unsigned int expected, const char *context);
static int expect_uint64(unsigned long long actual, unsigned long long expected,
                         const char *context);
static int expect_bool(bool actual, bool expected, const char *context);

int main(void)
{
    int failures = 0;

    failures += test_integer_type_metadata();
    failures += test_integer_aliases();
    failures += test_display_width_metadata();
    failures += test_string_binary_type_metadata();
    failures += test_string_binary_aliases_and_charsets();
    failures += test_text_blob_length_mapping();
    failures += test_numeric_type_metadata();
    failures += test_numeric_aliases_and_attributes();
    failures += test_temporal_type_metadata();
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

static int test_string_binary_type_metadata(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures += describe_string_binary_type("CHAR", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "char", "char data type");
    failures += expect_string(descriptor.column_type, "char(1)", "char column type");
    failures +=
        expect_uint64(descriptor.character_maximum_length, char_default_length, "char length");
    failures += expect_uint64(descriptor.character_octet_length, char_four_length, "char octets");
    failures += expect_string(descriptor.character_set_name, "utf8mb4", "char charset");
    failures += expect_string(descriptor.collation_name, "utf8mb4_0900_ai_ci", "char collation");

    failures += describe_string_binary_type("CHAR", length_attribute(char_four_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "char(4)", "char(4) column type");
    failures += expect_uint64(descriptor.character_octet_length, utf8mb4_char_four_octets,
                              "char(4) octets");

    failures += describe_string_binary_type("VARCHAR", length_attribute(char_four_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "varchar", "varchar data type");
    failures += expect_string(descriptor.column_type, "varchar(4)", "varchar column type");
    failures += expect_uint64(descriptor.character_octet_length, utf8mb4_char_four_octets,
                              "varchar octets");

    failures += describe_string_binary_type("BINARY", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "binary", "binary data type");
    failures += expect_string(descriptor.column_type, "binary(1)", "binary column type");
    failures +=
        expect_uint64(descriptor.character_octet_length, char_default_length, "binary octets");
    failures += expect_bool(descriptor.character_set_name == NULL, true, "binary charset null");
    failures += expect_bool(descriptor.collation_name == NULL, true, "binary collation null");

    failures += describe_string_binary_type("VARBINARY", length_attribute(char_four_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "varbinary", "varbinary data type");
    failures += expect_string(descriptor.column_type, "varbinary(4)", "varbinary column type");

    return failures;
}

static int test_string_binary_aliases_and_charsets(void)
{
    struct mylite_column_type_descriptor descriptor;
    struct mylite_column_type_attributes attributes;
    int failures = 0;

    failures += describe_string_binary_type("CHAR", character_set_attribute("latin1"),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.character_set_name, "latin1", "latin1 char charset");
    failures += expect_string(descriptor.collation_name, "latin1_swedish_ci", "latin1 collation");
    failures +=
        expect_uint64(descriptor.character_octet_length, char_default_length, "latin1 char octets");

    failures += describe_string_binary_type("CHAR", collation_attribute("latin1_swedish_ci"),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.character_set_name, "latin1", "collation implied charset");

    failures += describe_string_binary_type("CHAR", collation_attribute("binary"),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "binary", "binary collation char data type");
    failures += expect_string(descriptor.column_type, "binary(1)", "binary collation char type");
    failures += expect_bool(descriptor.character_set_name == NULL, true,
                            "binary collation char charset null");

    attributes = length_attribute(char_four_length);
    attributes.has_character_set = true;
    attributes.character_set = "binary";
    attributes.character_set_length = strlen("binary");
    failures += describe_string_binary_type("CHAR", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "binary", "char charset binary data type");
    failures += expect_string(descriptor.column_type, "binary(4)", "char charset binary type");

    attributes = length_attribute(char_four_length);
    attributes.has_character_set = true;
    attributes.character_set = "binary";
    attributes.character_set_length = strlen("binary");
    failures +=
        describe_string_binary_type("VARCHAR", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.data_type, "varbinary", "varchar charset binary data type");
    failures +=
        expect_string(descriptor.column_type, "varbinary(4)", "varchar charset binary type");

    attributes = length_attribute(char_four_length);
    attributes.has_binary_attribute = true;
    failures +=
        describe_string_binary_type("VARCHAR", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "varchar", "varchar binary attr data type");
    failures += expect_string(descriptor.collation_name, "utf8mb4_bin", "varchar binary collation");
    failures += expect_bool(descriptor.is_deprecated_binary_attribute, true,
                            "varchar binary attribute flag");

    attributes = length_attribute(char_four_length);
    attributes.has_byte_attribute = true;
    failures +=
        describe_string_binary_type("VARCHAR", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "varbinary", "varchar byte data type");
    failures += expect_string(descriptor.column_type, "varbinary(4)", "varchar byte column type");

    attributes = length_attribute(char_four_length);
    attributes.is_national = true;
    failures +=
        describe_string_binary_type("VARCHAR", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.character_set_name, "utf8mb3", "national varchar charset");
    failures += expect_string(descriptor.collation_name, "utf8mb3_general_ci",
                              "national varchar collation");
    failures += expect_uint64(descriptor.character_octet_length, utf8mb3_char_four_octets,
                              "national varchar octets");

    return failures;
}

static int test_text_blob_length_mapping(void)
{
    struct mylite_column_type_descriptor descriptor;
    struct mylite_column_type_attributes attributes;
    int failures = 0;

    failures += describe_string_binary_type("TINYTEXT", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinytext", "tinytext data type");
    failures +=
        expect_uint64(descriptor.character_maximum_length, tiny_string_capacity, "tinytext length");

    failures += describe_string_binary_type("TEXT", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "text", "text data type");
    failures +=
        expect_uint64(descriptor.character_maximum_length, regular_string_capacity, "text length");

    failures += describe_string_binary_type("TEXT", length_attribute(text_utf8mb4_tiny_boundary),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinytext", "text(63) utf8mb4 type");

    failures += describe_string_binary_type("TEXT", length_attribute(text_utf8mb4_regular_boundary),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "text", "text(64) utf8mb4 type");

    attributes = length_attribute(text_latin1_tiny_boundary);
    attributes.has_character_set = true;
    attributes.character_set = "latin1";
    attributes.character_set_length = strlen("latin1");
    failures += describe_string_binary_type("TEXT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinytext", "text(255) latin1 type");

    attributes = no_column_type_attributes();
    attributes.has_collation = true;
    attributes.collation = "binary";
    attributes.collation_length = strlen("binary");
    failures += describe_string_binary_type("TEXT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "blob", "text collate binary data type");

    attributes = no_column_type_attributes();
    attributes.has_character_set = true;
    attributes.character_set = "binary";
    attributes.character_set_length = strlen("binary");
    failures += describe_string_binary_type("TEXT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "blob", "text charset binary data type");

    attributes = length_attribute(text_latin1_tiny_boundary);
    attributes.has_character_set = true;
    attributes.character_set = "binary";
    attributes.character_set_length = strlen("binary");
    failures += describe_string_binary_type("TEXT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyblob", "text(255) binary data type");

    failures += describe_string_binary_type("BLOB", length_attribute(text_latin1_tiny_boundary),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "tinyblob", "blob(255) type");

    failures += describe_string_binary_type("BLOB", length_attribute(char_binary_overflow_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "blob", "blob(256) type");

    failures += describe_string_binary_type("BLOB", length_attribute(regular_overflow_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "mediumblob", "blob(65536) type");

    failures += describe_string_binary_type("BLOB", length_attribute(medium_overflow_length),
                                            MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "longblob", "blob(16777216) type");
    failures +=
        expect_uint64(descriptor.character_maximum_length, long_string_capacity, "longblob length");

    return failures;
}

static int test_numeric_type_metadata(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures += describe_numeric_type("DECIMAL", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "decimal", "decimal data type");
    failures += expect_string(descriptor.column_type, "decimal(10,0)", "decimal column type");
    failures +=
        expect_uint(descriptor.numeric_precision, decimal_default_precision, "decimal precision");
    failures += expect_bool(descriptor.has_numeric_scale, true, "decimal scale flag");
    failures += expect_uint(descriptor.numeric_scale, 0U, "decimal scale");
    failures += expect_bool(descriptor.is_exact_numeric, true, "decimal exact flag");

    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(10,0)", "decimal(10) type");

    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                          .has_scale = true,
                                          .scale = 2ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(10,2)", "decimal(10,2) type");
    failures += expect_uint(descriptor.numeric_scale, 2U, "decimal(10,2) scale");

    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 0ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(10,0)", "decimal(0,0) type");

    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_max_precision,
                                          .has_scale = true,
                                          .scale = decimal_max_scale,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(65,30)", "decimal max type");

    failures += describe_numeric_type("FLOAT", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "float", "float data type");
    failures += expect_string(descriptor.column_type, "float", "float column type");
    failures +=
        expect_uint(descriptor.numeric_precision, float_default_precision, "float precision");
    failures += expect_bool(descriptor.has_numeric_scale, false, "float null scale flag");
    failures += expect_bool(descriptor.is_approximate_numeric, true, "float approximate flag");

    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = float_precision_single_max,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "float", "float(24) data type");

    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = float_precision_double_min,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "double", "float(25) data type");
    failures += expect_string(descriptor.column_type, "double", "float(25) column type");

    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = float_precision_double_min,
                                          .has_scale = true,
                                          .scale = 2ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "float", "float(25,2) data type");
    failures += expect_string(descriptor.column_type, "float(25,2)", "float(25,2) type");
    failures += expect_uint(descriptor.numeric_precision, (unsigned int)float_precision_double_min,
                            "float(25,2) precision");
    failures += expect_uint(descriptor.numeric_scale, 2U, "float(25,2) scale");

    failures += describe_numeric_type("DOUBLE", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "double", "double data type");
    failures += expect_string(descriptor.column_type, "double", "double column type");
    failures +=
        expect_uint(descriptor.numeric_precision, double_default_precision, "double precision");
    failures += expect_bool(descriptor.has_numeric_scale, false, "double null scale flag");

    failures += describe_numeric_type("DOUBLE",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                          .has_scale = true,
                                          .scale = 2ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double(10,2)", "double(10,2) type");

    return failures;
}

static int test_numeric_aliases_and_attributes(void)
{
    struct mylite_column_type_descriptor descriptor;
    struct mylite_column_type_attributes attributes;
    int failures = 0;

    failures += describe_numeric_type("DEC", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "decimal", "dec alias data type");
    failures += expect_bool(descriptor.is_alias, true, "dec alias flag");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = numeric_alias_precision;
    attributes.has_scale = true;
    attributes.scale = 3ULL;
    failures += describe_numeric_type("NUMERIC", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(8,3)", "numeric alias type");

    attributes.precision = fixed_alias_precision;
    attributes.scale = 2ULL;
    failures += describe_numeric_type("FIXED", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "decimal(7,2)", "fixed alias type");

    failures += describe_numeric_type("DOUBLE PRECISION", no_column_type_attributes(),
                                      MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double", "double precision alias");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = decimal_ten_precision;
    attributes.has_scale = true;
    attributes.scale = 2ULL;
    failures +=
        describe_numeric_type("DOUBLE PRECISION", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.column_type, "double(10,2)", "double precision scaled alias");

    failures += describe_numeric_type("REAL", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "double", "real default data type");

    failures += describe_numeric_type("REAL", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double(10,2)", "real scaled alias");

    failures += describe_numeric_type("FLOAT4", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "float", "float4 alias data type");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = decimal_ten_precision;
    failures += describe_numeric_type("FLOAT4", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "float", "float4 selector float");

    attributes.precision = float_precision_double_min;
    failures += describe_numeric_type("FLOAT4", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "double", "float4 selector double");

    attributes.has_scale = true;
    attributes.scale = 2ULL;
    failures += describe_numeric_type("FLOAT4", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "float(25,2)", "float4 scaled alias");

    failures += describe_numeric_type("FLOAT8", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                      &descriptor);
    failures += expect_string(descriptor.data_type, "double", "float8 alias data type");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = decimal_ten_precision;
    attributes.has_scale = true;
    attributes.scale = 2ULL;
    failures += describe_numeric_type("FLOAT8", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double(10,2)", "float8 scaled alias");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = decimal_ten_precision;
    attributes.has_scale = true;
    attributes.scale = 2ULL;
    attributes.has_unsigned = true;
    failures += describe_numeric_type("DECIMAL", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.column_type, "decimal(10,2) unsigned", "decimal unsigned type");

    attributes.has_signed = true;
    failures += describe_numeric_type("DECIMAL", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_bool(descriptor.is_unsigned, true, "decimal mixed signedness unsigned");

    attributes = no_column_type_attributes();
    attributes.has_zerofill_attribute = true;
    failures += describe_numeric_type("FLOAT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.column_type, "float unsigned zerofill", "float zerofill type");
    failures += expect_bool(descriptor.is_unsigned, true, "float zerofill unsigned");
    failures += expect_bool(descriptor.is_zerofill, true, "float zerofill flag");

    attributes.has_signed = true;
    failures += describe_numeric_type("DOUBLE", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double unsigned zerofill",
                              "double zerofill signed type");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = float_precision_double_min;
    attributes.has_unsigned = true;
    failures += describe_numeric_type("FLOAT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double unsigned", "float(25) unsigned");

    attributes.has_scale = true;
    attributes.scale = 2ULL;
    failures += describe_numeric_type("FLOAT", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures +=
        expect_string(descriptor.column_type, "float(25,2) unsigned", "float(25,2) unsigned");

    attributes = no_column_type_attributes();
    attributes.has_precision = true;
    attributes.precision = approximate_display_width_max;
    attributes.has_scale = true;
    attributes.scale = decimal_max_scale;
    failures += describe_numeric_type("DOUBLE", attributes, MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "double(255,30)", "double max display");

    return failures;
}

static int test_temporal_type_metadata(void)
{
    struct mylite_column_type_descriptor descriptor;
    int failures = 0;

    failures += describe_temporal_type("DATE", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                       &descriptor);
    failures += expect_string(descriptor.data_type, "date", "date data type");
    failures += expect_string(descriptor.column_type, "date", "date column type");
    failures += expect_bool(descriptor.has_datetime_precision, false, "date precision null");
    failures += expect_uint(descriptor.storage_bytes, 3U, "date storage");
    failures += expect_string(descriptor.range_min, "1000-01-01", "date min");
    failures += expect_string(descriptor.range_max, "9999-12-31", "date max");

    failures += describe_temporal_type("TIME", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                       &descriptor);
    failures += expect_string(descriptor.data_type, "time", "time data type");
    failures += expect_string(descriptor.column_type, "time", "time column type");
    failures += expect_bool(descriptor.has_datetime_precision, true, "time precision flag");
    failures += expect_uint(descriptor.datetime_precision, 0U, "time default fsp");
    failures += expect_uint(descriptor.storage_bytes, time_base_storage_bytes, "time storage");
    failures += expect_string(descriptor.range_min, "-838:59:59.000000", "time min");

    failures += describe_temporal_type("TIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 1ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "time(1)", "time(1) column type");
    failures += expect_uint(descriptor.datetime_precision, 1U, "time(1) fsp");
    failures +=
        expect_uint(descriptor.storage_bytes, time_base_storage_bytes + 1U, "time(1) storage");

    failures += describe_temporal_type("TIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = temporal_fsp_max,
                                       },
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "time(6)", "time(6) column type");
    failures +=
        expect_uint(descriptor.storage_bytes, time_base_storage_bytes + 3U, "time(6) storage");

    failures += describe_temporal_type("DATETIME", no_column_type_attributes(),
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "datetime", "datetime data type");
    failures += expect_string(descriptor.column_type, "datetime", "datetime column type");
    failures += expect_uint(descriptor.datetime_precision, 0U, "datetime default fsp");
    failures +=
        expect_uint(descriptor.storage_bytes, datetime_base_storage_bytes, "datetime storage");

    failures += describe_temporal_type("DATETIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = temporal_fsp_max,
                                       },
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "datetime(6)", "datetime(6) column type");
    failures += expect_uint(descriptor.datetime_precision, temporal_fsp_max, "datetime(6) fsp");
    failures += expect_uint(descriptor.storage_bytes, datetime_base_storage_bytes + 3U,
                            "datetime(6) storage");

    failures += describe_temporal_type("TIMESTAMP", no_column_type_attributes(),
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.data_type, "timestamp", "timestamp data type");
    failures += expect_string(descriptor.column_type, "timestamp", "timestamp column type");
    failures +=
        expect_uint(descriptor.storage_bytes, timestamp_base_storage_bytes, "timestamp storage");

    failures += describe_temporal_type("TIMESTAMP",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 1ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "timestamp(1)", "timestamp(1) column type");
    failures += expect_uint(descriptor.datetime_precision, 1U, "timestamp(1) fsp");
    failures += expect_uint(descriptor.storage_bytes, timestamp_base_storage_bytes + 1U,
                            "timestamp(1) storage");

    failures += describe_temporal_type("YEAR", no_column_type_attributes(), MYLITE_COLUMN_TYPE_OK,
                                       &descriptor);
    failures += expect_string(descriptor.data_type, "year", "year data type");
    failures += expect_string(descriptor.column_type, "year", "year column type");
    failures += expect_bool(descriptor.has_datetime_precision, false, "year precision null");
    failures += expect_uint(descriptor.storage_bytes, 1U, "year storage");
    failures += expect_string(descriptor.range_min, "0000", "year min");
    failures += expect_string(descriptor.range_max, "2155", "year max");

    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = year_display_width,
                                       },
                                       MYLITE_COLUMN_TYPE_OK, &descriptor);
    failures += expect_string(descriptor.column_type, "year", "year(4) column type");
    failures += expect_bool(descriptor.is_alias, true, "year(4) alias flag");

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
    failures += describe_string_binary_type("CHAR", length_attribute(char_binary_overflow_length),
                                            MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE, &descriptor);
    failures += describe_string_binary_type("BINARY", length_attribute(char_binary_overflow_length),
                                            MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE, &descriptor);
    failures += describe_string_binary_type("VARCHAR", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures +=
        describe_string_binary_type("VARCHAR", length_attribute(varchar_utf8mb4_overflow_length),
                                    MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE, &descriptor);
    failures += describe_string_binary_type("VARBINARY", no_column_type_attributes(),
                                            MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_string_binary_type("VARBINARY", length_attribute(regular_overflow_length),
                                            MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE, &descriptor);
    failures += describe_string_binary_type("TEXT", length_attribute(blob_text_overflow_length),
                                            MYLITE_COLUMN_TYPE_LENGTH_OUT_OF_RANGE, &descriptor);
    failures += describe_string_binary_type("TINYTEXT", length_attribute(char_default_length),
                                            MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_string_binary_type("BLOB", character_set_attribute("utf8mb4"),
                                            MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_max_precision + 1U,
                                      },
                                      MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_max_precision,
                                          .has_scale = true,
                                          .scale = decimal_max_scale + 1U,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                          .has_scale = true,
                                          .scale = decimal_excess_scale,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION, &descriptor);
    failures += describe_numeric_type("DECIMAL",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 1ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION, &descriptor);
    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = float_precision_out_of_range,
                                      },
                                      MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 0ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 1ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION, &descriptor);
    failures += describe_numeric_type("FLOAT",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = approximate_display_width_out_of_range,
                                          .has_scale = true,
                                          .scale = decimal_max_scale,
                                      },
                                      MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("DOUBLE",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                      },
                                      MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_numeric_type("DOUBLE",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = decimal_ten_precision,
                                          .has_scale = true,
                                          .scale = decimal_max_scale + 1U,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("DOUBLE",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 0ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_numeric_type("DOUBLE",
                                      (struct mylite_column_type_attributes){
                                          .has_precision = true,
                                          .precision = 0ULL,
                                          .has_scale = true,
                                          .scale = 1ULL,
                                      },
                                      MYLITE_COLUMN_TYPE_SCALE_EXCEEDS_PRECISION, &descriptor);
    failures += describe_temporal_type("DATE",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 0ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_temporal_type("TIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = temporal_fsp_out_of_range,
                                       },
                                       MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("DATETIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = temporal_fsp_out_of_range,
                                       },
                                       MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("TIMESTAMP",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = temporal_fsp_out_of_range,
                                       },
                                       MYLITE_COLUMN_TYPE_PRECISION_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 0ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 1ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 2ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 3ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("YEAR",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = year_invalid_width,
                                       },
                                       MYLITE_COLUMN_TYPE_DISPLAY_WIDTH_OUT_OF_RANGE, &descriptor);
    failures += describe_temporal_type("TIME",
                                       (struct mylite_column_type_attributes){
                                           .has_unsigned = true,
                                       },
                                       MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);
    failures += describe_temporal_type("TIME",
                                       (struct mylite_column_type_attributes){
                                           .has_precision = true,
                                           .precision = 1ULL,
                                           .has_scale = true,
                                           .scale = 2ULL,
                                       },
                                       MYLITE_COLUMN_TYPE_INVALID_SYNTAX, &descriptor);

    {
        struct mylite_column_type_attributes mismatch = character_set_attribute("utf8mb4");
        mismatch.has_collation = true;
        mismatch.collation = "latin1_swedish_ci";
        mismatch.collation_length = strlen("latin1_swedish_ci");
        failures += describe_string_binary_type(
            "CHAR", mismatch, MYLITE_COLUMN_TYPE_COLLATION_CHARACTER_SET_MISMATCH, &descriptor);
    }

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

static struct mylite_column_type_attributes length_attribute(unsigned long long length)
{
    struct mylite_column_type_attributes attributes = no_column_type_attributes();
    attributes.has_length = true;
    attributes.length = length;
    return attributes;
}

static struct mylite_column_type_attributes character_set_attribute(const char *character_set)
{
    struct mylite_column_type_attributes attributes = no_column_type_attributes();
    attributes.has_character_set = true;
    attributes.character_set = character_set;
    attributes.character_set_length = strlen(character_set);
    return attributes;
}

static struct mylite_column_type_attributes collation_attribute(const char *collation)
{
    struct mylite_column_type_attributes attributes = no_column_type_attributes();
    attributes.has_collation = true;
    attributes.collation = collation;
    attributes.collation_length = strlen(collation);
    return attributes;
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

static int describe_string_binary_type(const char *type_name,
                                       struct mylite_column_type_attributes attributes,
                                       enum mylite_column_type_status expected_status,
                                       struct mylite_column_type_descriptor *out_descriptor)
{
    enum mylite_column_type_status actual = mylite_column_type_describe_string_binary(
        type_name, strlen(type_name), attributes, out_descriptor);

    if (actual != expected_status) {
        fprintf(stderr, "%s: expected %s, got %s\n", type_name,
                mylite_column_type_status_name(expected_status),
                mylite_column_type_status_name(actual));
        return 1;
    }
    return 0;
}

static int describe_numeric_type(const char *type_name,
                                 struct mylite_column_type_attributes attributes,
                                 enum mylite_column_type_status expected_status,
                                 struct mylite_column_type_descriptor *out_descriptor)
{
    enum mylite_column_type_status actual = mylite_column_type_describe_numeric(
        type_name, strlen(type_name), attributes, out_descriptor);

    if (actual != expected_status) {
        fprintf(stderr, "%s: expected %s, got %s\n", type_name,
                mylite_column_type_status_name(expected_status),
                mylite_column_type_status_name(actual));
        return 1;
    }
    return 0;
}

static int describe_temporal_type(const char *type_name,
                                  struct mylite_column_type_attributes attributes,
                                  enum mylite_column_type_status expected_status,
                                  struct mylite_column_type_descriptor *out_descriptor)
{
    enum mylite_column_type_status actual = mylite_column_type_describe_temporal(
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

static int expect_uint64(unsigned long long actual, unsigned long long expected,
                         const char *context)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %llu, got %llu\n", context, expected, actual);
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
