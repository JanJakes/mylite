static int validate_single_table_dml_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause
) {
    const struct mylite_sql_ast_node *order_key = NULL;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "ORDER BY supports only descriptor columns");
        return MYLITE_ERROR;
    }

    order_key = unwrap_parenthesized_expression(child_at(order_clause, 0U));
    if (order_key != NULL &&
        (order_key->kind == MYLITE_SQL_AST_IDENTIFIER ||
         order_key->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "ORDER BY supports only descriptor columns");
    return MYLITE_ERROR;
}

static int plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    int rc = MYLITE_OK;

    *out_order = (struct planned_select_order){
        .has_order = false,
        .direction = PLANNED_SELECT_ORDER_DEFAULT,
        .column_source_index = 0U,
        .qualify_column_reference = select_source_context_is_joined(source_context),
    };
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
        return MYLITE_ERROR;
    }

    order_items = child_at(order_clause, 0U);
    if (order_items != NULL && order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        rc = plan_select_order_item_list(
            database,
            order_items,
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
    } else {
        rc = plan_select_order_ast_item_and_append(
            database,
            (struct select_order_ast_item_nodes){
                .order_key = child_at(order_clause, 0U),
                .direction = child_at(order_clause, 1U),
            },
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_order->item_count > 0U) {
        out_order->has_order = true;
        if (out_order->items[0U].kind == PLANNED_SELECT_ORDER_ITEM_COLUMN) {
            out_order->column = out_order->items[0U].column;
            out_order->column_source_index = out_order->items[0U].column_source_index;
        }
        out_order->direction = out_order->items[0U].direction;
    }

    return MYLITE_OK;
}

static int plan_select_order_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *item = child_at(order_items, 0U);

    if (order_item_list_contains_field_order_key(order_items) &&
        mylite_sql_ast_node_child_count(order_items) != 1U) {
        set_unsupported_error(database, "SELECT ORDER BY FIELD() supports only one order key");
        return MYLITE_ERROR;
    }
    if (order_item_list_contains_rand_order_key(order_items) &&
        mylite_sql_ast_node_child_count(order_items) != 1U) {
        set_unsupported_error(database, "SELECT ORDER BY RAND() supports only one order key");
        return MYLITE_ERROR;
    }

    while (item != NULL) {
        int rc = MYLITE_OK;

        if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
            set_unsupported_error(database, "SELECT supports only descriptor ORDER BY columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_ast_item_and_append(
            database,
            (struct select_order_ast_item_nodes){
                .order_key = child_at(item, 0U),
                .direction = child_at(item, 1U),
            },
            source_context,
            select_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            allow_rand_order,
            out_order
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order *out_order
) {
    struct planned_select_order_item planned_item = {0};
    int rc = MYLITE_OK;

    if (select_order_key_is_constant_noop(item_nodes.order_key)) {
        return MYLITE_OK;
    }
    rc = plan_select_order_ast_item(
        database,
        item_nodes,
        source_context,
        select_plan,
        table_columns,
        table_column_count,
        allow_field_order,
        allow_rand_order,
        &planned_item
    );

    if (rc == MYLITE_OK) {
        rc = append_planned_select_order_item(out_order, planned_item);
    }
    if (rc != MYLITE_OK) {
        planned_select_order_item_deinit(&planned_item);
    }

    return rc;
}

static int plan_select_order_ast_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_field_order,
    bool allow_rand_order,
    struct planned_select_order_item *out_item
) {
    bool resolved_alias = false;
    int rc = MYLITE_OK;

    *out_item = (struct planned_select_order_item){
        .kind = PLANNED_SELECT_ORDER_ITEM_COLUMN,
        .direction = PLANNED_SELECT_ORDER_ASC,
        .column_source_index = 0U,
    };

    if (select_order_key_is_field_function(item_nodes.order_key)) {
        if (!allow_field_order) {
            set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_field_expression(
            database,
            item_nodes.order_key,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
        if (rc == MYLITE_OK) {
            apply_select_order_item_direction(item_nodes.direction, out_item);
        }
        return rc;
    }
    if (select_order_key_is_rand_function(item_nodes.order_key)) {
        if (!allow_rand_order || select_source_context_is_joined(source_context)) {
            set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_rand_expression(database, item_nodes.order_key, out_item);
        if (rc == MYLITE_OK) {
            apply_select_order_item_direction(item_nodes.direction, out_item);
        }
        return rc;
    }
    if (select_order_key_is_supported_row_scalar_expression(item_nodes.order_key)) {
        if (select_source_context_is_joined(source_context) &&
            !select_order_key_is_supported_joined_row_scalar_expression(item_nodes.order_key)) {
            set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
            return MYLITE_ERROR;
        }
        rc = plan_select_order_row_scalar_expression(
            database,
            item_nodes.order_key,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
        if (rc == MYLITE_OK) {
            apply_select_order_item_direction(item_nodes.direction, out_item);
        }
        return rc;
    }

    if (select_order_key_is_integer_ordinal(item_nodes.order_key)) {
        rc = plan_select_order_ordinal(database, item_nodes.order_key, select_plan, out_item);
    } else {
        rc = resolve_order_alias(
            database,
            item_nodes.order_key,
            select_plan,
            &out_item->column,
            &out_item->column_source_index,
            &resolved_alias
        );
        if (rc == MYLITE_OK && !resolved_alias) {
            rc = resolve_order_column(
                database,
                item_nodes.order_key,
                source_context,
                table_columns,
                table_column_count,
                &out_item->column,
                &out_item->column_source_index
            );
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_set(&out_item->column)) {
        set_unsupported_error(database, "ORDER BY does not yet support SET columns");
        return MYLITE_ERROR;
    }
    if (select_order_column_needs_integer_validation(&out_item->column)) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &out_item->column,
            "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or "
            "nonbinary string descriptor columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    apply_select_order_item_direction(item_nodes.direction, out_item);

    return MYLITE_OK;
}

static int plan_select_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_select *select_plan,
    struct planned_select_order_item *out_item
) {
    size_t ordinal = 0U;

    if (select_plan == NULL || select_plan->columns == NULL ||
        select_plan->column_source_indexes == NULL) {
        set_unsupported_error(database, "SELECT ORDER BY supports only descriptor columns");
        return MYLITE_ERROR;
    }
    if (parse_select_order_ordinal(database, order_key, select_plan->column_count, &ordinal) !=
        MYLITE_OK) {
        return MYLITE_ERROR;
    }

    out_item->column = select_plan->columns[ordinal - 1U];
    out_item->column_source_index = select_plan->column_source_indexes[ordinal - 1U];
    return MYLITE_OK;
}

static void apply_select_order_item_direction(
    const struct mylite_sql_ast_node *direction,
    struct planned_select_order_item *out_item
) {
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_item->direction = PLANNED_SELECT_ORDER_DESC;
    }
}

static bool select_order_column_needs_integer_validation(
    const struct mylite_catalog_column_descriptor *column
) {
    return !column_descriptor_is_string_family(column) && !column_descriptor_is_date(column) &&
           !column_descriptor_is_time(column) && !column_descriptor_is_datetime(column) &&
           !column_descriptor_is_timestamp(column) && !column_descriptor_is_year(column) &&
           !column_descriptor_is_bit(column);
}

static bool order_item_list_contains_field_order_key(
    const struct mylite_sql_ast_node *order_items
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        return false;
    }
    item = child_at(order_items, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_ORDER_BY_ITEM &&
            select_order_key_is_field_function(child_at(item, 0U))) {
            return true;
        }
        item = item->next_sibling;
    }
    return false;
}

static bool order_item_list_contains_rand_order_key(const struct mylite_sql_ast_node *order_items) {
    const struct mylite_sql_ast_node *item = NULL;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        return false;
    }
    item = child_at(order_items, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_ORDER_BY_ITEM &&
            select_order_key_is_rand_function(child_at(item, 0U))) {
            return true;
        }
        item = item->next_sibling;
    }
    return false;
}

