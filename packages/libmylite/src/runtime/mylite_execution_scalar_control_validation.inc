
static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_PI_FUNCTION:
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static int validate_if_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    struct if_validation_stack stack = {0};
    int rc = MYLITE_OK;

    rc = if_validation_stack_push(database, &stack, expression);
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = stack.items[stack.count];
        rc = validate_if_value_node(database, &stack, current, function_name);
    }

    if_validation_stack_deinit(&stack);
    return rc;
}

static int validate_if_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IF_FUNCTION) {
        return validate_if_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_IFNULL_FUNCTION) {
        return validate_ifnull_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_COALESCE_FUNCTION) {
        return validate_coalesce_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        return validate_nullif_function_value_node(database, stack, expression, function_name);
    }
    if (expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION) {
        return validate_isnull_function_value_node(database, stack, expression, function_name);
    }
    if (generic_function_name_equals(expression, "GET_LOCK")) {
        const struct mylite_sql_ast_node *arguments = expression->first_child;

        while (arguments != NULL && arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
            arguments = arguments->next_sibling;
        }
        if (arguments == NULL || mylite_sql_ast_node_child_count(arguments) != 2U) {
            set_native_function_parameter_count_error(database, "GET_LOCK");
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (!is_if_non_function_value_expression(expression)) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_if_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 3U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 2U));
    }
    return rc;
}

static int validate_ifnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    return rc;
}

static int validate_coalesce_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        mylite_sql_ast_node_child_count(arguments) == 0U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    argument = child_at(arguments, 0U);
    while (rc == MYLITE_OK && argument != NULL) {
        rc = if_validation_stack_push(database, stack, argument);
        argument = argument->next_sibling;
    }
    return rc;
}

static int validate_nullif_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 0U));
    if (rc == MYLITE_OK) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    }
    return rc;
}

static int validate_isnull_function_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        set_if_unsupported_error(database, function_name);
        return MYLITE_ERROR;
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *case_value = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_case_expression_kind(expression->kind)) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        if (child_count < 1U || child_count > 2U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        when_list = child_at(expression, 0U);
        else_clause = child_at(expression, 1U);
    } else {
        if (child_count < 2U || child_count > 3U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        case_value = child_at(expression, 0U);
        when_list = child_at(expression, 1U);
        else_clause = child_at(expression, 2U);
        rc = validate_case_value_expression(database, case_value);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST ||
        mylite_sql_ast_node_child_count(when_list) == 0U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            mylite_sql_ast_node_child_count(when_clause) != 2U) {
            set_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
        rc = validate_case_value_expression(database, child_at(when_clause, 0U));
        if (rc == MYLITE_OK) {
            rc = validate_case_value_expression(database, child_at(when_clause, 1U));
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause == NULL) {
        return MYLITE_OK;
    }
    if (else_clause->kind != MYLITE_SQL_AST_CASE_ELSE_CLAUSE ||
        mylite_sql_ast_node_child_count(else_clause) != 1U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return validate_case_value_expression(database, child_at(else_clause, 0U));
}

static int validate_case_value_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct if_validation_stack stack = {0};
    int rc = if_validation_stack_push(database, &stack, expression);

    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];

        rc = validate_case_value_node(database, &stack, current);
    }
    if_validation_stack_deinit(&stack);
    return rc;
}

static int validate_case_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    bool handled = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = validate_case_argument_count_error_node(database, expression, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (!case_value_expression_is_admitted(expression)) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }

    if (is_scalar_function_expression(expression)) {
        return validate_if_value_expression(database, expression, if_function_name(expression));
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        return validate_case_mod_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return validate_case_unary_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return validate_case_binary_value_node(database, stack, expression);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return validate_case_literal_value_node(database, expression);
    }

    return MYLITE_OK;
}

static int validate_case_argument_count_error_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    switch (expression->kind) {
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        *out_handled = true;
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        *out_handled = true;
        return MYLITE_ERROR;
    default:
        return MYLITE_OK;
    }
}

static int validate_case_mod_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        set_case_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
    if (rc != MYLITE_OK) {
        return rc;
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_unary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const struct mylite_sql_ast_node *operand = child_at(expression, 0U);
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(operand);

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
            return validate_case_literal_value_node(database, expression);
        }
        return if_validation_stack_push(database, stack, operand);
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return if_validation_stack_push(database, stack, operand);
    }
    set_case_unsupported_error(database);
    return MYLITE_ERROR;
}

