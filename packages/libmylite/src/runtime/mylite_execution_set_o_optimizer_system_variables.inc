enum {
    o_optimizer_flag_text_capacity = 1024,
    o_optimizer_flag_token_capacity = 128,
};

static const uint64_t o_optimizer_prune_level_default_value = 1ULL;
static const uint64_t o_optimizer_prune_level_max_value = 1ULL;
static const uint64_t o_optimizer_search_depth_default_value = 62ULL;
static const uint64_t o_optimizer_search_depth_max_value = 62ULL;
static const uint64_t o_optimizer_trace_limit_default_value = 1ULL;
static const uint64_t o_optimizer_trace_max_mem_size_default_value = 1048576ULL;
static const int64_t o_optimizer_trace_offset_default_value = -1;
static const uint64_t o_optimizer_parser_max_mem_size_min_value = 10000000ULL;
static const uint64_t o_optimizer_signed_max_magnitude = 9223372036854775807ULL;
static const uint64_t o_optimizer_signed_min_overflow_magnitude = 9223372036854775808ULL;

struct o_optimizer_flag_descriptor {
    const char *name;
    bool default_enabled;
};

struct o_optimizer_flag_set {
    const struct o_optimizer_flag_descriptor *flags;
    size_t flag_count;
};

static int parse_set_o_optimizer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
);
static int parse_set_o_optimizer_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
);
static int apply_o_optimizer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value
);
static int validate_set_o_optimizer_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_o_optimizer_fixed_global_string_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
);
static int validate_set_o_optimizer_fixed_global_signed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
);
static int parse_o_optimizer_unsigned_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_unsigned_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_unsigned_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
);
static int parse_o_optimizer_signed_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_signed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_signed_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    int64_t *out_value
);
static int parse_o_optimizer_flag_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_flag_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
);
static int parse_o_optimizer_flag_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char *out_value,
    size_t out_value_size
);
static int render_o_optimizer_flag_state(
    struct mylite_db *database,
    struct o_optimizer_flag_set flag_set,
    const bool *state,
    char *out_value,
    size_t out_value_size
);
static int render_o_optimizer_default_flags(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    char *out_value,
    size_t out_value_size
);
static struct o_optimizer_flag_set o_optimizer_flag_set_for_kind(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t o_optimizer_unsigned_default_value(
    enum mylite_execution_system_variable_kind kind
);
static uint64_t o_optimizer_unsigned_min_value(enum mylite_execution_system_variable_kind kind);
static uint64_t o_optimizer_unsigned_max_value(enum mylite_execution_system_variable_kind kind);
static bool o_optimizer_system_variable_is_unsigned(
    enum mylite_execution_system_variable_kind kind
);
static bool o_optimizer_system_variable_is_flag_set(
    enum mylite_execution_system_variable_kind kind
);
static int copy_o_optimizer_text_for_error(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
);
static bool o_optimizer_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
);
static void trim_o_optimizer_ascii_spaces(const char **text, size_t *text_size);
static bool o_optimizer_ascii_space(char byte);
static int format_o_optimizer_int64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);

static const struct o_optimizer_flag_descriptor optimizer_switch_flags[] = {
    {"index_merge", true},
    {"index_merge_union", true},
    {"index_merge_sort_union", true},
    {"index_merge_intersection", true},
    {"engine_condition_pushdown", true},
    {"index_condition_pushdown", true},
    {"mrr", true},
    {"mrr_cost_based", true},
    {"block_nested_loop", true},
    {"batched_key_access", false},
    {"materialization", true},
    {"semijoin", true},
    {"loosescan", true},
    {"firstmatch", true},
    {"duplicateweedout", true},
    {"subquery_materialization_cost_based", true},
    {"use_index_extensions", true},
    {"condition_fanout_filter", true},
    {"derived_merge", true},
    {"use_invisible_indexes", false},
    {"skip_scan", true},
    {"hash_join", true},
    {"subquery_to_derived", false},
    {"prefer_ordering_index", true},
    {"hypergraph_optimizer", false},
    {"derived_condition_pushdown", true},
    {"hash_set_operations", true},
};

