static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    for (size_t left = 0U; left < column_count; ++left) {
        for (size_t right = left + 1U; right < column_count; ++right) {
            if (text_equals_ascii_case_insensitive(columns[left].name, columns[right].name)) {
                set_duplicate_column_error(database, columns[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static bool text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    if (text == NULL || prefix == NULL) {
        return false;
    }
    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return false;
        }
        ++index;
    }

    return true;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }

    return (char)byte;
}

static bool enum_label_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static int map_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    if (type_node == NULL || out_column == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (type_node->kind == MYLITE_SQL_AST_INTEGER_TYPE) {
        return map_integer_type(
            database,
            type_node,
            column_name,
            &out_column->logical_type,
            &out_column->physical_type
        );
    }
    if (type_node->kind == MYLITE_SQL_AST_VARCHAR_TYPE) {
        return map_varchar_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_CHAR_TYPE) {
        (void)column_name;
        return map_char_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_TEXT_TYPE) {
        return map_text_family_type(database, column_name, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_JSON_TYPE) {
        (void)database;
        (void)column_name;
        return map_json_type(out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_SPATIAL_TYPE) {
        (void)column_name;
        return map_spatial_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_ENUM_TYPE) {
        return map_enum_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_SET_TYPE) {
        return map_set_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_BINARY_STRING_TYPE) {
        return map_binary_string_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_BIT_TYPE) {
        return map_bit_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_YEAR_TYPE) {
        return map_year_type(database, type_node, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_DECIMAL_TYPE) {
        return map_decimal_type(database, type_node, column_name, out_column);
    }
    if (type_node->kind == MYLITE_SQL_AST_APPROXIMATE_TYPE) {
        return map_approximate_type(database, type_node, column_name, out_column);
    }
    if (ast_node_is_temporal_column_type(type_node)) {
        return map_temporal_column_type(database, type_node, column_name, out_column);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static bool ast_node_is_temporal_column_type(const struct mylite_sql_ast_node *type_node) {
    if (type_node == NULL) {
        return false;
    }

    return (type_node->kind == MYLITE_SQL_AST_DATE_TYPE ||
            type_node->kind == MYLITE_SQL_AST_TIME_TYPE ||
            type_node->kind == MYLITE_SQL_AST_DATETIME_TYPE ||
            type_node->kind == MYLITE_SQL_AST_TIMESTAMP_TYPE) != 0;
}

static int map_temporal_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const char *logical_type = "DATE";

    (void)database;
    (void)column_name;

    if (type_node->kind == MYLITE_SQL_AST_TIME_TYPE) {
        logical_type = "TIME";
    } else if (type_node->kind == MYLITE_SQL_AST_DATETIME_TYPE) {
        logical_type = "DATETIME";
    } else if (type_node->kind == MYLITE_SQL_AST_TIMESTAMP_TYPE) {
        logical_type = "TIMESTAMP";
    }

    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_varchar_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    struct mylite_sql_source_span length_span =
        mylite_sql_ast_node_varchar_type_length_span(type_node);
    uint64_t length = 0U;
    bool is_national = mylite_sql_ast_node_varchar_type_is_national(type_node) != 0;
    int rc = MYLITE_OK;
    int written = 0;

    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR supports only lengths 0 through 16383");
        return MYLITE_ERROR;
    }
    if (length > varchar_max_length) {
        set_column_length_too_big_error(database, column_name, varchar_max_length);
        return MYLITE_ERROR;
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s(%" PRIu64 ")",
        is_national ? "NVARCHAR" : "VARCHAR",
        length
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;

    if (is_national) {
        rc = append_national_character_set_warning(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int map_char_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    uint64_t length = char_default_length;
    bool is_national = mylite_sql_ast_node_char_type_is_national(type_node) != 0;
    int rc = MYLITE_OK;
    int written = 0;

    if (mylite_sql_ast_node_char_type_has_explicit_length(type_node)) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_char_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "CHAR supports only lengths 0 through 255");
            return MYLITE_ERROR;
        }
    }
    if (length > char_max_length) {
        set_unsupported_error(database, "CHAR supports only lengths 0 through 255");
        return MYLITE_ERROR;
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s(%" PRIu64 ")",
        is_national ? "NCHAR" : "CHAR",
        length
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;

    if (is_national) {
        rc = append_national_character_set_warning(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_national_character_set_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_national_character_set_deprecated,
        "HY000",
        national_character_set_warning_message
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int map_text_family_type(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    enum mylite_sql_ast_text_type text_type = mylite_sql_ast_node_text_type(type_node);
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    switch (text_type) {
    case MYLITE_SQL_AST_TEXT_TYPE_NONE:
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_TINYTEXT:
        logical_type = "TINYTEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_TEXT:
        logical_type = "TEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT:
        logical_type = "MEDIUMTEXT";
        break;
    case MYLITE_SQL_AST_TEXT_TYPE_LONGTEXT:
        logical_type = "LONGTEXT";
        break;
    }
    if (logical_type == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = assign_text_family_descriptor_type(database, out_column, logical_type);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_sql_ast_node_text_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_text_type_length_span(type_node);
        uint64_t length = 0U;

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK || length > longtext_max_length) {
            set_text_display_width_out_of_range_error(database, column_name);
            return MYLITE_ERROR;
        }
        out_column->has_text_length_argument = true;
        out_column->text_length_argument = length;
    }

    return MYLITE_OK;
}

static int assign_text_family_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *logical_type
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type == NULL ? "" : logical_type
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static const char *text_family_logical_type_for_length(
    uint64_t character_length,
    uint64_t max_bytes_per_character
) {
    uint64_t byte_requirement =
        saturating_text_byte_requirement(character_length, max_bytes_per_character);

    if (byte_requirement <= tinytext_max_length) {
        return "TINYTEXT";
    }
    if (byte_requirement <= text_max_length) {
        return "TEXT";
    }
    if (byte_requirement <= mediumtext_max_length) {
        return "MEDIUMTEXT";
    }
    return "LONGTEXT";
}

static uint64_t saturating_text_byte_requirement(
    uint64_t character_length,
    uint64_t max_bytes_per_character
) {
    if (max_bytes_per_character == 0U) {
        max_bytes_per_character = utf8mb4_max_bytes_per_character;
    }
    if (character_length > longtext_max_length / max_bytes_per_character) {
        return longtext_max_length;
    }
    return character_length * max_bytes_per_character;
}

static uint64_t max_bytes_per_character_for_charset(const char *charset_name) {
    const struct mylite_execution_catalog_character_set *charset = NULL;
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (charset_name == NULL || charset_name[0] == '\0') {
        return utf8mb4_max_bytes_per_character;
    }
    if (charset_name_is_binary(charset_name)) {
        return 1U;
    }

    charset = character_set_by_name(charset_name);
    if (charset == NULL) {
        return utf8mb4_max_bytes_per_character;
    }
    maxlen_span = (struct mylite_sql_source_span){
        .text = charset->maxlen,
        .length = strlen(charset->maxlen),
    };
    if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) == MYLITE_OK && maxlen > 0U) {
        return maxlen;
    }

    return utf8mb4_max_bytes_per_character;
}

static int map_json_type(struct planned_column *out_column) {
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        "JSON"
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_spatial_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    const char *logical_type =
        mylite_sql_ast_spatial_type_name(mylite_sql_ast_node_spatial_type(type_node));

    if (strcmp(logical_type, "none") == 0 || strcmp(logical_type, "unknown") == 0) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "BLOB"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_enum_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *label_list = child_at(type_node, 0U);
    struct enum_type_info info = {0};
    struct mylite_dynamic_string descriptor;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&descriptor);
    if (label_list == NULL || label_list->kind != MYLITE_SQL_AST_ENUM_LABEL_LIST ||
        mylite_sql_ast_node_child_count(label_list) == 0U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&descriptor, "ENUM(");
    }
    for (size_t label_index = 0U;
         rc == MYLITE_OK && label_index < mylite_sql_ast_node_child_count(label_list);
         ++label_index) {
        rc = map_enum_label(
            database,
            column_name,
            child_at(label_list, label_index),
            label_index,
            &info,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&descriptor, ')');
    }
    if (rc == MYLITE_OK && (descriptor.text == NULL ||
                            descriptor.length >= sizeof(out_column->logical_type_storage))) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(out_column->logical_type_storage, descriptor.text, descriptor.length + 1U);
        snprintf(
            out_column->physical_type_storage,
            sizeof(out_column->physical_type_storage),
            "TEXT"
        );
        out_column->logical_type = out_column->logical_type_storage;
        out_column->physical_type = out_column->physical_type_storage;
    }

    mylite_dynamic_string_deinit(&descriptor);
    return rc;
}

static int map_enum_label(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *label_node,
    size_t label_index,
    struct enum_type_info *info,
    struct mylite_dynamic_string *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (info->label_count >= enum_label_count_capacity) {
        set_unsupported_error(database, "ENUM supports at most 255 labels in this build");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        label_node,
        "ENUM labels support only string literals",
        "ENUM labels do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "ENUM labels must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_enum_label_unique(database, text, text_length, info, column_name);
    }
    if (rc == MYLITE_OK) {
        rc = store_enum_label(database, text, text_length, character_length, info);
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_descriptor_label(descriptor, text, text_length, label_index != 0U);
    }

    free(text);
    return rc;
}

static int check_enum_label_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct enum_type_info *info,
    const char *column_name
) {
    for (size_t previous = 0U; previous < info->label_count; ++previous) {
        if (enum_label_equals_ascii_case_insensitive(
                info->labels[previous].text,
                info->labels[previous].text_length,
                text,
                text_length
            )) {
            set_duplicated_enum_value_error(
                database,
                column_name,
                info->labels[previous].text,
                info->labels[previous].text_length
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int store_enum_label(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct enum_type_info *info
) {
    char *stored = NULL;

    if (info->decoded_length + text_length + 1U > sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "ENUM definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }

    stored = &info->decoded_storage[info->decoded_length];
    memcpy(stored, text, text_length);
    stored[text_length] = '\0';
    info->labels[info->label_count] = (struct enum_label_descriptor){
        .text = stored,
        .text_length = text_length,
        .character_length = character_length,
    };
    ++info->label_count;
    info->decoded_length += text_length + 1U;
    if (info->max_label_character_length < character_length) {
        info->max_label_character_length = character_length;
    }
    return MYLITE_OK;
}

static int append_enum_descriptor_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length,
    bool needs_comma
) {
    int rc = MYLITE_OK;

    if (needs_comma) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_quoted_label(string, text, text_length);
    }
    return rc;
}

static int append_enum_quoted_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else if (byte == '\\') {
            rc = mylite_dynamic_string_append(string, "\\\\");
        } else if (byte == '\n') {
            rc = mylite_dynamic_string_append(string, "\\n");
        } else if (byte == '\r') {
            rc = mylite_dynamic_string_append(string, "\\r");
        } else if (byte == '\t') {
            rc = mylite_dynamic_string_append(string, "\\t");
        } else {
            rc = mylite_dynamic_string_append_char(string, (char)byte);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int map_set_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *member_list = child_at(type_node, 0U);
    struct set_type_info info = {0};
    struct mylite_dynamic_string descriptor;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&descriptor);
    if (member_list == NULL || member_list->kind != MYLITE_SQL_AST_SET_MEMBER_LIST ||
        mylite_sql_ast_node_child_count(member_list) == 0U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&descriptor, "SET(");
    }
    for (size_t member_index = 0U;
         rc == MYLITE_OK && member_index < mylite_sql_ast_node_child_count(member_list);
         ++member_index) {
        rc = map_set_member(
            database,
            column_name,
            child_at(member_list, member_index),
            member_index,
            &info,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&descriptor, ')');
    }
    if (rc == MYLITE_OK && (descriptor.text == NULL ||
                            descriptor.length >= sizeof(out_column->logical_type_storage))) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(out_column->logical_type_storage, descriptor.text, descriptor.length + 1U);
        snprintf(
            out_column->physical_type_storage,
            sizeof(out_column->physical_type_storage),
            "TEXT"
        );
        out_column->logical_type = out_column->logical_type_storage;
        out_column->physical_type = out_column->physical_type_storage;
    }

    mylite_dynamic_string_deinit(&descriptor);
    return rc;
}

static int map_set_member(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *member_node,
    size_t member_index,
    struct set_type_info *info,
    struct mylite_dynamic_string *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (info->member_count >= set_member_count_capacity) {
        set_unsupported_error(database, "SET supports at most 64 members in this build");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        member_node,
        "SET members support only string literals",
        "SET members do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK && text_length == 0U) {
        set_unsupported_error(database, "SET empty-string members are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && set_member_contains_comma(text, text_length)) {
        set_illegal_set_value_error(database, text, text_length);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "SET members must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_set_member_unique(database, text, text_length, info, column_name);
    }
    if (rc == MYLITE_OK) {
        rc = store_set_member(database, text, text_length, character_length, info);
    }
    if (rc == MYLITE_OK) {
        rc = append_enum_descriptor_label(descriptor, text, text_length, member_index != 0U);
    }

    free(text);
    return rc;
}

static int check_set_member_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct set_type_info *info,
    const char *column_name
) {
    for (size_t previous = 0U; previous < info->member_count; ++previous) {
        if (enum_label_equals_ascii_case_insensitive(
                info->members[previous].text,
                info->members[previous].text_length,
                text,
                text_length
            )) {
            set_duplicated_set_value_error(
                database,
                column_name,
                info->members[previous].text,
                info->members[previous].text_length
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static bool set_member_contains_comma(const char *text, size_t text_length) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == ',') {
            return true;
        }
    }
    return false;
}

static int store_set_member(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct set_type_info *info
) {
    char *stored = NULL;

    if (info->decoded_length + text_length + 1U > sizeof(info->decoded_storage)) {
        set_unsupported_error(database, "SET definition is too large for this MyLite build");
        return MYLITE_ERROR;
    }

    stored = &info->decoded_storage[info->decoded_length];
    memcpy(stored, text, text_length);
    stored[text_length] = '\0';
    info->members[info->member_count] = (struct enum_label_descriptor){
        .text = stored,
        .text_length = text_length,
        .character_length = character_length,
    };
    ++info->member_count;
    info->decoded_length += text_length + 1U;
    info->max_display_character_length += character_length;
    if (info->member_count > 1U) {
        ++info->max_display_character_length;
    }
    return MYLITE_OK;
}

static int map_binary_string_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    switch (mylite_sql_ast_node_binary_string_type(type_node)) {
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE:
        break;
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY:
        return map_binary_type(database, type_node, column_name, out_column);
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY:
        return map_varbinary_type(database, type_node, column_name, out_column);
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB:
        return map_blob_family_type(database, type_node, column_name, out_column);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int map_binary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t length = binary_default_length;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_binary_string_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_binary_string_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "BINARY supports only lengths 0 through 255");
            return MYLITE_ERROR;
        }
    }
    if (length > binary_max_length) {
        set_column_length_too_big_error(database, column_name, binary_max_length);
        return MYLITE_ERROR;
    }

    return assign_binary_descriptor_type(database, out_column, "BINARY(%" PRIu64 ")", length);
}