static int validate_case_binary_value_node(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    int rc = MYLITE_OK;

    if (!is_scalar_is_operator(operator_kind)) {
        rc = if_validation_stack_push(database, stack, child_at(expression, 1U));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return if_validation_stack_push(database, stack, child_at(expression, 0U));
}

static int validate_case_literal_value_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    if (is_if_non_function_value_expression(expression)) {
        return MYLITE_OK;
    }
    set_case_unsupported_error(database);
    return MYLITE_ERROR;
}

static bool case_value_expression_is_admitted(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || is_session_scalar_expression(expression)) {
        return false;
    }
    if (is_case_expression_kind(expression->kind)) {
        return false;
    }
    if (is_scalar_logical_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_comparison_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_arithmetic_projection_expression(expression)) {
        return true;
    }
    return is_scalar_value_projection_expression(expression);
}

static bool is_case_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_case_expression_kind(expression->kind)) {
        return false;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        if (child_count < 1U || child_count > 2U) {
            return false;
        }
        when_list = child_at(expression, 0U);
        else_clause = child_at(expression, 1U);
    } else {
        if (child_count < 2U || child_count > 3U || child_at(expression, 0U) == NULL) {
            return false;
        }
        when_list = child_at(expression, 1U);
        else_clause = child_at(expression, 2U);
    }
    if (!is_case_when_list_projection_expression(when_list)) {
        return false;
    }
    if (else_clause != NULL && !is_case_else_clause_projection_expression(else_clause)) {
        return false;
    }
    return true;
}

static bool is_case_when_list_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *when_clause = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_WHEN_LIST ||
        mylite_sql_ast_node_child_count(expression) == 0U) {
        return false;
    }
    when_clause = child_at(expression, 0U);
    while (when_clause != NULL) {
        if (!is_case_when_clause_projection_expression(when_clause)) {
            return false;
        }
        when_clause = when_clause->next_sibling;
    }
    return true;
}

static bool is_case_when_clause_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    if (child_at(expression, 0U) == NULL) {
        return false;
    }
    return child_at(expression, 1U) != NULL;
}

static bool is_case_else_clause_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CASE_ELSE_CLAUSE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_case_expression_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return true;
    default:
        return false;
    }
}

static void set_case_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "SELECT CASE supports only no-source and FROM DUAL signed 64-bit integer, boolean, "
        "NULL, scalar arithmetic, scalar comparison, scalar logical, scalar IS, "
        "and existing scalar IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() expressions"
    );
}

static void set_if_unsupported_error(struct mylite_db *database, const char *function_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int message_length = snprintf(
        message,
        sizeof(message),
        "SELECT %s() supports only signed 64-bit integer, boolean, NULL, and nested "
        "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL() arguments",
        function_name
    );

    if (message_length < 0 || (size_t)message_length >= sizeof(message)) {
        set_runtime_error(database, "failed to format scalar control-flow diagnostic");
        return;
    }
    set_unsupported_error(database, message);
}

static const char *if_function_name(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_IFNULL_FUNCTION) {
        return "IFNULL";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_COALESCE_FUNCTION) {
        return "COALESCE";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_NULLIF_FUNCTION) {
        return "NULLIF";
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION) {
        return "ISNULL";
    }
    return "IF";
}

static bool is_if_non_function_value_expression(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal = child_at(expression, 0U);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
            return false;
        }
        if (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return false;
        }
        return is_if_integer_literal_in_range(literal);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return is_if_integer_literal_in_range(expression);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return true;
    }
    return false;
}

static bool is_if_integer_literal_in_range(const struct mylite_sql_ast_node *literal) {
    uint64_t magnitude = 0U;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        return false;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        return false;
    }
    return true;
}

static int if_validation_stack_push(
    struct mylite_db *database,
    struct if_validation_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_MISUSE;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = (const struct mylite_sql_ast_node **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = expression;
    ++stack->count;
    return MYLITE_OK;
}

static void if_validation_stack_deinit(struct if_validation_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct if_validation_stack){0};
}