static const struct o_optimizer_flag_descriptor optimizer_trace_flags[] = {
    {"enabled", false},
    {"one_line", false},
};

static const struct o_optimizer_flag_descriptor optimizer_trace_feature_flags[] = {
    {"greedy_search", true},
    {"range_optimizer", true},
    {"dynamic_range", true},
    {"repeated_subselect", true},
};

static int apply_set_o_optimizer_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    char value[o_optimizer_flag_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_o_optimizer(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_boolean_value(database, value_node, false);
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return validate_set_o_optimizer_fixed_global_value(database, target, value_node);
    }

    rc = parse_set_o_optimizer_system_variable_value(
        database,
        target,
        value_node,
        value,
        sizeof(value)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_o_optimizer_system_variable_value(database, target, value);
}

static int apply_set_o_optimizer_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    char parsed_value[o_optimizer_flag_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_o_optimizer(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PARTIAL_REVOKES) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        set_unsupported_error(
            database,
            "SET system variables from user variables is not supported for global scope"
        );
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(
            database,
            "SET system variables from user variables is not supported for global scope"
        );
        return MYLITE_ERROR;
    }

    rc = parse_set_o_optimizer_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        parsed_value,
        sizeof(parsed_value)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return apply_o_optimizer_system_variable_value(database, target, parsed_value);
}

static int parse_set_o_optimizer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
) {
    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (o_optimizer_system_variable_is_flag_set(target->kind)) {
        return parse_o_optimizer_flag_value_node(
            database,
            target,
            value_node,
            out_value,
            out_value_size
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET) {
        return parse_o_optimizer_signed_value_node(
            database,
            target,
            value_node,
            out_value,
            out_value_size
        );
    }
    if (o_optimizer_system_variable_is_unsigned(target->kind)) {
        return parse_o_optimizer_unsigned_value_node(
            database,
            target,
            value_node,
            out_value,
            out_value_size
        );
    }

    set_runtime_error(database, "invalid O optimizer system variable target");
    return MYLITE_ERROR;
}

static int parse_set_o_optimizer_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
) {
    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (o_optimizer_system_variable_is_flag_set(target->kind)) {
        return parse_o_optimizer_flag_cell_value(
            database,
            target,
            value,
            value_kind,
            out_value,
            out_value_size
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET) {
        return parse_o_optimizer_signed_cell_value(
            database,
            target,
            value,
            value_kind,
            out_value,
            out_value_size
        );
    }
    if (o_optimizer_system_variable_is_unsigned(target->kind)) {
        return parse_o_optimizer_unsigned_cell_value(
            database,
            target,
            value,
            value_kind,
            out_value,
            out_value_size
        );
    }

    set_runtime_error(database, "invalid O optimizer system variable target");
    return MYLITE_ERROR;
}

static int apply_o_optimizer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value
) {
    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    return set_session_system_variable_override(database, target->kind, value, strlen(value));
}

static int validate_set_o_optimizer_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    static const char unsupported_message[] =
        "SET supports only fixed no-op system variable assignments";

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (o_optimizer_system_variable_is_flag_set(target->kind)) {
        return validate_set_o_optimizer_fixed_global_string_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_OFFSET) {
        return validate_set_o_optimizer_fixed_global_signed_value(database, value_node);
    }
    if (o_optimizer_system_variable_is_unsigned(target->kind)) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            o_optimizer_unsigned_default_value(target->kind),
            unsupported_message
        );
    }

    set_runtime_error(database, "invalid O optimizer global system variable target");
    return MYLITE_ERROR;
}

