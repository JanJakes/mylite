static int plan_row_scalar_select_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct row_scalar_window_definitions *window_definitions,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_unsupported_error(database, "row-scalar SELECT supports only explicit select items");
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        if (!out_plan->has_source) {
            set_no_tables_used_error(database);
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, "row-scalar SELECT supports only explicit select items");
        return MYLITE_ERROR;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        int rc = append_row_scalar_select_item(
            database,
            out_plan,
            item,
            window_definitions,
            source_context,
            table,
            table_columns,
            table_column_count
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int append_row_scalar_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct mylite_sql_ast_node *select_item,
    struct row_scalar_window_definitions *window_definitions,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct planned_row_scalar_select_item item = {0};
    const struct mylite_sql_ast_node *source_expression = NULL;
    int rc = MYLITE_OK;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source_expression = child_at(select_item, 0U);
    const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);
    const struct mylite_sql_ast_node *unwrapped_expression =
        unwrap_parenthesized_expression(source_expression);

    if (source_expression != NULL && source_expression->kind == MYLITE_SQL_AST_WILDCARD) {
        if (alias != NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        return append_row_scalar_wildcard_select_items(
            database,
            plan,
            source_context,
            table_columns,
            table_column_count
        );
    }
    if (source_expression != NULL && source_expression->kind == MYLITE_SQL_AST_QUALIFIED_WILDCARD) {
        if (alias != NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        return append_row_scalar_qualified_wildcard_select_items(
            database,
            plan,
            source_expression,
            source_context,
            table_columns,
            table_column_count
        );
    }
    if (unwrapped_expression != NULL &&
        unwrapped_expression->kind == MYLITE_SQL_AST_EXISTS_PREDICATE) {
        return plan_row_scalar_exists_select_item(
            database,
            plan,
            &item,
            source_expression,
            alias,
            source_context,
            table_columns,
            table_column_count
        );
    }

    rc = plan_row_scalar_expression_with_window_definitions(
        database,
        source_expression,
        plan->has_source,
        source_context,
        table,
        table_columns,
        table_column_count,
        window_definitions,
        &item.expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(&item.expression);
        return rc;
    }

    return append_planned_row_scalar_select_item(database, plan, &item, source_expression, alias);
}

static int plan_row_scalar_exists_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    struct planned_row_scalar_select_item *item,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    const struct mylite_sql_ast_node *predicate = NULL;
    size_t outer_source_count = 0U;
    bool handled = false;
    int rc = MYLITE_OK;

    if (plan == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }

    predicate = unwrap_parenthesized_expression(source_expression);
    if (predicate == NULL || predicate->kind != MYLITE_SQL_AST_EXISTS_PREDICATE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (plan->has_source && source_context == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->has_source) {
        outer_source_count =
            select_source_context_is_joined(source_context) ? source_context->source_count : 1U;
    }

    rc = plan_row_scalar_information_schema_exists_select_item(
        database,
        plan,
        item,
        predicate,
        source_expression,
        alias,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }

    item->is_exists_expression = true;
    rc = plan_exists_subquery(
        database,
        child_at(predicate, 0U),
        &(const struct exists_subquery_outer_context){
            .source_context = plan->has_source ? source_context : NULL,
            .columns = table_columns,
            .column_count = table_column_count,
            .source_count = outer_source_count,
        },
        &item->exists_subquery
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_row_scalar_select_item(database, plan, item, source_expression, alias);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_item_deinit(item);
    }

    return rc;
}

static int plan_row_scalar_information_schema_exists_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    struct planned_row_scalar_select_item *item,
    const struct mylite_sql_ast_node *predicate,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node *alias,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    bool targets_information_schema = false;
    bool matches = false;
    int rc = MYLITE_OK;

    if (plan == NULL || item == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    if (predicate == NULL || predicate->kind != MYLITE_SQL_AST_EXISTS_PREDICATE) {
        return MYLITE_OK;
    }
    statement = child_at(predicate, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }
    from_clause = select_statement_source_clause(statement);
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc =
        table_source_targets_information_schema(database, from_clause, &targets_information_schema);
    if (rc != MYLITE_OK || !targets_information_schema) {
        return rc;
    }
    *out_handled = true;
    if (plan->has_source) {
        set_unsupported_error(
            database,
            "EXISTS information_schema scalar projections do not support outer table sources"
        );
        return MYLITE_ERROR;
    }

    rc = information_schema_exists_subquery_matches(database, statement, &matches);
    if (rc == MYLITE_OK) {
        item->expression.kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        item->expression.value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = matches ? 1 : 0,
        };
        rc = append_planned_row_scalar_select_item(database, plan, item, source_expression, alias);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_item_deinit(item);
    }
    return rc;
}

static int information_schema_exists_subquery_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    struct information_schema_query query = {0};
    struct information_schema_row_set rows = {0};
    struct information_schema_row_index_set indexes = {0};
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "EXISTS supports only SELECT subqueries");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "EXISTS subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    for (const struct mylite_sql_ast_node *optional_clause =
             select_statement_optional_clause_start(statement);
         optional_clause != NULL;
         optional_clause = optional_clause->next_sibling) {
        if (optional_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE &&
            optional_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
            set_unsupported_error(database, "EXISTS subqueries support only WHERE and LIMIT");
            return MYLITE_ERROR;
        }
    }

    rc = resolve_information_schema_query(database, statement, &query);
    if (rc == MYLITE_OK && query.definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        set_unsupported_error(
            database,
            "EXISTS information_schema.processlist scalar projections are not supported"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows(database, NULL, query.definition, &rows);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_matching_row_indexes(database, statement, &query, &rows, &indexes);
    }
    if (rc == MYLITE_OK) {
        *out_matches =
            indexes.count != 0U && !(query.limit.has_limit && query.limit.row_count == 0);
    }

    information_schema_row_index_set_deinit(&indexes);
    information_schema_row_set_deinit(&rows);
    information_schema_query_deinit(&query);
    return rc;
}

