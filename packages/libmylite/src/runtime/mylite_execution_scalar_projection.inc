static bool is_scalar_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return is_collate_projection_expression(expression);
    }
    return is_scalar_projection_expression_without_collate(expression);
}

static bool is_collate_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *value = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }

    value = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (value == NULL) {
        return false;
    }
    if (value->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(value);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
                literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                literal_kind == MYLITE_SQL_AST_LITERAL_BIT) != 0;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(value, 0U));

        return ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
                 operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
                literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    return (is_cast_binary_projection_expression(value) ||
            is_convert_binary_type_projection_expression(value) ||
            is_convert_using_binary_projection_expression(value) ||
            is_convert_using_charset_projection_expression(value)) != 0;
}

static bool is_scalar_projection_expression_without_collate(
    const struct mylite_sql_ast_node *expression
) {
    if (is_scalar_subquery_projection_expression(expression)) {
        return true;
    }
    if (is_session_scalar_expression(expression)) {
        return true;
    }
    if (is_user_variable_assignment_projection_expression(expression)) {
        return true;
    }
    if (is_last_insert_id_set_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_function_projection_expression(expression)) {
        return true;
    }
    if (mylite_execution_scalar_is_date_format_numeric_comparison_expression(expression)) {
        return true;
    }
    if (is_scalar_logical_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_comparison_projection_expression(expression)) {
        return true;
    }
    if (is_row_constructor_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_concat_projection_expression(expression)) {
        return true;
    }
    if (mylite_execution_is_scalar_division_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        return true;
    }
    if (is_scalar_arithmetic_projection_expression(expression)) {
        return true;
    }
    if (is_case_projection_expression(expression)) {
        return true;
    }
    return is_scalar_value_projection_expression(expression);
}

static bool is_user_variable_assignment_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL ||
        expression->kind != MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    if (child_at(expression, 0U) == NULL ||
        child_at(expression, 0U)->kind != MYLITE_SQL_AST_USER_VARIABLE) {
        return false;
    }
    return is_scalar_projection_expression(child_at(expression, 1U));
}

static bool is_last_insert_id_set_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_row_constructor_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        const struct mylite_sql_ast_node *left =
            unwrap_parenthesized_expression(child_at(expression, 0U));
        const struct mylite_sql_ast_node *right =
            unwrap_parenthesized_expression(child_at(expression, 1U));
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (!is_scalar_comparison_operator(operator_kind)) {
            return false;
        }
        return (left != NULL && left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) ||
               (right != NULL && right->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        return is_row_constructor_projection_expression(child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return is_row_constructor_projection_expression(child_at(expression, 0U));
    }
    return false;
}

static bool is_scalar_function_projection_expression(const struct mylite_sql_ast_node *expression) {
    return (is_numeric_scalar_function_projection_expression(expression) ||
            is_string_scalar_function_projection_expression(expression) ||
            is_named_lock_and_info_projection_expression(expression) ||
            is_scalar_conversion_projection_expression(expression) ||
            is_temporal_scalar_function_projection_expression(expression) ||
            is_json_scalar_function_projection_expression(expression)) != 0;
}

static bool is_named_lock_and_info_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sys_function_kind sys_function_kind = MYLITE_SYS_FUNCTION_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (generic_sys_function_kind(expression, &sys_function_kind)) {
        return true;
    }
    return (generic_function_name_equals(expression, "GET_LOCK") ||
            generic_function_name_equals(expression, "IS_FREE_LOCK") ||
            generic_function_name_equals(expression, "IS_USED_LOCK") ||
            generic_function_name_equals(expression, "RELEASE_LOCK") ||
            generic_function_name_equals(expression, "RELEASE_ALL_LOCKS") ||
            generic_function_name_equals(expression, "ICU_VERSION") ||
            generic_function_name_equals(expression, "BENCHMARK") ||
            generic_function_name_equals(expression, "SLEEP") ||
            generic_function_name_equals(expression, "NAME_CONST") ||
            generic_function_name_equals(expression, "LOAD_FILE") ||
            generic_function_name_equals(expression, "EXTRACTVALUE") ||
            generic_function_name_equals(expression, "UPDATEXML") ||
            generic_replication_function_name_is_supported(expression)) != 0;
}

static bool is_numeric_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (is_abs_projection_expression(expression)) {
        return true;
    }
    if (is_sign_projection_expression(expression)) {
        return true;
    }
    if (is_rounding_projection_expression(expression)) {
        return true;
    }
    if (is_sqrt_projection_expression(expression)) {
        return true;
    }
    if (is_angle_conversion_projection_expression(expression)) {
        return true;
    }
    if (is_inverse_trig_projection_expression(expression)) {
        return true;
    }
    if (is_direct_trig_projection_expression(expression)) {
        return true;
    }
    if (is_atan_projection_expression(expression)) {
        return true;
    }
    if (is_exp_log_power_projection_expression(expression)) {
        return true;
    }
    if (is_base_conversion_projection_expression(expression)) {
        return true;
    }
    if (is_bit_count_projection_expression(expression)) {
        return true;
    }
    if (is_crc32_projection_expression(expression)) {
        return true;
    }
    if (is_interval_projection_expression(expression)) {
        return true;
    }
    return false;
}

