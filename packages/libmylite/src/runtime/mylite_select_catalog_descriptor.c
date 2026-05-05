#include "mylite_select_catalog_descriptor.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct mylite_catalog_text_match {
    const char *text;
    const char *word;
};

struct mylite_catalog_column_descriptor_source {
    sqlite3_stmt *select;
    const char *extra;
    const char *is_nullable;
    const char *data_type;
    const char *collation_name;
    const char *column_type;
    const char *column_key;
    int column_default_index;
    int character_octet_length_index;
    int numeric_precision_index;
    int numeric_scale_index;
    int datetime_precision_index;
    bool nullable;
    bool is_unsigned;
    bool is_zerofill;
    bool auto_increment;
};

static const uint64_t mylite_select_catalog_decimal_radix = 10U;

static struct mylite_catalog_column_descriptor_source
catalog_column_descriptor_source(sqlite3_stmt *select);
static bool catalog_column_descriptor_source_is_nullable(const char *is_nullable);
static int
apply_catalog_column_type_descriptor(mylite_db *database,
                                     const struct mylite_catalog_column_descriptor_source *source,
                                     struct mylite_field_descriptor *descriptor);
static int apply_catalog_integer_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor);
static int apply_catalog_character_column_descriptor(
    mylite_db *database, const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor);
static int
apply_catalog_binary_column_descriptor(const struct mylite_catalog_column_descriptor_source *source,
                                       struct mylite_field_descriptor *descriptor);
static int apply_catalog_numeric_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor);
static int apply_catalog_temporal_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor);
static void apply_catalog_column_flags(const struct mylite_catalog_column_descriptor_source *source,
                                       struct mylite_field_descriptor *descriptor);
static struct mylite_field_descriptor catalog_field_descriptor_defaults(void);
static int field_descriptor_collation_id(mylite_db *database, const char *collation_name,
                                         unsigned int *out_charset_id);
static bool catalog_text_contains_word(struct mylite_catalog_text_match match);
static uint64_t catalog_int64_or_zero(sqlite3_stmt *stmt, int column);
static uint64_t catalog_text_type_length(const char *data_type);
static uint64_t
catalog_integer_type_length(const struct mylite_catalog_column_descriptor_source *source);
static uint64_t
catalog_integer_type_default_length(const struct mylite_catalog_column_descriptor_source *source);
static uint64_t catalog_integer_display_width(const char *column_type);

int mylite_select_catalog_load_column_descriptor(mylite_db *database, sqlite3_stmt *select,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_catalog_column_descriptor_source source =
        catalog_column_descriptor_source(select);
    struct mylite_field_descriptor descriptor = catalog_field_descriptor_defaults();
    int status = MYLITE_OK;

    descriptor.nullable = source.nullable;
    status = apply_catalog_column_type_descriptor(database, &source, &descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    apply_catalog_column_flags(&source, &descriptor);
    mylite_field_descriptor_set_nullable(&descriptor, source.nullable);

    *out_descriptor = descriptor;
    return MYLITE_OK;
}

static struct mylite_catalog_column_descriptor_source
catalog_column_descriptor_source(sqlite3_stmt *select)
{
    enum {
        select_extra = 1,
        select_is_nullable = 2,
        select_data_type = 3,
        select_character_octet_length = 4,
        select_numeric_precision = 5,
        select_numeric_scale = 6,
        select_datetime_precision = 7,
        select_collation_name = 8,
        select_column_type = 9,
        select_column_key = 10,
        select_column_default = 11,
    };
    struct mylite_catalog_column_descriptor_source source = {
        .select = select,
        .extra = (const char *)sqlite3_column_text(select, select_extra),
        .is_nullable = (const char *)sqlite3_column_text(select, select_is_nullable),
        .data_type = (const char *)sqlite3_column_text(select, select_data_type),
        .collation_name = (const char *)sqlite3_column_text(select, select_collation_name),
        .column_type = (const char *)sqlite3_column_text(select, select_column_type),
        .column_key = (const char *)sqlite3_column_text(select, select_column_key),
        .column_default_index = select_column_default,
        .character_octet_length_index = select_character_octet_length,
        .numeric_precision_index = select_numeric_precision,
        .numeric_scale_index = select_numeric_scale,
        .datetime_precision_index = select_datetime_precision,
    };

    source.nullable = catalog_column_descriptor_source_is_nullable(source.is_nullable);
    source.is_unsigned = catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.column_type, .word = "unsigned"});
    source.is_zerofill = catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.column_type, .word = "zerofill"});
    source.auto_increment = catalog_text_contains_word(
        (struct mylite_catalog_text_match){.text = source.extra, .word = "auto_increment"});
    return source;
}

