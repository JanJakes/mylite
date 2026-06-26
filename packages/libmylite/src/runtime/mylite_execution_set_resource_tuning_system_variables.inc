enum {
    resource_tuning_decimal_fraction_digits = 6,
    resource_tuning_decimal_text_capacity = 96,
    resource_tuning_uint64_text_capacity = integer_text_capacity,
};

static int apply_set_resource_tuning_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    char *value = NULL;
    size_t value_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!resource_tuning_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (resource_tuning_system_variable_is_global_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (resource_tuning_system_variable_is_session_read_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = copy_resource_tuning_system_variable_node_value(
        database,
        target,
        value_node,
        &value,
        &value_size
    );
    if (rc != MYLITE_OK) {
        free(value);
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        rc = validate_resource_tuning_fixed_global_value(database, target, value, value_size);
    } else {
        rc = set_session_system_variable_override(database, target->kind, value, value_size);
    }
    if (rc == MYLITE_OK) {
        rc = append_resource_tuning_system_variable_set_warning_if_needed(
            database,
            target->kind
        );
    }

    free(value);
    return rc;
}

static int apply_set_resource_tuning_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    char *text = NULL;
    size_t text_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!resource_tuning_system_variable_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (resource_tuning_system_variable_is_global_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (resource_tuning_system_variable_is_session_read_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_session_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = copy_resource_tuning_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &text,
        &text_size
    );
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        rc = validate_resource_tuning_fixed_global_value(database, target, text, text_size);
    } else {
        rc = set_session_system_variable_override(database, target->kind, text, text_size);
    }
    if (rc == MYLITE_OK) {
        rc = append_resource_tuning_system_variable_set_warning_if_needed(
            database,
            target->kind
        );
    }

    free(text);
    return rc;
}

static bool resource_tuning_system_variable_handles_kind(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_TIME_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_BUFFER_LENGTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRELOAD_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROFILING_HISTORY_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_QUERY_PREALLOC_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RANGE_OPTIMIZER_MAX_MEM_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_READ_RND_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECONDARY_ENGINE_COST_THRESHOLD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SYSTEM_VARIABLES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SET_OPERATIONS_BUFFER_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_GIPK_IN_CREATE_TABLE_AND_INFORMATION_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TMP_TABLE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ALLOC_BLOCK_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_PREALLOC_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WINDOWING_USE_HIGH_PRECISION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_XA_DETACH_ON_PREPARE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS:
        return true;
    default:
        return false;
    }
}

static bool resource_tuning_system_variable_is_global_only(
    enum mylite_execution_system_variable_kind kind
) {
    return kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_STACK_LIMIT ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_REGEXP_TIME_LIMIT;
}

static bool resource_tuning_system_variable_is_session_read_only(
    enum mylite_execution_system_variable_kind kind
) {
    return kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_NET_BUFFER_LENGTH;
}