static int validate_set_o_optimizer_fixed_global_string_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    char parsed[o_optimizer_flag_text_capacity];
    char expected[o_optimizer_flag_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    rc = parse_o_optimizer_flag_value_node(database, target, value_node, parsed, sizeof(parsed));
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = render_o_optimizer_default_flags(database, target, expected, sizeof(expected));
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(parsed, expected) == 0) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "SET supports only fixed no-op system variable assignments");
    return MYLITE_ERROR;
}

static int validate_set_o_optimizer_fixed_global_signed_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool negative = false;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }

    literal_node = unwrap_auto_increment_step_value_literal(value_node, &negative);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(database, "SET supports only fixed no-op system variable assignments");
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal_node);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal_node->span, &magnitude) != MYLITE_OK ||
        !negative || magnitude != 1ULL) {
        set_unsupported_error(database, "SET supports only fixed no-op system variable assignments");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int parse_o_optimizer_unsigned_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t value = 0U;
    bool negative = false;
    char value_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    value = o_optimizer_unsigned_default_value(target->kind);
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
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
            return parse_o_optimizer_unsigned_integer(
                database,
                target,
                &true_span,
                negative,
                negative ? "-1" : "1",
                &value
            ) == MYLITE_OK
                       ? format_uint64(database, value, out_value, out_value_size)
                       : MYLITE_ERROR;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            const struct mylite_sql_source_span false_span = {.text = "0", .length = 1U};

            if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
                return set_incorrect_system_variable_argument_type_error(database, target->name);
            }
            return parse_o_optimizer_unsigned_integer(
                database,
                target,
                &false_span,
                negative,
                negative ? "-0" : "0",
                &value
            ) == MYLITE_OK
                       ? format_uint64(database, value, out_value, out_value_size)
                       : MYLITE_ERROR;
        }
        if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }

        copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
        rc = parse_o_optimizer_unsigned_integer(
            database,
            target,
            &literal_node->span,
            negative,
            value_text,
            &value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return format_uint64(database, value, out_value, out_value_size);
}

static int parse_o_optimizer_unsigned_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t parsed_value = 0U;
    bool negative = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    parsed_value = o_optimizer_unsigned_default_value(target->kind);
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
    rc = parse_o_optimizer_unsigned_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return format_uint64(database, parsed_value, out_value, out_value_size);
}

static int parse_o_optimizer_unsigned_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    uint64_t minimum = 0U;
    uint64_t maximum = 0U;
    bool truncated = false;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &value) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    minimum = o_optimizer_unsigned_min_value(target->kind);
    maximum = o_optimizer_unsigned_max_value(target->kind);
    if (negative) {
        value = 0U;
        truncated = true;
    }
    if (value < minimum) {
        value = minimum;
        truncated = true;
    }
    if (value > maximum) {
        value = maximum;
        truncated = true;
    }

    *out_value = value;
    return truncated ? append_truncated_incorrect_system_variable_warning(
                           database,
                           target->name,
                           value_text
                       )
                     : MYLITE_OK;
}

static int parse_o_optimizer_signed_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int64_t value = o_optimizer_trace_offset_default_value;
    bool negative = false;
    char value_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
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
            return parse_o_optimizer_signed_integer(
                database,
                target,
                &true_span,
                negative,
                negative ? "-1" : "1",
                &value
            ) == MYLITE_OK
                       ? format_o_optimizer_int64(database, value, out_value, out_value_size)
                       : MYLITE_ERROR;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            const struct mylite_sql_source_span false_span = {.text = "0", .length = 1U};

            if (!sql_mode_token_matches(literal_node->span.text, literal_node->span.length, "FALSE")) {
                return set_incorrect_system_variable_argument_type_error(database, target->name);
            }
            return parse_o_optimizer_signed_integer(
                database,
                target,
                &false_span,
                negative,
                negative ? "-0" : "0",
                &value
            ) == MYLITE_OK
                       ? format_o_optimizer_int64(database, value, out_value, out_value_size)
                       : MYLITE_ERROR;
        }
        if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }

        copy_auto_increment_step_value_text(value_node, value_text, sizeof(value_text));
        rc = parse_o_optimizer_signed_integer(
            database,
            target,
            &literal_node->span,
            negative,
            value_text,
            &value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return format_o_optimizer_int64(database, value, out_value, out_value_size);
}

