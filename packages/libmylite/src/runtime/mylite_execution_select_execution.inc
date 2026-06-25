static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    mylite_result *result = NULL;
    char *sql = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = load_result_column_metadata_context(database, plan, &metadata_context);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_select_result_column(database, result, plan, &metadata_context, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = build_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(
            database,
            statement,
            result,
            plan->columns,
            plan->column_count
        );
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = set_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }
    if (plan->calc_found_rows) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    return finish_successful_result(database, result, out_result);
}

static bool expression_contains_generic_function_named(
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool found = false;

    if (function_name == NULL || !scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && !found) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (generic_function_name_equals(current, function_name)) {
            found = true;
            break;
        }
        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                scalar_arithmetic_node_stack_deinit(&stack);
                return false;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return found;
}

static bool expression_contains_sys_function(const struct mylite_sql_ast_node *expression) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool found = false;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && !found) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        enum mylite_sys_function_kind kind = MYLITE_SYS_FUNCTION_NONE;
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (generic_sys_function_kind(current, &kind)) {
            found = true;
            break;
        }
        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                scalar_arithmetic_node_stack_deinit(&stack);
                return false;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return found;
}

static bool select_statement_is_row_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool has_dual_source =
        (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) != 0;
    bool has_row_source =
        (from_clause != NULL && (from_clause->kind == MYLITE_SQL_AST_FROM_TABLE ||
                                 from_clause->kind == MYLITE_SQL_AST_FROM_JOIN ||
                                 from_clause->kind == MYLITE_SQL_AST_FROM_DERIVED)) != 0;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (select_list_is_wildcard(select_list)) {
        return false;
    }
    if (has_row_source && !select_statement_has_group_by_clause(statement) &&
        select_statement_optional_clauses_contain_row_scalar_function(statement)) {
        return true;
    }
    if ((from_clause == NULL || has_dual_source) &&
        select_statement_optional_clause_start(statement) != NULL) {
        return true;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (expression != NULL &&
            (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION ||
             expression->kind == MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION)) {
            return false;
        }
        if (select_item->kind == MYLITE_SQL_AST_SELECT_ITEM &&
            ((expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) ||
             row_scalar_expression_is_window_function(expression) ||
             (has_row_source &&
              row_scalar_expression_is_context_expression_attempt(expression, false)) ||
             (has_row_source && row_scalar_expression_contains_bitwise_attempt(expression)) ||
             (expression != NULL && expression->kind == MYLITE_SQL_AST_EXISTS_PREDICATE) ||
             (has_row_source && expression_is_supported_row_scalar_system_function(expression)) ||
             (has_row_source &&
              expression_contains_supported_row_scalar_system_function(expression)) ||
             (has_row_source && expression != NULL && expression->kind == MYLITE_SQL_AST_LITERAL) ||
             (has_row_source && expression != NULL &&
              expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION))) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static bool expression_is_supported_row_scalar_system_function(
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sys_function_kind kind = MYLITE_SYS_FUNCTION_NONE;

    if (generic_sys_function_kind(expression, &kind)) {
        return true;
    }
    return generic_function_name_equals(expression, "GET_LOCK") ||
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
           generic_function_name_equals(expression, "UPDATEXML");
}

static bool expression_contains_supported_row_scalar_system_function(
    const struct mylite_sql_ast_node *expression
) {
    if (expression_contains_sys_function(expression)) {
        return true;
    }
    return expression_contains_generic_function_named(expression, "GET_LOCK") ||
           expression_contains_generic_function_named(expression, "IS_FREE_LOCK") ||
           expression_contains_generic_function_named(expression, "IS_USED_LOCK") ||
           expression_contains_generic_function_named(expression, "RELEASE_LOCK") ||
           expression_contains_generic_function_named(expression, "RELEASE_ALL_LOCKS") ||
           expression_contains_generic_function_named(expression, "ICU_VERSION") ||
           expression_contains_generic_function_named(expression, "BENCHMARK") ||
           expression_contains_generic_function_named(expression, "SLEEP") ||
           expression_contains_generic_function_named(expression, "NAME_CONST") ||
           expression_contains_generic_function_named(expression, "LOAD_FILE") ||
           expression_contains_generic_function_named(expression, "EXTRACTVALUE") ||
           expression_contains_generic_function_named(expression, "UPDATEXML");
}

static bool select_statement_optional_clauses_contain_row_scalar_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *optional_clause = NULL;

    for (optional_clause = select_statement_optional_clause_start(statement);
         optional_clause != NULL;
         optional_clause = optional_clause->next_sibling) {
        if (select_optional_clause_contains_row_scalar_function(optional_clause)) {
            return true;
        }
    }
    return false;
}

static bool select_optional_clause_contains_row_scalar_function(
    const struct mylite_sql_ast_node *clause
) {
    const struct mylite_sql_ast_node *order_child = NULL;

    if (clause == NULL) {
        return false;
    }
    if (clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        return select_where_clause_contains_row_scalar_context_attempt(child_at(clause, 0U));
    }
    if (clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        return false;
    }

    order_child = child_at(clause, 0U);
    if (order_child != NULL && order_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *item = child_at(order_child, 0U);

        while (item != NULL) {
            if (item->kind == MYLITE_SQL_AST_ORDER_BY_ITEM &&
                select_order_key_is_supported_row_scalar_expression(child_at(item, 0U))) {
                return true;
            }
            item = item->next_sibling;
        }
        return false;
    }
    return select_order_key_is_supported_row_scalar_expression(order_child);
}

