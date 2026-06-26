static int apply_set_myisam_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        *out_handled = true;
        return validate_set_myisam_fixed_global_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
        *out_handled = true;
        return apply_set_myisam_sort_buffer_size_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
        *out_handled = true;
        return apply_set_myisam_stats_method_value(database, target, value_node);
    default:
        return MYLITE_OK;
    }
}

static int apply_set_myisam_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        *out_handled = true;
        return validate_set_myisam_fixed_global_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_SORT_BUFFER_SIZE:
        *out_handled = true;
        return apply_set_myisam_sort_buffer_size_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_STATS_METHOD:
        *out_handled = true;
        return apply_set_myisam_stats_method_cell_value(database, target, value, value_kind);
    default:
        return MYLITE_OK;
    }
}

static int validate_set_myisam_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            myisam_data_pointer_size_default_value,
            "SET GLOBAL myisam_data_pointer_size assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            myisam_max_sort_file_size_default_value,
            "SET GLOBAL myisam_max_sort_file_size assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        return validate_set_fixed_boolean_value(database, value_node, false);
    default:
        return MYLITE_OK;
    }
}

static int validate_set_myisam_fixed_global_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_DATA_POINTER_SIZE:
        return validate_set_myisam_fixed_uint64_cell_value(
            database,
            target,
            myisam_data_pointer_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_MAX_SORT_FILE_SIZE:
        return validate_set_myisam_fixed_uint64_cell_value(
            database,
            target,
            myisam_max_sort_file_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYISAM_USE_MMAP:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    default:
        return MYLITE_OK;
    }
}

static int validate_set_myisam_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t actual = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U || text[0] == '-' || text[0] == '+') {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, &actual) != MYLITE_OK ||
        actual != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int apply_set_myisam_sort_buffer_size_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = myisam_sort_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            myisam_sort_buffer_size_default_value,
            "SET GLOBAL myisam_sort_buffer_size assignment is not supported"
        );
    }

    rc = parse_set_myisam_sort_buffer_size_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.myisam_sort_buffer_size = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_myisam_sort_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t sort_buffer_size = myisam_sort_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_myisam_sort_buffer_size_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &sort_buffer_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (sort_buffer_size != myisam_sort_buffer_size_default_value) {
            set_unsupported_error(
                database,
                "SET GLOBAL myisam_sort_buffer_size assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.myisam_sort_buffer_size = sort_buffer_size;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_myisam_sort_buffer_size_value(
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
        set_runtime_error(database, "invalid myisam_sort_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = myisam_sort_buffer_size_default_value;
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
        const struct mylite_sql_source_span true_span = {.text = "1", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_myisam_sort_buffer_size_integer(
            database,
            variable_name,
            &true_span,
            negative,
            negative ? "-1" : "1",
            out_value
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        const struct mylite_sql_source_span false_span = {.text = "0", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_myisam_sort_buffer_size_integer(
            database,
            variable_name,
            &false_span,
            negative,
            negative ? "-0" : "0",
            out_value
        );
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_myisam_sort_buffer_size_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_myisam_sort_buffer_size_cell_value(
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
        set_runtime_error(database, "invalid myisam_sort_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = myisam_sort_buffer_size_default_value;
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
    return parse_myisam_sort_buffer_size_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_myisam_sort_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid myisam_sort_buffer_size output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = myisam_sort_buffer_size_min_value;
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
    if (magnitude < myisam_sort_buffer_size_min_value) {
        *out_value = myisam_sort_buffer_size_min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    *out_value = magnitude;
    return MYLITE_OK;
}

static int apply_set_myisam_stats_method_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    enum mylite_session_myisam_stats_method value =
        MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_myisam_stats_method_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value != MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL) {
            set_unsupported_error(
                database,
                "SET GLOBAL myisam_stats_method assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.myisam_stats_method = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_myisam_stats_method_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    enum mylite_session_myisam_stats_method parsed_value =
        MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_myisam_stats_method_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (parsed_value != MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL) {
            set_unsupported_error(
                database,
                "SET GLOBAL myisam_stats_method assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.myisam_stats_method = parsed_value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_myisam_stats_method_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_myisam_stats_method *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char value_text[integer_text_capacity];
    char *decoded = NULL;
    size_t decoded_size = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid myisam_stats_method output");
        return MYLITE_ERROR;
    }
    *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_identifier_text(value_node, identifier, sizeof(identifier), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return parse_myisam_stats_method_text(database, variable_name, identifier, out_value);
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal(
            database,
            literal_node,
            "SET system variables support only string literals",
            "SET system variable string values do not support NUL bytes",
            &decoded,
            &decoded_size
        );
        (void)decoded_size;
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = parse_myisam_stats_method_text(database, variable_name, decoded, out_value);
        free(decoded);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        if (negative) {
            set_system_variable_cant_be_set_value_error(database, variable_name, "-1");
            return MYLITE_ERROR;
        }
        return parse_myisam_stats_method_integer(database, variable_name, 1U, "1", out_value);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_myisam_stats_method_integer(
            database,
            variable_name,
            0U,
            negative ? "-0" : "0",
            out_value
        );
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    if (negative) {
        set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    return parse_myisam_stats_method_integer(
        database,
        variable_name,
        magnitude,
        value_text,
        out_value
    );
}

static int parse_set_myisam_stats_method_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    enum mylite_session_myisam_stats_method *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid myisam_stats_method output");
        return MYLITE_ERROR;
    }
    *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
    if (value == NULL || text == NULL || value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return parse_myisam_stats_method_text(database, variable_name, text, out_value);
    }
    if (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
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
    if (negative) {
        set_system_variable_cant_be_set_value_error(database, variable_name, value->value);
        return MYLITE_ERROR;
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    return parse_myisam_stats_method_integer(
        database,
        variable_name,
        magnitude,
        value->value,
        out_value
    );
}

static int parse_myisam_stats_method_text(
    struct mylite_db *database,
    const char *variable_name,
    const char *text,
    enum mylite_session_myisam_stats_method *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (text == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "");
        return MYLITE_ERROR;
    }
    if (text_equals_ascii_case_insensitive(text, "nulls_unequal")) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(text, "nulls_equal")) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_EQUAL;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(text, "nulls_ignored")) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_IGNORED;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, text);
    return MYLITE_ERROR;
}

static int parse_myisam_stats_method_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    const char *value_text,
    enum mylite_session_myisam_stats_method *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (magnitude == 0U) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL;
        return MYLITE_OK;
    }
    if (magnitude == 1U) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_EQUAL;
        return MYLITE_OK;
    }
    if (magnitude == 2U) {
        *out_value = MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_IGNORED;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
    return MYLITE_ERROR;
}

static const char *myisam_stats_method_text(enum mylite_session_myisam_stats_method value) {
    switch (value) {
    case MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_EQUAL:
        return "nulls_equal";
    case MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_IGNORED:
        return "nulls_ignored";
    case MYLITE_SESSION_MYISAM_STATS_METHOD_NULLS_UNEQUAL:
    default:
        return "nulls_unequal";
    }
}