static int parse_o_optimizer_signed_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    int64_t parsed_value = o_optimizer_trace_offset_default_value;
    bool negative = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
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
    rc = parse_o_optimizer_signed_integer(
        database,
        target,
        &unsigned_span,
        negative,
        value->value,
        &parsed_value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return format_o_optimizer_int64(database, parsed_value, out_value, out_value_size);
}

static int parse_o_optimizer_signed_integer(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_source_span *unsigned_span,
    bool negative,
    const char *value_text,
    int64_t *out_value
) {
    uint64_t magnitude = 0U;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (unsigned_span == NULL ||
        parse_unsigned_integer_literal(unsigned_span, &magnitude) != MYLITE_OK) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (negative) {
        if (magnitude > o_optimizer_signed_min_overflow_magnitude) {
            return set_incorrect_system_variable_argument_type_error(database, target->name);
        }
        if (magnitude == o_optimizer_signed_min_overflow_magnitude) {
            *out_value = -(int64_t)o_optimizer_signed_max_magnitude;
            return append_truncated_incorrect_system_variable_warning(
                database,
                target->name,
                value_text
            );
        }
        *out_value = -(int64_t)magnitude;
        return MYLITE_OK;
    }
    if (magnitude > o_optimizer_signed_max_magnitude) {
        *out_value = (int64_t)o_optimizer_signed_max_magnitude;
        return append_truncated_incorrect_system_variable_warning(database, target->name, value_text);
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int parse_o_optimizer_flag_value_node(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char *out_value,
    size_t out_value_size
) {
    const struct mylite_sql_ast_node *literal_node = NULL;
    char *decoded = NULL;
    size_t decoded_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return render_o_optimizer_default_flags(database, target, out_value, out_value_size);
    }
    literal_node = unwrap_parenthesized_expression(value_node);
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    rc = decode_sql_string_literal(
        database,
        literal_node,
        "SET system variables support only string literals",
        "SET system variable string values do not support NUL bytes",
        &decoded,
        &decoded_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = parse_o_optimizer_flag_text(
        database,
        target,
        decoded,
        decoded_size,
        out_value,
        out_value_size
    );
    free(decoded);
    return rc;
}

static int parse_o_optimizer_flag_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char *out_value,
    size_t out_value_size
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }
    if (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    return parse_o_optimizer_flag_text(
        database,
        target,
        text,
        text_size,
        out_value,
        out_value_size
    );
}

static int parse_o_optimizer_flag_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char *out_value,
    size_t out_value_size
) {
    struct o_optimizer_flag_set flag_set = {0};
    const char *remaining = text;
    size_t remaining_size = text_size;
    bool state[sizeof(optimizer_switch_flags) / sizeof(optimizer_switch_flags[0])] = {false};

    if (target == NULL || text == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    flag_set = o_optimizer_flag_set_for_kind(target->kind);
    if (flag_set.flags == NULL || flag_set.flag_count > sizeof(state) / sizeof(state[0])) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < flag_set.flag_count; ++index) {
        state[index] = flag_set.flags[index].default_enabled;
    }

    trim_o_optimizer_ascii_spaces(&remaining, &remaining_size);
    if (o_optimizer_span_equals_ascii_case_insensitive(remaining, remaining_size, "default")) {
        return render_o_optimizer_flag_state(database, flag_set, state, out_value, out_value_size);
    }

    while (remaining_size > 0U) {
        const char *token = remaining;
        const char *equals = NULL;
        size_t token_size = 0U;
        const char *flag_name = NULL;
        size_t flag_name_size = 0U;
        const char *flag_value = NULL;
        size_t flag_value_size = 0U;
        char token_text[o_optimizer_flag_token_capacity];
        bool found = false;

        while (token_size < remaining_size && token[token_size] != ',') {
            ++token_size;
        }
        remaining = token + token_size;
        remaining_size -= token_size;
        if (remaining_size > 0U && remaining[0] == ',') {
            ++remaining;
            --remaining_size;
        }

        trim_o_optimizer_ascii_spaces(&token, &token_size);
        if (token_size == 0U) {
            set_system_variable_cant_be_set_value_error(database, target->name, "");
            return MYLITE_ERROR;
        }
        for (size_t index = 0U; index < token_size; ++index) {
            if (token[index] == '=') {
                equals = token + index;
                break;
            }
        }
        if (equals == NULL) {
            copy_o_optimizer_text_for_error(
                token,
                token_size,
                token_text,
                sizeof(token_text)
            );
            set_system_variable_cant_be_set_value_error(database, target->name, token_text);
            return MYLITE_ERROR;
        }

        flag_name = token;
        flag_name_size = (size_t)(equals - token);
        flag_value = equals + 1;
        flag_value_size = token_size - flag_name_size - 1U;
        trim_o_optimizer_ascii_spaces(&flag_name, &flag_name_size);
        trim_o_optimizer_ascii_spaces(&flag_value, &flag_value_size);
        copy_o_optimizer_text_for_error(token, token_size, token_text, sizeof(token_text));

        for (size_t index = 0U; index < flag_set.flag_count; ++index) {
            if (!o_optimizer_span_equals_ascii_case_insensitive(
                    flag_name,
                    flag_name_size,
                    flag_set.flags[index].name
                )) {
                continue;
            }
            found = true;
            if (o_optimizer_span_equals_ascii_case_insensitive(flag_value, flag_value_size, "on")) {
                state[index] = true;
            } else if (o_optimizer_span_equals_ascii_case_insensitive(
                           flag_value,
                           flag_value_size,
                           "off"
                       )) {
                state[index] = false;
            } else if (o_optimizer_span_equals_ascii_case_insensitive(
                           flag_value,
                           flag_value_size,
                           "default"
                       )) {
                state[index] = flag_set.flags[index].default_enabled;
            } else {
                set_system_variable_cant_be_set_value_error(database, target->name, token_text);
                return MYLITE_ERROR;
            }
            break;
        }
        if (!found) {
            set_system_variable_cant_be_set_value_error(database, target->name, token_text);
            return MYLITE_ERROR;
        }
    }

    return render_o_optimizer_flag_state(database, flag_set, state, out_value, out_value_size);
}