static bool select_where_clause_contains_row_scalar_context_attempt(
    const struct mylite_sql_ast_node *predicate
) {
    predicate = unwrap_parenthesized_predicate(predicate);
    if (predicate == NULL || row_scalar_expression_contains_integer_arithmetic_attempt(predicate) ||
        row_scalar_control_flow_condition_operator(predicate) == MYLITE_SQL_AST_OPERATOR_LIKE ||
        row_scalar_control_flow_condition_operator(predicate) ==
            MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE) {
        return false;
    }
    return row_scalar_expression_contains_context_expression_attempt(predicate, false);
}

static int execute_row_scalar_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_row_scalar_select plan = {false};
    int rc = plan_row_scalar_select(database, statement, true, &plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_row_scalar_select_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_row_scalar_select_from_plan(database, &plan, out_result);
    }
    planned_row_scalar_select_deinit(&plan);

    return rc;
}

static int execute_row_scalar_select_from_plan(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    mylite_result *result = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (plan->has_source && plan->source_count == 0U) {
        rc = load_result_column_metadata_context_for_table(
            database,
            plan->table.table_id,
            &metadata_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_result_columns(database, result, plan, &metadata_context);
    }
    if (rc == MYLITE_OK) {
        rc = build_row_scalar_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = row_scalar_select_step_error(database, statement, sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(database, statement, result, NULL, 0U);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = set_row_scalar_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }
    if (plan->calc_found_rows) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    return finish_successful_result(database, result, out_result);
}

static int row_scalar_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc
) {
    const char *message = NULL;

    if (sqlite_rc != SQLITE_ERROR || statement == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    message = sqlite3_errmsg(sqlite3_db_handle(statement));
    if (message == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (map_system_function_row_scalar_step_error(database, message) ||
        map_numeric_row_scalar_step_error(database, message) ||
        map_json_extract_row_scalar_step_error(database, message) ||
        map_json_mutation_row_scalar_step_error(database, message) ||
        map_json_search_row_scalar_step_error(database, message) ||
        map_json_type_row_scalar_step_error(database, message) ||
        map_json_constructor_row_scalar_step_error(database, message)) {
        return MYLITE_ERROR;
    }
    if (strcmp(message, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE) == 0) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(message, "CONVERT USING latin1 supports only ASCII scalar values") == 0) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static bool map_system_function_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strncmp(message, "XPATH syntax error", strlen("XPATH syntax error")) == 0) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_unknown,
            "HY000",
            message
        );
        return true;
    }
    if (strcmp(message, "Incorrect arguments to sleep.") == 0) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_incorrect_arguments,
            "HY000",
            message
        );
        return true;
    }
    return false;
}

static bool map_numeric_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "BIGINT value is out of range in 'abs(...)'") == 0 ||
        strcmp(message, "DOUBLE value is out of range in 'cot(0)'") == 0 ||
        strcmp(message, "DOUBLE value is out of range") == 0) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            message
        );
        return true;
    }
    return false;
}

static bool map_json_extract_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_EXTRACT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_EXTRACT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_EXTRACT()") == 0) {
        set_unsupported_error(database, "JSON_EXTRACT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_VALUE()") == 0) {
        set_invalid_json_data_type_error(database, "json_value");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_VALUE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_VALUE()") == 0) {
        set_unsupported_error(database, "JSON_VALUE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_LENGTH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_LENGTH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_LENGTH()") == 0) {
        set_unsupported_error(database, "JSON_LENGTH() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_DEPTH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_DEPTH");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_DEPTH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_DEPTH()") == 0) {
        set_unsupported_error(database, "JSON_DEPTH() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_STORAGE_SIZE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_STORAGE_SIZE");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_STORAGE_SIZE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_STORAGE_SIZE()") == 0) {
        set_unsupported_error(database, "JSON_STORAGE_SIZE() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_STORAGE_FREE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_STORAGE_FREE");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_STORAGE_FREE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_STORAGE_FREE()") == 0) {
        set_unsupported_error(database, "JSON_STORAGE_FREE() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_KEYS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_KEYS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_KEYS()") == 0) {
        set_unsupported_error(database, "JSON_KEYS() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_mutation_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_SET()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_SET");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_SET()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_SET()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_SET()") == 0) {
        set_unsupported_error(database, "JSON_SET() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_INSERT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_INSERT");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_INSERT()") == 0) {
        set_unsupported_error(database, "JSON_INSERT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REPLACE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REPLACE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REPLACE()") == 0) {
        set_unsupported_error(database, "JSON_REPLACE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REMOVE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REMOVE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "JSON_REMOVE path not allowed") == 0) {
        set_json_path_not_allowed_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REMOVE()") == 0) {
        set_unsupported_error(database, "JSON_REMOVE() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_search_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS()") == 0) {
        set_unsupported_error(database, "JSON_CONTAINS() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS_PATH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid oneOrAll in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_one_or_all_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS_PATH()") == 0) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() path or document shape is not supported"
        );
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_OVERLAPS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_OVERLAPS");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_OVERLAPS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_OVERLAPS()") == 0) {
        set_unsupported_error(database, "JSON_OVERLAPS() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in MEMBER OF()") == 0) {
        set_invalid_json_data_type_error(database, "MEMBER OF");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in MEMBER OF()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in MEMBER OF()") == 0) {
        set_unsupported_error(database, "MEMBER OF() document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_type_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_TYPE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_TYPE");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_TYPE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_TYPE()") == 0) {
        set_unsupported_error(database, "JSON_TYPE() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_PRETTY()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_PRETTY");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_PRETTY()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_PRETTY()") == 0) {
        set_unsupported_error(database, "JSON_PRETTY() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_UNQUOTE()") == 0) {
        set_json_unquote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_QUOTE()") == 0) {
        set_json_quote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_UNQUOTE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON string in JSON_UNQUOTE()") == 0) {
        set_unsupported_error(database, "JSON_UNQUOTE() JSON string shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_constructor_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "JSON documents may not contain NULL member names.") == 0) {
        set_json_null_member_name_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON constructor") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON value in JSON constructor") == 0) {
        set_unsupported_error(database, "JSON constructors support only baseline JSON values");
        return true;
    }
    return false;
}