static bool is_string_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (is_binary_string_projection_expression(expression)) {
        return true;
    }
    if (is_digest_projection_expression(expression)) {
        return true;
    }
    if (is_format_truncate_projection_expression(expression)) {
        return true;
    }
    if (is_concat_projection_expression(expression)) {
        return true;
    }
    if (is_concat_ws_projection_expression(expression)) {
        return true;
    }
    if (is_string_metadata_projection_expression(expression)) {
        return true;
    }
    if (is_string_bitmask_projection_expression(expression)) {
        return true;
    }
    if (is_elt_projection_expression(expression)) {
        return true;
    }
    if (is_field_projection_expression(expression)) {
        return true;
    }
    if (is_greatest_least_projection_expression(expression)) {
        return true;
    }
    if (is_regexp_like_projection_expression(expression)) {
        return true;
    }
    if (is_regexp_string_projection_expression(expression)) {
        return true;
    }
    return false;
}

static bool is_temporal_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMP_FUNCTION) {
        child_count = mylite_sql_ast_node_child_count(expression);
        return (child_count == 1U || child_count == 2U) != 0;
    }
    if (is_date_interval_second_projection_expression(expression)) {
        return true;
    }
    if (is_time_arithmetic_projection_expression(expression)) {
        return true;
    }
    if (is_date_format_projection_expression(expression)) {
        return true;
    }
    return is_temporal_extract_projection_expression(expression);
}

static bool is_json_scalar_function_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    return (is_json_valid_projection_expression(expression) ||
            is_json_contains_projection_expression(expression) ||
            is_json_extract_projection_expression(expression) ||
            is_json_value_projection_expression(expression) ||
            is_json_introspection_projection_expression(expression) ||
            is_json_unquote_projection_expression(expression) ||
            is_json_quote_projection_expression(expression) ||
            is_json_set_projection_expression(expression) ||
            is_json_construction_projection_expression(expression) ||
            mylite_execution_scalar_is_date_format_numeric_comparison_expression(expression)) != 0;
}

static bool is_scalar_conversion_projection_expression(const struct mylite_sql_ast_node *expression
) {
    return (is_cast_binary_projection_expression(expression) ||
            is_cast_convert_basic_projection_expression(expression) ||
            is_convert_binary_type_projection_expression(expression) ||
            is_convert_using_binary_projection_expression(expression) ||
            is_convert_using_charset_projection_expression(expression)) != 0;
}

static bool is_scalar_subquery_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    return expression->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY;
}

static bool is_abs_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ABS_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_sign_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SIGN_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_rounding_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_CEIL_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_CEILING_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_FLOOR_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_ROUND_FUNCTION) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_ROUND_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) == 2U) {
        if (child_at(expression, 0U) == NULL) {
            return false;
        }
        return child_at(expression, 1U) != NULL;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_sqrt_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SQRT_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_angle_conversion_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_DEGREES_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_RADIANS_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_inverse_trig_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_ACOS_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_ASIN_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_direct_trig_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_SIN_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_COS_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_TAN_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_COT_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_atan_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_ATAN_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_ATAN2_FUNCTION) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        return false;
    }
    if (child_at(expression, 0U) == NULL) {
        return false;
    }
    if (child_count == 2U) {
        return child_at(expression, 1U) != NULL;
    }
    return true;
}

static bool is_exp_log_power_projection_expression(const struct mylite_sql_ast_node *expression) {
    bool has_first_child = false;
    bool has_required_children = false;
    bool has_second_child = false;
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    switch (expression->kind) {
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
        has_first_child = child_at(expression, 0U) != NULL;
        if (child_count != 1U) {
            return false;
        }
        return has_first_child;
    case MYLITE_SQL_AST_LOG_FUNCTION:
        if (child_count != 1U && child_count != 2U) {
            return false;
        }
        if (child_at(expression, 0U) == NULL) {
            return false;
        }
        has_second_child = child_at(expression, 1U) != NULL;
        if (child_count == 1U) {
            return true;
        }
        return has_second_child;
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
        has_first_child = child_at(expression, 0U) != NULL;
        has_second_child = child_at(expression, 1U) != NULL;
        if (child_count != 2U) {
            return false;
        }
        has_required_children = has_first_child;
        if (has_required_children) {
            has_required_children = has_second_child;
        }
        return has_required_children;
    default:
        return false;
    }
}

static bool is_base_conversion_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_CONV_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 3U) {
            return false;
        }
        if (child_at(expression, 0U) == NULL || child_at(expression, 1U) == NULL ||
            child_at(expression, 2U) == NULL) {
            return false;
        }
        return true;
    }
    if (expression->kind != MYLITE_SQL_AST_BIN_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_OCT_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_bit_count_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BIT_COUNT_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_crc32_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CRC32_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_digest_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    switch (expression->kind) {
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
        return child_count == 1U && child_at(expression, 0U) != NULL;
    case MYLITE_SQL_AST_SHA2_FUNCTION:
        return child_count == 2U && child_at(expression, 0U) != NULL &&
               child_at(expression, 1U) != NULL;
    default:
        break;
    }
    return false;
}

