static int init_select_source_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct table_name_resolution *source,
    struct select_source_context *out_context
) {
    const struct mylite_sql_ast_node *alias = from_table_alias_node(from_clause);

    *out_context = (struct select_source_context){.source = source};
    if (alias == NULL) {
        return MYLITE_OK;
    }
    if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    int rc = copy_identifier_text(alias, out_context->alias, sizeof(out_context->alias), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_context->has_alias = true;

    return MYLITE_OK;
}

static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    struct select_optional_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    struct planned_select_predicate having_predicate = {0};
    struct select_predicate_plan_options predicate_options = {
        .allow_exists = true,
        .allow_in_subquery = true,
        .allow_date_format_numeric_predicate = true,
    };
    bool allow_field_order = allow_order_by_field;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_select){0};
    out_plan->is_distinct =
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
    if (out_plan->is_distinct) {
        allow_field_order = false;
    }
    out_plan->calc_found_rows = mylite_sql_ast_node_select_calc_found_rows(statement) != 0;
    if (out_plan->calc_found_rows && out_plan->is_distinct) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS supports only non-distinct descriptor-backed table SELECT"
        );
        return MYLITE_ERROR;
    }
    if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        const struct joined_select_ast joined_select = {
            .statement = statement,
            .from_clause = from_clause,
        };
        return plan_joined_select(database, &joined_select, out_plan);
    }
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "SELECT supports only descriptor-backed table reads");
        return MYLITE_ERROR;
    }
    rc = collect_select_optional_clauses(database, statement, &clauses);

    if (rc == MYLITE_OK) {
        rc = resolve_visible_table_reference(
            database,
            child_at(from_clause, 0U),
            &out_plan->source,
            &out_plan->table
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    copy_plan_source_alias_if_present(out_plan, &source_context);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_projection(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate_with_options(
            database,
            clauses.where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &predicate_options,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK && clauses.having_clause != NULL) {
        rc = plan_select_having_predicate_with_options(
            database,
            clauses.having_clause,
            &source_context,
            table_columns,
            table_column_count,
            &predicate_options,
            &having_predicate
        );
    }
    if (rc == MYLITE_OK && clauses.having_clause != NULL) {
        rc = merge_select_predicate_with_and(database, &out_plan->predicate, &having_predicate);
    }
    planned_select_predicate_deinit(&having_predicate);
    if (rc == MYLITE_OK && out_plan->predicate.qualify_column_references) {
        out_plan->requires_source_alias = true;
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses.order_clause,
            &source_context,
            out_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            true,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK && out_plan->requires_source_alias) {
        out_plan->order.qualify_column_reference = true;
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_distinct_order(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_select_deinit(out_plan);
    }

    return rc;
}

static int validate_select_distinct_order(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    if (plan == NULL || !plan->is_distinct || !plan->order.has_order) {
        return MYLITE_OK;
    }
    if (!session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY)) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; item_index < plan->order.item_count; ++item_index) {
        const struct planned_select_order_item *item = &plan->order.items[item_index];

        if (item->kind != PLANNED_SELECT_ORDER_ITEM_COLUMN ||
            !select_distinct_order_item_is_selected(plan, item)) {
            set_unsupported_error(
                database,
                "SELECT DISTINCT supports ORDER BY only on selected columns"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool select_distinct_order_item_is_selected(
    const struct planned_select *plan,
    const struct planned_select_order_item *item
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (item->column_source_index == plan->column_source_indexes[column_index] &&
            item->column.column_id == plan->columns[column_index].column_id) {
            return true;
        }
    }

    return false;
}

static int collect_select_optional_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    *out_clauses = (struct select_optional_clauses){0};
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            out_clauses->where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_HAVING_CLAUSE) {
            out_clauses->having_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            out_clauses->order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            out_clauses->limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "SELECT supports only WHERE, HAVING, ORDER BY, and LIMIT"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    int rc = plan_select_columns(
        database,
        select_list,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );

    if (rc == MYLITE_OK && out_plan->is_distinct) {
        rc = validate_select_distinct_columns(database, out_plan);
    }

    return rc;
}

static int validate_select_distinct_columns(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (!select_distinct_column_is_supported(&plan->columns[column_index])) {
            set_unsupported_error(
                database,
                "SELECT DISTINCT supports only integer, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or "
                "nonbinary string descriptor columns"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool select_distinct_column_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }

    return integer_range_for_column(NULL, column, "", &range) == MYLITE_OK;
}

static void copy_plan_source_alias_if_present(
    struct planned_select *plan,
    const struct select_source_context *context
) {
    if (plan == NULL || context == NULL || !context->has_alias) {
        return;
    }

    memcpy(plan->source_alias, context->alias, sizeof(plan->source_alias));
    plan->source_has_alias = true;
}

static int plan_joined_select(
    struct mylite_db *database,
    const struct joined_select_ast *joined_select,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    struct joined_select_temp_nodes temp_nodes = {0};
    struct select_optional_clauses clauses = {0};
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    if (joined_select == NULL || joined_select->statement == NULL ||
        joined_select->from_clause == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    select_list = child_at(joined_select->statement, 0U);
    rc = collect_select_optional_clauses(database, joined_select->statement, &clauses);
    if (rc == MYLITE_OK) {
        rc =
            prepare_joined_select_plan(database, joined_select->from_clause, out_plan, &temp_nodes);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_source_join_edges(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_joined_select_order_ambiguity_for_literal_projection(
            database,
            select_list,
            clauses.order_clause,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_joined_select_having(database, select_list, clauses.having_clause);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_conditions(
            database,
            temp_nodes.join_condition_nodes,
            temp_nodes.join_condition_node_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->join_condition = out_plan->join_conditions[0];
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_columns(database, select_list, &source_context, NULL, 0U, out_plan);
    }
    if (rc == MYLITE_OK && out_plan->is_distinct) {
        rc = validate_select_distinct_columns(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_predicate(
            database,
            clauses.where_clause,
            &source_context,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses.order_clause,
            &source_context,
            out_plan,
            NULL,
            0U,
            false,
            false,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    if (rc != MYLITE_OK) {
        planned_select_deinit(out_plan);
    }
    joined_select_temp_nodes_deinit(&temp_nodes);
    return rc;
}

static int reject_joined_select_order_ambiguity_for_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
) {
    if (order_clause == NULL || !select_list_has_only_literal_projection_items(select_list)) {
        return MYLITE_OK;
    }

    return reject_ambiguous_joined_order_references(database, order_clause, source_context);
}

static int reject_ambiguous_joined_order_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *item = NULL;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
        return MYLITE_ERROR;
    }

    order_items = child_at(order_clause, 0U);
    if (order_items != NULL && order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        item = child_at(order_items, 0U);
        while (item != NULL) {
            int rc = MYLITE_OK;

            if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
                set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
                return MYLITE_ERROR;
            }
            rc = reject_ambiguous_joined_order_reference(
                database,
                child_at(item, 0U),
                source_context
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
        }
        return MYLITE_OK;
    }

    return reject_ambiguous_joined_order_reference(
        database,
        child_at(order_clause, 0U),
        source_context
    );
}

static int reject_ambiguous_joined_order_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_sql_ast_node *key = unwrap_parenthesized_expression(order_key);
    size_t match_count = 0U;
    int rc = MYLITE_OK;

    if (key == NULL || key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(key, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    match_count = joined_source_unqualified_column_match_count(source_context, column_name);
    if (match_count > 1U) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_ORDER, column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_unsupported_joined_select_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *having_clause
) {
    int rc = MYLITE_OK;

    if (having_clause == NULL) {
        return MYLITE_OK;
    }
    if (having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE) {
        set_unsupported_error(database, "joined SELECT does not yet support HAVING");
        return MYLITE_ERROR;
    }

    rc = reject_joined_select_having_reference_error(
        database,
        (struct joined_select_having_reference_request){
            .select_list = select_list,
            .having_node = child_at(having_clause, 0U),
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    set_unsupported_error(database, "joined SELECT does not yet support HAVING");
    return MYLITE_ERROR;
}

static int reject_joined_select_having_reference_error(
    struct mylite_db *database,
    struct joined_select_having_reference_request request
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_sql_ast_node *reference =
        joined_select_having_reference_node(request.having_node);
    size_t match_count = 0U;
    int rc = MYLITE_OK;

    if (reference == NULL || reference->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(reference, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc =
            count_select_list_output_name_matches(
                database,
                request.select_list,
                column_name,
                &match_count
            );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (match_count == 0U) {
        set_unknown_having_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (match_count > 1U) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_HAVING, column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *joined_select_having_reference_node(
    const struct mylite_sql_ast_node *having_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(having_node);

    if (node == NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_IDENTIFIER ||
        node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return node;
    }
    if (node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
        node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        return unwrap_parenthesized_expression(child_at(node, 0U));
    }

    return NULL;
}

static int count_select_list_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const char *name,
    size_t *out_match_count
) {
    const struct mylite_sql_ast_node *item = NULL;

    *out_match_count = 0U;
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST || name == NULL) {
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        bool matches = false;
        int rc = select_item_output_name_matches(database, item, name, &matches);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matches) {
            ++(*out_match_count);
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int select_item_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    const char *name,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *alias = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM || name == NULL) {
        return MYLITE_OK;
    }

    alias = child_at(select_item, 1U);
    if (alias != NULL) {
        char *alias_text = NULL;

        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc == MYLITE_OK) {
            *out_matches = text_equals_ascii_case_insensitive(alias_text, name);
        }
        free(alias_text);
        return rc;
    }

    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
                               expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_OK;
    }

    rc = collect_column_reference_parts(database, expression, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count > 0U) {
        *out_matches = text_equals_ascii_case_insensitive(parts[part_count - 1U], name);
    }

    return MYLITE_OK;
}

static size_t joined_source_unqualified_column_match_count(
    const struct select_source_context *source_context,
    const char *column_name
) {
    size_t match_count = 0U;

    if (source_context == NULL || column_name == NULL) {
        return 0U;
    }

    for (size_t source_index = 0U; source_index < source_context->source_count; ++source_index) {
        const struct planned_select_source *source = &source_context->sources[source_index];
        size_t column_index = 0U;

        if (find_column_index(source->columns, source->column_count, column_name, &column_index) ==
            MYLITE_OK) {
            ++match_count;
        }
    }

    return match_count;
}

static bool select_list_has_only_literal_projection_items(
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *item = NULL;
    bool saw_item = false;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        if (!select_item_is_literal_projection(item)) {
            return false;
        }
        saw_item = true;
        item = item->next_sibling;
    }

    return saw_item;
}

static bool select_item_is_literal_projection(const struct mylite_sql_ast_node *select_item) {
    const struct mylite_sql_ast_node *expression = NULL;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }

    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    return expression != NULL && expression->kind == MYLITE_SQL_AST_LITERAL;
}

static int prepare_joined_select_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    struct joined_select_temp_nodes *temp_nodes
) {
    int rc = MYLITE_OK;

    if (temp_nodes == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->source_count = joined_select_source_count(from_clause);
    if (out_plan->source_count < 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->join_count = out_plan->source_count - 1U;
    temp_nodes->source_node_count = out_plan->source_count;
    temp_nodes->join_condition_node_count = out_plan->join_count;
    out_plan->sources = calloc(out_plan->source_count, sizeof(*out_plan->sources));
    out_plan->join_kinds = calloc(out_plan->join_count, sizeof(*out_plan->join_kinds));
    out_plan->join_conditions = calloc(out_plan->join_count, sizeof(*out_plan->join_conditions));
    temp_nodes->source_nodes = (const struct mylite_sql_ast_node **)
        calloc(out_plan->source_count, sizeof(*temp_nodes->source_nodes));
    temp_nodes->join_condition_nodes = (const struct mylite_sql_ast_node **)
        calloc(out_plan->join_count, sizeof(*temp_nodes->join_condition_nodes));

    if (out_plan->sources == NULL || out_plan->join_kinds == NULL ||
        out_plan->join_conditions == NULL || temp_nodes->source_nodes == NULL ||
        temp_nodes->join_condition_nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = collect_joined_select_parts(database, from_clause, out_plan, temp_nodes);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_sources(database, temp_nodes->source_nodes, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->join_kind = out_plan->join_kinds[0];
    }

    return rc;
}

static void joined_select_temp_nodes_deinit(struct joined_select_temp_nodes *temp_nodes) {
    if (temp_nodes == NULL) {
        return;
    }

    free((void *)temp_nodes->source_nodes);
    free((void *)temp_nodes->join_condition_nodes);
    *temp_nodes = (struct joined_select_temp_nodes){0};
}

static size_t joined_select_source_count(const struct mylite_sql_ast_node *from_clause) {
    const struct mylite_sql_ast_node *node = from_clause;
    size_t source_count = 0U;

    while (node != NULL && node->kind == MYLITE_SQL_AST_FROM_JOIN) {
        ++source_count;
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return 0U;
    }
    return source_count + 1U;
}

static int collect_joined_select_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    const struct joined_select_temp_nodes *temp_nodes
) {
    const struct mylite_sql_ast_node *node = from_clause;
    size_t join_index = 0U;

    if (node == NULL || out_plan == NULL || temp_nodes == NULL ||
        temp_nodes->source_nodes == NULL || temp_nodes->join_condition_nodes == NULL ||
        temp_nodes->source_node_count != out_plan->source_count ||
        temp_nodes->join_condition_node_count != out_plan->join_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    join_index = out_plan->join_count;
    while (node != NULL && node->kind == MYLITE_SQL_AST_FROM_JOIN) {
        const struct mylite_sql_ast_node *right_source = child_at(node, 1U);

        if (join_index == 0U || right_source == NULL ||
            right_source->kind != MYLITE_SQL_AST_FROM_TABLE) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        --join_index;
        if (join_index + 1U >= temp_nodes->source_node_count ||
            join_index >= temp_nodes->join_condition_node_count) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        temp_nodes->source_nodes[join_index + 1U] = right_source;
        out_plan->join_kinds[join_index] = mylite_sql_ast_node_join_kind(node);
        temp_nodes->join_condition_nodes[join_index] = child_at(node, 2U);
        node = child_at(node, 0U);
    }

    if (join_index != 0U || node == NULL || node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    temp_nodes->source_nodes[0] = node;
    return MYLITE_OK;
}

static int plan_joined_select_sources(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *source_nodes,
    struct planned_select *out_plan
) {
    if (source_nodes == NULL || out_plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t source_index = 0U; source_index < out_plan->source_count; ++source_index) {
        if (source_nodes[source_index] == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        const int rc = plan_joined_select_source(
            database,
            source_nodes[source_index],
            &out_plan->sources[source_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int reject_unsupported_multi_source_join_edges(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (plan->source_count <= 2U) {
        return MYLITE_OK;
    }

    for (size_t join_index = 0U; join_index < plan->join_count; ++join_index) {
        if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            set_unsupported_error(
                database,
                "multi-source SELECT does not support RIGHT JOIN chains"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int plan_joined_select_conditions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *join_condition_nodes,
    size_t join_condition_node_count,
    struct planned_select *out_plan
) {
    if (join_condition_nodes == NULL || out_plan == NULL ||
        join_condition_node_count != out_plan->join_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t join_index = 0U; join_index < join_condition_node_count; ++join_index) {
        struct select_source_context edge_source_context = {0};
        int rc = init_join_select_source_context(
            out_plan->sources,
            join_index + 2U,
            &edge_source_context
        );

        if (rc == MYLITE_OK) {
            rc = plan_joined_select_condition(
                database,
                join_condition_nodes[join_index],
                out_plan->join_kinds[join_index],
                &edge_source_context,
                &out_plan->join_conditions[join_index]
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_joined_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
) {
    *out_source = (struct planned_select_source){0};
    return plan_joined_select_table_source(database, source_node, out_source);
}

static int plan_joined_select_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
) {
    const struct mylite_sql_ast_node *alias = NULL;
    int rc = MYLITE_OK;

    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(source_node, 0U),
        &out_source->source,
        &out_source->table
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = validate_select_index_hints(database, source_node, &out_source->table);
    if (rc != MYLITE_OK) {
        return rc;
    }

    alias = from_table_alias_node(source_node);
    if (alias != NULL) {
        if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(alias, out_source->alias, sizeof(out_source->alias), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_source->has_alias = true;
    }

    return mylite_execution_load_table_columns(
        database,
        out_source->table.table_id,
        &out_source->columns,
        &out_source->column_count
    );
}

static int plan_joined_select_derived_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
) {
    const struct mylite_sql_ast_node *select_statement = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    int rc = MYLITE_OK;

    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_FROM_DERIVED) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    select_statement = child_at(source_node, 0U);
    alias = child_at(source_node, 1U);
    if (select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (alias == NULL || alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(database, "joined source derived tables require an alias");
        return MYLITE_ERROR;
    }

    out_source->derived_select = calloc(1U, sizeof(*out_source->derived_select));
    if (out_source->derived_select == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = copy_identifier_text(alias, out_source->alias, sizeof(out_source->alias), database);
    if (rc == MYLITE_OK) {
        out_source->has_alias = true;
        out_source->is_derived = true;
        rc = plan_select(database, select_statement, false, out_source->derived_select);
    }
    if (rc == MYLITE_OK) {
        rc = validate_derived_select_output_columns(database, out_source->derived_select);
    }
    if (rc == MYLITE_OK) {
        rc = copy_derived_select_source_columns(database, out_source->derived_select, out_source);
    }
    if (rc != MYLITE_OK) {
        planned_select_source_deinit(out_source);
    }
    return rc;
}

static int validate_derived_select_output_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan
) {
    if (derived_plan == NULL || derived_plan->column_count == 0U) {
        set_unsupported_error(database, "joined source derived tables require descriptor columns");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < derived_plan->column_count; ++column_index) {
        if (derived_plan->column_aliases != NULL &&
            derived_plan->column_aliases[column_index] != NULL) {
            set_unsupported_error(
                database,
                "joined source derived tables do not yet support output column aliases"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int copy_derived_select_source_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan,
    struct planned_select_source *out_source
) {
    if (derived_plan == NULL || out_source == NULL) {
        return MYLITE_MISUSE;
    }
    if (derived_plan->column_count > SIZE_MAX / sizeof(*out_source->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_source->columns = calloc(derived_plan->column_count, sizeof(*out_source->columns));
    if (out_source->columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(
        out_source->columns,
        derived_plan->columns,
        derived_plan->column_count * sizeof(*out_source->columns)
    );
    out_source->column_count = derived_plan->column_count;
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_INDEX_HINT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int validate_select_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_table,
    const struct mylite_catalog_table_descriptor *table
) {
    const struct mylite_sql_ast_node *hint_list = from_table_index_hint_list_node(from_table);
    bool has_use = false;
    bool has_force = false;

    if (hint_list == NULL) {
        return MYLITE_OK;
    }
    if (table == NULL || hint_list->kind != MYLITE_SQL_AST_INDEX_HINT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (const struct mylite_sql_ast_node *hint = child_at(hint_list, 0U); hint != NULL;
         hint = hint->next_sibling) {
        int rc = validate_select_index_hint(database, hint, table, &has_use, &has_force);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_select_index_hint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *hint,
    const struct mylite_catalog_table_descriptor *table,
    bool *has_use,
    bool *has_force
) {
    const struct mylite_sql_ast_node *name_list = NULL;

    if (hint == NULL || table == NULL || has_use == NULL || has_force == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (hint->kind == MYLITE_SQL_AST_USE_INDEX_HINT) {
        *has_use = true;
    } else if (hint->kind == MYLITE_SQL_AST_FORCE_INDEX_HINT) {
        *has_force = true;
    } else if (hint->kind != MYLITE_SQL_AST_IGNORE_INDEX_HINT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*has_use && *has_force) {
        set_index_hint_use_force_error(database);
        return MYLITE_ERROR;
    }

    name_list = index_hint_name_list_node(hint);
    if (name_list == NULL || name_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (hint->kind != MYLITE_SQL_AST_USE_INDEX_HINT && child_at(name_list, 0U) == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return validate_select_index_hint_names(database, name_list, table);
}

static const struct mylite_sql_ast_node *index_hint_name_list_node(
    const struct mylite_sql_ast_node *hint
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (hint == NULL) {
        return NULL;
    }
    child = child_at(hint, 0U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER_LIST) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int validate_select_index_hint_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_list,
    const struct mylite_catalog_table_descriptor *table
) {
    char first_missing_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_name = false;
    bool has_existing_name = false;

    for (const struct mylite_sql_ast_node *name_node = child_at(name_list, 0U); name_node != NULL;
         name_node = name_node->next_sibling) {
        char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        bool exists = false;
        int rc = copy_identifier_text(name_node, index_name, sizeof(index_name), database);

        has_name = true;
        if (rc == MYLITE_OK) {
            rc = select_index_hint_name_exists(database, table->table_id, index_name, &exists);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (exists) {
            has_existing_name = true;
        } else if (first_missing_name[0] == '\0') {
            int copied = snprintf(first_missing_name, sizeof(first_missing_name), "%s", index_name);

            if (copied < 0 || (size_t)copied >= sizeof(first_missing_name)) {
                set_identifier_too_long_error(database, "index");
                return MYLITE_ERROR;
            }
        }
    }
    if (has_name && !has_existing_name) {
        set_key_does_not_exist_in_table_error(database, first_missing_name, table->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int select_index_hint_name_exists(
    struct mylite_db *database,
    int64_t table_id,
    const char *index_name,
    bool *out_exists
) {
    struct select_index_hint_lookup_context context = {
        .name = index_name,
        .exact_match = false,
        .prefix_match_count = 0U,
    };
    int rc = MYLITE_OK;

    if (out_exists == NULL) {
        return MYLITE_MISUSE;
    }
    *out_exists = false;
    if (table_id < 0) {
        rc = mylite_temporary_catalog_for_each_index_in_table(
            &database->session.temporary_catalog,
            table_id,
            match_select_index_hint_name,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_index_in_table(
            database,
            table_id,
            match_select_index_hint_name,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to validate index hints");
        return rc;
    }

    if (context.exact_match || context.prefix_match_count == 1U) {
        *out_exists = true;
    }
    return MYLITE_OK;
}

static int match_select_index_hint_name(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct select_index_hint_lookup_context *context = user_data;

    if (index != NULL && context != NULL &&
        text_equals_ascii_case_insensitive(index->name, context->name)) {
        context->exact_match = true;
    } else if (
        index != NULL && context != NULL &&
        text_has_ascii_case_insensitive_prefix(index->name, context->name)
    ) {
        ++context->prefix_match_count;
    }
    return MYLITE_OK;
}

static int init_join_select_source_context(
    const struct planned_select_source *sources,
    size_t source_count,
    struct select_source_context *out_context
) {
    *out_context = (struct select_source_context){
        .sources = sources,
        .source_count = source_count,
    };
    return MYLITE_OK;
}

static int reject_duplicate_select_source_reference(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    size_t source_count
) {
    for (size_t left_index = 0U; left_index < source_count; ++left_index) {
        const char *left_name = planned_select_source_reference_name(&sources[left_index]);

        for (size_t right_index = left_index + 1U; right_index < source_count; ++right_index) {
            const char *right_name = planned_select_source_reference_name(&sources[right_index]);

            if (text_equals_ascii_case_insensitive(left_name, right_name)) {
                set_not_unique_table_alias_error(database, right_name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static const char *planned_select_source_reference_name(
    const struct planned_select_source *source
) {
    if (source->has_alias) {
        return source->alias;
    }
    return source->source.table_name;
}

static int plan_joined_select_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    enum mylite_sql_ast_join_kind join_kind,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
) {
    const struct mylite_sql_ast_node *right_node = NULL;
    int rc = MYLITE_OK;

    *out_condition = (struct planned_select_join_condition){.has_condition = false};
    if (condition_node == NULL) {
        if (join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER ||
            join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    condition_node = unwrap_parenthesized_expression(condition_node);
    if ((condition_node->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE &&
         condition_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) ||
        mylite_sql_ast_node_operator(condition_node) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        set_unsupported_error(database, "joined SELECT supports only descriptor equality ON");
        return MYLITE_ERROR;
    }
    right_node = child_at(condition_node, 1U);
    if (right_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        child_at(condition_node, 0U),
        source_context,
        COLUMN_REFERENCE_ON,
        "joined SELECT supports only descriptor equality ON",
        NULL,
        0U,
        &out_condition->left_column,
        &out_condition->left_source_index
    );
    if (rc == MYLITE_OK && (right_node->kind == MYLITE_SQL_AST_IDENTIFIER ||
                            right_node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            right_node,
            source_context,
            COLUMN_REFERENCE_ON,
            "joined SELECT supports only descriptor equality ON",
            NULL,
            0U,
            &out_condition->right_column,
            &out_condition->right_source_index
        );
        if (rc == MYLITE_OK &&
            !join_condition_columns_are_compatible(
                &out_condition->left_column,
                &out_condition->right_column
            )) {
            set_unsupported_error(
                database,
                "joined SELECT supports only same-family integer or string descriptor equality ON"
            );
            return MYLITE_ERROR;
        }
    } else if (rc == MYLITE_OK) {
        out_condition->right_row_scalar_expression =
            (struct planned_row_scalar_expression *)calloc(
                1U,
                sizeof(*out_condition->right_row_scalar_expression)
            );
        if (out_condition->right_row_scalar_expression == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        rc = plan_row_scalar_expression(
            database,
            right_node,
            true,
            source_context,
            NULL,
            NULL,
            0U,
            out_condition->right_row_scalar_expression
        );
        if (rc == MYLITE_OK) {
            out_condition->right_value_is_row_scalar_expression = true;
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_condition->has_condition = true;
    return MYLITE_OK;
}

static bool join_condition_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *left,
    const struct mylite_catalog_column_descriptor *right
) {
    if (join_condition_column_is_integer_family(left)) {
        return join_condition_column_is_integer_family(right);
    }
    if (column_descriptor_is_string_family(left)) {
        return column_descriptor_is_string_family(right);
    }
    return false;
}

static bool join_condition_column_is_integer_family(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->physical_type[0] == '\0') {
        return false;
    }
    return strcmp(column->physical_type, "INTEGER") == 0;
}

static void planned_select_order_deinit(struct planned_select_order *order) {
    if (order == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < order->item_count; ++item_index) {
        planned_select_order_item_deinit(&order->items[item_index]);
    }
    free(order->items);
    memset(order, 0, sizeof(*order));
}

static void planned_select_deinit(struct planned_select *plan) {
    if (plan == NULL) {
        return;
    }

    struct planned_select_source *derived_source = planned_select_find_deepest_derived_source(plan);

    while (derived_source != NULL) {
        planned_select_deinit_shallow(derived_source->derived_select);
        free(derived_source->derived_select);
        derived_source->derived_select = NULL;
        derived_source->is_derived = false;
        derived_source = planned_select_find_deepest_derived_source(plan);
    }

    planned_select_deinit_shallow(plan);
}

static void planned_select_deinit_shallow(struct planned_select *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        planned_select_source_deinit_shallow(&plan->sources[source_index]);
    }
    free(plan->sources);
    free(plan->columns);
    free((void *)plan->column_aliases);
    free(plan->column_source_indexes);
    free(plan->join_kinds);
    if (plan->join_conditions != NULL) {
        for (size_t join_index = 0U; join_index < plan->join_count; ++join_index) {
            planned_select_join_condition_deinit(&plan->join_conditions[join_index]);
        }
    } else {
        planned_select_join_condition_deinit(&plan->join_condition);
    }
    free(plan->join_conditions);
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_select){0};
}

static struct planned_select_source *planned_select_find_deepest_derived_source(
    struct planned_select *plan
) {
    struct planned_select_source *deepest_source = NULL;
    struct planned_select *current_plan = plan;
    bool descended = true;

    while (current_plan != NULL && descended) {
        descended = false;
        for (size_t source_index = 0U; source_index < current_plan->source_count;
             ++source_index) {
            struct planned_select_source *source = &current_plan->sources[source_index];

            if (source->derived_select != NULL) {
                deepest_source = source;
                current_plan = source->derived_select;
                descended = true;
                break;
            }
        }
    }

    return deepest_source;
}

static void planned_select_source_deinit(struct planned_select_source *source) {
    if (source == NULL) {
        return;
    }
    if (source->derived_select != NULL) {
        planned_select_deinit(source->derived_select);
        free(source->derived_select);
        source->derived_select = NULL;
    }
    planned_select_source_deinit_shallow(source);
}

static void planned_select_source_deinit_shallow(struct planned_select_source *source) {
    if (source == NULL) {
        return;
    }
    assert(source->derived_select == NULL);
    free(source->columns);
    *source = (struct planned_select_source){0};
}

static void planned_select_join_condition_deinit(
    struct planned_select_join_condition *condition
) {
    if (condition == NULL) {
        return;
    }
    if (condition->right_row_scalar_expression != NULL) {
        planned_row_scalar_expression_deinit(condition->right_row_scalar_expression);
        free(condition->right_row_scalar_expression);
    }
    *condition = (struct planned_select_join_condition){0};
}

static int plan_row_scalar_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct select_source_context *item_source_context = NULL;
    struct row_scalar_select_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    const struct mylite_catalog_table_descriptor *item_table = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_row_scalar_select){false};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT) {
        out_plan->has_distinct = true;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS does not support row-scalar SELECT projection"
        );
        return MYLITE_ERROR;
    }

    if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) {
        out_plan->has_source = true;
        rc = collect_row_scalar_select_clauses(database, child_at(statement, 2U), &clauses);
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_select_source(
                database,
                from_clause,
                out_plan,
                &source_context,
                &table_columns,
                &table_column_count
            );
        }
        if (rc == MYLITE_OK) {
            item_source_context = &source_context;
        }
    } else if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        out_plan->has_source = true;
        rc = collect_row_scalar_select_clauses(database, child_at(statement, 2U), &clauses);
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_select_join_source(
                database,
                from_clause,
                out_plan,
                &source_context
            );
        }
        if (rc == MYLITE_OK) {
            item_source_context = &source_context;
        }
    } else if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        rc = collect_row_scalar_select_clauses(database, child_at(statement, 2U), &clauses);
    } else if (from_clause != NULL) {
        set_unsupported_error(
            database,
            "row-scalar SELECT supports only no-source, DUAL, descriptor table, or joined "
            "descriptor table sources"
        );
        return MYLITE_ERROR;
    }

    if (out_plan->has_source && out_plan->source_count == 0U) {
        item_table = &out_plan->table;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_select_items(
            database,
            select_list,
            item_source_context,
            item_table,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK && out_plan->has_source) {
        rc = plan_row_scalar_select_row_envelope(
            database,
            &clauses,
            &source_context,
            table_columns,
            table_column_count,
            allow_order_by_field,
            out_plan
        );
    }
    if (rc == MYLITE_OK && !out_plan->has_source) {
        rc = plan_row_scalar_select_tableless_filter(database, &clauses, out_plan);
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_deinit(out_plan);
    }

    return rc;
}

static int collect_row_scalar_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct row_scalar_select_clauses *out_clauses
) {
    *out_clauses = (struct row_scalar_select_clauses){0};
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            out_clauses->where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            out_clauses->order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            out_clauses->limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "row-scalar SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_table_columns,
    size_t *out_table_column_count
) {
    int rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->source,
            out_source_context
        );
    }
    if (rc == MYLITE_OK && out_source_context->has_alias) {
        memcpy(out_plan->source_alias, out_source_context->alias, sizeof(out_plan->source_alias));
        out_plan->source_has_alias = true;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            out_table_columns,
            out_table_column_count
        );
    }

    return rc;
}

static int plan_row_scalar_select_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context
) {
    struct planned_select source_plan = {0};
    struct joined_select_temp_nodes temp_nodes = {0};
    int rc = prepare_joined_select_plan(database, from_clause, &source_plan, &temp_nodes);

    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            source_plan.sources,
            source_plan.source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            source_plan.sources,
            source_plan.source_count,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_source_join_edges(database, &source_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_conditions(
            database,
            temp_nodes.join_condition_nodes,
            temp_nodes.join_condition_node_count,
            &source_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->sources = source_plan.sources;
        out_plan->source_count = source_plan.source_count;
        out_plan->join_kinds = source_plan.join_kinds;
        out_plan->join_conditions = source_plan.join_conditions;
        out_plan->join_count = source_plan.join_count;
        source_plan.sources = NULL;
        source_plan.source_count = 0U;
        source_plan.join_kinds = NULL;
        source_plan.join_conditions = NULL;
        source_plan.join_count = 0U;
    }

    planned_select_deinit(&source_plan);
    joined_select_temp_nodes_deinit(&temp_nodes);
    return rc;
}

static int plan_row_scalar_select_row_envelope(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
) {
    struct planned_select order_alias_plan = {
        .columns = NULL,
        .column_aliases = NULL,
        .column_count = 0U,
    };
    int rc = plan_select_predicate(
        database,
        clauses->where_clause,
        source_context,
        table_columns,
        table_column_count,
        &out_plan->predicate
    );

    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses->order_clause,
            source_context,
            &order_alias_plan,
            table_columns,
            table_column_count,
            allow_order_by_field,
            true,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses->limit_clause, &out_plan->limit);
    }

    return rc;
}

static int plan_row_scalar_select_tableless_filter(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    struct planned_row_scalar_select *out_plan
) {
    if (clauses->order_clause != NULL || clauses->limit_clause != NULL) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }
    if (clauses->where_clause == NULL) {
        return MYLITE_OK;
    }
    if (row_scalar_select_where_is_exists_filter(clauses->where_clause)) {
        return plan_row_scalar_select_exists_filter(database, clauses->where_clause, out_plan);
    }

    return plan_row_scalar_select_scalar_filter(database, clauses->where_clause, out_plan);
}

static int plan_row_scalar_select_exists_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *predicate = NULL;
    bool negate = false;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    predicate = unwrap_parenthesized_predicate(child_at(where_clause, 0U));
    if (predicate != NULL && predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        negate = true;
        predicate = unwrap_parenthesized_predicate(child_at(predicate, 0U));
    }
    if (predicate == NULL || predicate->kind != MYLITE_SQL_AST_EXISTS_PREDICATE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate, 0U),
        &(const struct exists_subquery_outer_context){
            .source_context = NULL,
            .columns = NULL,
            .column_count = 0U,
            .source_count = 0U,
        },
        &out_plan->tableless_filter.subquery
    );
    if (rc == MYLITE_OK) {
        out_plan->tableless_filter.has_filter = true;
        out_plan->tableless_filter.negate = negate;
    }

    return rc;
}

static bool row_scalar_select_where_is_exists_filter(
    const struct mylite_sql_ast_node *where_clause
) {
    const struct mylite_sql_ast_node *predicate = NULL;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        return false;
    }
    predicate = unwrap_parenthesized_predicate(child_at(where_clause, 0U));
    if (predicate != NULL && predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        predicate = unwrap_parenthesized_predicate(child_at(predicate, 0U));
    }

    return (predicate != NULL && predicate->kind == MYLITE_SQL_AST_EXISTS_PREDICATE) != 0;
}

static int plan_row_scalar_select_scalar_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *predicate = NULL;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    predicate = child_at(where_clause, 0U);
    rc = plan_select_predicate_node_without_exists(
        database,
        predicate,
        NULL,
        NULL,
        0U,
        NULL,
        &out_plan->tableless_filter.predicate
    );
    if (rc == MYLITE_OK &&
        !planned_row_scalar_tableless_filter_is_supported(&out_plan->tableless_filter.predicate)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->tableless_filter.has_filter = true;
        out_plan->tableless_filter.has_scalar_predicate = true;
    }
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&out_plan->tableless_filter.predicate);
    }

    return rc;
}

static bool planned_row_scalar_tableless_filter_is_supported(
    const struct planned_select_predicate *predicate
) {
    if (!planned_select_predicate_has_expression(predicate)) {
        return false;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        if (!planned_row_scalar_tableless_filter_node_is_supported(&predicate->nodes[node_index])) {
            return false;
        }
    }

    return true;
}

static bool planned_row_scalar_tableless_filter_node_is_supported(
    const struct planned_select_predicate_node *node
) {
    if (node == NULL) {
        return false;
    }

    switch (node->kind) {
    case PLANNED_SELECT_PREDICATE_AND:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND;
    case PLANNED_SELECT_PREDICATE_OR:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR;
    case PLANNED_SELECT_PREDICATE_XOR:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR;
    case PLANNED_SELECT_PREDICATE_NOT:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT;
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH:
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON:
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL:
        return (node->row_scalar_expression != NULL &&
                node->row_scalar_expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) != 0;
    default:
        return false;
    }
}

static void planned_row_scalar_select_deinit(struct planned_row_scalar_select *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
        planned_row_scalar_expression_deinit(&plan->items[item_index].expression);
    }
    free(plan->items);
    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        planned_select_source_deinit(&plan->sources[source_index]);
    }
    if (plan->join_conditions != NULL) {
        for (size_t join_index = 0U; join_index < plan->join_count; ++join_index) {
            planned_select_join_condition_deinit(&plan->join_conditions[join_index]);
        }
    }
    free(plan->sources);
    free(plan->join_kinds);
    free(plan->join_conditions);
    if (plan->tableless_filter.has_filter && plan->tableless_filter.has_scalar_predicate) {
        planned_select_predicate_deinit_without_exists(&plan->tableless_filter.predicate);
    } else if (plan->tableless_filter.has_filter) {
        planned_exists_subquery_deinit(&plan->tableless_filter.subquery);
    }
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_row_scalar_select){false};
}