static int append_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = append_row_scalar_result_column(
            database,
            result,
            plan,
            metadata_context,
            &plan->items[item_index]
        );
    }

    return rc;
}

static int append_row_scalar_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item
) {
    char *column_name = NULL;
    struct mylite_result_column_descriptor descriptor;
    int rc = copy_row_scalar_result_column_name(database, item, &column_name);

    if (rc == MYLITE_OK) {
        rc = make_row_scalar_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            column_name,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(column_name);

    return rc;
}

static int make_row_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return make_row_scalar_column_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            label,
            out_descriptor
        );
    }
    if (item->is_exists_expression) {
        populate_scalar_binary_numeric_result_column_descriptor(
            out_descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    }

    int rc = populate_row_scalar_source_result_column_descriptor(database, item, out_descriptor);

    if (rc != MYLITE_OK || row_scalar_descriptor_has_known_type(out_descriptor)) {
        return rc;
    }

    return populate_row_scalar_expression_result_column_descriptor(
        database,
        &item->expression,
        out_descriptor
    );
}

static int make_row_scalar_column_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const char *table_name = NULL;

    if (plan == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (metadata_context == NULL || !metadata_context->has_single_source_metadata) {
        return MYLITE_OK;
    }

    table_name = plan->source.table_name;
    if (plan->source_has_alias) {
        table_name = plan->source_alias;
    }
    out_descriptor->schema_name = plan->source.schema.name;
    out_descriptor->table_name = table_name;
    out_descriptor->origin_schema_name = plan->source.schema.name;
    out_descriptor->origin_table_name = plan->table.name;
    out_descriptor->origin_column_name = item->expression.column.name;
    out_descriptor->nullable = item->expression.column.is_nullable;

    return populate_select_result_column_descriptor(
        database,
        &plan->table,
        &item->expression.column,
        metadata_context,
        out_descriptor
    );
}

static int populate_row_scalar_source_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *expression = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || item == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    expression = unwrap_parenthesized_expression(item->source_expression);
    if (expression == NULL) {
        return MYLITE_OK;
    }

    if (!ast_subtree_has_column_reference(expression)) {
        rc = make_scalar_result_column_descriptor(
            database,
            expression,
            descriptor->label,
            descriptor
        );
        if (rc != MYLITE_OK || row_scalar_descriptor_has_known_type(descriptor)) {
            return rc;
        }
    }

    return populate_row_scalar_special_source_result_column_descriptor(
        database,
        expression,
        descriptor
    );
}

static int populate_row_scalar_special_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return populate_row_scalar_integer_expression_result_column_descriptor(
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_ABS_FUNCTION:
        return populate_row_scalar_abs_result_column_descriptor(database, expression, descriptor);
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
        return populate_row_scalar_scalar_subquery_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
        return populate_row_scalar_concat_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_YEAR_FUNCTION:
        populate_row_scalar_year_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
        return populate_row_scalar_conversion_source_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return populate_row_scalar_coalesce_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
        return populate_row_scalar_searched_case_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    default:
        return MYLITE_OK;
    }
}

static int populate_row_scalar_integer_expression_result_column_descriptor(
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    uint64_t width = 0U;
    bool matched = false;
    int rc = row_scalar_integer_expression_token_width(expression, &matched, &width);

    if (rc != MYLITE_OK || !matched) {
        return rc;
    }

    populate_scalar_binary_numeric_result_column_descriptor(
        descriptor,
        (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = width > mysql_scalar_bigint_display_length
                                  ? mysql_scalar_bigint_display_length
                                  : width,
            .decimals = 0U,
            .extra_flags = 0U,
            .nullable = false,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_abs_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    struct session_scalar_cell cell = {0};
    uint64_t display_length = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        return MYLITE_OK;
    }

    rc = session_scalar_value(database, expression, &cell);
    if (rc != MYLITE_OK) {
        session_scalar_cell_deinit(&cell);
        return MYLITE_OK;
    }
    if (cell.value == NULL ||
        !row_scalar_metadata_text_is_integer(cell.value, strlen(cell.value))) {
        session_scalar_cell_deinit(&cell);
        return MYLITE_OK;
    }
    display_length = (uint64_t)strlen(cell.value) + 1U;
    populate_scalar_binary_numeric_result_column_descriptor(
        descriptor,
        (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = display_length,
            .decimals = 0U,
            .extra_flags = 0U,
            .nullable = false,
        }
    );
    session_scalar_cell_deinit(&cell);
    return MYLITE_OK;
}

static int populate_row_scalar_scalar_subquery_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    struct session_scalar_cell cell = {0};
    uint64_t display_length = 0U;
    int rc = session_scalar_value(database, expression, &cell);

    if (rc != MYLITE_OK) {
        session_scalar_cell_deinit(&cell);
        return MYLITE_OK;
    }
    if (cell.value != NULL && row_scalar_metadata_text_is_integer(cell.value, strlen(cell.value))) {
        display_length = (uint64_t)strlen(cell.value) + 1U;
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
    }
    session_scalar_cell_deinit(&cell);
    return MYLITE_OK;
}

