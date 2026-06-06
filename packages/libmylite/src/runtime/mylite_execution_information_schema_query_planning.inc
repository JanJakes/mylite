
static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
) {
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        return information_schema_plan_wildcard_projection(database, out_query);
    }

    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    bool select_list_has_one_item = mylite_sql_ast_node_child_count(select_list) == 1U;

    while (item != NULL) {
        int rc = information_schema_plan_select_item_projection(
            database,
            select_list_has_one_item,
            item,
            out_query
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int information_schema_plan_wildcard_projection(
    struct mylite_db *database,
    struct information_schema_query *out_query
) {
    for (size_t column_index = 0U; column_index < out_query->definition->column_count;
         ++column_index) {
        int rc = information_schema_append_projection(database, out_query, column_index, NULL);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int information_schema_plan_select_item_projection(
    struct mylite_db *database,
    bool select_list_has_one_item,
    const struct mylite_sql_ast_node *item,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *expression =
        unwrap_parenthesized_expression(child_at(item, 0U));
    enum planned_count_function count_function = count_function_from_expression(expression);
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (count_function == PLANNED_COUNT_STAR) {
        if (!select_list_has_one_item) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports either COUNT(*) or metadata columns"
            );
            return MYLITE_ERROR;
        }
        out_query->is_count_star = true;
        out_query->count_expression = expression;
        out_query->count_alias = child_at(item, 1U);
        return MYLITE_OK;
    }
    if (count_function != PLANNED_COUNT_NONE || expression == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only metadata columns and COUNT(*)"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        rc = information_schema_resolve_column_reference(
            database,
            out_query,
            expression,
            COLUMN_REFERENCE_FIELD,
            &column_index
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_append_projection(
                database,
                out_query,
                column_index,
                child_at(item, 1U)
            );
        }
        return rc;
    }
    if (expression->kind == MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION) {
        return information_schema_append_expression_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return information_schema_append_integer_literal_projection(
            database,
            out_query,
            expression,
            child_at(item, 1U)
        );
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA SELECT supports only metadata columns, COUNT(*), integer literals, "
        "and CAST(numeric metadata expression AS UNSIGNED)"
    );
    return MYLITE_ERROR;
}

static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
) {
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = column_index,
            .kind = INFORMATION_SCHEMA_PROJECTION_COLUMN,
            .expression = NULL,
            .alias = alias,
        }
    );
}

static int information_schema_append_expression_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    int rc = information_schema_validate_unsigned_projection_expression(database, query, expression);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = 0U,
            .kind = INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_integer_literal_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    return information_schema_append_projection_slot(
        database,
        query,
        &(const struct information_schema_projection_slot_request){
            .column_index = 0U,
            .kind = INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL,
            .expression = expression,
            .alias = alias,
        }
    );
}