static int append_row_scalar_wildcard_select_items(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    if (!plan->has_source) {
        set_no_tables_used_error(database);
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        for (size_t source_index = 0U; source_index < source_context->source_count;
             ++source_index) {
            const struct planned_select_source *source = &source_context->sources[source_index];

            for (size_t column_index = 0U; column_index < source->column_count; ++column_index) {
                int rc = MYLITE_OK;

                if (!source->columns[column_index].is_visible) {
                    continue;
                }
                rc = append_row_scalar_column_select_item(
                    database,
                    plan,
                    &source->columns[column_index],
                    source_index
                );
                if (rc != MYLITE_OK) {
                    return rc;
                }
            }
        }
        return MYLITE_OK;
    }

    for (size_t column_index = 0U; column_index < table_column_count; ++column_index) {
        int rc = MYLITE_OK;

        if (!table_columns[column_index].is_visible) {
            continue;
        }
        rc = append_row_scalar_column_select_item(database, plan, &table_columns[column_index], 0U);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_row_scalar_qualified_wildcard_select_items(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_catalog_column_descriptor *columns = table_columns;
    size_t column_count = table_column_count;
    size_t source_index = 0U;
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            const struct planned_select_source *source = &source_context->sources[index];

            if (!joined_source_matches_qualified_wildcard_parts(
                    source,
                    (const char(*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
                    part_count
                )) {
                continue;
            }
            columns = source->columns;
            column_count = source->column_count;
            source_index = index;
            matched = true;
            break;
        }
    } else {
        matched = select_source_matches_qualified_wildcard_parts(
            source_context,
            (const char(*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }
    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (!columns[column_index].is_visible) {
            continue;
        }
        rc = append_row_scalar_column_select_item(
            database,
            plan,
            &columns[column_index],
            source_index
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_row_scalar_column_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index
) {
    struct planned_row_scalar_select_item item = {0};

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    item.expression.kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    if (retain_planned_row_scalar_column(database, &item.expression.column, column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    item.expression.column_has_source_index = plan->source_count != 0U;
    item.expression.column_source_index = source_index;

    return append_planned_row_scalar_select_item(database, plan, &item, NULL, NULL);
}

static int append_planned_row_scalar_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    struct planned_row_scalar_select_item *item,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node *alias
) {
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (plan == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }

    rc = finalize_planned_row_scalar_select_item(database, item, source_expression, alias);
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_item_deinit(item);
        return rc;
    }

    if (plan->item_count == SIZE_MAX) {
        planned_row_scalar_select_item_deinit(item);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    required_count = plan->item_count + 1U;
    rc = reserve_planned_row_scalar_select_items(database, plan, required_count);
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_item_deinit(item);
        return rc;
    }

    plan->items[plan->item_count] = *item;
    plan->item_count = required_count;

    return MYLITE_OK;
}

static int finalize_planned_row_scalar_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select_item *item,
    const struct mylite_sql_ast_node *source_expression,
    const struct mylite_sql_ast_node *alias
) {
    const struct mylite_sql_ast_node *unwrapped_expression =
        unwrap_parenthesized_expression(source_expression);
    struct mylite_result_column_descriptor descriptor = unknown_result_column_descriptor(NULL);
    char *label = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }
    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &label);
        if (rc == MYLITE_OK) {
            item->has_alias = true;
            item->alias_is_identifier = alias->kind == MYLITE_SQL_AST_IDENTIFIER;
        }
    } else if (unwrapped_expression != NULL &&
               unwrapped_expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        rc = copy_scalar_projection_column_name(database, source_expression, &label);
    } else if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        rc = duplicate_text(database, item->expression.column->name, &label);
    } else if (source_expression != NULL) {
        rc = copy_scalar_projection_column_name(database, source_expression, &label);
    } else {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(label);
        return rc;
    }
    item->result_label = label;

    if (source_expression == NULL ||
        item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN ||
        item->is_exists_expression) {
        return MYLITE_OK;
    }
    rc = populate_row_scalar_source_result_column_descriptor(
        database,
        source_expression,
        &descriptor
    );
    if (rc != MYLITE_OK || !row_scalar_descriptor_has_known_type(&descriptor)) {
        return rc;
    }
    item->source_result_shape = (struct planned_result_column_shape){
        .logical_type = descriptor.logical_type,
        .flags = descriptor.flags,
        .charset_id = descriptor.charset_id,
        .collation_id = descriptor.collation_id,
        .display_length = descriptor.display_length,
        .decimals = descriptor.decimals,
        .nullable = descriptor.nullable,
    };
    item->has_source_result_shape = true;
    return MYLITE_OK;
}

static int reserve_planned_row_scalar_select_items(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    size_t required_capacity
) {
    enum { initial_row_scalar_select_item_capacity = 4 };

    struct planned_row_scalar_select_item *items = NULL;
    size_t capacity = plan->item_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_row_scalar_select_item_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    items = realloc(plan->items, capacity * sizeof(*items));
    if (items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->items = items;
    plan->item_capacity = capacity;
    return MYLITE_OK;
}

static void planned_row_scalar_select_item_deinit(struct planned_row_scalar_select_item *item) {
    if (item == NULL) {
        return;
    }

    planned_row_scalar_expression_deinit(&item->expression);
    free(item->result_label);
    if (item->is_exists_expression) {
        planned_exists_subquery_deinit(&item->exists_subquery);
    }
    *item = (struct planned_row_scalar_select_item){0};
}