static int render_o_optimizer_flag_state(
    struct mylite_db *database,
    struct o_optimizer_flag_set flag_set,
    const bool *state,
    char *out_value,
    size_t out_value_size
) {
    size_t used = 0U;

    if (flag_set.flags == NULL || state == NULL || out_value == NULL || out_value_size == 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < flag_set.flag_count; ++index) {
        const char *separator = index == 0U ? "" : ",";
        const char *flag_value = state[index] ? "on" : "off";
        int written = snprintf(
            out_value + used,
            out_value_size - used,
            "%s%s=%s",
            separator,
            flag_set.flags[index].name,
            flag_value
        );

        if (written < 0 || (size_t)written >= out_value_size - used) {
            set_runtime_error(database, "failed to format optimizer system variable value");
            return MYLITE_ERROR;
        }
        used += (size_t)written;
    }
    return MYLITE_OK;
}

static int render_o_optimizer_default_flags(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    char *out_value,
    size_t out_value_size
) {
    struct o_optimizer_flag_set flag_set = {0};
    bool state[sizeof(optimizer_switch_flags) / sizeof(optimizer_switch_flags[0])] = {false};

    if (target == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    flag_set = o_optimizer_flag_set_for_kind(target->kind);
    if (flag_set.flags == NULL || flag_set.flag_count > sizeof(state) / sizeof(state[0])) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < flag_set.flag_count; ++index) {
        state[index] = flag_set.flags[index].default_enabled;
    }
    return render_o_optimizer_flag_state(database, flag_set, state, out_value, out_value_size);
}