static bool is_binary_string_projection_expression(const struct mylite_sql_ast_node *expression) {
    return (is_hex_projection_expression(expression) ||
            is_base64_projection_expression(expression) ||
            is_compression_random_projection_expression(expression) ||
            is_unhex_projection_expression(expression) ||
            is_weight_string_projection_expression(expression) ||
            is_uuid_projection_expression(expression) ||
            is_char_projection_expression(expression)) != 0;
}

static bool is_hex_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_HEX_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_unhex_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNHEX_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_base64_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);

    while (expression != NULL && (expression->kind == MYLITE_SQL_AST_TO_BASE64_FUNCTION ||
                                  expression->kind == MYLITE_SQL_AST_FROM_BASE64_FUNCTION)) {
        if (mylite_sql_ast_node_child_count(expression) != 1U) {
            return false;
        }
        argument = unwrap_parenthesized_expression(child_at(expression, 0U));
        if (argument == NULL) {
            return false;
        }
        if (argument->kind == MYLITE_SQL_AST_LITERAL) {
            enum mylite_sql_ast_literal_kind literal_kind =
                mylite_sql_ast_node_literal_kind(argument);

            return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                    literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                    literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                    literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                    literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                    literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
        }
        if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
            enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
            const struct mylite_sql_ast_node *literal =
                unwrap_parenthesized_expression(child_at(argument, 0U));

            if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
                operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
                return false;
            }
            return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                    mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) !=
                   0;
        }

        switch (argument->kind) {
        case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
        case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
            expression = argument;
            break;
        case MYLITE_SQL_AST_DATABASE_FUNCTION:
        case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        case MYLITE_SQL_AST_USER_FUNCTION:
        case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
        case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        case MYLITE_SQL_AST_VERSION_FUNCTION:
        case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        case MYLITE_SQL_AST_UTC_DATE_VALUE:
        case MYLITE_SQL_AST_UTC_TIME_VALUE:
        case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
        case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
        case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
            return true;
        default:
            return false;
        }
    }
    return false;
}

static bool is_compression_random_projection_expression(const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        break;
    default:
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(argument);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(argument, 0U));

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
            return false;
        }
        switch (mylite_sql_ast_node_literal_kind(literal)) {
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
        case MYLITE_SQL_AST_LITERAL_FLOAT:
            return true;
        default:
            return false;
        }
    }

    switch (argument->kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return is_compression_random_projection_expression(argument);
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool scalar_projection_expression_contains_compression_random_function(
    const struct mylite_sql_ast_node *expression
) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
        return true;
    default:
        break;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    for (size_t child_index = 0U; child_index < child_count; ++child_index) {
        if (scalar_projection_expression_contains_compression_random_function(
                child_at(expression, child_index)
            )) {
            return true;
        }
    }
    return false;
}

static bool is_weight_string_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(argument);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(argument, 0U));

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }

    switch (argument->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return true;
    default:
        return false;
    }
}

static bool is_uuid_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }

    child_count = mylite_sql_ast_node_child_count(expression);
    switch (expression->kind) {
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
        return child_count == 0U;
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
        if (child_count != 1U) {
            return false;
        }
        argument = child_at(expression, 0U);
        return is_uuid_value_projection_argument_supported(argument);
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        if (child_count != 1U && child_count != 2U) {
            return false;
        }
        argument = child_at(expression, 0U);
        if (!is_uuid_value_projection_argument_supported(argument)) {
            return false;
        }
        if (child_count == 1U) {
            return true;
        }
        return child_at(expression, 1U) != NULL;
    default:
        break;
    }

    return false;
}

static bool is_uuid_value_projection_argument_supported(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_HEX ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static bool is_char_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CHAR_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST ||
        mylite_sql_ast_node_child_count(arguments) == 0U) {
        return false;
    }

    for (const struct mylite_sql_ast_node *item = child_at(arguments, 0U); item != NULL;
         item = item->next_sibling) {
        if (!is_char_projection_argument_supported(item)) {
            return false;
        }
    }
    return true;
}

static bool is_char_projection_argument_supported(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return is_char_projection_literal_supported(expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return is_char_projection_unary_supported(expression);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return true;
    default:
        return false;
    }
}

static bool is_char_projection_literal_supported(const struct mylite_sql_ast_node *literal) {
    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool is_char_projection_unary_supported(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
    const struct mylite_sql_ast_node *literal =
        unwrap_parenthesized_expression(child_at(expression, 0U));

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }
    return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
}

static bool is_format_truncate_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED) {
        if (mylite_sql_ast_node_child_count(expression) != 3U) {
            return false;
        }
        if (child_at(expression, 0U) == NULL) {
            return false;
        }
        if (child_at(expression, 1U) == NULL) {
            return false;
        }
        return child_at(expression, 2U) != NULL;
    }
    if (expression->kind != MYLITE_SQL_AST_FORMAT_FUNCTION &&
        expression->kind != MYLITE_SQL_AST_TRUNCATE_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    if (child_at(expression, 0U) == NULL) {
        return false;
    }
    return child_at(expression, 1U) != NULL;
}