static bool catalog_column_descriptor_source_is_nullable(const char *is_nullable)
{
    if (is_nullable == NULL) {
        return true;
    }
    if (mylite_ascii_case_equal(is_nullable, "YES")) {
        return true;
    }
    return false;
}

static int
apply_catalog_column_type_descriptor(mylite_db *database,
                                     const struct mylite_catalog_column_descriptor_source *source,
                                     struct mylite_field_descriptor *descriptor)
{
    int status = apply_catalog_integer_column_descriptor(source, descriptor);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_character_column_descriptor(database, source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_binary_column_descriptor(source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_numeric_column_descriptor(source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_temporal_column_descriptor(source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return MYLITE_OK;
}

static int apply_catalog_integer_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "tinyint")) {
        descriptor->type = MYLITE_FIELD_TYPE_TINY;
        descriptor->length = catalog_integer_type_length(source);
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "smallint")) {
        descriptor->type = MYLITE_FIELD_TYPE_SHORT;
        descriptor->length = catalog_integer_type_length(source);
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "mediumint")) {
        descriptor->type = MYLITE_FIELD_TYPE_INT24;
        descriptor->length = catalog_integer_type_length(source);
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "int")) {
        descriptor->type = MYLITE_FIELD_TYPE_LONG;
        descriptor->length = catalog_integer_type_length(source);
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "bigint")) {
        descriptor->type = MYLITE_FIELD_TYPE_LONGLONG;
        descriptor->length = catalog_integer_type_length(source);
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int apply_catalog_character_column_descriptor(
    mylite_db *database, const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "char")) {
        descriptor->type = MYLITE_FIELD_TYPE_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        if (field_descriptor_collation_id(database, source->collation_name,
                                          &descriptor->charset_id) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else if (mylite_ascii_case_equal(data_type, "varchar")) {
        descriptor->type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        if (field_descriptor_collation_id(database, source->collation_name,
                                          &descriptor->charset_id) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else if (mylite_ascii_case_equal(data_type, "tinytext") ||
               mylite_ascii_case_equal(data_type, "text") ||
               mylite_ascii_case_equal(data_type, "mediumtext") ||
               mylite_ascii_case_equal(data_type, "longtext")) {
        descriptor->type = MYLITE_FIELD_TYPE_BLOB;
        descriptor->flags |= MYLITE_FIELD_FLAG_BLOB;
        descriptor->length = catalog_text_type_length(data_type);
        if (field_descriptor_collation_id(database, source->collation_name,
                                          &descriptor->charset_id) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int
apply_catalog_binary_column_descriptor(const struct mylite_catalog_column_descriptor_source *source,
                                       struct mylite_field_descriptor *descriptor)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "binary")) {
        descriptor->type = MYLITE_FIELD_TYPE_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "varbinary")) {
        descriptor->type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "tinyblob") ||
               mylite_ascii_case_equal(data_type, "blob") ||
               mylite_ascii_case_equal(data_type, "mediumblob") ||
               mylite_ascii_case_equal(data_type, "longblob")) {
        descriptor->type = MYLITE_FIELD_TYPE_BLOB;
        descriptor->flags |= MYLITE_FIELD_FLAG_BLOB | MYLITE_FIELD_FLAG_BINARY;
        descriptor->length = catalog_text_type_length(data_type);
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int apply_catalog_numeric_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "decimal")) {
        uint64_t precision = catalog_int64_or_zero(source->select, source->numeric_precision_index);
        uint64_t scale = catalog_int64_or_zero(source->select, source->numeric_scale_index);
        uint64_t scale_separator_length = 0U;
        uint64_t sign_length = 1U;

        if (scale != 0U) {
            scale_separator_length = 1U;
        }
        if (source->is_unsigned) {
            sign_length = 0U;
        }

        descriptor->type = MYLITE_FIELD_TYPE_NEWDECIMAL;
        descriptor->length = precision + scale_separator_length + sign_length;
        descriptor->decimals = (unsigned int)scale;
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "float")) {
        descriptor->type = MYLITE_FIELD_TYPE_FLOAT;
        descriptor->length = catalog_int64_or_zero(source->select, source->numeric_precision_index);
        descriptor->decimals = mylite_mysql_not_fixed_decimals;
        if (sqlite3_column_type(source->select, source->numeric_scale_index) != SQLITE_NULL) {
            descriptor->decimals =
                (unsigned int)catalog_int64_or_zero(source->select, source->numeric_scale_index);
        }
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else if (mylite_ascii_case_equal(data_type, "double")) {
        descriptor->type = MYLITE_FIELD_TYPE_DOUBLE;
        descriptor->length = catalog_int64_or_zero(source->select, source->numeric_precision_index);
        descriptor->decimals = mylite_mysql_not_fixed_decimals;
        if (sqlite3_column_type(source->select, source->numeric_scale_index) != SQLITE_NULL) {
            descriptor->decimals =
                (unsigned int)catalog_int64_or_zero(source->select, source->numeric_scale_index);
        }
        descriptor->flags |= MYLITE_FIELD_FLAG_NUM;
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int apply_catalog_temporal_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "date")) {
        descriptor->type = MYLITE_FIELD_TYPE_DATE;
        descriptor->length = mylite_mysql_date_display_length;
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "time")) {
        unsigned int precision =
            (unsigned int)catalog_int64_or_zero(source->select, source->datetime_precision_index);

        descriptor->type = MYLITE_FIELD_TYPE_TIME;
        descriptor->length = mylite_mysql_time_display_length;
        if (precision != 0U) {
            descriptor->length = mylite_mysql_time_fraction_display_base + precision;
        }
        descriptor->decimals = precision;
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "datetime")) {
        unsigned int precision =
            (unsigned int)catalog_int64_or_zero(source->select, source->datetime_precision_index);

        descriptor->type = MYLITE_FIELD_TYPE_DATETIME;
        descriptor->length = mylite_mysql_datetime_display_length;
        if (precision != 0U) {
            descriptor->length = mylite_mysql_datetime_fraction_display_base + precision;
        }
        descriptor->decimals = precision;
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "timestamp")) {
        unsigned int precision =
            (unsigned int)catalog_int64_or_zero(source->select, source->datetime_precision_index);

        descriptor->type = MYLITE_FIELD_TYPE_TIMESTAMP;
        descriptor->length = mylite_mysql_datetime_display_length;
        if (precision != 0U) {
            descriptor->length = mylite_mysql_datetime_fraction_display_base + precision;
        }
        descriptor->decimals = precision;
        descriptor->flags |= MYLITE_FIELD_FLAG_BINARY;
    } else if (mylite_ascii_case_equal(data_type, "year")) {
        descriptor->type = MYLITE_FIELD_TYPE_YEAR;
        descriptor->length = mylite_mysql_year_display_length;
        descriptor->flags |=
            MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_ZEROFILL | MYLITE_FIELD_FLAG_NUM;
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static void apply_catalog_column_flags(const struct mylite_catalog_column_descriptor_source *source,
                                       struct mylite_field_descriptor *descriptor)
{
    bool no_default = false;

    if (sqlite3_column_type(source->select, source->column_default_index) == SQLITE_NULL) {
        if (!source->nullable && !source->auto_increment) {
            no_default = true;
        }
    }
    if (source->is_unsigned) {
        descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    }
    if (source->is_zerofill) {
        descriptor->flags |= MYLITE_FIELD_FLAG_ZEROFILL | MYLITE_FIELD_FLAG_UNSIGNED;
    }
    if (mylite_ascii_case_equal(source->column_key, "PRI")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_PRI_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    } else if (mylite_ascii_case_equal(source->column_key, "UNI")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_UNIQUE_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    } else if (mylite_ascii_case_equal(source->column_key, "MUL")) {
        descriptor->flags |= MYLITE_FIELD_FLAG_MULTIPLE_KEY | MYLITE_FIELD_FLAG_PART_KEY;
    }
    if (source->auto_increment) {
        descriptor->flags |= MYLITE_FIELD_FLAG_AUTO_INCREMENT;
    }
    if (catalog_text_contains_word((struct mylite_catalog_text_match){
            .text = source->extra,
            .word = "on update CURRENT_TIMESTAMP",
        })) {
        descriptor->flags |= MYLITE_FIELD_FLAG_ON_UPDATE_NOW;
    }
    if (no_default) {
        descriptor->flags |= MYLITE_FIELD_FLAG_NO_DEFAULT_VALUE;
    }
}

static struct mylite_field_descriptor catalog_field_descriptor_defaults(void)
{
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_NULL,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
}

static int field_descriptor_collation_id(mylite_db *database, const char *collation_name,
                                         unsigned int *out_charset_id)
{
    const struct mylite_collation *collation = NULL;

    if (out_charset_id == NULL) {
        return MYLITE_MISUSE;
    }
    if (collation_name == NULL || collation_name[0] == '\0') {
        *out_charset_id = mylite_mysql_binary_charset_id;
        return MYLITE_OK;
    }

    collation = mylite_collation_lookup(collation_name);
    if (collation == NULL) {
        (void)mylite_diagnostics_set_unknown_collation_error(database, collation_name);
        return MYLITE_EXEC_ERROR;
    }

    *out_charset_id = (unsigned int)collation->id;
    return MYLITE_OK;
}

static bool catalog_text_contains_word(struct mylite_catalog_text_match match)
{
    size_t word_length = match.word == NULL ? 0U : strlen(match.word);

    if (match.text == NULL || word_length == 0U) {
        return false;
    }
    for (const char *cursor = match.text; *cursor != '\0'; ++cursor) {
        size_t index = 0U;

        while (index < word_length && cursor[index] != '\0' &&
               tolower((unsigned char)cursor[index]) == tolower((unsigned char)match.word[index])) {
            ++index;
        }
        if (index == word_length) {
            return true;
        }
    }
    return false;
}

static uint64_t catalog_int64_or_zero(sqlite3_stmt *stmt, int column)
{
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return 0U;
    }
    return (uint64_t)sqlite3_column_int64(stmt, column);
}