static int populate_row_scalar_concat_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    uint64_t display_length = 0U;

    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return MYLITE_OK;
    }

    argument = child_at(arguments, 0U);
    while (argument != NULL) {
        bool matched = false;
        uint64_t argument_display_length = 0U;
        int rc = row_scalar_source_string_display_length(
            database,
            argument,
            &matched,
            &argument_display_length
        );

        if (rc != MYLITE_OK || !matched) {
            return rc;
        }
        if (argument_display_length > UINT64_MAX - display_length) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        display_length += argument_display_length;
        argument = argument->next_sibling;
    }

    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = false,
        }
    );
    return MYLITE_OK;
}

static void populate_row_scalar_year_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_YEAR;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = year_text_length;
    descriptor->decimals = 0U;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM |
                        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_ZEROFILL;
    descriptor->nullable = true;
}

static int populate_row_scalar_conversion_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *argument = child_at(expression, 0U);
    bool matched = false;
    uint64_t display_length = 0U;
    int rc = MYLITE_OK;

    if (row_scalar_expression_is_date_target_conversion(expression)) {
        populate_scalar_date_result_column_descriptor(descriptor);
        return MYLITE_OK;
    }

    if (expression->kind == MYLITE_SQL_AST_CAST_BINARY_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION ||
        expression->kind == MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION) {
        rc = row_scalar_source_binary_display_length(database, argument, &matched, &display_length);
        if (rc != MYLITE_OK || !matched) {
            return rc;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    }

    rc = row_scalar_source_string_display_length(database, argument, &matched, &display_length);
    if (rc != MYLITE_OK || !matched) {
        return rc;
    }
    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_coalesce_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    bool has_string_argument = false;
    uint64_t display_length = 0U;

    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return MYLITE_OK;
    }

    argument = child_at(arguments, 0U);
    while (argument != NULL) {
        bool matched = false;
        bool argument_is_string = row_scalar_source_expression_is_string_literal(argument);
        uint64_t argument_display_length = 0U;
        int rc = row_scalar_source_string_display_length(
            database,
            argument,
            &matched,
            &argument_display_length
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matched) {
            if (argument_is_string) {
                has_string_argument = true;
            }
            if (argument_display_length > display_length) {
                display_length = argument_display_length;
            }
        }
        argument = argument->next_sibling;
    }
    if (!has_string_argument) {
        return MYLITE_OK;
    }

    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_searched_case_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *when_list = child_at(expression, 0U);
    const struct mylite_sql_ast_node *else_clause = child_at(expression, 1U);
    const struct mylite_sql_ast_node *when_clause = NULL;
    bool has_string_result = false;
    uint64_t display_length = 0U;

    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        return MYLITE_OK;
    }

    when_clause = child_at(when_list, 0U);
    while (when_clause != NULL) {
        const struct mylite_sql_ast_node *result_expression = child_at(when_clause, 1U);
        bool matched = false;
        bool result_is_string = row_scalar_source_expression_is_string_literal(result_expression);
        uint64_t result_display_length = 0U;
        int rc = row_scalar_source_string_display_length(
            database,
            result_expression,
            &matched,
            &result_display_length
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matched) {
            if (result_is_string) {
                has_string_result = true;
            }
            if (result_display_length > display_length) {
                display_length = result_display_length;
            }
        }
        when_clause = when_clause->next_sibling;
    }
    if (else_clause != NULL) {
        const struct mylite_sql_ast_node *else_expression = child_at(else_clause, 0U);
        bool matched = false;
        bool else_is_string = row_scalar_source_expression_is_string_literal(else_expression);
        uint64_t else_display_length = 0U;
        int rc = row_scalar_source_string_display_length(
            database,
            else_expression,
            &matched,
            &else_display_length
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matched) {
            if (else_is_string) {
                has_string_result = true;
            }
            if (else_display_length > display_length) {
                display_length = else_display_length;
            }
        }
    }
    if (!has_string_result) {
        return MYLITE_OK;
    }

    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int row_scalar_source_string_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t max_bytes_per_character = scalar_connection_max_bytes_per_character(database);

    if (out_matched == NULL || out_display_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;
    *out_display_length = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    } else {
        literal = expression;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        int rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "row-scalar metadata supports only ordinary string literals",
            "row-scalar metadata does not support NUL bytes in string literals",
            true,
            &text,
            &text_length
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        free(text);
        if (text_length > UINT64_MAX / max_bytes_per_character) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_display_length = (uint64_t)text_length * max_bytes_per_character;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        uint64_t width = 0U;
        bool matched = false;
        int rc = row_scalar_integer_expression_token_width(expression, &matched, &width);

        if (rc != MYLITE_OK || !matched) {
            return rc;
        }
        if (width > UINT64_MAX / max_bytes_per_character) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_display_length = width * max_bytes_per_character;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_matched = true;
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static bool row_scalar_source_expression_is_string_literal(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *literal = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    } else {
        literal = expression;
    }
    return literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
           mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING;
}

static int row_scalar_source_binary_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_matched == NULL || out_display_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;
    *out_display_length = 0U;
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_HEX || literal_kind == MYLITE_SQL_AST_LITERAL_BIT) {
        size_t byte_count = 0U;
        int rc = scalar_binary_literal_byte_count(database, literal, &byte_count);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_display_length = (uint64_t)byte_count;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        int rc = decode_sql_string_literal_with_policy(
            database,
            literal,
            "row-scalar metadata supports only ordinary string literals",
            "row-scalar metadata does not support NUL bytes in string literals",
            true,
            &text,
            &text_length
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        free(text);
        *out_display_length = (uint64_t)text_length;
        *out_matched = true;
    }
    return MYLITE_OK;
}

