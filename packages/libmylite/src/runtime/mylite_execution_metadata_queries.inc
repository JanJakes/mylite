static int execute_do_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    const char *argument_count_error_function = NULL;
    struct session_scalar_cell *cells = NULL;
    mylite_result *result = NULL;
    size_t expression_count = mylite_sql_ast_node_child_count(expression_list);
    size_t expression_index = 0U;
    uint64_t previous_last_insert_id = database->session.last_insert_id;
    int rc = MYLITE_OK;

    argument_count_error_function = do_statement_argument_count_error_function(statement);
    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    if (!do_statement_has_only_scalar_projection_expressions(statement)) {
        set_unsupported_error(
            database,
            "DO supports only session scalar values, integer/boolean/NULL values, scalar "
            "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL(), signed 64-bit +, binary -, and * "
            "arithmetic, %, MOD, DIV, top-level / division, limited numeric bitwise operators, "
            "signed 64-bit scalar comparison, keyword scalar logical operators, scalar IS "
            "predicates, limited numeric "
            "ABS()/SIGN()/BIT_COUNT()/BIN()/OCT()/CONV()/PI()/RAND()/SQRT()/DEGREES()/"
            "RADIANS()/ACOS()/ASIN()/SIN()/COS()/TAN()/COT()/ATAN()/ATAN2()/EXP()/LN()/"
            "LOG()/LOG10()/LOG2()/POW()/POWER(), CEIL()/CEILING()/FLOOR()/ROUND(), "
            "CRC32()/HEX()/UNHEX()/FORMAT()/TRUNCATE(), limited CAST(value AS BINARY/CHAR/"
            "SIGNED/UNSIGNED), limited CONVERT(value, BINARY/CHAR/SIGNED/UNSIGNED), limited "
            "DATE_ADD(... INTERVAL ... unit), limited DATE_FORMAT(), limited temporal "
            "extract, limited ELT(), FIELD(), GREATEST(), LEAST(), limited CONCAT_WS(), limited "
            "JSON_VALID(), JSON_VALUE(), JSON_ARRAY(), JSON_OBJECT(), and limited string length, "
            "string case, string slice, CHARSET(), COLLATION(), and COERCIBILITY() functions, "
            "and top-level CASE expressions"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (expression_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(expression_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_session_scalar_do_warnings(database, statement);
    while (rc == MYLITE_OK && expression != NULL) {
        rc = session_scalar_value(database, expression, &cells[expression_index]);
        if (rc == MYLITE_OK) {
            ++expression_index;
            expression = expression->next_sibling;
        }
    }
    if (rc == MYLITE_OK || expression_index != 0U) {
        int warning_rc = append_session_scalar_cells_warnings(database, cells, expression_index);

        if (warning_rc != MYLITE_OK) {
            rc = warning_rc;
        }
    }
    session_scalar_cell_array_deinit(cells, expression_index);
    if (rc != MYLITE_OK) {
        database->session.last_insert_id = previous_last_insert_id;
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const char *argument_count_error_function = NULL;
    bool is_information_schema_query = false;
    bool is_mysql_data_dictionary_table_query = false;
    bool is_absent_mysql_enterprise_table_query = false;
    bool is_mysql_system_table_query = false;
    bool is_absent_builtin_schema_table_query = false;
    bool projected_statement_handled = false;
    char mysql_data_dictionary_table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char absent_mysql_enterprise_table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char absent_builtin_schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char absent_builtin_table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    memset(mysql_data_dictionary_table_name, 0, sizeof(mysql_data_dictionary_table_name));
    memset(absent_mysql_enterprise_table_name, 0, sizeof(absent_mysql_enterprise_table_name));
    memset(absent_builtin_schema_name, 0, sizeof(absent_builtin_schema_name));
    memset(absent_builtin_table_name, 0, sizeof(absent_builtin_table_name));
    argument_count_error_function = select_statement_argument_count_error_function(statement);
    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    rc = reject_select_modifier_usage_if_needed(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_statement_has_aggregate_window_expression(statement)) {
        set_unsupported_error(database, "aggregate window functions are not supported");
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_information_schema(
        database,
        statement,
        &is_information_schema_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_information_schema_query) {
        return execute_information_schema_select_statement(
            database,
            context,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    rc = select_statement_targets_mysql_data_dictionary_table(
        database,
        statement,
        mysql_data_dictionary_table_name,
        sizeof(mysql_data_dictionary_table_name),
        &is_mysql_data_dictionary_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_mysql_data_dictionary_table_query) {
        set_mysql_data_dictionary_table_access_error(database, mysql_data_dictionary_table_name);
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_absent_mysql_enterprise_table(
        database,
        statement,
        absent_mysql_enterprise_table_name,
        sizeof(absent_mysql_enterprise_table_name),
        &is_absent_mysql_enterprise_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_absent_mysql_enterprise_table_query) {
        set_table_does_not_exist_error(database, "mysql", absent_mysql_enterprise_table_name);
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_mysql_system_table(
        database,
        statement,
        &is_mysql_system_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_mysql_system_table_query) {
        return execute_mysql_system_table_select_statement(
            database,
            context,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    rc = select_statement_targets_absent_builtin_schema_table(
        database,
        statement,
        absent_builtin_schema_name,
        sizeof(absent_builtin_schema_name),
        absent_builtin_table_name,
        sizeof(absent_builtin_table_name),
        &is_absent_builtin_schema_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_absent_builtin_schema_table_query) {
        set_table_does_not_exist_error(
            database,
            absent_builtin_schema_name,
            absent_builtin_table_name
        );
        return MYLITE_ERROR;
    }
    if (select_statement_has_group_by_clause(statement)) {
        return execute_grouped_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_has_count_having_clause(statement)) {
        return execute_count_having_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_has_column_aggregate(statement) ||
        select_statement_has_multi_count_aggregate(statement)) {
        return execute_column_aggregate_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    rc = execute_scalar_or_row_scalar_select_if_needed(
        database,
        statement,
        apply_sql_select_limit,
        out_result,
        &projected_statement_handled
    );
    if (projected_statement_handled || rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_bare_native_function_identifier_lookup_if_needed(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_statement_has_count_expression_aggregate(statement)) {
        return execute_count_expression_aggregate_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_has_count_aggregate(statement)) {
        return execute_count_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_scalar_projection_attempt(statement)) {
        set_unsupported_error(
            database,
            "SELECT scalar projection supports only session scalar values, integer/boolean/string/"
            "NULL, "
            "scalar comparison, scalar logical, scalar IS, signed 64-bit +, binary -, and * "
            "arithmetic, %, MOD, DIV, top-level / division, limited numeric bitwise, scalar "
            "functions, "
            "ABS()/SIGN()/BIT_COUNT()/BIN()/OCT()/CONV()/PI()/RAND()/SQRT()/DEGREES()/"
            "RADIANS()/ACOS()/ASIN()/SIN()/COS()/TAN()/COT()/ATAN()/ATAN2()/EXP()/LN()/"
            "LOG()/LOG10()/LOG2()/POW()/POWER()/CEIL()/CEILING()/FLOOR()/ROUND()/"
            "CRC32()/FORMAT()/TRUNCATE(), and "
            "CAST(value AS BINARY/CHAR/SIGNED/UNSIGNED), CONVERT(value, BINARY/CHAR/SIGNED/"
            "UNSIGNED), DATE_ADD(... INTERVAL ... unit), DATE_FORMAT(), and "
            "limited ELT(), FIELD(), GREATEST(), LEAST(), JSON_VALID(), JSON_VALUE(), "
            "JSON_ARRAY(), JSON_OBJECT(), "
            "string length, string case, string trim, and string slice functions"
        );
        return MYLITE_ERROR;
    }

    return execute_descriptor_select_statement(
        database,
        statement,
        apply_sql_select_limit,
        out_result
    );
}

static bool select_statement_has_aggregate_window_expression(
    const struct mylite_sql_ast_node *statement
) {
    return ast_node_has_aggregate_window_expression(statement);
}

static bool ast_node_has_aggregate_window_expression(const struct mylite_sql_ast_node *node) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return false;
    }
    if (ast_node_is_aggregate_function(node)) {
        child = child_at(node, 0U);
        while (child != NULL) {
            if (ast_node_is_window_clause_marker(child)) {
                return true;
            }
            child = child->next_sibling;
        }
    }
    child = child_at(node, 0U);
    while (child != NULL) {
        if (ast_node_has_aggregate_window_expression(child)) {
            return true;
        }
        child = child->next_sibling;
    }
    return false;
}

static bool ast_node_is_generic_aggregate_window_function(const struct mylite_sql_ast_node *node) {
    static const char *const names[] = {
        "JSON_ARRAYAGG",
        "JSON_OBJECTAGG",
        "STD",
        "STDDEV",
        "STDDEV_POP",
        "STDDEV_SAMP",
        "VAR_POP",
        "VAR_SAMP",
        "VARIANCE",
    };
    const struct mylite_sql_ast_node *name = NULL;

    if (node == NULL || node->kind != MYLITE_SQL_AST_GENERIC_FUNCTION) {
        return false;
    }

    name = child_at(node, 0U);
    if (name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (source_span_text_equals_ascii_case_insensitive(&name->span, names[index])) {
            return true;
        }
    }
    return false;
}

static bool source_span_text_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
) {
    size_t length = strlen(text);

    if (span == NULL || span->text == NULL || span->length != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        char left = span->text[index];
        char right = text[index];

        if (left >= 'a' && left <= 'z') {
            left = (char)(left - ('a' - 'A'));
        }
        if (right >= 'a' && right <= 'z') {
            right = (char)(right - ('a' - 'A'));
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

static bool ast_node_is_aggregate_function(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    if (ast_node_is_generic_aggregate_window_function(node)) {
        return true;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION:
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STD_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_STDDEV_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_POP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VAR_SAMP_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_VARIANCE_AGGREGATE_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool ast_node_is_window_clause_marker(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    return node->kind == MYLITE_SQL_AST_WINDOW_SPEC ||
           node->kind == MYLITE_SQL_AST_WINDOW_REFERENCE;
}

static int execute_grouped_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_grouped_aggregate grouped_plan = {0};
    int rc = plan_grouped_aggregate(database, statement, &grouped_plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &grouped_plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_grouped_aggregate_from_plan(database, &grouped_plan, out_result);
    }
    planned_grouped_aggregate_deinit(&grouped_plan);
    return rc;
}

static int execute_count_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_count count_plan = {false};
    int rc = plan_count(database, statement, &count_plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK && mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
    }
    if (rc == MYLITE_OK) {
        rc = execute_count_from_plan(database, &count_plan, apply_sql_select_limit, out_result);
    }
    planned_count_deinit(&count_plan);
    return rc;
}

static int execute_column_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_column_aggregate_select aggregate_plan = {0};
    int rc = plan_column_aggregate_select(database, statement, &aggregate_plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK && mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
    }
    if (rc == MYLITE_OK) {
        rc = execute_column_aggregate_from_plan(
            database,
            &aggregate_plan,
            apply_sql_select_limit,
            out_result
        );
    }
    planned_column_aggregate_select_deinit(&aggregate_plan);
    return rc;
}

static int execute_descriptor_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_select plan = {0};
    int rc = plan_select(database, statement, true, &plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_select_from_plan(database, &plan, out_result);
    }
    planned_select_deinit(&plan);

    return rc;
}

static int execute_compound_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    mylite_result *result = NULL;
    struct compound_branch_results branch_results = {0};
    struct compound_row_comparison_columns comparison_columns = {0};
    size_t branch_index = 1U;
    enum mylite_sql_ast_set_operator operator_kind = MYLITE_SQL_AST_SET_OPERATOR_UNION;
    int rc = validate_compound_select_operator_chain(database, terms, &operator_kind);

    if (rc == MYLITE_OK) {
        rc = execute_compound_select_branch_results(
            database,
            context,
            statement,
            operator_kind,
            &branch_results
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_compound_result_columns(database, result, branch_results.items[0]);
    }
    if (rc == MYLITE_OK) {
        rc = init_compound_row_comparison_columns(
            database,
            statement,
            &branch_results,
            &comparison_columns
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_all_compound_rows(database, result, branch_results.items[0]);
    }

    term = child_at(terms, 0U);
    while (rc == MYLITE_OK && term != NULL) {
        rc = apply_compound_term_result(
            database,
            term,
            branch_results.items[branch_index],
            &result,
            &comparison_columns
        );
        ++branch_index;
        term = term->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = apply_compound_select_final_order_and_limit(database, statement, result);
    }

    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        deinit_compound_branch_results(&branch_results);
        deinit_compound_row_comparison_columns(&comparison_columns);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    apply_sql_select_limit_to_result(database, result);
    deinit_compound_branch_results(&branch_results);
    deinit_compound_row_comparison_columns(&comparison_columns);
    return finish_successful_result(database, result, out_result);
}

static int execute_compound_select_branch_results(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_set_operator operator_kind,
    struct compound_branch_results *out_results
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    const struct mylite_sql_ast_node *branch = child_at(statement, 0U);
    struct compound_branch_results results = {0};
    size_t branch_index = 0U;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    results.count = compound_select_branch_count(statement);
    if (results.count > SIZE_MAX / sizeof(*results.items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    results.items = (mylite_result **)calloc(results.count, sizeof(*results.items));
    if (results.items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = reject_compound_select_branch_shape(database, branch, operator_kind, false);
    if (rc == MYLITE_OK) {
        rc = execute_select_statement(
            database,
            context,
            branch,
            false,
            &results.items[branch_index]
        );
    }
    if (rc == MYLITE_OK) {
        column_count = mylite_result_column_count(results.items[branch_index]);
    }

    term = child_at(terms, 0U);
    while (rc == MYLITE_OK && term != NULL) {
        mylite_result *branch_result = NULL;

        ++branch_index;
        branch = child_at(term, 0U);
        rc = reject_compound_select_branch_shape(
            database,
            branch,
            operator_kind,
            branch_index + 1U == results.count
        );
        if (rc == MYLITE_OK) {
            rc = execute_select_statement(database, context, branch, false, &branch_result);
        }
        if (rc == MYLITE_OK && mylite_result_column_count(branch_result) != column_count) {
            set_union_column_count_mismatch_error(database);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            results.items[branch_index] = branch_result;
            branch_result = NULL;
        }
        mylite_result_free(branch_result);
        term = term->next_sibling;
    }

    if (rc != MYLITE_OK) {
        deinit_compound_branch_results(&results);
        return rc;
    }

    *out_results = results;
    return MYLITE_OK;
}

static void deinit_compound_branch_results(struct compound_branch_results *results) {
    if (results == NULL) {
        return;
    }

    if (results->items != NULL) {
        for (size_t index = 0U; index < results->count; ++index) {
            mylite_result_free(results->items[index]);
        }
    }
    free((void *)results->items);
    *results = (struct compound_branch_results){0};
}

static size_t compound_select_branch_count(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    size_t count = 1U;

    for (const struct mylite_sql_ast_node *term = child_at(terms, 0U); term != NULL;
         term = term->next_sibling) {
        ++count;
    }
    return count;
}

static int init_compound_row_comparison_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct compound_branch_results *branch_results,
    struct compound_row_comparison_columns *out_columns
) {
    struct compound_row_comparison_columns columns = {0};
    size_t column_count = mylite_result_column_count(branch_results->items[0]);
    int rc = MYLITE_OK;

    if (column_count > SIZE_MAX / sizeof(*columns.string_collation_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (column_count != 0U) {
        columns.string_collation_columns =
            calloc(column_count, sizeof(*columns.string_collation_columns));
        columns.bytewise_collation_columns =
            calloc(column_count, sizeof(*columns.bytewise_collation_columns));
        if (columns.string_collation_columns == NULL ||
            columns.bytewise_collation_columns == NULL) {
            deinit_compound_row_comparison_columns(&columns);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    columns.column_count = column_count;

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        columns.bytewise_collation_columns[column_index] =
            compound_statement_column_has_binary_expression(statement, column_index);
    }

    for (size_t branch_index = 0U; rc == MYLITE_OK && branch_index < branch_results->count;
         ++branch_index) {
        rc = merge_compound_string_collation_columns(
            database,
            branch_results->items[branch_index],
            &columns
        );
    }
    if (rc != MYLITE_OK) {
        deinit_compound_row_comparison_columns(&columns);
        return rc;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (!columns.bytewise_collation_columns[column_index] &&
            compound_statement_column_uses_string_collation(statement, column_index)) {
            columns.string_collation_columns[column_index] = true;
        }
    }

    *out_columns = columns;
    return MYLITE_OK;
}

static void deinit_compound_row_comparison_columns(struct compound_row_comparison_columns *columns
) {
    if (columns == NULL) {
        return;
    }

    free(columns->string_collation_columns);
    free(columns->bytewise_collation_columns);
    *columns = (struct compound_row_comparison_columns){0};
}

static int apply_compound_term_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *term,
    const mylite_result *branch_result,
    mylite_result **in_out_result,
    struct compound_row_comparison_columns *comparison_columns
) {
    bool distinct =
        mylite_sql_ast_node_union_modifier(term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
    enum mylite_sql_ast_set_operator operator_kind = mylite_sql_ast_node_set_operator_kind(term);
    int rc = MYLITE_OK;

    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        rc = append_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        rc = apply_intersect_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        rc = apply_except_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    }

    return rc;
}

static void apply_sql_select_limit_to_plan_limit(
    const struct mylite_db *database,
    struct planned_select_limit *limit
) {
    uint64_t row_count = UINT64_MAX;

    if (database == NULL || limit == NULL || limit->has_limit) {
        return;
    }
    row_count = database->session.sql_select_limit;
    if (row_count == UINT64_MAX || row_count > (uint64_t)INT64_MAX) {
        return;
    }

    limit->has_limit = true;
    limit->row_count = (int64_t)row_count;
    limit->has_offset = false;
    limit->offset = 0;
}

static void apply_sql_select_limit_to_result(
    const struct mylite_db *database,
    mylite_result *result
) {
    uint64_t row_count = UINT64_MAX;

    if (database == NULL || result == NULL) {
        return;
    }
    row_count = database->session.sql_select_limit;
    if (row_count == UINT64_MAX || row_count > (uint64_t)SIZE_MAX) {
        return;
    }
    if ((uint64_t)mylite_result_row_count(result) > row_count) {
        mylite_result_truncate_rows(result, (size_t)row_count);
    }
}

static int apply_compound_select_final_order_and_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
) {
    const struct mylite_sql_ast_node *final_branch = compound_select_final_branch(statement);
    int rc = MYLITE_OK;

    if (final_branch == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = apply_compound_select_final_order(
        database,
        result,
        child_with_kind(final_branch, MYLITE_SQL_AST_ORDER_BY_CLAUSE)
    );
    if (rc == MYLITE_OK) {
        rc = apply_compound_select_final_limit(
            database,
            result,
            child_with_kind(final_branch, MYLITE_SQL_AST_LIMIT_CLAUSE)
        );
    }

    return rc;
}

static const struct mylite_sql_ast_node *compound_select_final_branch(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = child_at(terms, 0U);
    const struct mylite_sql_ast_node *branch = child_at(statement, 0U);

    while (term != NULL) {
        branch = child_at(term, 0U);
        term = term->next_sibling;
    }
    return branch;
}

static int apply_compound_select_final_order(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *order_clause
) {
    const struct mylite_sql_ast_node *order_key = NULL;
    size_t column_index = 0U;
    bool descending = false;
    int rc = MYLITE_OK;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    rc = compound_select_order_key_and_direction(database, order_clause, &order_key, &descending);
    if (rc == MYLITE_OK) {
        rc = compound_select_order_column_index(database, result, order_key, &column_index);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    compound_select_sort_result(result, column_index, descending);
    return MYLITE_OK;
}

static int compound_select_order_key_and_direction(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct mylite_sql_ast_node **out_order_key,
    bool *out_descending
) {
    const struct mylite_sql_ast_node *first_child = child_at(order_clause, 0U);

    *out_order_key = NULL;
    *out_descending = false;
    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        if (mylite_sql_ast_node_child_count(first_child) != 1U) {
            set_unsupported_error(database, "UNION ORDER BY supports only one output column");
            return MYLITE_ERROR;
        }
        const struct mylite_sql_ast_node *item = child_at(first_child, 0U);

        if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        *out_order_key = child_at(item, 0U);
        *out_descending = mylite_sql_ast_node_order_direction(child_at(item, 1U)) ==
                          MYLITE_SQL_AST_ORDER_DIRECTION_DESC;
        return MYLITE_OK;
    }

    *out_order_key = first_child;
    *out_descending = mylite_sql_ast_node_order_direction(child_at(order_clause, 1U)) ==
                      MYLITE_SQL_AST_ORDER_DIRECTION_DESC;
    return MYLITE_OK;
}

static int compound_select_order_column_index(
    struct mylite_db *database,
    const mylite_result *result,
    const struct mylite_sql_ast_node *order_key,
    size_t *out_column_index
) {
    char *order_name = NULL;
    bool found = false;
    int rc = MYLITE_OK;

    if (order_key == NULL || order_key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(database, "UNION ORDER BY supports only output column names");
        return MYLITE_ERROR;
    }

    rc = copy_select_item_identifier_alias_text(database, order_key, &order_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t index = 0U; index < mylite_result_column_count(result); ++index) {
        const char *column_name = mylite_result_column_name(result, index);

        if (column_name != NULL && text_equals_ascii_case_insensitive(column_name, order_name)) {
            if (found) {
                set_ambiguous_order_column_error(database, order_name);
                free(order_name);
                return MYLITE_ERROR;
            }
            *out_column_index = index;
            found = true;
        }
    }

    if (!found) {
        set_unknown_column_error(database, order_name);
        free(order_name);
        return MYLITE_ERROR;
    }

    free(order_name);
    return MYLITE_OK;
}

static void compound_select_sort_result(
    mylite_result *result,
    size_t column_index,
    bool descending
) {
    size_t row_count = mylite_result_row_count(result);

    for (size_t index = 1U; index < row_count; ++index) {
        size_t position = index;

        while (position > 0U && compound_select_compare_order_rows(
                                    result,
                                    position,
                                    position - 1U,
                                    column_index,
                                    descending
                                ) < 0) {
            compound_select_swap_result_rows(result, position, position - 1U);
            --position;
        }
    }
}

static int compound_select_compare_order_rows(
    const mylite_result *result,
    size_t left_row,
    size_t right_row,
    size_t column_index,
    bool descending
) {
    const void *left = mylite_result_value_bytes(result, left_row, column_index);
    const void *right = mylite_result_value_bytes(result, right_row, column_index);
    size_t left_size = mylite_result_value_size(result, left_row, column_index);
    size_t right_size = mylite_result_value_size(result, right_row, column_index);
    size_t common_size = left_size < right_size ? left_size : right_size;
    int comparison = 0;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        comparison = -1;
    } else if (right == NULL) {
        comparison = 1;
    } else {
        comparison = memcmp(left, right, common_size);
        if (comparison == 0) {
            if (left_size < right_size) {
                comparison = -1;
            } else if (left_size > right_size) {
                comparison = 1;
            }
        }
    }
    return descending ? -comparison : comparison;
}

static void compound_select_swap_result_rows(mylite_result *result, size_t left, size_t right) {
    size_t column_count = mylite_result_column_count(result);

    if (result == NULL || left == right) {
        return;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        size_t left_index = (left * column_count) + column_index;
        size_t right_index = (right * column_count) + column_index;
        char *value = result->values[left_index];
        size_t size = result->value_sizes[left_index];

        result->values[left_index] = result->values[right_index];
        result->value_sizes[left_index] = result->value_sizes[right_index];
        result->values[right_index] = value;
        result->value_sizes[right_index] = size;
    }
}

static int apply_compound_select_final_limit(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *limit_clause
) {
    struct planned_select_limit limit = {0};
    int rc = MYLITE_OK;

    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    rc = plan_select_limit(database, limit_clause, &limit);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (limit.has_offset && limit.offset > 0) {
        compound_select_discard_result_prefix(result, (uint64_t)limit.offset);
    }
    if (limit.has_limit && limit.row_count >= 0 &&
        (uint64_t)mylite_result_row_count(result) > (uint64_t)limit.row_count) {
        mylite_result_truncate_rows(result, (size_t)limit.row_count);
    }
    return MYLITE_OK;
}

static void compound_select_discard_result_prefix(mylite_result *result, uint64_t offset) {
    size_t column_count = 0U;
    size_t row_count = 0U;
    size_t removed_values = 0U;
    size_t kept_values = 0U;

    if (result == NULL || offset == 0U) {
        return;
    }
    row_count = mylite_result_row_count(result);
    if (offset >= (uint64_t)row_count) {
        mylite_result_truncate_rows(result, 0U);
        return;
    }

    column_count = mylite_result_column_count(result);
    removed_values = ((size_t)offset) * column_count;
    kept_values = (row_count - (size_t)offset) * column_count;
    for (size_t index = 0U; index < removed_values; ++index) {
        free(result->values[index]);
        result->values[index] = NULL;
        result->value_sizes[index] = 0U;
    }
    memmove(
        (void *)result->values,
        (const void *)(result->values + removed_values),
        kept_values * sizeof(*result->values)
    );
    memmove(
        result->value_sizes,
        result->value_sizes + removed_values,
        kept_values * sizeof(*result->value_sizes)
    );
    memset((void *)(result->values + kept_values), 0, removed_values * sizeof(*result->values));
    memset(result->value_sizes + kept_values, 0, removed_values * sizeof(*result->value_sizes));
    result->row_count = row_count - (size_t)offset;
}

static int reject_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    enum mylite_sql_ast_set_operator operator_kind,
    bool allow_final_order_limit
) {
    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, compound_select_non_select_branch_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(branch)) {
        set_unsupported_error(database, compound_select_calc_found_rows_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_options(branch) != 0U) {
        set_unsupported_error(database, compound_select_options_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (!allow_final_order_limit &&
        child_with_kind(branch, MYLITE_SQL_AST_ORDER_BY_CLAUSE) != NULL) {
        set_unsupported_error(database, compound_select_order_by_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (!allow_final_order_limit && child_with_kind(branch, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        set_unsupported_error(database, compound_select_limit_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_locking_clause(branch) !=
        MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_unsupported_error(database, compound_select_locking_clause_error(operator_kind));
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_compound_select_operator_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms,
    enum mylite_sql_ast_set_operator *out_operator_kind
) {
    const struct mylite_sql_ast_node *term = child_at(terms, 0U);
    enum mylite_sql_ast_set_operator first_operator = MYLITE_SQL_AST_SET_OPERATOR_UNION;

    if (term == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_operator = mylite_sql_ast_node_set_operator_kind(term);
    for (const struct mylite_sql_ast_node *current = term; current != NULL;
         current = current->next_sibling) {
        if (mylite_sql_ast_node_set_operator_kind(current) != first_operator) {
            set_unsupported_error(database, "mixed set operations are not supported");
            return MYLITE_ERROR;
        }
    }

    *out_operator_kind = first_operator;
    return MYLITE_OK;
}

static int append_compound_result_columns(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
) {
    size_t column_count = mylite_result_column_count(source);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const struct mylite_result_column *column =
            mylite_result_column_metadata_at(source, column_index);
        int rc = MYLITE_OK;

        if (column == NULL) {
            rc = mylite_result_append_column(
                target,
                mylite_result_column_name(source, column_index)
            );
        } else {
            const struct mylite_result_column_descriptor descriptor = {
                .label = column->label,
                .schema_name = column->schema_name,
                .table_name = column->table_name,
                .origin_schema_name = column->origin_schema_name,
                .origin_table_name = column->origin_table_name,
                .origin_column_name = column->origin_column_name,
                .logical_type = column->logical_type,
                .flags = column->flags,
                .charset_id = column->charset_id,
                .collation_id = column->collation_id,
                .display_length = column->display_length,
                .decimals = column->decimals,
                .nullable = column->nullable,
            };

            rc = mylite_result_append_column_descriptor(target, &descriptor);
        }
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    if (!distinct) {
        return append_all_compound_rows(database, *in_out_result, branch_result);
    }

    int rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_distinct_compound_rows(
        database,
        *in_out_result,
        branch_result,
        string_collation_columns
    );
}

static int apply_intersect_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    mylite_result *old_result = NULL;
    mylite_result *intersected = NULL;
    size_t row_count = 0U;
    int rc = MYLITE_OK;

    if (!distinct) {
        return apply_intersect_all_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            string_collation_columns
        );
    }

    rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }

    old_result = *in_out_result;
    row_count = mylite_result_row_count(old_result);
    rc = create_empty_compound_result_like(database, old_result, &intersected);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (compound_result_contains_row(
                branch_result,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, intersected, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(intersected);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = intersected;
    return MYLITE_OK;
}

static int apply_intersect_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *intersected = NULL;
    bool *matched_rows = NULL;
    size_t branch_row_count = mylite_result_row_count(branch_result);
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (branch_row_count == 0U) {
        rc = create_empty_compound_result_like(database, old_result, &intersected);
        if (rc != MYLITE_OK) {
            return rc;
        }
        mylite_result_free(old_result);
        *in_out_result = intersected;
        return MYLITE_OK;
    }

    matched_rows = calloc(branch_row_count, sizeof(*matched_rows));
    if (matched_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = create_empty_compound_result_like(database, old_result, &intersected);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        size_t branch_row_index = 0U;

        if (compound_result_find_unmatched_row(
                branch_result,
                old_result,
                row_index,
                matched_rows,
                &branch_row_index,
                string_collation_columns
            )) {
            matched_rows[branch_row_index] = true;
            rc = append_compound_row(database, intersected, old_result, row_index);
        }
    }
    free(matched_rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(intersected);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = intersected;
    return MYLITE_OK;
}

static int apply_except_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    mylite_result *old_result = NULL;
    mylite_result *difference = NULL;
    size_t row_count = 0U;
    int rc = MYLITE_OK;

    if (!distinct) {
        return apply_except_all_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            string_collation_columns
        );
    }

    rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }

    old_result = *in_out_result;
    row_count = mylite_result_row_count(old_result);
    rc = create_empty_compound_result_like(database, old_result, &difference);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(
                branch_result,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, difference, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(difference);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = difference;
    return MYLITE_OK;
}

static int apply_except_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *difference = NULL;
    bool *matched_rows = NULL;
    size_t branch_row_count = mylite_result_row_count(branch_result);
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (branch_row_count == 0U) {
        return MYLITE_OK;
    }

    matched_rows = calloc(branch_row_count, sizeof(*matched_rows));
    if (matched_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = create_empty_compound_result_like(database, old_result, &difference);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        size_t branch_row_index = 0U;

        if (compound_result_find_unmatched_row(
                branch_result,
                old_result,
                row_index,
                matched_rows,
                &branch_row_index,
                string_collation_columns
            )) {
            matched_rows[branch_row_index] = true;
        } else {
            rc = append_compound_row(database, difference, old_result, row_index);
        }
    }
    free(matched_rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(difference);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = difference;
    return MYLITE_OK;
}

static int create_empty_compound_result_like(
    struct mylite_db *database,
    const mylite_result *source,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_compound_result_columns(database, result, source);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    *out_result = result;
    return MYLITE_OK;
}

static int deduplicate_compound_result(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *deduplicated = NULL;
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (row_count <= 1U) {
        return MYLITE_OK;
    }

    rc = mylite_result_create(&deduplicated);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_compound_result_columns(database, deduplicated, old_result);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(
                deduplicated,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, deduplicated, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(deduplicated);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = deduplicated;
    return MYLITE_OK;
}

static int append_distinct_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(source);
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(target, source, row_index, string_collation_columns)) {
            rc = append_compound_row(database, target, source, row_index);
        }
    }

    return rc;
}

static int append_all_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
) {
    size_t row_count = mylite_result_row_count(source);
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        rc = append_compound_row(database, target, source, row_index);
    }

    return rc;
}

static int append_compound_row(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    size_t row_index
) {
    size_t column_count = mylite_result_column_count(source);
    struct mylite_result_cell *cells = calloc(column_count, sizeof(*cells));
    int rc = MYLITE_OK;

    if (cells == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const void *bytes = mylite_result_value_bytes(source, row_index, column_index);

        cells[column_index] = (struct mylite_result_cell){
            .bytes = bytes,
            .size = mylite_result_value_size(source, row_index, column_index),
            .is_null = bytes == NULL,
        };
    }

    rc = mylite_result_append_bytes_row(target, cells);
    free(cells);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int merge_compound_string_collation_columns(
    struct mylite_db *database,
    const mylite_result *branch_result,
    struct compound_row_comparison_columns *columns
) {
    (void)database;
    if (columns == NULL) {
        return MYLITE_ERROR;
    }
    if (columns->column_count == 0U) {
        return MYLITE_OK;
    }
    if (branch_result == NULL || columns->string_collation_columns == NULL ||
        columns->bytewise_collation_columns == NULL ||
        mylite_result_column_count(branch_result) != columns->column_count) {
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; column_index < columns->column_count; ++column_index) {
        if (compound_result_column_uses_bytewise_collation(branch_result, column_index)) {
            columns->bytewise_collation_columns[column_index] = true;
            columns->string_collation_columns[column_index] = false;
        } else if (!columns->bytewise_collation_columns[column_index] &&
                   compound_result_column_uses_string_collation(branch_result, column_index)) {
            columns->string_collation_columns[column_index] = true;
        }
    }

    return MYLITE_OK;
}

static bool compound_result_column_uses_string_collation(
    const mylite_result *result,
    size_t column_index
) {
    return result_column_uses_string_collation(
        mylite_result_column_metadata_at(result, column_index)
    );
}

static bool result_column_uses_string_collation(const struct mylite_result_column *column) {
    if (column == NULL) {
        return false;
    }
    if (result_column_uses_bytewise_collation(column) || column->collation_id == 0U) {
        return false;
    }

    switch (column->logical_type) {
    case MYLITE_RESULT_LOGICAL_TYPE_STRING:
    case MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING:
    case MYLITE_RESULT_LOGICAL_TYPE_BLOB:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_result_column_uses_bytewise_collation(
    const mylite_result *result,
    size_t column_index
) {
    return result_column_uses_bytewise_collation(
        mylite_result_column_metadata_at(result, column_index)
    );
}

static bool result_column_uses_bytewise_collation(const struct mylite_result_column *column) {
    if (column == NULL) {
        return false;
    }

    if ((column->flags & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U) {
        return true;
    }
    return column->collation_id == mysql_collation_binary_id;
}

static bool compound_statement_column_uses_string_collation(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;

    if (compound_select_item_uses_string_collation(child_at(statement, 0U), column_index)) {
        return true;
    }

    term = child_at(terms, 0U);
    while (term != NULL) {
        if (compound_select_item_uses_string_collation(child_at(term, 0U), column_index)) {
            return true;
        }
        term = term->next_sibling;
    }
    return false;
}

static bool compound_select_item_uses_string_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
) {
    const struct mylite_sql_ast_node *select_list = child_at(branch, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT || select_list == NULL ||
        select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    for (size_t index = 0U; select_item != NULL && index < column_index; ++index) {
        select_item = select_item->next_sibling;
    }
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }

    return compound_expression_uses_string_collation(child_at(select_item, 0U));
}

static bool compound_expression_uses_string_collation(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING;
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
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
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_statement_column_has_binary_expression(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;

    if (compound_select_item_uses_binary_collation(child_at(statement, 0U), column_index)) {
        return true;
    }

    term = child_at(terms, 0U);
    while (term != NULL) {
        if (compound_select_item_uses_binary_collation(child_at(term, 0U), column_index)) {
            return true;
        }
        term = term->next_sibling;
    }
    return false;
}

static bool compound_select_item_uses_binary_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
) {
    const struct mylite_sql_ast_node *select_list = child_at(branch, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT || select_list == NULL ||
        select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    for (size_t index = 0U; select_item != NULL && index < column_index; ++index) {
        select_item = select_item->next_sibling;
    }
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }

    return compound_expression_uses_binary_collation(child_at(select_item, 0U));
}

static bool compound_expression_uses_binary_collation(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_result_contains_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(target);

    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        if (compound_result_rows_equal(
                target,
                row_index,
                source,
                source_row_index,
                string_collation_columns
            )) {
            return true;
        }
    }

    return false;
}

static bool compound_result_find_unmatched_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *matched_rows,
    size_t *out_row_index,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(target);

    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        if (matched_rows != NULL && matched_rows[row_index]) {
            continue;
        }
        if (compound_result_rows_equal(
                target,
                row_index,
                source,
                source_row_index,
                string_collation_columns
            )) {
            *out_row_index = row_index;
            return true;
        }
    }

    return false;
}

static const char *compound_select_non_select_branch_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT supports only SELECT query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT supports only SELECT query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION supports only SELECT query blocks";
}

static const char *compound_select_calc_found_rows_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support SQL_CALC_FOUND_ROWS";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support SQL_CALC_FOUND_ROWS";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support SQL_CALC_FOUND_ROWS";
}

static const char *compound_select_options_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support SELECT options in query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support SELECT options in query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support SELECT options in query blocks";
}

static const char *compound_select_order_by_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT branch ORDER BY is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT branch ORDER BY is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION branch ORDER BY is not supported";
}

static const char *compound_select_limit_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT branch LIMIT is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT branch LIMIT is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION branch LIMIT is not supported";
}

static const char *compound_select_locking_clause_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support locking clauses";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support locking clauses";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support locking clauses";
}

static bool compound_result_rows_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    const bool *string_collation_columns
) {
    size_t column_count = mylite_result_column_count(left);

    if (column_count != mylite_result_column_count(right)) {
        return false;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (!compound_result_cells_equal(
                left,
                left_row_index,
                right,
                right_row_index,
                column_index,
                string_collation_columns
            )) {
            return false;
        }
    }

    return true;
}

static bool compound_result_cells_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    size_t column_index,
    const bool *string_collation_columns
) {
    const void *left_bytes = mylite_result_value_bytes(left, left_row_index, column_index);
    const void *right_bytes = mylite_result_value_bytes(right, right_row_index, column_index);
    size_t left_size = mylite_result_value_size(left, left_row_index, column_index);
    size_t right_size = mylite_result_value_size(right, right_row_index, column_index);

    if (left_bytes == NULL || right_bytes == NULL) {
        if (left_bytes == NULL && right_bytes == NULL) {
            return true;
        }
        return false;
    }
    if (left_size != right_size) {
        return false;
    }
    if (string_collation_columns != NULL && string_collation_columns[column_index]) {
        return compound_result_cells_equal_ascii_ci(left_bytes, left_size, right_bytes, right_size);
    }
    return memcmp(left_bytes, right_bytes, left_size) == 0;
}

static bool compound_result_cells_equal_ascii_ci(
    const void *left_bytes,
    size_t left_size,
    const void *right_bytes,
    size_t right_size
) {
    const unsigned char *left = left_bytes;
    const unsigned char *right = right_bytes;

    if (left_size != right_size) {
        return false;
    }
    for (size_t index = 0U; index < left_size; ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) {
            return false;
        }
    }
    return true;
}

static int execute_scalar_or_row_scalar_select_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
) {
    *out_handled = true;
    if (select_statement_has_group_by_clause(statement)) {
        *out_handled = false;
        return MYLITE_OK;
    }
    if (select_statement_is_row_function_scalar_projection(statement)) {
        return execute_scalar_projection_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_row_scalar_projection_attempt(statement)) {
        return execute_row_scalar_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_scalar_projection(statement)) {
        return execute_scalar_projection_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int reject_select_modifier_usage_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    bool has_descriptor_table_source = false;

    if (from_clause != NULL && (from_clause->kind == MYLITE_SQL_AST_FROM_TABLE ||
                                from_clause->kind == MYLITE_SQL_AST_FROM_JOIN)) {
        has_descriptor_table_source = true;
    }

    if (mylite_sql_ast_node_select_calc_found_rows(statement) && !has_descriptor_table_source) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS supports only descriptor-backed column-list and wildcard SELECT"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT &&
        !has_descriptor_table_source) {
        set_unsupported_error(
            database,
            "SELECT DISTINCT supports only descriptor-backed table reads"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int execute_information_schema_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_query query = {0};
    bool handled = false;
    int rc = execute_information_schema_join_compat_select_if_needed(
        database,
        context,
        statement,
        apply_sql_select_limit,
        out_result,
        &handled
    );

    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = resolve_information_schema_query(database, statement, &query);
    if (rc == MYLITE_OK) {
        rc = execute_information_schema_query(
            database,
            context,
            statement,
            &query,
            apply_sql_select_limit,
            out_result
        );
    }
    information_schema_query_deinit(&query);
    return rc;
}

static int select_statement_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    struct scalar_arithmetic_node_stack statement_stack = {0};
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (!scalar_arithmetic_node_stack_push(&statement_stack, statement)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && statement_stack.count != 0U && !*out_matches) {
        const struct mylite_sql_ast_node *current_statement =
            statement_stack.items[--statement_stack.count];
        const struct mylite_sql_ast_node *from_clause =
            select_statement_source_clause(current_statement);

        if (from_clause != NULL) {
            rc = select_source_targets_information_schema(
                database,
                from_clause,
                &statement_stack,
                out_matches
            );
        }
        if (rc == MYLITE_OK && !*out_matches) {
            rc = information_schema_join_compat_select_statement_is_supported(
                database,
                current_statement,
                out_matches
            );
        }
    }
    scalar_arithmetic_node_stack_deinit(&statement_stack);
    return rc;
}

static int select_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    struct scalar_arithmetic_node_stack *statement_stack,
    bool *out_matches
) {
    struct scalar_arithmetic_node_stack source_stack = {0};
    int rc = MYLITE_OK;

    if (source == NULL || statement_stack == NULL || out_matches == NULL) {
        return MYLITE_MISUSE;
    }

    *out_matches = false;
    if (!scalar_arithmetic_node_stack_push(&source_stack, source)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && source_stack.count != 0U && !*out_matches) {
        const struct mylite_sql_ast_node *current_source = source_stack.items[--source_stack.count];

        if (current_source->kind == MYLITE_SQL_AST_FROM_TABLE) {
            rc = table_source_targets_information_schema(database, current_source, out_matches);
        } else if (current_source->kind == MYLITE_SQL_AST_FROM_JOIN) {
            if (!scalar_arithmetic_node_stack_push(&source_stack, child_at(current_source, 1U)) ||
                !scalar_arithmetic_node_stack_push(&source_stack, child_at(current_source, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        } else if (current_source->kind == MYLITE_SQL_AST_FROM_DERIVED) {
            rc = derived_source_targets_information_schema(
                database,
                current_source,
                statement_stack,
                out_matches
            );
        }
    }
    scalar_arithmetic_node_stack_deinit(&source_stack);
    return rc;
}

static int table_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    bool *out_matches
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (source == NULL || source->kind != MYLITE_SQL_AST_FROM_TABLE || out_matches == NULL) {
        return MYLITE_MISUSE;
    }

    *out_matches = false;
    rc = collect_identifier_parts(
        child_at(source, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && schema_name_is_information_schema(parts[0])) {
        *out_matches = true;
        return MYLITE_OK;
    }
    if (part_count == 1U && selected_schema_is_information_schema(database)) {
        *out_matches = true;
    }

    return MYLITE_OK;
}

static int derived_source_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    struct scalar_arithmetic_node_stack *statement_stack,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *inner_statement = child_at(source, 0U);

    if (source == NULL || source->kind != MYLITE_SQL_AST_FROM_DERIVED || statement_stack == NULL ||
        out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (inner_statement == NULL || inner_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }

    if (!scalar_arithmetic_node_stack_push(statement_stack, inner_statement)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int execute_mysql_system_table_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_query query = {0};
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    int rc = resolve_mysql_system_table_query(database, statement, &query, &definition);

    if (rc == MYLITE_OK) {
        rc = execute_mysql_system_table_query(
            database,
            context,
            statement,
            &query,
            definition,
            apply_sql_select_limit,
            out_result
        );
    }
    information_schema_query_deinit(&query);
    return rc;
}

static int select_statement_targets_mysql_data_dictionary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_table_name == NULL || table_name_size == 0U || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    out_table_name[0] = '\0';
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_mysql_system_schema(database)) {
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else {
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name) ||
        !mysql_data_dictionary_table_is_hidden(table_name)) {
        return MYLITE_OK;
    }

    *out_matches = true;
    written = snprintf(out_table_name, table_name_size, "%s", table_name);
    if (written < 0 || (size_t)written >= table_name_size) {
        set_runtime_error(database, "failed to format mysql data dictionary table name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int select_statement_targets_absent_mysql_enterprise_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_table_name == NULL || table_name_size == 0U || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    out_table_name[0] = '\0';
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_mysql_system_schema(database)) {
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else {
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name) ||
        !mysql_enterprise_table_is_target_absent(table_name)) {
        return MYLITE_OK;
    }

    *out_matches = true;
    written = snprintf(out_table_name, table_name_size, "%s", table_name);
    if (written < 0 || (size_t)written >= table_name_size) {
        set_runtime_error(database, "failed to format absent mysql Enterprise table name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int select_statement_targets_mysql_system_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && find_mysql_system_table_definition(parts[0], parts[1]) != NULL) {
        *out_matches = true;
        return MYLITE_OK;
    }
    if (part_count == 1U && database != NULL && database->session.has_selected_schema &&
        find_mysql_system_table_definition(database->session.selected_schema, parts[0]) != NULL) {
        *out_matches = true;
    }

    return MYLITE_OK;
}

static int select_statement_targets_absent_builtin_schema_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_schema_name,
    size_t schema_name_size,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_execution_catalog_builtin_schema *schema = NULL;
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_schema_name == NULL || schema_name_size == 0U || out_table_name == NULL ||
        table_name_size == 0U || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    out_schema_name[0] = '\0';
    out_table_name[0] = '\0';
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else if (part_count == 1U && database != NULL && database->session.has_selected_schema) {
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else {
        return MYLITE_OK;
    }

    schema = mylite_execution_catalog_builtin_schema_by_name(schema_name);
    if (schema == NULL || schema_name_is_information_schema(schema->name)) {
        return MYLITE_OK;
    }
    if (find_mysql_system_table_definition(schema->name, table_name) != NULL) {
        return MYLITE_OK;
    }

    *out_matches = true;
    written = snprintf(out_schema_name, schema_name_size, "%s", schema->name);
    if (written < 0 || (size_t)written >= schema_name_size) {
        set_runtime_error(database, "failed to format absent built-in schema name");
        return MYLITE_ERROR;
    }
    written = snprintf(out_table_name, table_name_size, "%s", table_name);
    if (written < 0 || (size_t)written >= table_name_size) {
        set_runtime_error(database, "failed to format absent built-in table name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int resolve_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *optional_clause =
        select_statement_optional_clause_start(statement);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int rc = MYLITE_OK;

    if (out_definition == NULL) {
        return MYLITE_MISUSE;
    }
    *out_query = (struct information_schema_query){0};
    *out_definition = NULL;
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "mysql system table SELECT supports only non-distinct metadata queries"
        );
        return MYLITE_ERROR;
    }

    rc = mysql_system_table_resolve_source(database, from_clause, out_query, out_definition);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "mysql system table SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            rc = MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_where(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    *out_definition = NULL;
    return rc;
}

static int mysql_system_table_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "mysql system table SELECT requires a supported mysql table"
        );
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        definition = find_mysql_system_table_definition(parts[0], parts[1]);
    } else if (part_count == 1U && database != NULL && database->session.has_selected_schema) {
        definition =
            find_mysql_system_table_definition(database->session.selected_schema, parts[0]);
    }
    if (definition == NULL) {
        set_unsupported_error(
            database,
            "mysql system table SELECT requires a supported mysql table"
        );
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "mysql system table SELECT does not support index hints");
        return MYLITE_ERROR;
    }
    out_query->definition = &definition->query_definition;
    *out_definition = definition;
    if (from_table_alias_node(from_clause) != NULL) {
        rc = copy_identifier_text(
            from_table_alias_node(from_clause),
            out_query->alias,
            sizeof(out_query->alias),
            database
        );
        if (rc == MYLITE_OK) {
            out_query->has_alias = true;
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_mysql_system_table *find_mysql_system_table_definition(
    const char *schema_name,
    const char *table_name
) {
    return mylite_execution_catalog_mysql_system_table_definition_by_name(schema_name, table_name);
}

static bool mysql_data_dictionary_table_is_hidden(const char *table_name) {
    static const char *const hidden_table_names[] = {
        "catalogs",
        "character_sets",
        "check_constraints",
        "collations",
        "column_statistics",
        "column_type_elements",
        "columns",
        "dd_properties",
        "events",
        "foreign_keys",
        "foreign_key_column_usage",
        "index_column_usage",
        "index_partitions",
        "index_stats",
        "indexes",
        "innodb_ddl_log",
        "innodb_dynamic_metadata",
        "parameter_type_elements",
        "parameters",
        "resource_groups",
        "routines",
        "schemata",
        "st_spatial_reference_systems",
        "table_partition_values",
        "table_partitions",
        "table_stats",
        "tables",
        "tablespace_files",
        "tablespaces",
        "triggers",
        "view_routine_usage",
        "view_table_usage",
    };

    if (table_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(hidden_table_names) / sizeof(hidden_table_names[0]);
         ++index) {
        if (strcmp(table_name, hidden_table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool mysql_enterprise_table_is_target_absent(const char *table_name) {
    static const char *const absent_table_names[] = {
        "audit_log_filter",
        "audit_log_user",
        "firewall_group_allowlist",
        "firewall_groups",
        "firewall_membership",
        "firewall_users",
        "firewall_whitelist",
    };

    if (table_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(absent_table_names) / sizeof(absent_table_names[0]);
         ++index) {
        if (strcmp(table_name, absent_table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}