static bool select_order_key_is_constant_noop(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL || order_key->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(order_key);
    return literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
           literal_kind == MYLITE_SQL_AST_LITERAL_STRING;
}

static bool select_order_key_is_integer_ordinal(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    return order_key != NULL && order_key->kind == MYLITE_SQL_AST_LITERAL &&
           mylite_sql_ast_node_literal_kind(order_key) == MYLITE_SQL_AST_LITERAL_INTEGER;
}

static bool select_order_key_is_field_function(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        return false;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_FUNCTION) {
        return true;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    return false;
}

static bool select_order_key_is_rand_function(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        return false;
    }
    if (order_key->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
        order_key->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION ||
        order_key->kind == MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR) {
        return true;
    }
    return false;
}

static bool select_order_key_is_searched_case_expression(
    const struct mylite_sql_ast_node *order_key
) {
    order_key = unwrap_parenthesized_expression(order_key);
    return order_key != NULL && order_key->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION;
}

static int plan_select_order_field_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
) {
    struct planned_row_scalar_expression *expression = NULL;
    int rc = MYLITE_OK;

    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        set_unsupported_error(
            database,
            "SELECT ORDER BY supports only descriptor columns or FIELD(column, value, ...)"
        );
        return MYLITE_ERROR;
    }
    if (order_key->kind == MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }

    expression = (struct planned_row_scalar_expression *)calloc(1U, sizeof(*expression));
    if (expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_field_expression_with_context(
        database,
        order_key,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_ORDER,
        "SELECT ORDER BY FIELD() supports only descriptor search columns",
        expression
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_order_field_expression(database, expression);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(expression);
        free(expression);
        return rc;
    }

    out_item->kind = PLANNED_SELECT_ORDER_ITEM_FIELD;
    out_item->expression = expression;
    return MYLITE_OK;
}

