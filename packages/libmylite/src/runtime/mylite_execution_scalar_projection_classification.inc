static bool select_statement_is_scalar_projection(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        return false;
    }
    if (child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_select_item_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static bool is_scalar_projection_select_item_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING) {
        return true;
    }
    return is_scalar_projection_expression(expression);
}

static bool select_statement_is_row_function_scalar_projection(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool found_row_function = false;

    if (!select_statement_is_scalar_projection(statement)) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        if (is_string_metadata_projection_expression(expression) ||
            is_hex_projection_expression(expression) ||
            is_base64_projection_expression(expression) ||
            is_unhex_projection_expression(expression) ||
            is_uuid_projection_expression(expression) ||
            is_char_projection_expression(expression) ||
            is_json_introspection_projection_expression(expression) ||
            is_json_extract_projection_expression(expression) ||
            is_json_value_projection_expression(expression) ||
            is_json_contains_projection_expression(expression) ||
            is_json_unquote_projection_expression(expression) ||
            is_json_set_projection_expression(expression) ||
            is_json_construction_projection_expression(expression)) {
            found_row_function = true;
        } else if (row_scalar_expression_contains_row_function(expression)) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return found_row_function;
}

static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);

    if (from_clause == NULL) {
        return true;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return true;
    }
    return false;
}

static int reject_bare_native_function_identifier_lookup_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (!select_statement_has_no_source_or_dual(statement) || child_at(statement, 2U) != NULL) {
        return MYLITE_OK;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != 1U) {
        return MYLITE_OK;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return MYLITE_OK;
    }
    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(expression, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(column_name, "pi") &&
        !text_equals_ascii_case_insensitive(column_name, "rand") &&
        !text_equals_ascii_case_insensitive(column_name, "sqrt") &&
        !text_equals_ascii_case_insensitive(column_name, "degrees") &&
        !text_equals_ascii_case_insensitive(column_name, "radians") &&
        !text_equals_ascii_case_insensitive(column_name, "acos") &&
        !text_equals_ascii_case_insensitive(column_name, "asin") &&
        !text_equals_ascii_case_insensitive(column_name, "sin") &&
        !text_equals_ascii_case_insensitive(column_name, "cos") &&
        !text_equals_ascii_case_insensitive(column_name, "tan") &&
        !text_equals_ascii_case_insensitive(column_name, "cot") &&
        !text_equals_ascii_case_insensitive(column_name, "atan") &&
        !text_equals_ascii_case_insensitive(column_name, "atan2") &&
        !text_equals_ascii_case_insensitive(column_name, "exp") &&
        !text_equals_ascii_case_insensitive(column_name, "ln") &&
        !text_equals_ascii_case_insensitive(column_name, "log") &&
        !text_equals_ascii_case_insensitive(column_name, "log10") &&
        !text_equals_ascii_case_insensitive(column_name, "log2") &&
        !text_equals_ascii_case_insensitive(column_name, "pow") &&
        !text_equals_ascii_case_insensitive(column_name, "power") &&
        !text_equals_ascii_case_insensitive(column_name, "uuid")) {
        return MYLITE_OK;
    }

    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static bool select_statement_is_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool saw_scalar_projection_expression = false;

    if (!select_statement_has_no_source_or_dual(statement)) {
        return false;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_attempt_expression(child_at(select_item, 0U))) {
            return false;
        }
        saw_scalar_projection_expression = true;
        select_item = select_item->next_sibling;
    }

    return saw_scalar_projection_expression;
}