static int row_scalar_integer_expression_token_width(
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_width
) {
    struct row_scalar_integer_width_stack stack = {0};
    struct row_scalar_integer_width_result result = {0};
    int rc = MYLITE_OK;

    if (out_matched == NULL || out_width == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;
    *out_width = 0U;
    rc = row_scalar_integer_width_push_frame(
        &stack,
        (struct row_scalar_integer_width_frame){
            .kind = ROW_SCALAR_INTEGER_WIDTH_ENTER,
            .expression = expression,
        }
    );
    while (rc == MYLITE_OK && stack.frame_count != 0U) {
        struct row_scalar_integer_width_frame frame = stack.frames[--stack.frame_count];

        switch (frame.kind) {
        case ROW_SCALAR_INTEGER_WIDTH_ENTER:
            rc = row_scalar_integer_width_enter_frame(&stack, frame.expression);
            break;
        case ROW_SCALAR_INTEGER_WIDTH_FINISH_UNARY:
            rc = row_scalar_integer_width_finish_unary_frame(&stack);
            break;
        case ROW_SCALAR_INTEGER_WIDTH_FINISH_BINARY:
            rc = row_scalar_integer_width_finish_binary_frame(&stack);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_width_pop_result(&stack, &result);
    }
    if (rc == MYLITE_OK && stack.result_count != 0U) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_matched = result.matched;
        *out_width = result.width;
    }
    row_scalar_integer_width_stack_deinit(&stack);
    return rc;
}

static int row_scalar_integer_width_enter_frame(
    struct row_scalar_integer_width_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return row_scalar_integer_width_push_result(
            stack,
            (struct row_scalar_integer_width_result){0}
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            return row_scalar_integer_width_push_result(
                stack,
                (struct row_scalar_integer_width_result){.matched = true, .width = 1U}
            );
        }
        if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return row_scalar_integer_width_push_result(
                stack,
                (struct row_scalar_integer_width_result){0}
            );
        }
        return row_scalar_integer_width_push_result(
            stack,
            (struct row_scalar_integer_width_result){
                .matched = true,
                .width = (uint64_t)expression->span.length,
            }
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return row_scalar_integer_width_push_result(
                stack,
                (struct row_scalar_integer_width_result){0}
            );
        }
        int rc = row_scalar_integer_width_push_frame(
            stack,
            (struct row_scalar_integer_width_frame){
                .kind = ROW_SCALAR_INTEGER_WIDTH_FINISH_UNARY,
            }
        );

        if (rc == MYLITE_OK) {
            rc = row_scalar_integer_width_push_frame(
                stack,
                (struct row_scalar_integer_width_frame){
                    .kind = ROW_SCALAR_INTEGER_WIDTH_ENTER,
                    .expression = child_at(expression, 0U),
                }
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_MULTIPLY) {
            return row_scalar_integer_width_push_result(
                stack,
                (struct row_scalar_integer_width_result){0}
            );
        }
        int rc = row_scalar_integer_width_push_frame(
            stack,
            (struct row_scalar_integer_width_frame){
                .kind = ROW_SCALAR_INTEGER_WIDTH_FINISH_BINARY,
            }
        );
        if (rc == MYLITE_OK) {
            rc = row_scalar_integer_width_push_frame(
                stack,
                (struct row_scalar_integer_width_frame){
                    .kind = ROW_SCALAR_INTEGER_WIDTH_ENTER,
                    .expression = child_at(expression, 1U),
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = row_scalar_integer_width_push_frame(
                stack,
                (struct row_scalar_integer_width_frame){
                    .kind = ROW_SCALAR_INTEGER_WIDTH_ENTER,
                    .expression = child_at(expression, 0U),
                }
            );
        }
        return rc;
    }
    return row_scalar_integer_width_push_result(stack, (struct row_scalar_integer_width_result){0});
}

static int row_scalar_integer_width_finish_unary_frame(struct row_scalar_integer_width_stack *stack
) {
    struct row_scalar_integer_width_result operand = {0};
    int rc = row_scalar_integer_width_pop_result(stack, &operand);

    if (rc != MYLITE_OK || !operand.matched) {
        return rc == MYLITE_OK ? row_scalar_integer_width_push_result(
                                     stack,
                                     (struct row_scalar_integer_width_result){0}
                                 )
                               : rc;
    }
    if (operand.width == UINT64_MAX) {
        return MYLITE_NOMEM;
    }
    return row_scalar_integer_width_push_result(
        stack,
        (struct row_scalar_integer_width_result){
            .matched = true,
            .width = operand.width + 1U,
        }
    );
}

static int row_scalar_integer_width_finish_binary_frame(struct row_scalar_integer_width_stack *stack
) {
    struct row_scalar_integer_width_result left = {0};
    struct row_scalar_integer_width_result right = {0};
    int rc = row_scalar_integer_width_pop_result(stack, &right);

    if (rc == MYLITE_OK) {
        rc = row_scalar_integer_width_pop_result(stack, &left);
    }
    if (rc != MYLITE_OK || !left.matched || !right.matched) {
        return rc == MYLITE_OK ? row_scalar_integer_width_push_result(
                                     stack,
                                     (struct row_scalar_integer_width_result){0}
                                 )
                               : rc;
    }
    if (left.width > UINT64_MAX - right.width) {
        return MYLITE_NOMEM;
    }
    left.width += right.width;
    if (left.width == UINT64_MAX) {
        return MYLITE_NOMEM;
    }
    return row_scalar_integer_width_push_result(
        stack,
        (struct row_scalar_integer_width_result){
            .matched = true,
            .width = left.width + 1U,
        }
    );
}

static int row_scalar_integer_width_push_frame(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_frame frame
) {
    struct row_scalar_integer_width_frame *frames = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_ERROR;
    }
    if (stack->frame_count == stack->frame_capacity) {
        capacity =
            stack->frame_capacity == 0U ? if_stack_initial_capacity : stack->frame_capacity * 2U;
        if (capacity < stack->frame_capacity || capacity > SIZE_MAX / sizeof(*frames)) {
            return MYLITE_NOMEM;
        }
        frames = realloc(stack->frames, capacity * sizeof(*frames));
        if (frames == NULL) {
            return MYLITE_NOMEM;
        }
        stack->frames = frames;
        stack->frame_capacity = capacity;
    }
    stack->frames[stack->frame_count] = frame;
    ++stack->frame_count;
    return MYLITE_OK;
}

static int row_scalar_integer_width_push_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result result
) {
    struct row_scalar_integer_width_result *results = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_ERROR;
    }
    if (stack->result_count == stack->result_capacity) {
        capacity =
            stack->result_capacity == 0U ? if_stack_initial_capacity : stack->result_capacity * 2U;
        if (capacity < stack->result_capacity || capacity > SIZE_MAX / sizeof(*results)) {
            return MYLITE_NOMEM;
        }
        results = realloc(stack->results, capacity * sizeof(*results));
        if (results == NULL) {
            return MYLITE_NOMEM;
        }
        stack->results = results;
        stack->result_capacity = capacity;
    }
    stack->results[stack->result_count] = result;
    ++stack->result_count;
    return MYLITE_OK;
}

