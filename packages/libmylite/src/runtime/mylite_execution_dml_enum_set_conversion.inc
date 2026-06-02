static int convert_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_enum_string_literal(
            database,
            value_node,
            column,
            row_number,
            true,
            true,
            ENUM_STRING_TRIM_TRAILING_SPACES,
            out_value
        );
    }
    return convert_enum_integer_literal(database, value_node, column, row_number, true, out_value);
}

static int convert_enum_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_ordinal_fallback,
    bool missing_is_error,
    enum enum_string_trailing_space_policy trailing_space_policy,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    const struct enum_label_descriptor *label = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "ENUM values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        value_node,
        "ENUM values support only string literals",
        "ENUM values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "ENUM values must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && trailing_space_policy == ENUM_STRING_TRIM_TRAILING_SPACES) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK) {
        rc = enum_type_info_for_logical_type(
            database,
            column->logical_type,
            "statement supports only baseline ENUM descriptors",
            &info
        );
    }
    if (rc == MYLITE_OK && enum_find_label(&info, text, text_length, &label)) {
        rc = copy_enum_label_value(database, label, out_value);
    } else if (rc == MYLITE_OK && allow_numeric_ordinal_fallback) {
        uint64_t ordinal = 0U;

        if (enum_text_is_unsigned_integer(text, text_length, &ordinal) &&
            enum_find_ordinal(&info, ordinal, &label)) {
            rc = copy_enum_label_value(database, label, out_value);
        } else if (missing_is_error) {
            set_data_truncated_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = assign_text_value(database, text, text_length, out_value);
            text = NULL;
        }
    } else if (rc == MYLITE_OK && missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        rc = MYLITE_ERROR;
    } else if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, out_value);
        text = NULL;
    }

    free(text);
    (void)character_length;
    return rc;
}

static void trim_trailing_ascii_spaces(char *text, size_t *text_length) {
    if (text == NULL || text_length == NULL) {
        return;
    }
    while (*text_length > 0U && text[*text_length - 1U] == ' ') {
        *text_length -= 1U;
    }
    text[*text_length] = '\0';
}

static int convert_enum_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    const struct enum_label_descriptor *label = NULL;
    uint64_t ordinal = 0U;
    bool is_negative = false;
    int rc = enum_literal_ordinal(value_node, &ordinal, &is_negative);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "ENUM values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline ENUM descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_negative && enum_find_ordinal(&info, ordinal, &label)) {
        return copy_enum_label_value(database, label, out_value);
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_enum_no_match_value(database, out_value);
}

static int enum_literal_ordinal(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_ordinal,
    bool *out_is_negative
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;

    if (value_node == NULL || out_ordinal == NULL || out_is_negative == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_ordinal) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    *out_is_negative = is_negative;
    return MYLITE_OK;
}

static int copy_enum_label_value(
    struct mylite_db *database,
    const struct enum_label_descriptor *label,
    struct planned_value *out_value
) {
    char *copy = NULL;

    if (label == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(label->text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (label->text_length != 0U) {
        memcpy(copy, label->text, label->text_length);
    }
    copy[label->text_length] = '\0';
    return assign_text_value(database, copy, label->text_length, out_value);
}

static int copy_enum_no_match_value(struct mylite_db *database, struct planned_value *out_value) {
    char *copy = malloc(1U);

    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy[0] = '\0';
    return assign_text_value(database, copy, 1U, out_value);
}

static int make_enum_first_label_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline ENUM descriptors",
        &info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_enum_label_value(database, &info.labels[0], out_value);
}

static int convert_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_set_string_literal(
            database,
            literal,
            column,
            row_number,
            true,
            true,
            true,
            out_value
        );
    }
    return convert_set_integer_literal(database, value_node, column, row_number, true, out_value);
}

static int convert_set_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    struct set_type_info info = {0};
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    bool has_comma = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "SET values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        value_node,
        "SET values support only string literals",
        "SET values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "SET values must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK) {
        rc = set_type_info_for_logical_type(
            database,
            column->logical_type,
            "statement supports only baseline SET descriptors",
            &info
        );
    }
    if (rc == MYLITE_OK) {
        has_comma = memchr(text, ',', text_length) != NULL;
        if (text_length == 0U) {
            rc = convert_empty_set_string_literal(
                database,
                &info,
                canonical_definition_order,
                out_value
            );
        } else {
            rc = convert_set_member_list_literal(
                database,
                text,
                text_length,
                has_comma,
                &info,
                column,
                row_number,
                allow_numeric_bitmap_fallback,
                missing_is_error,
                canonical_definition_order,
                out_value
            );
        }
    }

    free(text);
    (void)character_length;
    return rc;
}

static int convert_empty_set_string_literal(
    struct mylite_db *database,
    const struct set_type_info *info,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    if (canonical_definition_order) {
        return copy_set_bitmap_value(database, info, 0U, out_value);
    }
    return make_empty_text_value(database, out_value);
}

