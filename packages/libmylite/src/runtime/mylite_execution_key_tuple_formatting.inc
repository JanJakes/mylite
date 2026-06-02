static int format_sqlite_key_tuple(
    sqlite3_stmt *statement,
    const struct loaded_index_part *parts,
    size_t part_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;
    int rc = MYLITE_OK;

    if (statement == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

        if (part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = format_sqlite_key_part(
            statement,
            parts == NULL ? NULL : &parts[part_index],
            (int)part_index,
            value_text,
            sizeof(value_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                value_text
            );
        }
    }

    return rc;
}

static int format_sqlite_key_part(
    sqlite3_stmt *statement,
    const struct loaded_index_part *part,
    int column_index,
    char *destination,
    size_t destination_size
) {
    int written = 0;
    int sqlite_type = SQLITE_NULL;

    if (destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    sqlite_type = sqlite3_column_type(statement, column_index);
    switch (sqlite_type) {
    case SQLITE_NULL:
        written = snprintf(destination, destination_size, "NULL");
        break;
    case SQLITE_INTEGER:
        written = snprintf(
            destination,
            destination_size,
            "%" PRId64,
            (int64_t)sqlite3_column_int64(statement, column_index)
        );
        break;
    default: {
        if (loaded_index_part_uses_binary_key_display(part)) {
            const void *bytes = sqlite3_column_blob(statement, column_index);
            int byte_count = sqlite3_column_bytes(statement, column_index);
            size_t display_byte_count = 0U;

            if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
                return MYLITE_ERROR;
            }
            display_byte_count = binary_key_display_byte_count(part, bytes, (size_t)byte_count);
            return format_binary_key_bytes(
                bytes,
                display_byte_count,
                destination,
                destination_size
            );
        }
        const unsigned char *sqlite_text = sqlite3_column_text(statement, column_index);
        size_t text_length = 0U;

        if (sqlite_text == NULL) {
            return MYLITE_ERROR;
        }
        text_length = strlen((const char *)sqlite_text);
        if (text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, sqlite_text, text_length);
        destination[text_length] = '\0';
        return MYLITE_OK;
    }
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_formatted_key_part(
    char *destination,
    size_t destination_size,
    size_t *used,
    size_t part_index,
    const char *value_text
) {
    size_t display_capacity = duplicate_key_value_display_length;
    size_t value_length = 0U;
    size_t copy_length = 0U;

    if (destination == NULL || destination_size == 0U || used == NULL || value_text == NULL) {
        return MYLITE_ERROR;
    }
    value_length = strlen(value_text);
    if (display_capacity > destination_size - 1U) {
        display_capacity = destination_size - 1U;
    }
    if (*used > display_capacity) {
        return MYLITE_ERROR;
    }
    if (*used == display_capacity) {
        return MYLITE_OK;
    }

    if (part_index != 0U) {
        destination[*used] = '-';
        ++(*used);
        destination[*used] = '\0';
        if (*used == display_capacity) {
            return MYLITE_OK;
        }
    }
    copy_length = value_length;
    if (copy_length > display_capacity - *used) {
        copy_length = display_capacity - *used;
    }
    memcpy(&destination[*used], value_text, copy_length);
    *used += copy_length;
    destination[*used] = '\0';

    return MYLITE_OK;
}

static int bind_unique_key_tuple_parameters(
    sqlite3_stmt *statement,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count || part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(statement, (int)part_index + 1, &values[column_index]);
    }

    return rc;
}

static int format_key_tuple(
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;

    if (index == NULL || values == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        char part_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count) {
            return MYLITE_ERROR;
        }
        if (format_key_part_value(
                &index->parts[part_index],
                &values[column_index],
                part_text,
                sizeof(part_text)
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                part_text
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int format_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    if (part == NULL || value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_blob && loaded_index_part_uses_binary_key_display(part)) {
        return format_blob_key_part_value(part, value, true, destination, destination_size);
    }
    if (!part->index_column.has_prefix_length || value->is_null || !value->is_text) {
        if (part->index_column.has_prefix_length && value->is_blob) {
            return format_blob_key_part_value(part, value, false, destination, destination_size);
        }
        return format_key_value(value, destination, destination_size);
    }

    return format_text_prefix_key_part_value(part, value, destination, destination_size);
}

static int format_blob_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    bool trim_fixed_binary_padding,
    char *destination,
    size_t destination_size
) {
    size_t copy_length = 0U;

    if (part == NULL || value == NULL) {
        return MYLITE_ERROR;
    }
    copy_length = value->text_length;
    if (part->index_column.has_prefix_length) {
        size_t prefix_length = 0U;

        if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        copy_length = limited_copy_length(value->text_length, prefix_length);
    }
    if (trim_fixed_binary_padding) {
        copy_length = binary_key_display_byte_count(part, value->text, copy_length);
    }

    return format_binary_key_bytes(value->text, copy_length, destination, destination_size);
}

static int format_text_prefix_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    size_t prefix_length = 0U;
    size_t copy_length = 0U;

    if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    copy_length = limited_copy_length(value->text_length, prefix_length);
    if (value->text == NULL || copy_length >= destination_size) {
        return MYLITE_ERROR;
    }
    memcpy(destination, value->text, copy_length);
    destination[copy_length] = '\0';

    return MYLITE_OK;
}

static int index_part_prefix_length(
    const struct loaded_index_part *part,
    size_t *out_prefix_length
) {
    if (part == NULL || out_prefix_length == NULL || part->index_column.prefix_length < 0 ||
        (uint64_t)part->index_column.prefix_length > SIZE_MAX) {
        return MYLITE_ERROR;
    }
    *out_prefix_length = (size_t)part->index_column.prefix_length;

    return MYLITE_OK;
}

static size_t limited_copy_length(size_t value_length, size_t maximum_length) {
    return value_length < maximum_length ? value_length : maximum_length;
}

static int format_key_value(
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        written = snprintf(destination, destination_size, "NULL");
    } else if (value->is_text) {
        if (value->text == NULL || value->text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, value->text, value->text_length);
        destination[value->text_length] = '\0';
        return MYLITE_OK;
    } else if (value->is_blob) {
        return format_binary_key_bytes(
            value->text,
            value->text_length,
            destination,
            destination_size
        );
    } else {
        written = snprintf(destination, destination_size, "%" PRId64, value->integer);
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool loaded_index_part_uses_binary_key_display(const struct loaded_index_part *part) {
    if (part == NULL) {
        return false;
    }

    return column_descriptor_is_binary_string_family(&part->column);
}

static size_t binary_key_display_byte_count(
    const struct loaded_index_part *part,
    const void *bytes,
    size_t byte_count
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;
    const unsigned char *source = bytes;

    if (part == NULL || bytes == NULL || byte_count == 0U) {
        return byte_count;
    }
    info = binary_string_type_info_for_logical_type(part->column.logical_type, &storage);
    if (info == NULL || !info->fixed_length) {
        return byte_count;
    }
    while (byte_count > 0U && source[byte_count - 1U] == '\0') {
        --byte_count;
    }

    return byte_count;
}

static int format_binary_key_bytes(
    const void *bytes,
    size_t byte_count,
    char *destination,
    size_t destination_size
) {
    enum {
        printable_ascii_min = 0x20,
        printable_ascii_max = 0x7e,
        hex_high_nibble_shift = 4,
        hex_nibble_mask = 0x0f,
    };

    static const char hex_digits[] = "0123456789ABCDEF";
    const unsigned char *source = bytes;
    size_t used = 0U;

    if ((bytes == NULL && byte_count != 0U) || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = source[index];

        if (byte >= printable_ascii_min && byte <= printable_ascii_max) {
            if (used + 1U >= destination_size) {
                break;
            }
            destination[used] = (char)byte;
            ++used;
        } else {
            if (used + 4U >= destination_size) {
                break;
            }
            destination[used] = '\\';
            destination[used + 1U] = 'x';
            destination[used + 2U] = hex_digits[(byte >> hex_high_nibble_shift) & hex_nibble_mask];
            destination[used + 3U] = hex_digits[byte & hex_nibble_mask];
            used += 4U;
        }
        destination[used] = '\0';
    }

    return MYLITE_OK;
}