static int information_schema_append_projection_slot(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct information_schema_projection_slot_request *request
) {
    size_t required_count = query->projection_count + 1U;
    size_t *indexes = NULL;
    enum information_schema_projection_kind *kinds = NULL;
    const struct mylite_sql_ast_node **expressions = NULL;
    char **names = NULL;
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (required_count > SIZE_MAX / sizeof(*indexes) ||
        required_count > SIZE_MAX / sizeof(*kinds) ||
        required_count > SIZE_MAX / sizeof(*expressions) ||
        required_count > SIZE_MAX / sizeof(*names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(query->projection_indexes, required_count * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_indexes = indexes;

    kinds = realloc(query->projection_kinds, required_count * sizeof(*kinds));
    if (kinds == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_kinds = kinds;

    expressions = (const struct mylite_sql_ast_node **)realloc(
        (void *)query->projection_expressions,
        required_count * sizeof(*expressions)
    );
    if (expressions == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_expressions = expressions;

    names = (char **)realloc((void *)query->projection_names, required_count * sizeof(*names));
    if (names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_names = names;

    if (request->alias != NULL) {
        rc = copy_select_item_alias_text(database, request->alias, &column_name);
    } else if (request->expression != NULL) {
        rc = copy_source_span_text(database, &request->expression->span, &column_name);
    } else {
        rc = duplicate_text(
            database,
            query->definition->columns[request->column_index].name,
            &column_name
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    query->projection_indexes[query->projection_count] = request->column_index;
    query->projection_kinds[query->projection_count] = request->kind;
    query->projection_expressions[query->projection_count] = request->expression;
    query->projection_names[query->projection_count] = column_name;
    query->projection_count = required_count;
    return MYLITE_OK;
}

static int information_schema_validate_unsigned_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only CAST(... AS UNSIGNED) metadata expressions"
        );
        return MYLITE_ERROR;
    }
    return information_schema_validate_numeric_projection_expression(
        database,
        query,
        child_at(expression, 0U)
    );
}

static int information_schema_validate_numeric_projection_expression(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *unwrapped =
            unwrap_parenthesized_expression(stack.items[--stack.count]);

        if (unwrapped == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT numeric expression is invalid"
            );
            rc = MYLITE_ERROR;
        } else if (unwrapped->kind == MYLITE_SQL_AST_LITERAL &&
                   mylite_sql_ast_node_literal_kind(unwrapped) ==
                       MYLITE_SQL_AST_LITERAL_INTEGER) {
            rc = MYLITE_OK;
        } else if (unwrapped->kind == MYLITE_SQL_AST_IDENTIFIER ||
                   unwrapped->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
            size_t column_index = 0U;

            rc = information_schema_resolve_column_reference(
                database,
                query,
                unwrapped,
                COLUMN_REFERENCE_FIELD,
                &column_index
            );
        } else if (unwrapped->kind == MYLITE_SQL_AST_BINARY_EXPRESSION &&
                   mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(unwrapped, 1U)) ||
                !scalar_arithmetic_node_stack_push(&stack, child_at(unwrapped, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        } else {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports numeric metadata expressions with columns, "
                "integer literals, and /"
            );
            rc = MYLITE_ERROR;
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *direction = NULL;
    int rc = MYLITE_OK;

    out_query->has_order = false;
    out_query->order_direction = PLANNED_SELECT_ORDER_DEFAULT;
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports one ORDER BY column");
        return MYLITE_ERROR;
    }
    rc = information_schema_resolve_order_reference(
        database,
        out_query,
        child_at(order_clause, 0U),
        &out_query->order_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_query->has_order = true;
    out_query->order_direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_query->order_direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static int information_schema_resolve_order_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *order_reference,
    size_t *out_order_index
) {
    char reference_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (order_reference != NULL && order_reference->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_identifier_text(
            order_reference,
            reference_text,
            sizeof(reference_text),
            database
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        for (size_t projection_index = 0U; projection_index < query->projection_count;
             ++projection_index) {
            if (!text_equals_ascii_case_insensitive(
                    query->projection_names[projection_index],
                    reference_text
                )) {
                continue;
            }
            if (query->projection_kinds[projection_index] != INFORMATION_SCHEMA_PROJECTION_COLUMN) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT ORDER BY expression aliases are not supported"
                );
                return MYLITE_ERROR;
            }
            *out_order_index = query->projection_indexes[projection_index];
            return MYLITE_OK;
        }
    }
    return information_schema_resolve_column_reference(
        database,
        query,
        order_reference,
        COLUMN_REFERENCE_ORDER,
        out_order_index
    );
}

static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
) {
    out_query->limit = (struct planned_select_limit){
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE || child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    return plan_delete_limit(database, limit_clause, &out_query->limit);
}

static int information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *table_name = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
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
    if (part_count == 2U && schema_name_is_information_schema(parts[0])) {
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_information_schema(database)) {
        table_name = parts[0];
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
        );
        return MYLITE_ERROR;
    }

    out_query->definition = find_information_schema_table_definition(table_name);
    if (out_query->definition == NULL) {
        set_unknown_information_schema_table_error(database, table_name);
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT does not support index hints");
        return MYLITE_ERROR;
    }
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

static const struct mylite_execution_catalog_table_definition *find_information_schema_table_definition(
    const char *table_name
) {
    return mylite_execution_catalog_information_schema_table_definition_by_name(table_name);
}

static int information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
) {
    for (size_t column_index = 0U; column_index < definition->column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(
                definition->columns[column_index].name,
                column_name
            )) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}
