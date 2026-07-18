static const uint64_t m_session_limit_delayed_threads_default_value = 20ULL;
static const uint64_t m_session_limit_delayed_threads_max_value = 16384ULL;
static const uint64_t m_session_limit_heap_table_default_value = 16777216ULL;
static const uint64_t m_session_limit_heap_table_min_value = 16384ULL;
static const uint64_t m_session_limit_heap_table_max_value = 18446744073709550592ULL;
static const uint64_t m_session_limit_sort_data_default_value = 4096ULL;
static const uint64_t m_session_limit_sort_data_max_value = 8388608ULL;
static const uint64_t m_session_limit_points_geometry_default_value = 65536ULL;
static const uint64_t m_session_limit_points_geometry_max_value = 1048576ULL;
static const uint64_t m_session_limit_sort_length_default_value = 1024ULL;
static const uint64_t m_session_limit_sp_recursion_depth_max_value = 255ULL;

static int apply_set_m_session_limit_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_m_session_limit(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_session_read_only_system_variable_error(database, target->name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_uint64_value(database, value_node, 0U, unsupported_message);
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            m_session_limit_system_variable_default_value(target->kind),
            unsupported_message
        );
        if (rc == MYLITE_OK &&
            mylite_execution_system_variable_warns_on_scalar_read(target->kind)) {
            rc = append_system_variable_read_warning(database, target->kind);
        }
        return rc;
    }

    rc = parse_set_m_session_limit_system_variable_value(database, target, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_m_session_limit_system_variable_value(database, target, value);
}

static int apply_set_m_session_limit_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    uint64_t parsed_value = 0U;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_m_session_limit(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_session_read_only_system_variable_error(database, target->name);
            return MYLITE_ERROR;
        }
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET system variables from user variables is not supported for global scope"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_m_session_limit_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_m_session_limit_system_variable_value(database, target, parsed_value);
}

static int parse_set_m_session_limit_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool negative = false;
    char value_text[integer_text_capacity];

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = m_session_limit_system_variable_default_value(target->kind);
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        const struct mylite_sql_source_span true_span = {.text = "1", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "TRUE")) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        return parse_m_session_limit_system_variable_integer(
            database,
            target,
            &true_span,
            negative,
            negative ? "-1" : "1",
            out_value
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        const struct mylite_sql_source_span false_span = {.text = "0", .length = 1U};

        if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        return parse_m_session_limit_system_variable_integer(
            database,
            target,
            &false_span,
            negative,
            negative ? "-0" : "0",
            out_value
        );
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
    return parse_m_session_limit_system_variable_integer(
        database,
        target,
        &literal_node->span,
        negative,
        value_text,
        out_value
    );
}

static int parse_set_m_session_limit_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    uint64_t *out_value
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool negative = false;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = m_session_limit_system_variable_default_value(target->kind);
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        ++text;
        --text_size;
    }
    if (text_size == 0U) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    return parse_m_session_limit_system_variable_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        out_value
    );
}

static int parse_m_session_limit_system_variable_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (negative) {
        value = 0U;
    }
    if (m_session_limit_system_variable_is_zero_or_default_only(target->kind)) {
        if (value != 0U) {
            set_system_variable_cant_be_set_value_error(database, target->name, value_text);
            return MYLITE_ERROR;
        }
        *out_value = 0U;
        return MYLITE_OK;
    }
    if (value < m_session_limit_system_variable_min_value(target->kind)) {
        value = m_session_limit_system_variable_min_value(target->kind);
    }
    if (value > m_session_limit_system_variable_max_value(target->kind)) {
        value = m_session_limit_system_variable_max_value(target->kind);
    }

    *out_value = value;
    return MYLITE_OK;
}

static int apply_m_session_limit_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t value
) {
    char value_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    rc = format_uint64(database, value, value_text, sizeof(value_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = set_session_system_variable_override(
        database,
        target->kind,
        value_text,
        strlen(value_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE) {
        const char *sql_big_selects =
            value == m_session_limit_system_variable_default_value(target->kind) ? "1" : "0";

        rc = set_session_system_variable_override(
            database,
            MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_BIG_SELECTS,
            sql_big_selects,
            1U
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (mylite_execution_system_variable_warns_on_scalar_read(target->kind)) {
        rc = append_system_variable_read_warning(database, target->kind);
    }
    return rc;
}

static uint64_t m_session_limit_system_variable_default_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
        return m_session_limit_delayed_threads_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
        return m_session_limit_heap_table_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
        return UINT64_MAX;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
        return m_session_limit_sort_data_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
        return m_session_limit_points_geometry_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
        return m_session_limit_sort_length_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
    default:
        return 0ULL;
    }
}

static uint64_t m_session_limit_system_variable_min_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
        return m_session_limit_heap_table_min_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
        return 1ULL;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
        return 4ULL;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
        return 3ULL;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
    default:
        return 0ULL;
    }
}

static uint64_t m_session_limit_system_variable_max_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS:
        return m_session_limit_delayed_threads_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_HEAP_TABLE_SIZE:
        return m_session_limit_heap_table_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_LENGTH_FOR_SORT_DATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SORT_LENGTH:
        return m_session_limit_sort_data_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_POINTS_IN_GEOMETRY:
        return m_session_limit_points_geometry_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SP_RECURSION_DEPTH:
        return m_session_limit_sp_recursion_depth_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_EXECUTION_TIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_JOIN_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_SEEKS_FOR_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_USER_CONNECTIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MIN_EXAMINED_ROW_LIMIT:
    default:
        return UINT64_MAX;
    }
}

static bool m_session_limit_system_variable_is_zero_or_default_only(
    enum mylite_execution_system_variable_kind kind
) {
    return (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_DELAYED_THREADS ||
            kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_INSERT_DELAYED_THREADS) != 0;
}