static int row_scalar_integer_width_pop_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result *out_result
) {
    if (stack == NULL || out_result == NULL) {
        return MYLITE_ERROR;
    }
    if (stack->result_count == 0U) {
        return MYLITE_ERROR;
    }
    *out_result = stack->results[--stack->result_count];
    return MYLITE_OK;
}

static void row_scalar_integer_width_stack_deinit(struct row_scalar_integer_width_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->frames);
    free(stack->results);
    *stack = (struct row_scalar_integer_width_stack){0};
}

static bool row_scalar_expression_is_date_target_conversion(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_source_span *span = NULL;
    const char *text = NULL;
    size_t target_start = 0U;
    size_t target_end = 0U;
    size_t before_target = 0U;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_CAST_CHAR_EXPRESSION &&
                               expression->kind != MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION)) {
        return false;
    }
    span = &expression->span;
    if (span->text == NULL || span->length == 0U) {
        return false;
    }
    text = span->text;
    target_end = span->length;
    while (target_end > 0U && isspace((unsigned char)text[target_end - 1U])) {
        --target_end;
    }
    if (target_end > 0U && text[target_end - 1U] == ')') {
        --target_end;
    }
    while (target_end > 0U && isspace((unsigned char)text[target_end - 1U])) {
        --target_end;
    }
    target_start = target_end;
    while (target_start > 0U && row_scalar_metadata_byte_is_identifier(text[target_start - 1U])) {
        --target_start;
    }
    if (target_end - target_start != 4U || toupper((unsigned char)text[target_start]) != 'D' ||
        toupper((unsigned char)text[target_start + 1U]) != 'A' ||
        toupper((unsigned char)text[target_start + 2U]) != 'T' ||
        toupper((unsigned char)text[target_start + 3U]) != 'E') {
        return false;
    }

    before_target = target_start;
    while (before_target > 0U && isspace((unsigned char)text[before_target - 1U])) {
        --before_target;
    }
    if (before_target > 0U && text[before_target - 1U] == ',') {
        return true;
    }
    if (before_target < 2U || toupper((unsigned char)text[before_target - 2U]) != 'A' ||
        toupper((unsigned char)text[before_target - 1U]) != 'S') {
        return false;
    }
    return before_target == 2U || !row_scalar_metadata_byte_is_identifier(text[before_target - 3U]);
}

static bool row_scalar_metadata_byte_is_identifier(char byte) {
    return isalnum((unsigned char)byte) || byte == '_' || byte == '$';
}

static bool row_scalar_metadata_text_is_integer(const char *text, size_t text_length) {
    size_t index = 0U;

    if (text == NULL || text_length == 0U) {
        return false;
    }
    if (text[0] == '-' || text[0] == '+') {
        if (text_length == 1U) {
            return false;
        }
        index = 1U;
    }
    for (; index < text_length; ++index) {
        if (!isdigit((unsigned char)text[index])) {
            return false;
        }
    }
    return true;
}

static bool row_scalar_descriptor_has_known_type(
    const struct mylite_result_column_descriptor *descriptor
) {
    return descriptor != NULL && descriptor->logical_type != MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN;
}