static struct o_optimizer_flag_set o_optimizer_flag_set_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
        return (struct o_optimizer_flag_set){
            .flags = optimizer_switch_flags,
            .flag_count = sizeof(optimizer_switch_flags) / sizeof(optimizer_switch_flags[0]),
        };
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
        return (struct o_optimizer_flag_set){
            .flags = optimizer_trace_flags,
            .flag_count = sizeof(optimizer_trace_flags) / sizeof(optimizer_trace_flags[0]),
        };
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
        return (struct o_optimizer_flag_set){
            .flags = optimizer_trace_feature_flags,
            .flag_count =
                sizeof(optimizer_trace_feature_flags) / sizeof(optimizer_trace_feature_flags[0]),
        };
    default:
        return (struct o_optimizer_flag_set){0};
    }
}

static uint64_t o_optimizer_unsigned_default_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
        return o_optimizer_prune_level_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
        return o_optimizer_search_depth_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
        return o_optimizer_trace_limit_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
        return o_optimizer_trace_max_mem_size_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
        return UINT64_MAX;
    default:
        return 0U;
    }
}

static uint64_t o_optimizer_unsigned_min_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
        return o_optimizer_parser_max_mem_size_min_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    default:
        return 0U;
    }
}

static uint64_t o_optimizer_unsigned_max_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
        return o_optimizer_prune_level_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
        return o_optimizer_search_depth_max_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
        return o_optimizer_signed_max_magnitude;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
    default:
        return UINT64_MAX;
    }
}

static bool o_optimizer_system_variable_is_unsigned(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_PRUNE_LEVEL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SEARCH_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PARSER_MAX_MEM_SIZE:
        return true;
    default:
        return false;
    }
}

static bool o_optimizer_system_variable_is_flag_set(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_SWITCH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OPTIMIZER_TRACE_FEATURES:
        return true;
    default:
        return false;
    }
}

static int copy_o_optimizer_text_for_error(
    const char *text,
    size_t text_size,
    char *buffer,
    size_t buffer_size
) {
    size_t copy_size = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    if (text == NULL) {
        buffer[0] = '\0';
        return MYLITE_OK;
    }
    copy_size = text_size;
    if (copy_size >= buffer_size) {
        copy_size = buffer_size - 1U;
    }
    memcpy(buffer, text, copy_size);
    buffer[copy_size] = '\0';
    return MYLITE_OK;
}

static bool o_optimizer_span_equals_ascii_case_insensitive(
    const char *text,
    size_t text_size,
    const char *expected
) {
    size_t expected_size = expected == NULL ? 0U : strlen(expected);

    if (text == NULL || expected == NULL || text_size != expected_size) {
        return false;
    }
    for (size_t index = 0U; index < text_size; ++index) {
        if (ascii_lower((unsigned char)text[index]) !=
            ascii_lower((unsigned char)expected[index])) {
            return false;
        }
    }
    return true;
}

static void trim_o_optimizer_ascii_spaces(const char **text, size_t *text_size) {
    if (text == NULL || *text == NULL || text_size == NULL) {
        return;
    }
    while (*text_size > 0U && o_optimizer_ascii_space((*text)[0])) {
        ++*text;
        --*text_size;
    }
    while (*text_size > 0U && o_optimizer_ascii_space((*text)[*text_size - 1U])) {
        --*text_size;
    }
}

static bool o_optimizer_ascii_space(char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' ||
           byte == '\v';
}

static int format_o_optimizer_int64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format optimizer system variable value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