static bool is_string_length_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || !mylite_execution_is_string_length_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return mylite_execution_string_length_scalar_argument_is_admitted(child_at(expression, 0U)) ||
           is_scalar_projection_attempt_expression(child_at(expression, 0U));
}

static bool is_string_codepoint_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    if (!mylite_execution_is_string_codepoint_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return mylite_execution_string_codepoint_scalar_argument_is_admitted(child_at(expression, 0U));
}

static bool is_string_case_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || !mylite_execution_is_string_case_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return mylite_execution_string_case_scalar_argument_is_admitted(child_at(expression, 0U)) ||
           is_scalar_projection_attempt_expression(child_at(expression, 0U));
}

static bool is_string_trim_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || !mylite_execution_is_string_trim_function_kind(expression->kind)) {
        return false;
    }
    argument_count = mylite_sql_ast_node_child_count(expression);
    if (argument_count != 1U && argument_count != 2U) {
        return false;
    }
    if (!mylite_execution_string_trim_scalar_argument_is_admitted(child_at(expression, 0U))) {
        return false;
    }
    if (argument_count == 1U) {
        return true;
    }
    return mylite_execution_string_trim_scalar_argument_is_admitted(child_at(expression, 1U));
}

static bool is_string_slice_projection_expression(const struct mylite_sql_ast_node *expression) {
    enum planned_string_slice_function_kind function_kind = PLANNED_STRING_SLICE_FUNCTION_NONE;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    function_kind = mylite_execution_string_slice_function_kind(expression->kind);
    argument_count = mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_NONE) {
        return false;
    }
    if (function_kind != PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return (argument_count == 2U &&
                mylite_execution_string_slice_scalar_text_argument_is_admitted(
                    child_at(expression, 0U)
                ) &&
                mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 1U))
               ) != 0;
    }
    if (argument_count != 2U && argument_count != 3U) {
        return false;
    }
    if (!mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 0U)) ||
        !mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 1U))) {
        return false;
    }
    return (argument_count == 2U ||
            mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 2U))) !=
           0;
}

static bool is_string_search_projection_expression(const struct mylite_sql_ast_node *expression) {
    enum planned_string_search_function_kind function_kind = PLANNED_STRING_SEARCH_FUNCTION_NONE;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }

    function_kind = mylite_execution_string_search_function_kind(expression->kind);
    argument_count = mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE) {
        return false;
    }
    if (argument_count != 2U && argument_count != 3U) {
        return false;
    }
    if (function_kind != PLANNED_STRING_SEARCH_FUNCTION_LOCATE && argument_count != 2U) {
        return false;
    }
    if (!mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 0U)) ||
        !mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 1U))) {
        return false;
    }
    return (argument_count == 2U ||
            mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 2U))) !=
           0;
}

static bool is_find_in_set_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIND_IN_SET_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 2U &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 0U)
            ) &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 1U))
           ) != 0;
}

static bool is_strcmp_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STRCMP_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 2U &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 0U)
            ) &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 1U))
           ) != 0;
}

static bool is_substring_index_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 3U &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 0U)
            ) &&
            mylite_execution_string_slice_scalar_text_argument_is_admitted(child_at(expression, 1U)
            ) &&
            mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 2U))) !=
           0;
}

static bool is_string_padding_projection_expression(const struct mylite_sql_ast_node *expression) {
    enum planned_string_padding_function_kind function_kind = PLANNED_STRING_PADDING_FUNCTION_NONE;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }

    function_kind = mylite_execution_string_padding_function_kind(expression->kind);
    argument_count = mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ||
        function_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD) {
        return (argument_count == 3U &&
                mylite_execution_string_slice_scalar_text_argument_is_admitted(
                    child_at(expression, 0U)
                ) &&
                mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 1U)
                ) &&
                mylite_execution_string_slice_scalar_text_argument_is_admitted(
                    child_at(expression, 2U)
                )) != 0;
    }
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_REPEAT) {
        return (argument_count == 2U &&
                mylite_execution_string_slice_scalar_text_argument_is_admitted(
                    child_at(expression, 0U)
                ) &&
                mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 1U))
               ) != 0;
    }
    if (function_kind == PLANNED_STRING_PADDING_FUNCTION_SPACE) {
        return (argument_count == 1U &&
                mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 0U))
               ) != 0;
    }
    return false;
}

static bool is_concat_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        return false;
    }

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        if (!concat_operator_scalar_argument_is_admitted(argument) &&
            !is_scalar_projection_attempt_expression(argument)) {
            return false;
        }
        argument = argument->next_sibling;
    }
    return argument == NULL;
}

static bool is_concat_ws_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        return false;
    }

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        if (!mylite_execution_concat_ws_scalar_argument_is_admitted(argument)) {
            return false;
        }
        argument = argument->next_sibling;
    }
    return argument == NULL;
}

