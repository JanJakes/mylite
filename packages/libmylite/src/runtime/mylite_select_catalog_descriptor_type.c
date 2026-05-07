#include "mylite_select_catalog_descriptor_type.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_metadata_constants.h"
#include "mylite_select_catalog_descriptor_source.h"
#include "mylite_span.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

static const uint64_t mylite_select_catalog_decimal_radix = 10U;

static int apply_catalog_integer_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_character_column_descriptor(
    mylite_db *database,
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_binary_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_value_list_column_descriptor(
    mylite_db *database,
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_numeric_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_bit_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int apply_catalog_temporal_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
);

static int field_descriptor_collation_id(
    mylite_db *database,
    const char *collation_name,
    unsigned int *out_charset_id
);

static uint64_t catalog_int64_or_zero(sqlite3_stmt *stmt, int column);

static uint64_t catalog_text_type_length(const char *data_type);

static uint64_t catalog_integer_type_length(
    const struct mylite_catalog_column_descriptor_source *source
);

static uint64_t catalog_integer_type_default_length(
    const struct mylite_catalog_column_descriptor_source *source
);

static uint64_t catalog_integer_display_width(const char *column_type);

int mylite_select_catalog_apply_column_type_descriptor(
    mylite_db *database,
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
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
    status = apply_catalog_value_list_column_descriptor(database, source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_numeric_column_descriptor(source, descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = apply_catalog_bit_column_descriptor(source, descriptor);
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
    struct mylite_field_descriptor *descriptor
) {
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
    mylite_db *database,
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "char")) {
        descriptor->type = MYLITE_FIELD_TYPE_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        if (field_descriptor_collation_id(
                database,
                source->collation_name,
                &descriptor->charset_id
            ) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else if (mylite_ascii_case_equal(data_type, "varchar")) {
        descriptor->type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor->length =
            catalog_int64_or_zero(source->select, source->character_octet_length_index);
        if (field_descriptor_collation_id(
                database,
                source->collation_name,
                &descriptor->charset_id
            ) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else if (
        mylite_ascii_case_equal(data_type, "tinytext") ||
        mylite_ascii_case_equal(data_type, "text") ||
        mylite_ascii_case_equal(data_type, "mediumtext") ||
        mylite_ascii_case_equal(data_type, "longtext")
    ) {
        descriptor->type = MYLITE_FIELD_TYPE_BLOB;
        descriptor->flags |= MYLITE_FIELD_FLAG_BLOB;
        descriptor->length = catalog_text_type_length(data_type);
        if (field_descriptor_collation_id(
                database,
                source->collation_name,
                &descriptor->charset_id
            ) != MYLITE_OK) {
            return MYLITE_EXEC_ERROR;
        }
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int apply_catalog_binary_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
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
    } else if (
        mylite_ascii_case_equal(data_type, "tinyblob") ||
        mylite_ascii_case_equal(data_type, "blob") ||
        mylite_ascii_case_equal(data_type, "mediumblob") ||
        mylite_ascii_case_equal(data_type, "longblob")
    ) {
        descriptor->type = MYLITE_FIELD_TYPE_BLOB;
        descriptor->flags |= MYLITE_FIELD_FLAG_BLOB | MYLITE_FIELD_FLAG_BINARY;
        descriptor->length = catalog_text_type_length(data_type);
    } else {
        return MYLITE_UNSUPPORTED;
    }
    return MYLITE_OK;
}

static int apply_catalog_value_list_column_descriptor(
    mylite_db *database,
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
    const char *data_type = source->data_type;

    if (mylite_ascii_case_equal(data_type, "enum")) {
        descriptor->type = MYLITE_FIELD_TYPE_ENUM;
        descriptor->flags |= MYLITE_FIELD_FLAG_ENUM;
    } else if (mylite_ascii_case_equal(data_type, "set")) {
        descriptor->type = MYLITE_FIELD_TYPE_SET;
        descriptor->flags |= MYLITE_FIELD_FLAG_SET;
    } else {
        return MYLITE_UNSUPPORTED;
    }

    descriptor->length =
        catalog_int64_or_zero(source->select, source->character_octet_length_index);
    if (field_descriptor_collation_id(database, source->collation_name, &descriptor->charset_id) !=
        MYLITE_OK) {
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int apply_catalog_numeric_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
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

static int apply_catalog_bit_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
    const char *data_type = source->data_type;

    if (!mylite_ascii_case_equal(data_type, "bit")) {
        return MYLITE_UNSUPPORTED;
    }

    descriptor->type = MYLITE_FIELD_TYPE_BIT;
    descriptor->length = catalog_int64_or_zero(source->select, source->numeric_precision_index);
    descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
    return MYLITE_OK;
}

static int apply_catalog_temporal_column_descriptor(
    const struct mylite_catalog_column_descriptor_source *source,
    struct mylite_field_descriptor *descriptor
) {
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

static int field_descriptor_collation_id(
    mylite_db *database,
    const char *collation_name,
    unsigned int *out_charset_id
) {
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

static uint64_t catalog_int64_or_zero(sqlite3_stmt *stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return 0U;
    }
    return (uint64_t)sqlite3_column_int64(stmt, column);
}

static uint64_t catalog_text_type_length(const char *data_type) {
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

static uint64_t catalog_integer_type_length(
    const struct mylite_catalog_column_descriptor_source *source
) {
    uint64_t display_width = catalog_integer_display_width(source->column_type);

    if (display_width != 0U) {
        return display_width;
    }
    return catalog_integer_type_default_length(source);
}

static uint64_t catalog_integer_type_default_length(
    const struct mylite_catalog_column_descriptor_source *source
) {
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

static uint64_t catalog_integer_display_width(const char *column_type) {
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