static int map_varbinary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    struct mylite_sql_source_span length_span =
        mylite_sql_ast_node_binary_string_type_length_span(type_node);
    uint64_t length = 0U;
    int rc = parse_unsigned_integer_literal(&length_span, &length);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARBINARY supports only lengths 0 through 65535");
        return MYLITE_ERROR;
    }
    if (length > varbinary_max_length) {
        set_column_length_too_big_error(database, column_name, varbinary_max_length);
        return MYLITE_ERROR;
    }
    if (length > varbinary_row_size_max_length) {
        set_row_size_too_large_error(database);
        return MYLITE_ERROR;
    }

    return assign_binary_descriptor_type(database, out_column, "VARBINARY(%" PRIu64 ")", length);
}

static int map_blob_family_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    enum mylite_sql_ast_binary_string_type binary_type =
        mylite_sql_ast_node_binary_string_type(type_node);
    const uint64_t mediumblob_max_length = 16777215ULL;
    const uint64_t longblob_max_length = 4294967295ULL;
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    if (binary_type == MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB &&
        mylite_sql_ast_node_binary_string_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_binary_string_type_length_span(type_node);
        uint64_t length = 0U;

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK || length > longblob_max_length) {
            set_column_length_too_big_error(database, column_name, longblob_max_length);
            return MYLITE_ERROR;
        }
        if (length <= binary_max_length) {
            logical_type = "TINYBLOB";
        } else if (length <= varbinary_max_length) {
            logical_type = "BLOB";
        } else if (length <= mediumblob_max_length) {
            logical_type = "MEDIUMBLOB";
        } else {
            logical_type = "LONGBLOB";
        }
    } else {
        switch (binary_type) {
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_TINYBLOB:
            logical_type = "TINYBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_BLOB:
            logical_type = "BLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB:
            logical_type = "MEDIUMBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_LONGBLOB:
            logical_type = "LONGBLOB";
            break;
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE:
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_BINARY:
        case MYLITE_SQL_AST_BINARY_STRING_TYPE_VARBINARY:
            break;
        }
    }
    if (logical_type == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int assign_binary_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *format,
    uint64_t length
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        format,
        length
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_bit_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t length = bit_default_length;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_bit_type_has_length(type_node) != 0) {
        struct mylite_sql_source_span length_span =
            mylite_sql_ast_node_bit_type_length_span(type_node);

        rc = parse_unsigned_integer_literal(&length_span, &length);
        if (rc != MYLITE_OK) {
            set_bit_display_width_out_of_range_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    if (length == 0U) {
        set_invalid_column_size_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (length > bit_max_length) {
        set_bit_display_width_out_of_range_error(database, column_name);
        return MYLITE_ERROR;
    }

    return assign_bit_descriptor_type(database, out_column, length);
}

static int map_year_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
) {
    if (mylite_sql_ast_node_year_type_has_width(type_node) != 0) {
        struct mylite_sql_source_span width_span =
            mylite_sql_ast_node_year_type_width_span(type_node);
        uint64_t width = 0U;
        int rc = parse_unsigned_integer_literal(&width_span, &width);

        if (rc != MYLITE_OK || width != 4U) {
            set_invalid_year_display_width_error(database);
            return MYLITE_ERROR;
        }
        rc = append_year_display_width_warning(database);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    snprintf(out_column->logical_type_storage, sizeof(out_column->logical_type_storage), "YEAR");
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "TEXT");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int assign_bit_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    uint64_t length
) {
    int written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "BIT(%" PRIu64 ")",
        length
    );

    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "BLOB");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_decimal_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    uint64_t precision = decimal_default_precision;
    uint64_t scale = decimal_default_scale;
    int written = 0;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_decimal_type_has_precision(type_node) != 0) {
        struct mylite_sql_source_span precision_span =
            mylite_sql_ast_node_decimal_type_precision_span(type_node);

        rc = parse_unsigned_integer_literal(&precision_span, &precision);
        if (rc != MYLITE_OK) {
            set_decimal_precision_too_big_error(database, column_name, precision);
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_decimal_type_has_scale(type_node) != 0) {
            struct mylite_sql_source_span scale_span =
                mylite_sql_ast_node_decimal_type_scale_span(type_node);

            rc = parse_unsigned_integer_literal(&scale_span, &scale);
            if (rc != MYLITE_OK) {
                set_decimal_scale_too_big_error(database, column_name, scale);
                return MYLITE_ERROR;
            }
        } else {
            scale = decimal_default_scale;
        }
    }
    if (precision == 0U && scale == 0U) {
        precision = decimal_default_precision;
    }
    if (precision > decimal_max_precision) {
        set_decimal_precision_too_big_error(database, column_name, precision);
        return MYLITE_ERROR;
    }
    if (scale > decimal_max_scale) {
        set_decimal_scale_too_big_error(database, column_name, scale);
        return MYLITE_ERROR;
    }
    if (scale > precision) {
        set_decimal_scale_greater_than_precision_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_decimal_type_is_unsigned(type_node) != 0) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_decimal_unsigned_deprecated,
            "HY000",
            "UNSIGNED for decimal and floating point data types is deprecated and support for it "
            "will be removed in a future release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }

    written = snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "DECIMAL(%" PRIu64 ",%" PRIu64 ")%s",
        precision,
        scale,
        mylite_sql_ast_node_decimal_type_is_unsigned(type_node) != 0 ? " UNSIGNED" : ""
    );
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        "TEXT"
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_approximate_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
) {
    enum approximate_type_class type_class = APPROXIMATE_TYPE_FLOAT;
    uint64_t precision = 0U;
    uint64_t scale = 0U;
    int rc = MYLITE_OK;
    int written = 0;

    switch (mylite_sql_ast_node_approximate_type(type_node)) {
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT4:
        type_class = APPROXIMATE_TYPE_FLOAT;
        break;
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT8:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_DOUBLE:
        type_class = APPROXIMATE_TYPE_DOUBLE;
        break;
    case MYLITE_SQL_AST_APPROXIMATE_TYPE_REAL:
        if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_REAL_AS_FLOAT)) {
            type_class = APPROXIMATE_TYPE_FLOAT;
        } else {
            type_class = APPROXIMATE_TYPE_DOUBLE;
        }
        break;
    }

    if (mylite_sql_ast_node_approximate_type_has_precision(type_node) != 0) {
        struct mylite_sql_source_span precision_span =
            mylite_sql_ast_node_approximate_type_precision_span(type_node);

        rc = parse_unsigned_integer_literal(&precision_span, &precision);
        if (rc != MYLITE_OK) {
            set_incorrect_column_specifier_error(database, column_name);
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_approximate_type_has_scale(type_node) != 0) {
            struct mylite_sql_source_span scale_span =
                mylite_sql_ast_node_approximate_type_scale_span(type_node);

            rc = parse_unsigned_integer_literal(&scale_span, &scale);
            if (rc != MYLITE_OK) {
                set_incorrect_column_specifier_error(database, column_name);
                return MYLITE_ERROR;
            }
            if (precision > approximate_max_display_width) {
                set_display_width_out_of_range_error(database, column_name);
                return MYLITE_ERROR;
            }
            if (scale > decimal_max_scale) {
                set_decimal_scale_too_big_error(database, column_name, scale);
                return MYLITE_ERROR;
            }
            if (scale > precision) {
                set_decimal_scale_greater_than_precision_error(database, column_name);
                return MYLITE_ERROR;
            }
            rc = append_approximate_display_width_warning(database);
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            if (rc != MYLITE_OK) {
                return rc;
            }
        } else {
            if (precision > float_max_precision) {
                set_incorrect_column_specifier_error(database, column_name);
                return MYLITE_ERROR;
            }
            type_class = precision <= float_double_precision_threshold ? APPROXIMATE_TYPE_FLOAT
                                                                       : APPROXIMATE_TYPE_DOUBLE;
        }
    }
    if (mylite_sql_ast_node_approximate_type_is_unsigned(type_node) != 0) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_decimal_unsigned_deprecated,
            "HY000",
            "UNSIGNED for decimal and floating point data types is deprecated and support for it "
            "will be removed in a future release."
        );
        if (rc != MYLITE_OK) {
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
            return rc;
        }
    }

    const char *base_type = type_class == APPROXIMATE_TYPE_FLOAT ? "FLOAT" : "DOUBLE";

    if (mylite_sql_ast_node_approximate_type_has_scale(type_node) != 0) {
        written = snprintf(
            out_column->logical_type_storage,
            sizeof(out_column->logical_type_storage),
            "%s(%" PRIu64 ",%" PRIu64 ")%s",
            base_type,
            precision,
            scale,
            mylite_sql_ast_node_approximate_type_is_unsigned(type_node) != 0 ? " UNSIGNED" : ""
        );
    } else {
        written = snprintf(
            out_column->logical_type_storage,
            sizeof(out_column->logical_type_storage),
            "%s%s",
            base_type,
            mylite_sql_ast_node_approximate_type_is_unsigned(type_node) != 0 ? " UNSIGNED" : ""
        );
    }
    if (written < 0 || (size_t)written >= sizeof(out_column->logical_type_storage)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    snprintf(out_column->physical_type_storage, sizeof(out_column->physical_type_storage), "REAL");
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    return MYLITE_OK;
}