static bool is_string_replace_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REPLACE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        return false;
    }
    return (mylite_execution_string_replace_scalar_argument_is_admitted(child_at(expression, 0U)) &&
            mylite_execution_string_replace_scalar_argument_is_admitted(child_at(expression, 1U)) &&
            mylite_execution_string_replace_scalar_argument_is_admitted(child_at(expression, 2U))
           ) != 0;
}

static bool is_string_insert_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INSERT_STRING_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 4U) {
        return false;
    }
    return (mylite_execution_string_replace_scalar_argument_is_admitted(child_at(expression, 0U)) &&
            mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 1U)) &&
            mylite_execution_string_slice_length_argument_is_admitted(child_at(expression, 2U)) &&
            mylite_execution_string_replace_scalar_argument_is_admitted(child_at(expression, 3U))
           ) != 0;
}

static bool is_string_reverse_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REVERSE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return true;
    }
    return mylite_execution_string_reverse_scalar_argument_is_admitted(argument);
}

static bool is_soundex_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    if (expression->kind != MYLITE_SQL_AST_SOUNDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return true;
    }
    return mylite_execution_soundex_scalar_argument_is_admitted(argument);
}

static bool is_string_quote_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return true;
    }
    return mylite_execution_quote_scalar_argument_is_admitted(argument);
}

static bool is_charset_collation_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL ||
        !mylite_execution_is_charset_collation_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_string_metadata_projection_expression(const struct mylite_sql_ast_node *expression) {
    if (is_string_length_projection_expression(expression)) {
        return true;
    }
    if (is_string_codepoint_projection_expression(expression)) {
        return true;
    }
    if (is_string_case_projection_expression(expression)) {
        return true;
    }
    if (is_string_trim_projection_expression(expression)) {
        return true;
    }
    if (is_string_slice_projection_expression(expression)) {
        return true;
    }
    if (is_string_search_projection_expression(expression)) {
        return true;
    }
    if (is_find_in_set_projection_expression(expression)) {
        return true;
    }
    if (is_strcmp_projection_expression(expression)) {
        return true;
    }
    if (is_substring_index_projection_expression(expression)) {
        return true;
    }
    if (is_string_padding_projection_expression(expression)) {
        return true;
    }
    if (is_string_replace_projection_expression(expression)) {
        return true;
    }
    if (is_string_insert_projection_expression(expression)) {
        return true;
    }
    if (is_string_reverse_projection_expression(expression)) {
        return true;
    }
    if (is_soundex_projection_expression(expression)) {
        return true;
    }
    if (is_string_quote_projection_expression(expression)) {
        return true;
    }
    return is_charset_collation_projection_expression(expression);
}

static bool is_cast_binary_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_BINARY_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_child_count(expression) == 1U;
}

static bool is_cast_convert_basic_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
        return mylite_sql_ast_node_child_count(expression) == 1U;
    default:
        return false;
    }
}

static bool is_convert_binary_type_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_child_count(expression) == 1U;
}

static bool is_convert_using_binary_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_child_count(expression) == 1U;
}

static bool is_convert_using_charset_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_child_count(expression) == 2U;
}

static bool is_date_interval_second_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL ||
        !mylite_execution_is_date_interval_second_function_kind(expression->kind)) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 3U) {
            return false;
        }
        return (child_at(expression, 0U) != NULL && child_at(expression, 1U) != NULL &&
                child_at(expression, 2U) != NULL) != 0;
    }
    if (mylite_sql_ast_node_child_count(expression) != 3U) {
        return false;
    }
    return (child_at(expression, 0U) != NULL && child_at(expression, 1U) != NULL &&
            child_at(expression, 2U) != NULL) != 0;
}

static bool is_time_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression != NULL && (expression->kind == MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR ||
                               expression->kind == MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR)) {
        return true;
    }
    if (expression == NULL ||
        !mylite_execution_is_time_arithmetic_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    return (child_at(expression, 0U) != NULL && child_at(expression, 1U) != NULL) != 0;
}

static bool is_date_format_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_GET_FORMAT_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_TIME_FORMAT_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_STR_TO_DATE_FUNCTION)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 2U) {
        return false;
    }
    return (child_at(expression, 0U) != NULL && child_at(expression, 1U) != NULL) != 0;
}

static bool is_temporal_extract_projection_expression(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            return false;
        }
        return (child_at(expression, 0U) != NULL && child_at(expression, 1U) != NULL) != 0;
    }
    if (!mylite_execution_is_temporal_extract_function_kind(expression->kind)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    return child_at(expression, 0U) != NULL;
}

static bool is_field_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIELD_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    return mylite_sql_ast_node_child_count(arguments) >= 1U;
}

static bool is_elt_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ELT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    return mylite_sql_ast_node_child_count(arguments) >= 1U;
}

static bool is_string_bitmask_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_EXPORT_SET_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_MAKE_SET_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (expression->kind == MYLITE_SQL_AST_EXPORT_SET_FUNCTION) {
        if (argument_count < string_bitmask_export_set_min_argument_count) {
            return false;
        }
        if (argument_count > string_bitmask_export_set_max_argument_count) {
            return false;
        }
        return true;
    }
    if (argument_count < string_bitmask_make_set_min_argument_count) {
        return false;
    }
    return true;
}

