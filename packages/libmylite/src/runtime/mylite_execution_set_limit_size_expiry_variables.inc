static int apply_set_sql_select_limit_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = UINT64_MAX;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid sql_select_limit system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL sql_select_limit assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_sql_select_limit_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_select_limit = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_sql_select_limit_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sql_select_limit output");
        return MYLITE_ERROR;
    }
    *out_value = UINT64_MAX;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int parse_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid sql_select_limit output");
        return MYLITE_ERROR;
    }
    *out_value = UINT64_MAX;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, out_value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }

    return MYLITE_OK;
}

static int apply_set_group_concat_max_len_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = group_concat_max_len_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len system variable target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL group_concat_max_len assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_group_concat_max_len_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.group_concat_max_len = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_group_concat_max_len_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len output");
        return MYLITE_ERROR;
    }
    *out_value = group_concat_max_len_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        magnitude = 1U;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        magnitude = 0U;
    } else if (
        literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK
    ) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (negative || magnitude < group_concat_max_len_minimum_value) {
        *out_value = group_concat_max_len_minimum_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t group_concat_max_len = group_concat_max_len_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL group_concat_max_len assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_group_concat_max_len_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &group_concat_max_len
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.group_concat_max_len = group_concat_max_len;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_group_concat_max_len_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid group_concat_max_len output");
        return MYLITE_ERROR;
    }
    *out_value = group_concat_max_len_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, out_value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative || *out_value < group_concat_max_len_minimum_value) {
        *out_value = group_concat_max_len_minimum_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }

    return MYLITE_OK;
}

static int apply_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = information_schema_stats_expiry_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            information_schema_stats_expiry_default_value,
            "SET information_schema_stats_expiry supports only fixed no-op global assignments"
        );
    }

    rc =
        parse_set_information_schema_stats_expiry_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.information_schema_stats_expiry = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_information_schema_stats_expiry_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    *out_value = information_schema_stats_expiry_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_information_schema_stats_expiry_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_information_schema_stats_expiry_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (magnitude > information_schema_stats_expiry_max_value) {
        *out_value = information_schema_stats_expiry_max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t stats_expiry = information_schema_stats_expiry_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET GLOBAL information_schema_stats_expiry assignment is not supported"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_information_schema_stats_expiry_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &stats_expiry
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.information_schema_stats_expiry = stats_expiry;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_information_schema_stats_expiry_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid information_schema_stats_expiry output");
        return MYLITE_ERROR;
    }
    *out_value = information_schema_stats_expiry_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    if (value->has_value_size) {
        text_size = value->value_size;
    } else {
        text_size = strlen(text);
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_information_schema_stats_expiry_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int apply_set_max_error_count_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = max_error_count_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid max_error_count target");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            max_error_count_default_value,
            "SET GLOBAL max_error_count assignment is not supported"
        );
    }

    rc = parse_set_max_error_count_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.max_error_count = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_max_error_count_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        set_runtime_error(database, "invalid max_error_count output");
        return MYLITE_ERROR;
    }
    *out_value = max_error_count_default_value;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 1U;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    if (magnitude > max_error_count_max_value) {
        *out_value = max_error_count_max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_max_error_count_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t max_error_count = max_error_count_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL max_error_count assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_max_error_count_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &max_error_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.max_error_count = max_error_count;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_max_error_count_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid max_error_count output");
        return MYLITE_ERROR;
    }
    *out_value = max_error_count_default_value;
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, out_value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }
    if (*out_value > max_error_count_max_value) {
        *out_value = max_error_count_max_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value->value
        );
    }

    return MYLITE_OK;
}