static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
) {
    enum mylite_sql_ast_integer_type type = mylite_sql_ast_node_integer_type(type_node);
    int is_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(type_node);
    bool has_display_width = false;
    uint64_t display_width = 0U;
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    if (out_logical_type == NULL || out_physical_type == NULL) {
        return MYLITE_MISUSE;
    }

    rc = map_integer_display_width(
        database,
        type_node,
        column_name,
        &has_display_width,
        &display_width
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    logical_type = logical_type_for_mapped_integer((struct mapped_integer_type){
        .type = type,
        .is_unsigned = is_unsigned,
        .has_display_width = has_display_width,
        .is_bool_alias = mylite_sql_ast_node_integer_type_is_bool_alias(type_node) != 0,
        .display_width = display_width,
    });
    if (logical_type == NULL) {
        return MYLITE_ERROR;
    }
    *out_logical_type = logical_type;
    *out_physical_type = "INTEGER";

    return MYLITE_OK;
}

static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
) {
    enum { max_integer_display_width = 255 };

    struct mylite_sql_source_span display_width_span = {0};
    int rc = MYLITE_OK;

    *out_has_display_width = mylite_sql_ast_node_integer_type_has_display_width(type_node) != 0;
    *out_display_width = 0U;
    if (!*out_has_display_width) {
        return MYLITE_OK;
    }

    display_width_span = mylite_sql_ast_node_integer_type_display_width_span(type_node);
    rc = parse_unsigned_integer_literal(&display_width_span, out_display_width);
    if (rc != MYLITE_OK || *out_display_width > max_integer_display_width) {
        set_display_width_out_of_range_error(database, column_name);
        return MYLITE_ERROR;
    }

    rc = append_integer_display_width_warning(database);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    return rc;
}

static int append_integer_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_integer_display_width_deprecated,
        "HY000",
        "Integer display width is deprecated and will be removed in a future release."
    );
}

static int append_approximate_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_approximate_display_width_deprecated,
        "HY000",
        "Specifying number of digits for floating point data types is deprecated and will be "
        "removed in a future release."
    );
}

static int append_year_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_year_display_width_deprecated,
        "HY000",
        "'YEAR(4)' is deprecated and will be removed in a future release. Please use YEAR instead"
    );
}