static bool is_greatest_least_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_GREATEST_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_LEAST_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    return mylite_sql_ast_node_child_count(arguments) >= 1U;
}

static bool is_interval_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *thresholds = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INTERVAL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U || child_at(expression, 0U) == NULL) {
        return false;
    }

    thresholds = child_at(expression, 1U);
    if (thresholds == NULL || thresholds->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    return mylite_sql_ast_node_child_count(thresholds) >= 1U;
}

static bool is_json_valid_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALID_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 1U && child_at(expression, 0U) != NULL
           ) != 0;
}

static bool is_regexp_like_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    return ((child_count == 2U || child_count == 3U) && child_at(expression, 0U) != NULL &&
            child_at(expression, 1U) != NULL &&
            (child_count == 2U || child_at(expression, 2U) != NULL)) != 0;
}

static bool is_regexp_string_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    child_count = mylite_execution_regexp_string_function_argument_count(expression);
    switch (expression->kind) {
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
        return child_count >= 2U;
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        return child_count >= 3U;
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static bool is_json_contains_projection_expression(const struct mylite_sql_ast_node *expression) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION) {
        return ((child_count == 2U || child_count == 3U) && child_at(expression, 0U) != NULL &&
                child_at(expression, 1U) != NULL &&
                (child_count == 2U || child_at(expression, 2U) != NULL)) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_OVERLAPS_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION) {
        return (child_count == 2U && child_at(expression, 0U) != NULL &&
                child_at(expression, 1U) != NULL) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION) {
        const struct mylite_sql_ast_node *arguments = NULL;

        if (child_count != 1U) {
            return false;
        }
        arguments = child_at(expression, 0U);
        return (arguments != NULL && arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) != 0;
    }
    return false;
}

static bool is_json_extract_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) == 1U) {
        const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);

        return (arguments != NULL && arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) != 0;
    }
    return (mylite_sql_ast_node_child_count(expression) == 2U && child_at(expression, 0U) != NULL &&
            child_at(expression, 1U) != NULL) != 0;
}

static bool is_json_value_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_VALUE_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 2U && child_at(expression, 0U) != NULL &&
            child_at(expression, 1U) != NULL) != 0;
}

static bool is_json_introspection_projection_expression(const struct mylite_sql_ast_node *expression
) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_JSON_LENGTH_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_DEPTH_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_STORAGE_SIZE_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_STORAGE_FREE_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_KEYS_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_TYPE_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_PRETTY_FUNCTION)) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_JSON_TYPE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_DEPTH_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_STORAGE_SIZE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_STORAGE_FREE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_JSON_PRETTY_FUNCTION) {
        return (child_count == 1U && child_at(expression, 0U) != NULL) != 0;
    }
    return ((child_count == 1U || child_count == 2U) && child_at(expression, 0U) != NULL &&
            (child_count == 1U || child_at(expression, 1U) != NULL)) != 0;
}

static bool is_json_unquote_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 1U && child_at(expression, 0U) != NULL
           ) != 0;
}

static bool is_json_quote_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_JSON_QUOTE_FUNCTION) {
        return false;
    }
    return (mylite_sql_ast_node_child_count(expression) == 1U && child_at(expression, 0U) != NULL
           ) != 0;
}

static bool is_json_construction_projection_expression(const struct mylite_sql_ast_node *expression
) {
    size_t child_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_JSON_ARRAY_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_OBJECT_FUNCTION)) {
        return false;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 0U) {
        return true;
    }
    return (child_count == 1U && child_at(expression, 0U) != NULL &&
            child_at(expression, 0U)->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) != 0;
}

static bool is_json_set_projection_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = NULL;
    size_t argument_count = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_JSON_SET_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_INSERT_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_REPLACE_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_JSON_REMOVE_FUNCTION)) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (expression->kind == MYLITE_SQL_AST_JSON_REMOVE_FUNCTION) {
        return argument_count >= 2U;
    }
    return (argument_count >= 3U && (argument_count % 2U) != 0U) != 0;
}

static bool is_scalar_value_projection_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (is_scalar_projection_literal_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_USER_VARIABLE) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return true;
    }
    return is_scalar_function_expression(expression);
}

static bool is_scalar_arithmetic_projection_expression(const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        result = scalar_arithmetic_projection_node_is_admitted(stack.items[--stack.count], &stack);
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return result;
}

bool mylite_execution_is_scalar_division_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        return false;
    }

    if (!is_scalar_arithmetic_projection_expression(child_at(expression, 0U))) {
        return false;
    }
    return is_scalar_arithmetic_projection_expression(child_at(expression, 1U));
}

static bool is_scalar_bitwise_projection_expression(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);
    bool result = true;

    if (unwrapped == NULL) {
        return false;
    }
    if (!((unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
           is_scalar_bitwise_unary_operator(mylite_sql_ast_node_operator(unwrapped))) ||
          (unwrapped->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
           is_scalar_bitwise_operator(mylite_sql_ast_node_operator(unwrapped))))) {
        return false;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        result = scalar_bitwise_projection_node_is_admitted(stack.items[--stack.count], &stack);
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return result;
}

