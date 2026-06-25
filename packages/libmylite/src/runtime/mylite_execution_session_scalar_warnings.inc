
static bool do_statement_has_only_scalar_projection_expressions(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = NULL;

    if (expression_list == NULL || expression_list->kind != MYLITE_SQL_AST_DO_EXPRESSION_LIST) {
        return false;
    }
    expression = child_at(expression_list, 0U);
    if (expression == NULL) {
        return false;
    }
    while (expression != NULL) {
        if (is_scalar_subquery_projection_expression(expression) ||
            !is_scalar_projection_expression(expression)) {
            return false;
        }
        expression = expression->next_sibling;
    }

    return true;
}

static int append_session_scalar_do_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && expression != NULL) {
        rc = append_session_scalar_expression_warnings(database, expression);
        expression = expression->next_sibling;
    }

    return rc;
}

static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(select_item, 0U));

        rc = append_session_scalar_expression_warnings(database, expression);
        select_item = select_item->next_sibling;
    }

    return rc;
}

static int append_session_scalar_row_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_MISUSE;
    }

    rc = append_session_scalar_select_warnings(database, child_at(statement, 0U));
    optional_clause = select_statement_optional_clause_start(statement);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE ||
            optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            rc = append_session_scalar_expression_warnings(database, optional_clause);
        }
        optional_clause = optional_clause->next_sibling;
    }
    return rc;
}

static int append_session_scalar_expression_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (current->kind == MYLITE_SQL_AST_FOUND_ROWS_FUNCTION) {
            rc = append_found_rows_deprecation_warning(database);
        } else if (current->kind == MYLITE_SQL_AST_JSON_MERGE_FUNCTION) {
            rc = append_json_merge_deprecation_warning(database);
        } else if (current->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
            rc = append_convert_using_charset_warning(database, current);
        } else if (current->kind == MYLITE_SQL_AST_CHAR_FUNCTION) {
            rc = append_char_using_charset_warning(database, current);
        } else if (current->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
            enum mylite_execution_system_variable_kind variable =
                MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;

            rc = resolve_session_system_variable(database, current, &variable);
            if (rc == MYLITE_OK &&
                mylite_execution_system_variable_warns_on_scalar_read(variable)) {
                rc = append_system_variable_read_warning(database, variable);
            }
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; rc == MYLITE_OK && child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int append_select_modifier_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    unsigned int options = mylite_sql_ast_node_select_options(statement);

    if ((options & MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE) == 0U) {
        return MYLITE_OK;
    }

    return append_sql_no_cache_deprecation_warning(database);
}

static int append_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_found_rows_deprecated,
        "HY000",
        "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using "
        "COUNT(*) instead."
    );
}

static int append_json_merge_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_json_merge_deprecated,
        "HY000",
        "'JSON_MERGE' is deprecated and will be removed in a future release. Please use "
        "JSON_MERGE_PRESERVE/JSON_MERGE_PATCH instead"
    );
}

static int append_sql_calc_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_calc_found_rows_deprecated,
        "HY000",
        "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider "
        "using two separate queries instead."
    );
}

static int append_sql_no_cache_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_no_cache_deprecated,
        "HY000",
        "'SQL_NO_CACHE' is deprecated and will be removed in a future release."
    );
}

static int append_convert_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_convert_charset_info info = {0};
    int rc = scalar_convert_charset_info_for_expression(database, expression, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_scalar_charset_info_warning(database, &info);
}

static int append_char_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_convert_charset_info info = {0};
    int rc = mylite_execution_scalar_char_charset_info_for_expression(database, expression, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_scalar_charset_info_warning(database, &info);
}

static int append_scalar_charset_info_warning(
    struct mylite_db *database,
    const struct scalar_convert_charset_info *info
) {
    if (info == NULL) {
        return MYLITE_MISUSE;
    }
    switch (info->warning) {
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8_ALIAS:
        return append_utf8_alias_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8MB3_DEPRECATED:
        return append_utf8mb3_deprecation_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_NONE:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int append_utf8_alias_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8_alias,
        "HY000",
        "'utf8' is currently an alias for the character set UTF8MB3, but will be an alias "
        "for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be "
        "unambiguous."
    );
}

static int append_utf8mb3_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8mb3_deprecated,
        "HY000",
        "'utf8mb3' is deprecated and will be removed in a future release. Please use "
        "utf8mb4 instead"
    );
}

static int accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    return accumulate_staged_warning_count(database, cell_warning_count, total_warning_count);
}

static int accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    if (total_warning_count == NULL) {
        return MYLITE_MISUSE;
    }
    if (*total_warning_count > SIZE_MAX - cell_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *total_warning_count += cell_warning_count;
    return MYLITE_OK;
}

static int append_session_scalar_cell_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cell
) {
    int rc = MYLITE_OK;

    if (cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->has_staged_truncated_integer_warning) {
        rc = append_truncated_incorrect_integer_warning(
            database,
            cell->staged_truncated_integer_text
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_truncated_decimal_warning) {
        rc = append_truncated_incorrect_decimal_warning(
            database,
            cell->staged_truncated_decimal_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_signed_complement_warnings(
            database,
            cell->staged_signed_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_unsigned_complement_warnings(
            database,
            cell->staged_unsigned_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_division_by_zero_warnings(database, cell->staged_division_by_zero_warning_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_invalid_logarithm_warnings(
            database,
            cell->staged_invalid_logarithm_warning_count
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_unhex_incorrect_string_warning) {
        rc = append_unhex_incorrect_string_warning(
            database,
            cell->staged_unhex_incorrect_string_text
        );
    }
    return rc;
}

static int append_session_scalar_cells_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cells,
    size_t cell_count
) {
    int rc = MYLITE_OK;

    if (cells == NULL && cell_count != 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < cell_count; ++index) {
        rc = append_session_scalar_cell_warnings(database, &cells[index]);
    }
    return rc;
}

static int append_division_by_zero_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_division_by_zero,
            "22012",
            "Division by 0"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_invalid_logarithm_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_invalid_argument_for_logarithm,
            "HY000",
            "Invalid argument for logarithm"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const char *value_text
) {
    static const char prefix[] = "Truncated incorrect INTEGER value: '";
    static const char suffix[] = "'";
    char *message = NULL;
    size_t value_length = 0U;
    size_t message_length = 0U;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    value_length = strlen(value_text);
    if (value_length > SIZE_MAX - sizeof(prefix) - sizeof(suffix)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    message_length = (sizeof(prefix) - 1U) + value_length + (sizeof(suffix) - 1U);
    message = (char *)malloc(message_length + 1U);
    if (message == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(message, prefix, sizeof(prefix) - 1U);
    memcpy(message + sizeof(prefix) - 1U, value_text, value_length);
    memcpy(message + sizeof(prefix) - 1U + value_length, suffix, sizeof(suffix));

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_integer,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    free(message);
    return rc;
}

static int append_truncated_incorrect_decimal_warning(
    struct mylite_db *database,
    const char *value_text
) {
    char
        message[sizeof("Truncated incorrect DECIMAL value: ''") + literal_projection_text_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    written =
        snprintf(message, sizeof(message), "Truncated incorrect DECIMAL value: '%s'", value_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format CONV() warning");
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_decimal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_signed_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to signed converted positive out-of-range integer to its negative complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unsigned_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to unsigned converted negative integer to its positive complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unhex_incorrect_string_warning(
    struct mylite_db *database,
    const char *value_text
) {
    return mylite_string_unhex_append_incorrect_warning(
        database,
        value_text,
        value_text == NULL ? 0U : strlen(value_text)
    );
}