static int plan_select_order_rand_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    struct planned_select_order_item *out_item
) {
    struct planned_row_scalar_expression *expression = NULL;
    int rc = MYLITE_OK;

    expression = (struct planned_row_scalar_expression *)calloc(1U, sizeof(*expression));
    if (expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_rand_expression(database, order_key, false, NULL, NULL, 0U, expression);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(expression);
        free(expression);
        return rc;
    }

    out_item->kind = PLANNED_SELECT_ORDER_ITEM_RAND;
    out_item->expression = expression;
    return MYLITE_OK;
}

static int plan_select_order_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order_item *out_item
) {
    struct planned_row_scalar_expression *expression = NULL;
    int rc = MYLITE_OK;

    expression = (struct planned_row_scalar_expression *)calloc(1U, sizeof(*expression));
    if (expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        order_key,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(expression);
        free(expression);
        return rc;
    }

    out_item->kind = PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR;
    out_item->expression = expression;
    return MYLITE_OK;
}

static bool select_order_key_is_like_predicate(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL || order_key->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return false;
    }
    return mylite_sql_ast_node_operator(order_key) == MYLITE_SQL_AST_OPERATOR_LIKE;
}

static bool select_order_key_is_collate_expression(const struct mylite_sql_ast_node *order_key) {
    order_key = unwrap_parenthesized_expression(order_key);
    return order_key != NULL && order_key->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION;
}

static bool select_order_key_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
) {
    order_key = unwrap_parenthesized_expression(order_key);
    if (order_key == NULL) {
        return false;
    }
    if (row_scalar_expression_is_context_expression_attempt(order_key) ||
        row_scalar_expression_is_predicate_value_context_attempt(order_key) ||
        select_order_key_is_searched_case_expression(order_key) ||
        select_order_key_is_like_predicate(order_key) ||
        select_order_key_is_collate_expression(order_key)) {
        return true;
    }
    return false;
}

static bool select_order_key_is_supported_joined_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
) {
    enum planned_row_scalar_conversion_kind conversion_kind =
        row_scalar_conversion_kind_from_expression(order_key);

    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR) {
        return true;
    }
    if (conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_SIGNED ||
        conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED) {
        return true;
    }
    return row_scalar_expression_contains_integer_arithmetic_attempt(order_key);
}

static int validate_select_order_field_expression(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_FIELD ||
        expression->argument_count < 2U || expression->arguments == NULL) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }
    if (expression->arguments[0U].kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        set_unsupported_error(
            database,
            "SELECT ORDER BY FIELD() supports only descriptor search columns"
        );
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 1U; argument_index < expression->argument_count;
         ++argument_index) {
        if (expression->arguments[argument_index].kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE) {
            set_unsupported_error(
                database,
                "SELECT ORDER BY FIELD() supports only literal candidate values"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static void planned_select_order_item_deinit(struct planned_select_order_item *item) {
    if (item == NULL) {
        return;
    }
    if (item->expression != NULL) {
        planned_row_scalar_expression_deinit(item->expression);
        free(item->expression);
    }
    *item = (struct planned_select_order_item){0};
}

static int append_planned_select_order_item(
    struct planned_select_order *order,
    struct planned_select_order_item item
) {
    struct planned_select_order_item *items = NULL;
    size_t required_count = 0U;

    if (order->item_count >= SIZE_MAX / sizeof(*items)) {
        return MYLITE_NOMEM;
    }
    required_count = order->item_count + 1U;

    items = realloc(order->items, required_count * sizeof(*items));
    if (items == NULL) {
        return MYLITE_NOMEM;
    }

    order->items = items;
    order->items[order->item_count] = item;
    order->item_count = required_count;
    return MYLITE_OK;
}

static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index,
    bool *out_resolved
) {
    char *order_name = NULL;
    bool found = false;
    int rc = MYLITE_OK;

    *out_resolved = false;
    if (select_plan == NULL || column_node == NULL ||
        column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t column_index = 0U; column_index < select_plan->column_count; ++column_index) {
        const struct mylite_sql_ast_node *alias = select_plan->column_aliases[column_index];
        char *alias_text = NULL;

        if (alias == NULL) {
            continue;
        }

        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            free(order_name);
            return rc;
        }
        if (text_equals_ascii_case_insensitive(alias_text, order_name)) {
            if (found) {
                free(alias_text);
                set_ambiguous_order_column_error(database, order_name);
                free(order_name);
                return MYLITE_ERROR;
            }
            *out_column = select_plan->columns[column_index];
            if (out_source_index != NULL) {
                *out_source_index = select_plan->column_source_indexes[column_index];
            }
            found = true;
        }
        free(alias_text);
    }

    free(order_name);
    *out_resolved = found;
    return MYLITE_OK;
}

static int resolve_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "ORDER BY supports only unqualified descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc =
            convert_limit_integer_literal(database, child_at(limit_clause, 1U), &out_limit->offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}