static bool is_scalar_logical_projection_expression(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;
    bool saw_logical = false;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];

        result = scalar_logical_projection_node_is_admitted(current, &stack);
        if (result) {
            current = unwrap_parenthesized_expression(current);
            if (current != NULL &&
                ((current->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
                  is_scalar_logical_unary_operator(mylite_sql_ast_node_operator(current))) ||
                 (current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
                  (is_scalar_logical_operator(mylite_sql_ast_node_operator(current)) ||
                   is_scalar_is_operator(mylite_sql_ast_node_operator(current)))))) {
                saw_logical = true;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    if (!result) {
        return false;
    }
    return saw_logical;
}

static bool is_scalar_comparison_projection_expression(const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;
    bool saw_comparison = false;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];

        result = scalar_comparison_projection_node_is_admitted(current, &stack);
        if (result) {
            current = unwrap_parenthesized_expression(current);
            if (current != NULL && current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
                is_scalar_comparison_operator(mylite_sql_ast_node_operator(current))) {
                saw_comparison = true;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    if (!result) {
        return false;
    }
    return saw_comparison;
}

static bool is_scalar_concat_projection_expression(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;

    if (!row_scalar_expression_is_concat_operator(expression)) {
        return false;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        result = scalar_concat_projection_node_is_admitted(stack.items[--stack.count], &stack);
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return result;
}

static bool scalar_arithmetic_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (row_scalar_expression_is_concat_operator(expression)) {
        return is_scalar_concat_projection_expression(expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            return false;
        }
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return is_scalar_value_projection_expression(expression);
    }
    if (!is_scalar_arithmetic_operator(mylite_sql_ast_node_operator(expression))) {
        return false;
    }
    if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
        return false;
    }
    return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
}

static bool scalar_bitwise_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (!is_scalar_bitwise_unary_operator(operator_kind)) {
            return is_scalar_arithmetic_projection_expression(expression);
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        if (!is_scalar_bitwise_operator(mylite_sql_ast_node_operator(expression))) {
            return is_scalar_arithmetic_projection_expression(expression);
        }
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    return is_scalar_arithmetic_projection_expression(expression);
}

static bool scalar_logical_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        is_scalar_logical_unary_operator(mylite_sql_ast_node_operator(expression))) {
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        is_scalar_logical_operator(mylite_sql_ast_node_operator(expression))) {
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        is_scalar_comparison_operator(mylite_sql_ast_node_operator(expression))) {
        if (expression_is_unparenthesized_scalar_is(child_at(expression, 0U)) ||
            expression_is_unparenthesized_scalar_is(child_at(expression, 1U))) {
            return false;
        }
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
        is_scalar_is_operator(mylite_sql_ast_node_operator(expression))) {
        if (expression_is_unparenthesized_scalar_is(child_at(expression, 0U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    return is_scalar_arithmetic_projection_expression(expression);
}

static bool is_scalar_numeric_comparison_projection_expression(
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

        if (current == NULL) {
            result = false;
        } else if (current->kind == MYLITE_SQL_AST_LITERAL) {
            literal_kind = mylite_sql_ast_node_literal_kind(current);
            result = literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                     literal_kind == MYLITE_SQL_AST_LITERAL_FALSE;
        } else if (current->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
            enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(current);

            result = operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
                     operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
            if (result) {
                result = scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U));
            }
        } else if (current->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
            result = is_scalar_arithmetic_projection_expression(current);
        } else if (current->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
            result = is_scalar_arithmetic_operator(mylite_sql_ast_node_operator(current));
            if (result) {
                result = scalar_arithmetic_node_stack_push(&stack, child_at(current, 1U)) &&
                         scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U));
            }
        } else {
            result = is_scalar_value_projection_expression(current);
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return result;
}

static bool scalar_comparison_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return is_scalar_numeric_comparison_projection_expression(expression);
    }
    operator_kind = mylite_sql_ast_node_operator(expression);
    if (!is_scalar_comparison_operator(operator_kind)) {
        return is_scalar_numeric_comparison_projection_expression(expression);
    }
    if (expression_is_unparenthesized_scalar_is(child_at(expression, 0U)) ||
        expression_is_unparenthesized_scalar_is(child_at(expression, 1U))) {
        return false;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        return is_scalar_regexp_comparison_operand_expression(child_at(expression, 0U)) &&
               is_scalar_regexp_comparison_operand_expression(child_at(expression, 1U));
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE) {
        return is_scalar_sounds_like_operand_expression(child_at(expression, 0U)) &&
               is_scalar_sounds_like_operand_expression(child_at(expression, 1U));
    }
    if (is_scalar_logical_projection_expression(child_at(expression, 0U)) ||
        is_scalar_logical_projection_expression(child_at(expression, 1U))) {
        return is_scalar_logical_projection_expression(expression);
    }
    if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
        return false;
    }
    return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
}

static bool is_scalar_sounds_like_operand_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return true;
    }
    return mylite_execution_soundex_scalar_argument_is_admitted(expression);
}