static int convert_set_member_list_literal(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    struct mylite_dynamic_string display;
    size_t token_start = 0U;
    uint64_t bitmap = 0U;
    bool all_members_known = true;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&display);
    while (rc == MYLITE_OK) {
        size_t token_end = token_start;
        size_t member_index = 0U;
        bool found_member = false;

        while (token_end < text_length && text[token_end] != ',') {
            ++token_end;
        }

        found_member =
            set_find_member(info, text + token_start, token_end - token_start, &member_index);
        if (!found_member) {
            all_members_known = false;
        }
        if (canonical_definition_order) {
            if (!found_member) {
                break;
            }
            bitmap |= UINT64_C(1) << member_index;
        } else {
            rc = append_set_predicate_member_text((struct set_predicate_member_text_request){
                .display = &display,
                .info = info,
                .text = text,
                .token_start = token_start,
                .token_end = token_end,
                .member_index = member_index,
                .found_member = found_member,
            });
        }
        if (token_end == text_length) {
            break;
        }
        token_start = token_end + 1U;
    }
    if (rc == MYLITE_OK) {
        rc = finish_set_member_list_conversion(
            database,
            text,
            text_length,
            has_comma,
            info,
            column,
            row_number,
            allow_numeric_bitmap_fallback,
            missing_is_error,
            canonical_definition_order,
            all_members_known,
            bitmap,
            &display,
            out_value
        );
    }
    mylite_dynamic_string_deinit(&display);
    return rc;
}

static int append_set_predicate_member_text(struct set_predicate_member_text_request request) {
    int rc = MYLITE_OK;

    if (request.display == NULL || request.token_start > request.token_end) {
        return MYLITE_ERROR;
    }
    if (request.found_member) {
        if (request.info == NULL || request.member_index >= request.info->member_count) {
            return MYLITE_ERROR;
        }
    } else if (request.text == NULL) {
        return MYLITE_ERROR;
    }

    if (request.display->length > 0U || request.token_start > 0U) {
        rc = mylite_dynamic_string_append_char(request.display, ',');
    }
    if (rc == MYLITE_OK && request.found_member) {
        rc = mylite_dynamic_string_append(
            request.display,
            request.info->members[request.member_index].text
        );
    } else if (rc == MYLITE_OK) {
        char saved = request.text[request.token_end];

        request.text[request.token_end] = '\0';
        rc = mylite_dynamic_string_append(request.display, request.text + request.token_start);
        request.text[request.token_end] = saved;
    }
    return rc;
}

static int finish_set_member_list_conversion(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    bool all_members_known,
    uint64_t bitmap,
    struct mylite_dynamic_string *display,
    struct planned_value *out_value
) {
    if (!canonical_definition_order) {
        char *display_text = mylite_dynamic_string_take(display);

        if (display_text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        return assign_text_value(database, display_text, strlen(display_text), out_value);
    }
    if (all_members_known) {
        return copy_set_bitmap_value(database, info, bitmap, out_value);
    }
    if (allow_numeric_bitmap_fallback && !has_comma) {
        uint64_t numeric_bitmap = 0U;

        if (enum_text_is_unsigned_integer(text, text_length, &numeric_bitmap) &&
            set_bitmap_is_valid(info, numeric_bitmap)) {
            return copy_set_bitmap_value(database, info, numeric_bitmap, out_value);
        }
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_set_no_match_value(database, out_value);
}

static int convert_set_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
) {
    struct set_type_info info = {0};
    uint64_t bitmap = 0U;
    bool is_negative = false;
    int rc = enum_literal_ordinal(value_node, &bitmap, &is_negative);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SET values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = set_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline SET descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_negative && set_bitmap_is_valid(&info, bitmap)) {
        return copy_set_bitmap_value(database, &info, bitmap, out_value);
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_set_no_match_value(database, out_value);
}

static int copy_set_bitmap_value(
    struct mylite_db *database,
    const struct set_type_info *info,
    uint64_t bitmap,
    struct planned_value *out_value
) {
    struct mylite_dynamic_string string;
    bool needs_comma = false;
    int rc = MYLITE_OK;

    if (info == NULL || out_value == NULL || !set_bitmap_is_valid(info, bitmap)) {
        return MYLITE_MISUSE;
    }
    if (bitmap == 0U) {
        return make_empty_text_value(database, out_value);
    }

    mylite_dynamic_string_init(&string);
    for (size_t member_index = 0U; rc == MYLITE_OK && member_index < info->member_count;
         ++member_index) {
        if ((bitmap & (UINT64_C(1) << member_index)) == 0U) {
            continue;
        }
        if (needs_comma) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, info->members[member_index].text);
        }
        needs_comma = true;
    }
    if (rc == MYLITE_OK) {
        char *text = mylite_dynamic_string_take(&string);

        if (text == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        } else {
            rc = assign_text_value(database, text, strlen(text), out_value);
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int copy_set_no_match_value(struct mylite_db *database, struct planned_value *out_value) {
    char *copy = malloc(2U);

    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy[0] = '\x01';
    copy[1] = '\0';
    return assign_text_value(database, copy, 1U, out_value);
}
