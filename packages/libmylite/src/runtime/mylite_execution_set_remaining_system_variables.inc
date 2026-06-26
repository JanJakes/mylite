static int apply_set_remaining_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!remaining_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INSERT_ID:
        return apply_set_insert_id_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_THREAD_ID:
        return apply_set_pseudo_thread_id_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED2:
        return apply_set_rand_seed_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_RAM:
        return apply_set_temptable_max_ram_value(database, target, value_node);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPEN_FILES_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STATEMENT_ID:
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    default:
        return MYLITE_OK;
    }
}

static int apply_set_remaining_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!remaining_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INSERT_ID:
        return apply_set_insert_id_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_THREAD_ID:
        return apply_set_pseudo_thread_id_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED2:
        return apply_set_rand_seed_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_RAM:
        return apply_set_temptable_max_ram_cell_value(database, target, value, value_kind);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPEN_FILES_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STATEMENT_ID:
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    default:
        return MYLITE_OK;
    }
}

static bool remaining_system_variable_handles_kind(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INSERT_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPEN_FILES_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PSEUDO_THREAD_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED2:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_STATEMENT_ID:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TEMPTABLE_MAX_RAM:
        return true;
    default:
        return false;
    }
}

static int apply_set_insert_id_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_value(
        database,
        target->name,
        value_node,
        false,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.insert_id = value;
    return MYLITE_OK;
}

static int apply_set_insert_id_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t parsed_value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.insert_id = parsed_value;
    return MYLITE_OK;
}

static int apply_set_pseudo_thread_id_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const uint64_t pseudo_thread_id_max = UINT32_MAX;
    char value_text[integer_text_capacity];
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    rc = parse_remaining_uint64_system_variable_value(
        database,
        target->name,
        value_node,
        true,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value > pseudo_thread_id_max) {
        database->session.connection_id = pseudo_thread_id_max;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value_text
        );
    }
    database->session.connection_id = value;
    return MYLITE_OK;
}

static int apply_set_pseudo_thread_id_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const uint64_t pseudo_thread_id_max = UINT32_MAX;
    uint64_t parsed_value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (parsed_value > pseudo_thread_id_max) {
        database->session.connection_id = pseudo_thread_id_max;
        return append_truncated_incorrect_system_variable_warning(
            database,
            target->name,
            value == NULL ? "" : value->value
        );
    }
    database->session.connection_id = parsed_value;
    return MYLITE_OK;
}

static int apply_set_rand_seed_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_value(
        database,
        target->name,
        value_node,
        false,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1) {
        database->session.rand_seed1 = mylite_rand_canonical_state_word(value);
    } else {
        database->session.rand_seed2 = mylite_rand_canonical_state_word(value);
    }
    database->session.rand_seed_state_initialized = true;
    return MYLITE_OK;
}

static int apply_set_rand_seed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t parsed_value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_RAND_SEED1) {
        database->session.rand_seed1 = mylite_rand_canonical_state_word(parsed_value);
    } else {
        database->session.rand_seed2 = mylite_rand_canonical_state_word(parsed_value);
    }
    database->session.rand_seed_state_initialized = true;
    return MYLITE_OK;
}

static int apply_set_temptable_max_ram_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        database->session.temptable_max_ram = 0U;
        database->session.temptable_max_ram_has_override = false;
        return MYLITE_OK;
    }

    rc = parse_remaining_uint64_system_variable_value(
        database,
        target->name,
        value_node,
        false,
        false,
        &value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.temptable_max_ram = value;
    database->session.temptable_max_ram_has_override = true;
    return MYLITE_OK;
}

static int apply_set_temptable_max_ram_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t parsed_value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = parse_remaining_uint64_system_variable_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.temptable_max_ram = parsed_value;
    database->session.temptable_max_ram_has_override = true;
    return MYLITE_OK;
}

static int parse_remaining_uint64_system_variable_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node,
    bool allow_default,
    bool default_value,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0U;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        if (!allow_default) {
            set_variable_no_default_error(database, variable_name);
            return MYLITE_ERROR;
        }
        *out_value = default_value ? 1U : 0U;
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
    return parse_remaining_uint64_system_variable_integer(
        database,
        variable_name,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_remaining_uint64_system_variable_cell_value(
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
        return MYLITE_MISUSE;
    }
    *out_value = 0U;
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
    return parse_remaining_uint64_system_variable_integer(
        database,
        variable_name,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_remaining_uint64_system_variable_integer(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (negative) {
        *out_value = 0U;
        return append_truncated_incorrect_system_variable_warning(
            database,
            variable_name,
            value_text
        );
    }
    return finish_remaining_nonnegative_uint64_system_variable(
        database,
        variable_name,
        unsigned_span,
        out_value
    );
}

static int finish_remaining_nonnegative_uint64_system_variable(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_source_span *unsigned_span,
    uint64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, variable_name);
    }

    *out_value = magnitude;
    return MYLITE_OK;
}