static bool is_scalar_regexp_comparison_operand_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
                literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        return (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
                operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
               literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
               mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return mylite_sql_ast_node_child_count(expression) == 1U;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return mylite_sql_ast_node_child_count(expression) == 2U;
    default:
        return false;
    }
}

static bool scalar_concat_projection_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (row_scalar_expression_is_concat_operator(expression)) {
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    return concat_operator_scalar_argument_is_admitted(expression) ||
           is_scalar_projection_attempt_expression(expression);
}

static bool is_scalar_arithmetic_operator(enum mylite_sql_ast_operator operator_kind) {
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        return true;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return true;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_MULTIPLY) {
        return true;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO) {
        return true;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE) {
        return true;
    }
    return false;
}

static bool is_scalar_bitwise_operator(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_bitwise_unary_operator(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_BITWISE_NOT;
}

static bool is_scalar_logical_operator(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_logical_unary_operator(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT;
}

static bool is_scalar_comparison_operator(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_is_operator(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return true;
    default:
        return false;
    }
}

static bool expression_is_unparenthesized_scalar_is(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    return is_scalar_is_operator(mylite_sql_ast_node_operator(expression));
}

static bool is_scalar_projection_literal_expression(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

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
        return mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_NULL:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_function_expression(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return false;
    }
    if (is_compression_random_projection_expression(expression)) {
        return true;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_projection_attempt_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (is_session_scalar_expression(expression)) {
        return true;
    }
    if (is_scalar_arithmetic_attempt_expression(expression)) {
        return true;
    }
    return is_scalar_value_projection_attempt_expression(expression);
}

static bool is_scalar_value_projection_attempt_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        return true;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return is_scalar_value_projection_attempt_operand(expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return (is_scalar_arithmetic_attempt_expression(expression) ||
                mylite_execution_scalar_is_date_format_comparison_expression(expression)) != 0;
    default:
        return is_scalar_projection_known_function_attempt(expression->kind);
    }
}

static bool is_scalar_projection_known_function_attempt(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
    case MYLITE_SQL_AST_PI_FUNCTION:
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_SIGN_FUNCTION:
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_SQRT_FUNCTION:
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ASIN_FUNCTION:
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_COT_FUNCTION:
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LOG_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_CRC32_FUNCTION:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_SHORT_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_DATE_FUNCTION:
    case MYLITE_SQL_AST_TIME_FUNCTION:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEAR_FUNCTION:
    case MYLITE_SQL_AST_MONTH_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_HOUR_FUNCTION:
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
    case MYLITE_SQL_AST_SECOND_FUNCTION:
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
    case MYLITE_SQL_AST_ASCII_FUNCTION:
    case MYLITE_SQL_AST_ORD_FUNCTION:
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
    case MYLITE_SQL_AST_LEFT_FUNCTION:
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
    case MYLITE_SQL_AST_INSTR_FUNCTION:
    case MYLITE_SQL_AST_POSITION_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_JSON_OVERLAPS_FUNCTION:
    case MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_DEPTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_FUNCTION:
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
    case MYLITE_SQL_AST_JSON_PRETTY_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
    case MYLITE_SQL_AST_ELT_FUNCTION:
    case MYLITE_SQL_AST_FIELD_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_ADD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PERIOD_DIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONVERT_TZ_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_OVERLAPS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_DEPTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_STORAGE_SIZE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_STORAGE_FREE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_PRETTY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_APPEND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MD5_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA1_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SHA2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RANDOM_BYTES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
        return true;
    default:
        return false;
    }
}

static bool is_scalar_arithmetic_attempt_expression(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool result = true;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        result = scalar_arithmetic_attempt_node_is_admitted(stack.items[--stack.count], &stack);
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return result;
}

static bool scalar_arithmetic_attempt_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            return false;
        }
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return is_scalar_value_projection_attempt_operand(expression);
    }
    if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
        return false;
    }
    return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
}

static bool is_scalar_value_projection_attempt_operand(const struct mylite_sql_ast_node *expression
) {
    while (true) {
        enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

        expression = unwrap_parenthesized_expression(expression);
        if (expression == NULL) {
            return false;
        }
        if (expression->kind == MYLITE_SQL_AST_LITERAL) {
            return true;
        }
        if (is_session_scalar_expression(expression)) {
            return true;
        }
        if (expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
            return true;
        }
        if (is_scalar_function_expression(expression)) {
            return true;
        }
        if (is_scalar_numeric_function_attempt_operand(expression)) {
            return true;
        }
        if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
            return true;
        }
        if (expression->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
            return false;
        }
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        expression = child_at(expression, 0U);
    }
}

static bool is_scalar_numeric_function_attempt_operand(const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_SIGN_FUNCTION:
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_SQRT_FUNCTION:
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ASIN_FUNCTION:
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_COT_FUNCTION:
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LOG_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_CRC32_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
        return true;
    default:
        return false;
    }
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        expression = child_at(expression, 0U);
    }

    return expression;
}

static const struct mylite_sql_ast_node *unwrap_any_value_function_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        expression = unwrap_parenthesized_expression(child_at(expression, 0U));
    }

    return expression;
}