static int populate_row_scalar_expression_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return populate_window_function_result_column_descriptor(database, expression, descriptor);
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL,
                .display_length = mysql_sum_integer_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_regexp_string_function_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
        populate_row_scalar_digest_result_column_descriptor(database, expression, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
        return populate_row_scalar_compress_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_uncompress_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_uncompressed_length_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_random_bytes_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_inet_ntoa_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = 4U * scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
        if (expression->numeric_function_kind == PLANNED_ROW_SCALAR_NUMERIC_SIGN ||
            expression->numeric_function_kind == PLANNED_ROW_SCALAR_NUMERIC_BIT_COUNT) {
            populate_scalar_binary_numeric_result_column_descriptor(
                descriptor,
                (struct scalar_binary_numeric_result_column_shape){
                    .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                    .display_length = mysql_scalar_bigint_display_length,
                    .decimals = 0U,
                    .extra_flags = 0U,
                    .nullable = true,
                }
            );
            return MYLITE_OK;
        }
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = (base_conversion_text_capacity - 1U) *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
        if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_CRC32) {
            populate_scalar_binary_numeric_result_column_descriptor(
                descriptor,
                (struct scalar_binary_numeric_result_column_shape){
                    .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                    .display_length = mysql_unsigned_int_display_length,
                    .decimals = 0U,
                    .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                    .nullable = true,
                }
            );
            return MYLITE_OK;
        }
        if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT) {
            populate_scalar_connection_string_result_column_descriptor(
                database,
                descriptor,
                (struct scalar_connection_string_result_column_shape){
                    .display_length =
                        (scalar_exact_decimal_part_capacity + scalar_format_max_decimals +
                         scalar_format_grouping_overhead) *
                        scalar_connection_max_bytes_per_character(database),
                    .extra_flags = 0U,
                    .nullable = true,
                }
            );
            return MYLITE_OK;
        }
        if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE) {
            populate_scalar_binary_numeric_result_column_descriptor(
                descriptor,
                (struct scalar_binary_numeric_result_column_shape){
                    .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL,
                    .display_length =
                        scalar_exact_decimal_part_capacity + scalar_format_max_decimals,
                    .decimals = scalar_format_max_decimals,
                    .extra_flags = 0U,
                    .nullable = true,
                }
            );
            return MYLITE_OK;
        }
        if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI) {
            populate_scalar_binary_numeric_result_column_descriptor(
                descriptor,
                (struct scalar_binary_numeric_result_column_shape){
                    .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                    .display_length = mysql_scalar_double_display_length,
                    .decimals = mysql_approximate_decimals,
                    .extra_flags = 0U,
                    .nullable = false,
                }
            );
            return MYLITE_OK;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        populate_uuid_string_result_column_descriptor(descriptor, true);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = MYLITE_UUID_BINARY_SIZE;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = MYLITE_UUID_TEXT_SIZE,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_type_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_value_display_character_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        populate_scalar_json_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return populate_row_scalar_date_interval_second_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return populate_row_scalar_conversion_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE: {
        int rc = MYLITE_OK;

        if (expression->arguments == NULL || expression->argument_count != 1U) {
            return MYLITE_ERROR;
        }
        rc = populate_row_scalar_expression_result_column_descriptor(
            database,
            &expression->arguments[0],
            descriptor
        );
        if (rc == MYLITE_OK) {
            descriptor->charset_id = result_metadata_collation_id(expression->collation_name);
            descriptor->collation_id = descriptor->charset_id;
        }
        return rc;
    }
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        if (mylite_temporal_extract_kind_is_calendar_name(expression->temporal_extract_kind)) {
            populate_calendar_name_result_column_descriptor(database, descriptor);
        }
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int populate_window_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->window_function_kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_NTILE:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return populate_window_value_function_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int populate_window_value_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *argument = NULL;
    int rc = MYLITE_OK;

    if (expression == NULL || descriptor == NULL || expression->argument_count == 0U ||
        expression->arguments == NULL) {
        return MYLITE_MISUSE;
    }

    argument = &expression->arguments[0];
    if (argument->kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return populate_window_value_column_result_column_descriptor(
            database,
            &argument->column,
            descriptor
        );
    }
    if (argument->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
        return populate_window_value_literal_result_column_descriptor(
            database,
            &argument->value,
            descriptor
        );
    }

    rc = populate_row_scalar_expression_result_column_descriptor(database, argument, descriptor);
    if (rc != MYLITE_OK) {
        return rc;
    }
    descriptor->nullable = true;
    descriptor->flags &= ~((uint32_t)MYLITE_RESULT_COLUMN_FLAG_NOT_NULL);
    return MYLITE_OK;
}