static int copy_resource_tuning_system_variable_node_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
) {
    char *text = NULL;
    size_t text_size = 0U;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_size = 0U;
    if (mylite_execution_system_variable_is_boolean_session_placeholder(target->kind)) {
        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
            return copy_resource_tuning_static_text(
                database,
                mylite_execution_system_variable_default_value(target->kind),
                out_value,
                out_value_size
            );
        }
        rc = parse_set_big_tables_value(database, target->name, value_node, &boolean_value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return copy_resource_tuning_static_text(
            database,
            boolean_value ? "1" : "0",
            out_value,
            out_value_size
        );
    }

    rc = copy_resource_tuning_system_variable_node_text(
        database,
        target,
        value_node,
        &text,
        &text_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = canonicalize_resource_tuning_system_variable_text(
        database,
        target,
        text,
        text_size,
        out_value,
        out_value_size
    );
    free(text);
    return rc;
}

static int copy_resource_tuning_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    char **out_value,
    size_t *out_value_size
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_size = 0U;
    if (mylite_execution_system_variable_is_boolean_session_placeholder(target->kind)) {
        rc = parse_set_big_tables_cell_value(
            database,
            target->name,
            value,
            value_kind,
            &boolean_value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return copy_resource_tuning_static_text(
            database,
            boolean_value ? "1" : "0",
            out_value,
            out_value_size
        );
    }
    if (text == NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    return canonicalize_resource_tuning_system_variable_text(
        database,
        target,
        text,
        text_size,
        out_value,
        out_value_size
    );
}

static int copy_resource_tuning_system_variable_node_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;

    if (target == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_size = 0U;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    unwrapped = unwrap_parenthesized_expression(value_node);
    if (unwrapped == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(unwrapped) == MYLITE_SQL_AST_LITERAL_NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(unwrapped) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal(
            database,
            unwrapped,
            "SET system variable values support only string literals",
            "SET system variable string values do not support NUL bytes",
            out_value,
            out_value_size
        );
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL ||
        unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return copy_resource_tuning_node_span_text(database, unwrapped, out_value, out_value_size);
    }

    return copy_set_text_system_variable_node_value(
        database,
        unwrapped,
        target->kind,
        target->name,
        out_value,
        out_value_size
    );
}

static int copy_resource_tuning_node_span_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;

    if (span == NULL || span->text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_resource_tuning_text(database, span->text, span->length, out_value, out_value_size);
}

static int canonicalize_resource_tuning_system_variable_text(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
) {
    if (target == NULL || text == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LC_TIME_NAMES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SYSTEM_VARIABLES:
        return copy_resource_tuning_text(database, text, text_size, out_value, out_value_size);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TERMINOLOGY_USE_PREVIOUS:
        return copy_resource_tuning_terminology_value(
            database,
            target,
            text,
            text_size,
            out_value,
            out_value_size
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SECONDARY_ENGINE_COST_THRESHOLD:
        return copy_resource_tuning_decimal_value(
            database,
            target,
            text,
            text_size,
            out_value,
            out_value_size
        );
    default:
        return copy_resource_tuning_uint64_value(
            database,
            target,
            text,
            text_size,
            out_value,
            out_value_size
        );
    }
}

static int copy_resource_tuning_terminology_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
) {
    char *owned = NULL;
    int rc = copy_resource_tuning_text(database, text, text_size, &owned, out_value_size);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(owned, "NONE")) {
        free(owned);
        return copy_resource_tuning_static_text(database, "NONE", out_value, out_value_size);
    }
    if (text_equals_ascii_case_insensitive(owned, "BEFORE_8_0_26")) {
        free(owned);
        return copy_resource_tuning_static_text(
            database,
            "BEFORE_8_0_26",
            out_value,
            out_value_size
        );
    }

    set_system_variable_cant_be_set_value_error(database, target->name, owned);
    free(owned);
    return MYLITE_ERROR;
}

static int copy_resource_tuning_decimal_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
) {
    char *owned = NULL;
    char *end = NULL;
    long double parsed = 0.0L;
    char formatted[resource_tuning_decimal_text_capacity];
    int written = 0;
    int rc = copy_resource_tuning_text(database, text, text_size, &owned, out_value_size);

    if (rc != MYLITE_OK) {
        return rc;
    }
    errno = 0;
    parsed = strtold(owned, &end);
    if (end == owned || end == NULL || *end != '\0' || errno == ERANGE || !isfinite(parsed) ||
        parsed < 0.0L) {
        free(owned);
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (parsed == 0.0L) {
        parsed = 0.0L;
    }
    written = snprintf(
        formatted,
        sizeof(formatted),
        "%.*Lf",
        resource_tuning_decimal_fraction_digits,
        parsed
    );
    free(owned);
    if (written < 0 || (size_t)written >= sizeof(formatted)) {
        set_runtime_error(database, "failed to format resource tuning decimal system variable");
        return MYLITE_ERROR;
    }

    return copy_resource_tuning_text(
        database,
        formatted,
        (size_t)written,
        out_value,
        out_value_size
    );
}

static int copy_resource_tuning_uint64_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
) {
    uint64_t value = 0U;
    char formatted[resource_tuning_uint64_text_capacity];
    int written = 0;

    if (!parse_resource_tuning_uint64_text(text, text_size, &value)) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    written = snprintf(formatted, sizeof(formatted), "%" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(formatted)) {
        set_runtime_error(database, "failed to format resource tuning integer system variable");
        return MYLITE_ERROR;
    }

    return copy_resource_tuning_text(
        database,
        formatted,
        (size_t)written,
        out_value,
        out_value_size
    );
}

static bool parse_resource_tuning_uint64_text(
    const char *text,
    size_t text_size,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    size_t index = 0U;

    if (text == NULL || out_value == NULL || text_size == 0U) {
        return false;
    }
    if (text[index] == '+') {
        ++index;
    } else if (text[index] == '-') {
        return false;
    }
    if (index == text_size) {
        return false;
    }
    for (; index < text_size; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < (unsigned char)'0' || byte > (unsigned char)'9') {
            return false;
        }
        digit = (uint64_t)(byte - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return false;
        }
        value = (value * decimal_base) + digit;
    }

    *out_value = value;
    return true;
}

static int validate_resource_tuning_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *value,
    size_t value_size
) {
    const char *default_value = target == NULL
                                    ? NULL
                                    : mylite_execution_system_variable_default_value(target->kind);

    if (target == NULL || value == NULL || default_value == NULL) {
        set_runtime_error(database, "invalid resource tuning system variable default");
        return MYLITE_ERROR;
    }
    if (strlen(default_value) == value_size && memcmp(default_value, value, value_size) == 0) {
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "SET resource tuning system variable supports only fixed no-op global assignments"
    );
    return MYLITE_ERROR;
}

static int append_resource_tuning_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_RESTRICT_FK_ON_NON_STANDARD_KEY) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_restrict_fk_deprecated,
            "HY000",
            "'restrict_fk_on_non_standard_key' is deprecated and will be removed in a future "
            "release. Foreign key referring to non-unique or partial keys is unsafe and may "
            "break replication."
        );
    }
    if (mylite_execution_system_variable_warns_on_scalar_read(kind)) {
        return append_system_variable_read_warning(database, kind);
    }
    return MYLITE_OK;
}

static int copy_resource_tuning_static_text(
    struct mylite_db *database,
    const char *text,
    char **out_value,
    size_t *out_value_size
) {
    return copy_resource_tuning_text(database, text, strlen(text), out_value, out_value_size);
}

static int copy_resource_tuning_text(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char **out_value,
    size_t *out_value_size
) {
    char *owned = NULL;

    if (text == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    if (text_size == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    owned = (char *)malloc(text_size + 1U);
    if (owned == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(owned, text, text_size);
    owned[text_size] = '\0';
    *out_value = owned;
    *out_value_size = text_size;
    return MYLITE_OK;
}