static uint64_t catalog_text_type_length(const char *data_type)
{
    if (mylite_ascii_case_equal(data_type, "tinytext") ||
        mylite_ascii_case_equal(data_type, "tinyblob")) {
        return mylite_mysql_tiny_text_length;
    }
    if (mylite_ascii_case_equal(data_type, "mediumtext") ||
        mylite_ascii_case_equal(data_type, "mediumblob")) {
        return mylite_mysql_medium_text_length;
    }
    if (mylite_ascii_case_equal(data_type, "longtext") ||
        mylite_ascii_case_equal(data_type, "longblob")) {
        return mylite_mysql_long_text_length;
    }
    return mylite_mysql_text_length;
}

static uint64_t
catalog_integer_type_length(const struct mylite_catalog_column_descriptor_source *source)
{
    uint64_t display_width = catalog_integer_display_width(source->column_type);

    if (display_width != 0U) {
        return display_width;
    }
    return catalog_integer_type_default_length(source);
}

static uint64_t
catalog_integer_type_default_length(const struct mylite_catalog_column_descriptor_source *source)
{
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "tinyint")) {
        if (source->is_unsigned) {
            return mylite_mysql_tinyint_unsigned_display_length;
        }
        return mylite_mysql_tinyint_signed_display_length;
    }
    if (mylite_ascii_case_equal(data_type, "smallint")) {
        if (source->is_unsigned) {
            return mylite_mysql_smallint_unsigned_display_length;
        }
        return mylite_mysql_smallint_signed_display_length;
    }
    if (mylite_ascii_case_equal(data_type, "mediumint")) {
        if (source->is_unsigned) {
            return mylite_mysql_mediumint_unsigned_display_length;
        }
        return mylite_mysql_mediumint_signed_display_length;
    }
    if (mylite_ascii_case_equal(data_type, "int")) {
        if (source->is_unsigned) {
            return mylite_mysql_int_unsigned_display_length;
        }
        return mylite_mysql_int_signed_display_length;
    }
    if (mylite_ascii_case_equal(data_type, "bigint")) {
        return mylite_mysql_unsigned_longlong_display_length;
    }
    return 0U;
}

static uint64_t catalog_integer_display_width(const char *column_type)
{
    const char *open = column_type == NULL ? NULL : strchr(column_type, '(');
    const char *close = open == NULL ? NULL : strchr(open, ')');
    uint64_t value = 0U;

    if (open == NULL || close == NULL || close <= open + 1) {
        return 0U;
    }
    for (const char *cursor = open + 1; cursor < close; ++cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return 0U;
        }
        value = (value * mylite_select_catalog_decimal_radix) + (uint64_t)(*cursor - '0');
    }
    return value;
}