static int populate_window_value_column_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
) {
    bool matched = false;
    int rc = populate_numeric_result_column_descriptor(database, column, descriptor, &matched);

    if (rc == MYLITE_OK && !matched) {
        rc = populate_string_result_column_descriptor(database, NULL, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_binary_result_column_descriptor(database, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_temporal_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_bit_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!matched) {
        set_unsupported_error(
            database,
            "window value metadata supports only current baseline column descriptors"
        );
        return MYLITE_ERROR;
    }

    descriptor->nullable = true;
    descriptor->flags &= ~((uint32_t)MYLITE_RESULT_COLUMN_FLAG_NOT_NULL);
    return MYLITE_OK;
}

static int populate_window_value_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_value *value,
    struct mylite_result_column_descriptor *descriptor
) {
    uint64_t display_length = 0U;

    if (database == NULL || value == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NULL,
                .display_length = 0U,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_text) {
        if (value->text_length > UINT64_MAX / scalar_connection_max_bytes_per_character(database)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        display_length =
            (uint64_t)value->text_length * scalar_connection_max_bytes_per_character(database);
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_real) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }

    populate_scalar_binary_numeric_result_column_descriptor(
        descriptor,
        (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = mysql_scalar_bigint_display_length,
            .decimals = 0U,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_conversion_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (database == NULL || expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->conversion_kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_scalar_bigint_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = result_metadata_collation_id(expression->conversion_collation);
        descriptor->collation_id = descriptor->charset_id;
        descriptor->display_length = mysql_scalar_bigint_display_length *
                                     scalar_connection_max_bytes_per_character(database);
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = 0U;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static void populate_row_scalar_digest_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const enum mylite_digest_algorithm algorithm = row_scalar_digest_algorithm(expression);
    const uint64_t display_length = (uint64_t)mylite_digest_hex_length(algorithm) *
                                    scalar_connection_max_bytes_per_character(database);

    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
}

static int populate_row_scalar_compress_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    struct mylite_result_column_descriptor argument_descriptor = {0};
    uint64_t argument_display_length = 0U;
    int rc = MYLITE_OK;

    if (database == NULL || expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->argument_count == 1U && expression->arguments != NULL) {
        if (expression->arguments[0].kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            rc = populate_window_value_literal_result_column_descriptor(
                database,
                &expression->arguments[0].value,
                &argument_descriptor
            );
        } else if (expression->arguments[0].kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
            rc = populate_window_value_column_result_column_descriptor(
                database,
                &expression->arguments[0].column,
                &argument_descriptor
            );
        } else {
            rc = populate_row_scalar_expression_result_column_descriptor(
                database,
                &expression->arguments[0],
                &argument_descriptor
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        argument_display_length = argument_descriptor.display_length;
    }
    if (argument_display_length > UINT64_MAX - mysql_compress_overhead_display_length) {
        argument_display_length = UINT64_MAX - mysql_compress_overhead_display_length;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = argument_display_length + mysql_compress_overhead_display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
    return MYLITE_OK;
}

static int populate_row_scalar_date_interval_second_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *input_kind_argument = NULL;
    const struct planned_row_scalar_expression *unit_argument = NULL;
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;

    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->argument_count <= 3U || expression->arguments == NULL) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
    input_kind_argument = &expression->arguments[1];
    unit_argument = &expression->arguments[3];
    if (input_kind_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        input_kind_argument->value.is_null || !input_kind_argument->value.is_text ||
        !mylite_date_interval_second_input_kind_from_name(
            input_kind_argument->value.text,
            input_kind_argument->value.text_length,
            &input_kind
        ) ||
        unit_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        unit_argument->value.is_null || !unit_argument->value.is_text ||
        !mylite_date_interval_unit_from_name(
            unit_argument->value.text,
            unit_argument->value.text_length,
            &unit
        )) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }

    switch (input_kind) {
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE:
        if (mylite_date_interval_unit_has_time_part(unit)) {
            populate_scalar_datetime_result_column_descriptor(descriptor);
        } else {
            populate_scalar_date_result_column_descriptor(descriptor);
        }
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME:
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP:
        populate_scalar_datetime_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING:
    default:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
}

static int copy_row_scalar_result_column_name(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    char **out_name
) {
    const struct mylite_sql_ast_node *source_expression = NULL;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    source_expression = unwrap_parenthesized_expression(item->source_expression);
    if (item->alias != NULL) {
        return copy_select_item_alias_text(database, item->alias, out_name);
    }
    if (source_expression != NULL && source_expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return copy_scalar_projection_column_name(database, item->source_expression, out_name);
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return duplicate_text(database, item->expression.column.name, out_name);
    }

    return copy_scalar_projection_column_name(database, item->source_expression, out_name);
}

static int set_row_scalar_select_found_row_count(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result *result
) {
    struct planned_select count_plan = {0};
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (!plan->has_source) {
        if (mylite_result_row_count(result) > UINT64_MAX) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)mylite_result_row_count(result));
        return MYLITE_OK;
    }

    count_plan.table = plan->table;
    count_plan.sources = plan->sources;
    count_plan.source_count = plan->source_count;
    count_plan.join_kinds = plan->join_kinds;
    count_plan.join_conditions = plan->join_conditions;
    count_plan.join_count = plan->join_count;
    count_plan.predicate = plan->predicate;
    count_plan.limit = plan->limit;
    count_plan.requires_source_alias = plan->predicate.qualify_column_references;
    if (plan->calc_found_rows) {
        int64_t count = 0;

        rc = read_select_found_row_count(database, &count_plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)count);
        return MYLITE_OK;
    }
    rc = found_row_count_for_select_limit_envelope(
        database,
        &count_plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc == MYLITE_OK) {
        mylite_result_set_found_row_count(result, found_row_count);
    }
    return rc;
}

static int set_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result *result
) {
    int64_t count = 0;
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (plan->calc_found_rows) {
        rc = read_select_found_row_count(database, plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)count);
        return MYLITE_OK;
    }

    rc = found_row_count_for_select_limit_envelope(
        database,
        plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_result_set_found_row_count(result, found_row_count);
    return MYLITE_OK;
}

static int found_row_count_for_select_limit_envelope(
    struct mylite_db *database,
    const struct planned_select *plan,
    size_t visible_row_count,
    uint64_t *out_found_row_count
) {
    uint64_t visible_rows = (uint64_t)visible_row_count;
    uint64_t offset = 0U;
    int64_t total_count = 0;
    int rc = MYLITE_OK;

    if (out_found_row_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found_row_count = 0U;
    if (visible_row_count > UINT64_MAX) {
        set_runtime_error(database, "SELECT found-row count is out of range");
        return MYLITE_ERROR;
    }
    if (!plan->limit.has_limit) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }
    if (!plan->limit.has_offset) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }

    offset = (uint64_t)plan->limit.offset;
    if (plan->limit.row_count != 0 && visible_row_count != 0U) {
        if (offset > UINT64_MAX - visible_rows) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        *out_found_row_count = offset + visible_rows;
        return MYLITE_OK;
    }
    if (offset == 0U) {
        *out_found_row_count = 0U;
        return MYLITE_OK;
    }

    rc = read_select_found_row_count(database, plan, &total_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (total_count < 0) {
        set_runtime_error(database, "invalid SELECT found-row count");
        return MYLITE_ERROR;
    }
    *out_found_row_count = offset < (uint64_t)total_count ? offset : (uint64_t)total_count;
    return MYLITE_OK;
}

static int read_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int parameter_index = 1;
    int rc = build_select_found_rows_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        for (size_t join_index = 0U; rc == MYLITE_OK && join_index < plan->join_count;
             ++join_index) {
            rc = bind_select_join_condition_parameters(
                statement,
                &plan->join_conditions[join_index],
                &parameter_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    return rc;
}
