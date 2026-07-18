static int apply_set_jl_system_variable_assignment(
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
        *out_handled = true;
        return apply_set_internal_tmp_mem_storage_engine_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
        *out_handled = true;
        return apply_set_join_buffer_size_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
        *out_handled = true;
        return validate_set_lc_messages_value(database, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
        *out_handled = true;
        return validate_set_jl_fixed_placeholder_value(database, target, value_node);
    default:
        return MYLITE_OK;
    }
}

static int apply_set_jl_system_variable_cell_value(
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERNAL_TMP_MEM_STORAGE_ENGINE:
        *out_handled = true;
        return apply_set_internal_tmp_mem_storage_engine_cell_value(
            database,
            target,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_JOIN_BUFFER_SIZE:
        *out_handled = true;
        return apply_set_join_buffer_size_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_MESSAGES:
        *out_handled = true;
        return validate_set_lc_messages_cell_value(database, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
        *out_handled = true;
        return validate_set_jl_fixed_placeholder_cell_value(database, target, value, value_kind);
    default:
        return MYLITE_OK;
    }
}

static int apply_set_internal_tmp_mem_storage_engine_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    enum mylite_session_internal_tmp_mem_storage_engine value =
        MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc =
        parse_set_internal_tmp_mem_storage_engine_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (value != MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE) {
            set_unsupported_error(
                database,
                "SET GLOBAL internal_tmp_mem_storage_engine assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.internal_tmp_mem_storage_engine = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_internal_tmp_mem_storage_engine_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    enum mylite_session_internal_tmp_mem_storage_engine parsed_value =
        MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_set_internal_tmp_mem_storage_engine_cell_value(
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
        if (parsed_value != MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE) {
            set_unsupported_error(
                database,
                "SET GLOBAL internal_tmp_mem_storage_engine assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    database->session.internal_tmp_mem_storage_engine = parsed_value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_internal_tmp_mem_storage_engine_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
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
        set_runtime_error(database, "invalid internal_tmp_mem_storage_engine output");
        return MYLITE_ERROR;
    }
    *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
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
        return parse_internal_tmp_mem_storage_engine_text(
            database,
            variable_name,
            identifier,
            out_value
        );
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
        rc =
            parse_internal_tmp_mem_storage_engine_text(database, variable_name, decoded, out_value);
        free(decoded);
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        if (negative) {
            set_system_variable_cant_be_set_value_error(database, variable_name, "-1");
            return MYLITE_ERROR;
        }
        return parse_internal_tmp_mem_storage_engine_integer(
            database,
            variable_name,
            1U,
            "1",
            out_value
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, variable_name);
        }
        return parse_internal_tmp_mem_storage_engine_integer(
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
    if (negative) {
        copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
        set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_internal_tmp_mem_storage_engine_integer(
        database,
        variable_name,
        magnitude,
        value_text,
        out_value
    );
}

static int parse_set_internal_tmp_mem_storage_engine_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid internal_tmp_mem_storage_engine output");
        return MYLITE_ERROR;
    }
    *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
    if (value == NULL || text == NULL) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }
    if (value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return parse_internal_tmp_mem_storage_engine_text(database, variable_name, text, out_value);
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
    return parse_internal_tmp_mem_storage_engine_integer(
        database,
        variable_name,
        magnitude,
        value->value,
        out_value
    );
}

static int parse_internal_tmp_mem_storage_engine_text(
    struct mylite_db *database,
    const char *variable_name,
    const char *text,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (text_equals_ascii_case_insensitive(text, "TempTable")) {
        *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(text, "MEMORY")) {
        *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_MEMORY;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, text);
    return MYLITE_ERROR;
}

static int parse_internal_tmp_mem_storage_engine_integer(
    struct mylite_db *database,
    const char *variable_name,
    uint64_t magnitude,
    const char *value_text,
    enum mylite_session_internal_tmp_mem_storage_engine *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (magnitude == 0U) {
        *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_MEMORY;
        return MYLITE_OK;
    }
    if (magnitude == 1U) {
        *out_value = MYLITE_SESSION_INTERNAL_TMP_MEM_STORAGE_ENGINE_TEMPTABLE;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value_text);
    return MYLITE_ERROR;
}

static int apply_set_join_buffer_size_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = join_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            join_buffer_size_default_value,
            "SET GLOBAL join_buffer_size assignment is not supported"
        );
    }

    rc = parse_set_join_buffer_size_value(database, target->name, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.join_buffer_size = value;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_join_buffer_size_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t join_buffer_size = join_buffer_size_default_value;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET join_buffer_size from user variables is not supported for global scope"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_join_buffer_size_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &join_buffer_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.join_buffer_size = join_buffer_size;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int parse_set_join_buffer_size_value(
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
        set_runtime_error(database, "invalid join_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = join_buffer_size_default_value;
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
        return parse_join_buffer_size_integer(
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
        return parse_join_buffer_size_integer(
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
    return parse_join_buffer_size_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_join_buffer_size_cell_value(
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
        set_runtime_error(database, "invalid join_buffer_size output");
        return MYLITE_ERROR;
    }
    *out_value = join_buffer_size_default_value;
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
    return parse_join_buffer_size_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_join_buffer_size_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;
    uint64_t rounded = 0U;

    if (out_value == NULL) {
        set_runtime_error(database, "invalid join_buffer_size output");
        return MYLITE_ERROR;
    }
    if (negative) {
        *out_value = join_buffer_size_min_value;
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
    if (magnitude < join_buffer_size_min_value) {
        *out_value = join_buffer_size_min_value;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }

    rounded = magnitude - (magnitude % join_buffer_size_block_size);
    *out_value = rounded;
    if (rounded != magnitude) {
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    return MYLITE_OK;
}

static int validate_set_jl_fixed_placeholder_value(
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            key_buffer_size_default_value,
            "SET GLOBAL key_buffer_size assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            key_cache_age_threshold_default_value,
            "SET GLOBAL key_cache_age_threshold assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            key_cache_block_size_default_value,
            "SET GLOBAL key_cache_block_size assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            key_cache_division_limit_default_value,
            "SET GLOBAL key_cache_division_limit assignment is not supported"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
        return validate_set_fixed_text_value(database, value_node, "");
    default:
        return MYLITE_OK;
    }
}

static int validate_set_jl_fixed_placeholder_cell_value(
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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_BUFFER_SIZE:
        return validate_set_jl_fixed_uint64_cell_value(
            database,
            target,
            key_buffer_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_AGE_THRESHOLD:
        return validate_set_jl_fixed_uint64_cell_value(
            database,
            target,
            key_cache_age_threshold_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_BLOCK_SIZE:
        return validate_set_jl_fixed_uint64_cell_value(
            database,
            target,
            key_cache_block_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEY_CACHE_DIVISION_LIMIT:
        return validate_set_jl_fixed_uint64_cell_value(
            database,
            target,
            key_cache_division_limit_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEYRING_OPERATIONS:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOCAL_INFILE:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MANDATORY_ROLES:
        return validate_set_jl_fixed_text_cell_value(database, target, value, value_kind, "");
    default:
        return MYLITE_OK;
    }
}

static int validate_set_jl_fixed_uint64_cell_value(
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

static int validate_set_jl_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    const char *expected_value
) {
    const char *text = value == NULL ? NULL : value->value;

    if (target == NULL || expected_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (!text_equals_ascii_case_insensitive(text, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_set_lc_messages_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *decoded = NULL;
    size_t decoded_size = 0U;
    int rc = MYLITE_OK;

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
        if (text_equals_ascii_case_insensitive(identifier, "en_US")) {
            return MYLITE_OK;
        }
        set_unknown_locale_error(database, identifier);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, "lc_messages");
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, "lc_messages");
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "SET system variables support only string literals",
        "SET system variable string values do not support NUL bytes",
        &decoded,
        &decoded_size
    );
    (void)decoded_size;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(decoded, "en_US")) {
        set_unknown_locale_error(database, decoded);
        free(decoded);
        return MYLITE_ERROR;
    }

    free(decoded);
    return MYLITE_OK;
}

static int validate_set_lc_messages_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;

    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, "lc_messages");
    }
    if (text_equals_ascii_case_insensitive(text, "en_US")) {
        return MYLITE_OK;
    }

    set_unknown_locale_error(database, text);
    return MYLITE_ERROR;
}

static void set_unknown_locale_error(struct mylite_db *database, const char *value_text) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown locale: '%s'",
        value_text == NULL ? "" : value_text
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_locale,
        "HY000",
        message
    );
}
