static int plan_tableless_row_scalar_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (source_context != NULL || table_columns != NULL || table_column_count != 0U) {
        set_unsupported_error(
            database,
            "WHERE scalar subquery IS NULL is supported only for no-source SELECT filters"
        );
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        child_at(predicate_node, 0U),
        false,
        NULL,
        NULL,
        NULL,
        0U,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };

    if (predicate_node_is_find_in_set_expression(child_at(predicate_node, 0U))) {
        return plan_find_in_set_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(child_at(predicate_node, 0U))) {
        return plan_regexp_like_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(child_at(predicate_node, 0U))) {
        return plan_json_valid_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(child_at(predicate_node, 0U))) {
        return plan_json_contains_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(child_at(predicate_node, 0U))) {
        return plan_string_length_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(child_at(predicate_node, 0U))) {
        return plan_substring_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(child_at(predicate_node, 0U))) {
        return plan_temporal_extract_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }
    if (child_at(predicate_node, 0U) != NULL &&
        child_at(predicate_node, 0U)->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return plan_tableless_row_scalar_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_BOOLEAN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (rc == MYLITE_OK) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &node.column,
            "WHERE supports only integer boolean predicate columns",
            &range
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_BETWEEN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_between_values(
                database,
                child_at(predicate_node, 1U),
                child_at(predicate_node, 2U),
                &node.column,
                &node.value,
                &node.upper_value,
                &node.compare_date_as_datetime
            );
        } else {
            rc = convert_predicate_value(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value
            );
            if (rc == MYLITE_OK) {
                rc = convert_predicate_value(
                    database,
                    child_at(predicate_node, 2U),
                    &node.column,
                    &node.upper_value
                );
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    right = child_at(predicate_node, 1U);
    if (right == NULL || right->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (options == NULL || !options->allow_in_subquery) {
            set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
            return MYLITE_ERROR;
        }
        if (source_context == NULL || select_source_context_is_joined(source_context)) {
            set_unsupported_error(database, "IN subqueries support only one outer table source");
            return MYLITE_ERROR;
        }
        rc = plan_in_subquery_predicate(
            database,
            right,
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }
    predicate->qualify_column_references = true;

    return rc;
}

static int plan_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    if (child_at(predicate_node, 1U) != NULL &&
        child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count,
                &node.compare_date_as_datetime
            );
        } else {
            rc = convert_predicate_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        for (size_t value_index = 0U; value_index < node.value_count; ++value_index) {
            planned_value_deinit(&node.values[value_index]);
        }
        free(node.values);
        return rc;
    }

    return rc;
}

static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_select_predicate_node *node
) {
    struct planned_in_subquery *subquery = NULL;
    int rc = MYLITE_OK;

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_in_subquery(
        database,
        select_statement,
        outer_source_context,
        outer_columns,
        outer_column_count,
        subquery
    );
    if (rc == MYLITE_OK && !in_subquery_columns_are_compatible(&node->column, &subquery->column)) {
        set_unsupported_error(
            database,
            "IN subqueries support same-family integer or string descriptor columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node->in_subquery = subquery;
    return MYLITE_OK;
}

static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_in_subquery){0};
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries support only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "IN subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    for (const struct mylite_sql_ast_node *clause = optional_clause; clause != NULL;
         clause = clause->next_sibling) {
        if (clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            set_in_subquery_limit_error(database);
            return MYLITE_ERROR;
        }
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "IN subqueries support only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    rc = plan_in_subquery_table_source(database, from_clause, out_subquery);
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_select_column(
            database,
            select_list,
            out_subquery,
            &out_subquery->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_inner_predicate(
            database,
            where_clause,
            outer_source_context,
            outer_columns,
            outer_column_count,
            out_subquery
        );
    }

    return rc;
}

static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
) {
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "IN subqueries support one descriptor table source");
        return MYLITE_ERROR;
    }

    return plan_joined_select_source(database, from_clause, &out_subquery->source);
}

static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct select_source_context source_context = {0};

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expression = child_at(item, 0U);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        set_unsupported_error(
            database,
            "IN subqueries support one explicit descriptor select column"
        );
        return MYLITE_ERROR;
    }

    source_context = (struct select_source_context){
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    return resolve_descriptor_column_reference(
        database,
        expression,
        &source_context,
        COLUMN_REFERENCE_FIELD,
        "IN subqueries support one explicit descriptor select column",
        subquery->source.columns,
        subquery->source.column_count,
        out_column
    );
}

static bool in_subquery_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *outer_column,
    const struct mylite_catalog_column_descriptor *inner_column
) {
    return join_condition_columns_are_compatible(outer_column, inner_column);
}

static int plan_in_subquery_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *subquery
) {
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };
    int rc = MYLITE_OK;

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    subquery->predicate = (struct planned_select_predicate){0};
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node_without_exists(
        database,
        child_at(where_clause, 0U),
        &inner_source_context,
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }
    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
    }
    return rc;
}

static int convert_predicate_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        value_count == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_year(column)) {
        return convert_predicate_year_in_value_list(
            database,
            value_list,
            column,
            out_values,
            out_value_count
        );
    }
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = convert_predicate_in_value(
            database,
            child_at(value_list, value_index),
            column,
            &values[value_index]
        );
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_predicate_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_value(database, value_node, column, out_value);
}

static int convert_predicate_year_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    bool uses_string_conversion =
        year_in_value_list_uses_string_conversion(value_list, value_count);
    int rc = MYLITE_OK;

    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            values[value_index] = (struct planned_value){.is_null = true, .integer = 0};
        } else if (uses_string_conversion) {
            rc = convert_predicate_year_literal(database, value_node, column, &values[value_index]);
        } else {
            rc = convert_year_direct_value(database, value_node, &values[value_index]);
        }
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static bool year_in_value_list_uses_string_conversion(
    const struct mylite_sql_ast_node *value_list,
    size_t value_count
) {
    if (value_count == 1U) {
        return true;
    }
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(child_at(value_list, value_index));

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            continue;
        }
        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
            return false;
        }
    }
    return true;
}
