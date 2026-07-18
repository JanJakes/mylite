
static int append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (plan->column.default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE &&
        plan->column.default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) {
        return append_alter_table_add_column_explicit_default(database, string, plan);
    }
    if (plan->column.is_nullable) {
        return MYLITE_OK;
    }
    if (planned_column_is_string_family(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT ''");
    }
    if (planned_column_is_json(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT 'null'");
    }
    if (planned_column_is_binary_string_family(&plan->column)) {
        return append_alter_table_add_column_binary_zero(string, plan);
    }
    if (planned_column_is_bit(&plan->column)) {
        return append_alter_table_add_column_bit_zero(string, plan);
    }
    if (planned_column_is_enum(&plan->column)) {
        return append_alter_table_add_column_enum_default(database, string, plan);
    }
    if (planned_column_is_set(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT ''");
    }
    if (planned_column_is_year(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000'");
    }
    if (planned_column_is_decimal(&plan->column)) {
        return append_alter_table_add_column_decimal_zero(database, string, plan);
    }
    if (planned_column_is_approximate(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT 0");
    }
    if (planned_column_is_date(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00'");
    }
    if (planned_column_is_time(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '00:00:00'");
    }
    if (planned_column_is_datetime(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00 00:00:00'");
    }
    if (planned_column_is_timestamp(&plan->column)) {
        return mylite_dynamic_string_append(string, " DEFAULT '0000-00-00 00:00:00'");
    }

    return mylite_dynamic_string_append(string, " DEFAULT 0");
}

static int append_alter_table_add_column_explicit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (planned_column_is_bit(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return append_alter_table_add_column_bit_expression_default(database, string, plan);
    }
    if (planned_column_is_year(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return append_alter_table_add_column_year_expression_default(database, string, plan);
    }
    if (column_default_kind_has_integer_value(plan->column.default_kind)) {
        char default_text[integer_text_capacity];
        int written = snprintf(
            default_text,
            sizeof(default_text),
            " DEFAULT %" PRId64,
            plan->column.default_integer
        );

        if (written < 0 || (size_t)written >= sizeof(default_text)) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(string, default_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        return mylite_dynamic_string_append(string, " DEFAULT NULL");
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return append_alter_table_add_column_current_default(database, string, plan);
    }
    if (planned_column_is_bit(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_alter_table_add_column_bit_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return append_alter_table_add_column_binary_default(database, string, plan);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return append_alter_table_add_column_character_expression_default(database, string, plan);
    }
    if (planned_column_is_approximate(&plan->column) &&
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        int rc = mylite_dynamic_string_append(string, " DEFAULT ");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->column.default_text);
        }
        return rc;
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
        plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_quoted_sql_text(string, plan->column.default_text);
    }

    return MYLITE_OK;
}

static int append_alter_table_add_column_current_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        char timestamp_text[datetime_text_length + 1U];
        int rc = format_current_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, timestamp_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        char date_text[date_text_length + 1U];
        int rc = format_current_date_text(database, date_text, sizeof(date_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, date_text);
    }
    if (plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        char time_text[time_text_minimum_length + 1U];
        int rc = format_current_time_text(database, time_text, sizeof(time_text));

        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_quoted_sql_text(string, time_text);
    }

    return MYLITE_OK;
}

static int append_alter_table_add_column_character_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    descriptor.default_kind = plan->column.default_kind;
    descriptor.default_text = plan->column.default_text;
    rc = materialize_character_expression_default_value(database, &descriptor, false, &value);
    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_bit_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    descriptor.default_kind = plan->column.default_kind;
    descriptor.default_integer = plan->column.default_integer;
    rc = materialize_bit_expression_default_value(database, &descriptor, &value);
    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, value.text, value.text_length);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_year_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = make_year_value(database, (uint32_t)plan->column.default_integer, &value);

    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);
    return rc;
}

static int append_alter_table_add_column_enum_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        plan->column.logical_type,
        "invalid ENUM descriptor",
        &info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info.label_count == 0U) {
        set_unsupported_error(database, "ENUM requires at least one label");
        return MYLITE_ERROR;
    }

    return append_quoted_sql_text(string, info.labels[0].text);
}

static int append_alter_table_add_column_bit_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    struct mylite_sql_ast_node literal = {
        .kind = MYLITE_SQL_AST_LITERAL,
        .span =
            {
                .text = plan->column.default_text,
                .length = strlen(plan->column.default_text),
            },
        .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
    };
    int rc = MYLITE_OK;

    planned_column_descriptor_for_default(&plan->column, &descriptor);
    rc = convert_bit_literal(database, &literal, &descriptor, 1U, false, &value);
    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, value.text, value.text_length);
    }
    planned_value_deinit(&value);

    return rc;
}

static int append_alter_table_add_column_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, plan->column.default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = append_sqlite_blob_default(string, bytes, byte_count);
    }
    free(bytes);
    return rc;
}

static int append_alter_table_add_column_decimal_zero(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = decimal_type_info_for_logical_type(plan->column.logical_type, &info);

    if (rc == MYLITE_OK) {
        rc = build_decimal_zero_value(database, &info, &value);
    }
    if (rc == MYLITE_OK) {
        rc = append_quoted_sql_text(string, value.text);
    }
    planned_value_deinit(&value);

    return rc;
}

static int append_alter_table_add_column_binary_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(plan->column.logical_type, &storage);
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (info == NULL) {
        return MYLITE_ERROR;
    }
    if (info->fixed_length) {
        byte_count = (size_t)info->maximum_length;
    }

    rc = mylite_dynamic_string_append(string, " DEFAULT X'");
    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        rc = mylite_dynamic_string_append(string, "00");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_alter_table_add_column_bit_zero(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
) {
    uint64_t width = 0U;
    size_t byte_count = 0U;
    char bytes[sizeof(uint64_t)] = {0};
    int rc = bit_width_for_logical_type(plan->column.logical_type, &width);

    if (rc != MYLITE_OK) {
        return rc;
    }
    byte_count = bit_byte_width(width);
    return append_sqlite_blob_default(string, bytes, byte_count);
}

static int append_sqlite_blob_default(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
) {
    static const char hex_digits[] = "0123456789ABCDEF";

    int rc = mylite_dynamic_string_append(string, " DEFAULT X'");

    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        unsigned char byte = (unsigned char)bytes[byte_index];

        rc = mylite_dynamic_string_append_char(string, hex_digits[byte >> byte_high_nibble_shift]);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, hex_digits[byte & byte_low_nibble_mask]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_quoted_sql_text(struct mylite_dynamic_string *string, const char *text) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT '");

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_mysql_quoted_default_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT ");

    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_text(string, text);
    }

    return rc;
}

static int append_mysql_quoted_text(struct mylite_dynamic_string *string, const char *text) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_mysql_quoted_comment_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '\\' && text[index + 1U] == '0') {
            rc = mylite_dynamic_string_append(string, "\\0");
            ++index;
            continue;
        }

        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int build_alter_table_drop_column_sql(
    const struct planned_alter_table_drop_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DROP COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_rename_column_sql(
    const struct planned_alter_table_rename_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " RENAME COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " TO ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->new_column_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}
