static int enum_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct enum_type_info *out_info
) {
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t index = enum_logical_prefix_length;
    int rc = MYLITE_OK;

    if (logical_type == NULL || unsupported_message == NULL || out_info == NULL ||
        logical_length <= enum_logical_syntax_overhead ||
        strncmp(logical_type, "ENUM(", enum_logical_prefix_length) != 0 ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_info = (struct enum_type_info){0};
    while (rc == MYLITE_OK && index < logical_length - 1U) {
        if (out_info->label_count >= enum_label_count_capacity) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        rc = parse_enum_descriptor_label(
            database,
            logical_type,
            logical_length - 1U,
            &index,
            out_info
        );
        if (rc == MYLITE_OK && index < logical_length - 1U) {
            if (logical_type[index] != ',') {
                set_unsupported_error(database, unsupported_message);
                return MYLITE_ERROR;
            }
            ++index;
        }
    }
    if (rc == MYLITE_OK && out_info->label_count == 0U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return rc;
}

static int parse_enum_descriptor_label(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct enum_type_info *out_info
) {
    size_t label_start = out_info->decoded_length;

    if (*index >= end_index || logical_type[*index] != '\'') {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    ++*index;
    while (*index < end_index) {
        char byte = logical_type[*index];

        ++*index;
        if (byte == '\'') {
            if (*index < end_index && logical_type[*index] == '\'') {
                ++*index;
                if (enum_type_info_append_byte(database, out_info, '\'') != MYLITE_OK) {
                    return MYLITE_ERROR;
                }
                continue;
            }
            return finish_parse_enum_descriptor_label(database, label_start, out_info);
        }
        if (byte == '\\') {
            if (read_enum_descriptor_escaped_byte(
                    database,
                    logical_type,
                    end_index,
                    index,
                    &byte
                ) != MYLITE_OK) {
                return MYLITE_ERROR;
            }
        }
        if (enum_type_info_append_byte(database, out_info, byte) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    set_unsupported_error(database, "invalid ENUM descriptor");
    return MYLITE_ERROR;
}

static int finish_parse_enum_descriptor_label(
    struct mylite_db *database,
    size_t label_start,
    struct enum_type_info *out_info
) {
    size_t character_length = 0U;
    size_t text_length = 0U;

    if (enum_type_info_append_byte(database, out_info, '\0') != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    text_length = out_info->decoded_length - label_start - 1U;
    if (validate_utf8_text(
            &out_info->decoded_storage[label_start],
            text_length,
            &character_length
        ) != MYLITE_OK) {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    out_info->labels[out_info->label_count] = (struct enum_label_descriptor){
        .text = &out_info->decoded_storage[label_start],
        .text_length = text_length,
        .character_length = character_length,
    };
    if (out_info->max_label_character_length < character_length) {
        out_info->max_label_character_length = character_length;
    }
    ++out_info->label_count;
    return MYLITE_OK;
}

static int read_enum_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
) {
    char byte = '\0';

    if (*index >= end_index) {
        set_unsupported_error(database, "invalid ENUM descriptor");
        return MYLITE_ERROR;
    }
    byte = logical_type[*index];
    ++*index;
    if (byte == 'n') {
        byte = '\n';
    } else if (byte == 'r') {
        byte = '\r';
    } else if (byte == 't') {
        byte = '\t';
    }
    *out_byte = byte;
    return MYLITE_OK;
}

static int enum_type_info_append_byte(
    struct mylite_db *database,
    struct enum_type_info *info,
    char byte
) {
    if (info->decoded_length >= sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }
    info->decoded_storage[info->decoded_length] = byte;
    ++info->decoded_length;
    return MYLITE_OK;
}

static bool enum_find_label(
    const struct enum_type_info *info,
    const char *text,
    size_t text_length,
    const struct enum_label_descriptor **out_label
) {
    if (out_label != NULL) {
        *out_label = NULL;
    }
    if (info == NULL || text == NULL) {
        return false;
    }
    for (size_t label_index = 0U; label_index < info->label_count; ++label_index) {
        if (enum_label_equals_ascii_case_insensitive(
                info->labels[label_index].text,
                info->labels[label_index].text_length,
                text,
                text_length
            )) {
            if (out_label != NULL) {
                *out_label = &info->labels[label_index];
            }
            return true;
        }
    }
    return false;
}

static bool enum_find_ordinal(
    const struct enum_type_info *info,
    uint64_t ordinal,
    const struct enum_label_descriptor **out_label
) {
    if (out_label != NULL) {
        *out_label = NULL;
    }
    if (info == NULL || ordinal == 0U || ordinal > info->label_count) {
        return false;
    }
    if (out_label != NULL) {
        *out_label = &info->labels[(size_t)ordinal - 1U];
    }
    return true;
}

static bool enum_text_is_unsigned_integer(
    const char *text,
    size_t text_length,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (text == NULL || text_length == 0U || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        uint64_t digit = 0U;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint64_t)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return false;
        }
        value = (value * decimal_base) + digit;
    }
    *out_value = value;
    return true;
}

static int set_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct set_type_info *out_info
) {
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t index = set_logical_prefix_length;
    int rc = MYLITE_OK;

    if (logical_type == NULL || unsupported_message == NULL || out_info == NULL ||
        logical_length <= set_logical_syntax_overhead ||
        strncmp(logical_type, "SET(", set_logical_prefix_length) != 0 ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_info = (struct set_type_info){0};
    while (rc == MYLITE_OK && index < logical_length - 1U) {
        if (out_info->member_count >= set_member_count_capacity) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        rc = parse_set_descriptor_member(
            database,
            logical_type,
            logical_length - 1U,
            &index,
            out_info
        );
        if (rc == MYLITE_OK && index < logical_length - 1U) {
            if (logical_type[index] != ',') {
                set_unsupported_error(database, unsupported_message);
                return MYLITE_ERROR;
            }
            ++index;
        }
    }
    if (rc == MYLITE_OK && out_info->member_count == 0U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    return rc;
}

static int parse_set_descriptor_member(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct set_type_info *out_info
) {
    size_t member_start = out_info->decoded_length;

    if (*index >= end_index || logical_type[*index] != '\'') {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    ++*index;
    while (*index < end_index) {
        char byte = logical_type[*index];

        ++*index;
        if (byte == '\'') {
            if (*index < end_index && logical_type[*index] == '\'') {
                ++*index;
                if (set_type_info_append_byte(database, out_info, '\'') != MYLITE_OK) {
                    return MYLITE_ERROR;
                }
                continue;
            }
            return finish_parse_set_descriptor_member(database, member_start, out_info);
        }
        if (byte == '\\') {
            if (read_set_descriptor_escaped_byte(database, logical_type, end_index, index, &byte) !=
                MYLITE_OK) {
                return MYLITE_ERROR;
            }
        }
        if (set_type_info_append_byte(database, out_info, byte) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    set_unsupported_error(database, "invalid SET descriptor");
    return MYLITE_ERROR;
}

static int finish_parse_set_descriptor_member(
    struct mylite_db *database,
    size_t member_start,
    struct set_type_info *out_info
) {
    size_t character_length = 0U;
    size_t text_length = 0U;

    if (set_type_info_append_byte(database, out_info, '\0') != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    text_length = out_info->decoded_length - member_start - 1U;
    if (validate_utf8_text(
            &out_info->decoded_storage[member_start],
            text_length,
            &character_length
        ) != MYLITE_OK) {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    out_info->members[out_info->member_count] = (struct enum_label_descriptor){
        .text = &out_info->decoded_storage[member_start],
        .text_length = text_length,
        .character_length = character_length,
    };
    out_info->max_display_character_length += character_length;
    if (out_info->member_count > 0U) {
        ++out_info->max_display_character_length;
    }
    ++out_info->member_count;
    return MYLITE_OK;
}

static int read_set_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
) {
    char byte = '\0';

    if (*index >= end_index) {
        set_unsupported_error(database, "invalid SET descriptor");
        return MYLITE_ERROR;
    }
    byte = logical_type[*index];
    ++*index;
    if (byte == 'n') {
        byte = '\n';
    } else if (byte == 'r') {
        byte = '\r';
    } else if (byte == 't') {
        byte = '\t';
    }
    *out_byte = byte;
    return MYLITE_OK;
}

static int set_type_info_append_byte(
    struct mylite_db *database,
    struct set_type_info *info,
    char byte
) {
    if (info->decoded_length >= sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }
    info->decoded_storage[info->decoded_length] = byte;
    ++info->decoded_length;
    return MYLITE_OK;
}

static bool set_find_member(
    const struct set_type_info *info,
    const char *text,
    size_t text_length,
    size_t *out_index
) {
    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (info == NULL || text == NULL) {
        return false;
    }
    for (size_t member_index = 0U; member_index < info->member_count; ++member_index) {
        if (enum_label_equals_ascii_case_insensitive(
                info->members[member_index].text,
                info->members[member_index].text_length,
                text,
                text_length
            )) {
            if (out_index != NULL) {
                *out_index = member_index;
            }
            return true;
        }
    }
    return false;
}

static bool set_bitmap_is_valid(const struct set_type_info *info, uint64_t bitmap) {
    if (info == NULL || info->member_count == 0U ||
        info->member_count > set_member_count_capacity) {
        return false;
    }
    if (info->member_count == set_member_count_capacity) {
        return true;
    }
    return (bitmap >> info->member_count) == 0U;
}

static int decimal_type_info_for_logical_type(
    const char *logical_type,
    struct decimal_type_info *out_info
) {
    static const char prefix[] = "DECIMAL(";
    static const char unsigned_suffix[] = " UNSIGNED";
    struct mylite_sql_source_span span = {0};
    const char *comma = NULL;
    const char *close = NULL;
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    int rc = MYLITE_OK;

    if (logical_type == NULL || out_info == NULL || logical_length <= sizeof(prefix) ||
        strncmp(logical_type, prefix, sizeof(prefix) - 1U) != 0) {
        return MYLITE_ERROR;
    }

    comma = strchr(logical_type + sizeof(prefix) - 1U, ',');
    close = comma == NULL ? NULL : strchr(comma + 1, ')');
    if (comma == NULL || close == NULL) {
        return MYLITE_ERROR;
    }

    *out_info = (struct decimal_type_info){0};
    span = (struct mylite_sql_source_span){
        .text = logical_type + sizeof(prefix) - 1U,
        .length = (size_t)(comma - (logical_type + sizeof(prefix) - 1U)),
    };
    rc = parse_unsigned_integer_literal(&span, &out_info->precision);
    if (rc != MYLITE_OK) {
        return rc;
    }
    span = (struct mylite_sql_source_span){
        .text = comma + 1,
        .length = (size_t)(close - (comma + 1)),
    };
    rc = parse_unsigned_integer_literal(&span, &out_info->scale);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(close + 1, "") == 0) {
        out_info->is_unsigned = false;
        return MYLITE_OK;
    }
    if (strcmp(close + 1, unsigned_suffix) == 0) {
        out_info->is_unsigned = true;
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static int approximate_type_info_for_logical_type(
    const char *logical_type,
    struct approximate_type_info *out_info
) {
    if (logical_type == NULL || out_info == NULL) {
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "FLOAT") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_FLOAT,
            .is_unsigned = false,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "FLOAT UNSIGNED") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_FLOAT,
            .is_unsigned = true,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DOUBLE") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "DOUBLE UNSIGNED") == 0) {
        *out_info = (struct approximate_type_info){
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = true,
        };
        return MYLITE_OK;
    }

    return MYLITE_ERROR;
}

static const struct text_family_type_info *text_family_type_info_for_logical_type(
    const char *logical_type
) {
    static const struct text_family_type_info text_types[] = {
        {"TINYTEXT", "tinytext", tinytext_max_length},
        {"TEXT", "text", text_max_length},
        {"MEDIUMTEXT", "mediumtext", mediumtext_max_length},
        {"LONGTEXT", "longtext", longtext_max_length},
    };

    if (logical_type == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < sizeof(text_types) / sizeof(text_types[0]); ++index) {
        if (strcmp(logical_type, text_types[index].logical_type) == 0) {
            return &text_types[index];
        }
    }
    return NULL;
}

static const struct binary_string_type_info *binary_string_type_info_for_logical_type(
    const char *logical_type,
    struct binary_string_type_info *storage
) {
    static const struct binary_string_type_info blob_types[] = {
        {"TINYBLOB", "tinyblob", 255U, false, true},
        {"BLOB", "blob", 65535U, false, true},
        {"MEDIUMBLOB", "mediumblob", 16777215U, false, true},
        {"LONGBLOB", "longblob", 4294967295ULL, false, true},
    };
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);

    if (logical_type == NULL || storage == NULL) {
        return NULL;
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        uint64_t length = 0U;
        struct mylite_sql_source_span span = {0};

        if (logical_length <= binary_logical_syntax_overhead ||
            logical_type[logical_length - 1U] != ')') {
            return NULL;
        }
        span = (struct mylite_sql_source_span){
            .text = &logical_type[binary_logical_prefix_length],
            .length = logical_length - binary_logical_syntax_overhead,
        };
        if (parse_unsigned_integer_literal(&span, &length) != MYLITE_OK ||
            length > binary_max_length) {
            return NULL;
        }
        *storage = (struct binary_string_type_info){
            .logical_type = logical_type,
            .display_type = NULL,
            .maximum_length = length,
            .fixed_length = true,
            .blob_family = false,
        };
        return storage;
    }
    if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        uint64_t length = 0U;
        struct mylite_sql_source_span span = {0};

        if (logical_length <= varbinary_logical_syntax_overhead ||
            logical_type[logical_length - 1U] != ')') {
            return NULL;
        }
        span = (struct mylite_sql_source_span){
            .text = &logical_type[varbinary_logical_prefix_length],
            .length = logical_length - varbinary_logical_syntax_overhead,
        };
        if (parse_unsigned_integer_literal(&span, &length) != MYLITE_OK ||
            length > varbinary_max_length) {
            return NULL;
        }
        *storage = (struct binary_string_type_info){
            .logical_type = logical_type,
            .display_type = NULL,
            .maximum_length = length,
            .fixed_length = false,
            .blob_family = false,
        };
        return storage;
    }
    for (size_t index = 0U; index < sizeof(blob_types) / sizeof(blob_types[0]); ++index) {
        if (strcmp(logical_type, blob_types[index].logical_type) == 0) {
            return &blob_types[index];
        }
    }
    return NULL;
}

static int parse_binary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        logical_length <= sizeof("BINARY(") || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "BINARY(", binary_logical_prefix_length) != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[binary_logical_prefix_length],
        .length = logical_length - binary_logical_syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > binary_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int parse_varbinary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        logical_length <= sizeof("VARBINARY(") || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) != 0) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[varbinary_logical_prefix_length],
        .length = logical_length - varbinary_logical_syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > varbinary_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int bit_width_for_logical_type(const char *logical_type, uint64_t *out_width) {
    struct mylite_sql_source_span span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    uint64_t width = 0U;

    if (logical_type == NULL || out_width == NULL ||
        logical_length <= bit_logical_syntax_overhead || logical_type[logical_length - 1U] != ')' ||
        strncmp(logical_type, "BIT(", bit_logical_prefix_length) != 0) {
        return MYLITE_ERROR;
    }
    span = (struct mylite_sql_source_span){
        .text = &logical_type[bit_logical_prefix_length],
        .length = logical_length - bit_logical_syntax_overhead,
    };
    if (parse_unsigned_integer_literal(&span, &width) != MYLITE_OK || width == 0U ||
        width > bit_max_length) {
        return MYLITE_ERROR;
    }

    *out_width = width;
    return MYLITE_OK;
}

static size_t bit_byte_width(uint64_t width) {
    return (size_t)((width + bit_byte_rounding_offset) / byte_bit_count);
}

static uint64_t bit_width_max_value(uint64_t width) {
    if (width >= bit_max_length) {
        return UINT64_MAX;
    }
    return (UINT64_C(1) << width) - 1U;
}
