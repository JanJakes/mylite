
static int finalize_planned_generated_columns(
    struct mylite_db *database,
    struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = finalize_planned_generated_column(database, plan, column_index);
    }

    return rc;
}

static int finalize_planned_generated_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index
) {
    struct planned_column *column = &plan->columns[column_index];

    if (!column->is_generated) {
        return MYLITE_OK;
    }
    if (column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL &&
        column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column->physical_type == NULL || strcmp(column->physical_type, "INTEGER") != 0 ||
        planned_column_is_bit(column) || planned_column_is_year(column)) {
        set_unsupported_error(
            database,
            "generated columns support only baseline integer descriptor target columns"
        );
        return MYLITE_ERROR;
    }
    {
        struct integer_column_range range = {0};
        int rc = integer_range_for_logical_type(
            database,
            (struct integer_logical_type_range_request){
                .logical_type = column->logical_type,
                .unsupported_message =
                    "generated columns support only baseline integer descriptor target columns",
            },
            &range
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return render_generated_column_expression(
        database,
        plan,
        column_index,
        column->generation_expression_node,
        column->generation_expression,
        sizeof(column->generation_expression),
        column->sqlite_generation_expression,
        sizeof(column->sqlite_generation_expression)
    );
}

static int render_generated_column_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t generated_column_index,
    const struct mylite_sql_ast_node *expression,
    char *generation_expression,
    size_t generation_expression_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
) {
    struct mylite_dynamic_string generation_string;
    struct mylite_dynamic_string sqlite_string;
    struct generated_expression_render_context context = {
        .database = database,
        .plan = plan,
        .generated_column_index = generated_column_index,
        .generation_expression = &generation_string,
        .sqlite_expression = &sqlite_string,
    };
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&generation_string);
    mylite_dynamic_string_init(&sqlite_string);
    rc = mylite_dynamic_string_reserve(&generation_string, 1U);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_reserve(&sqlite_string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = render_generated_expression_node(&context, expression);
    }
    if (rc == MYLITE_OK && (generation_string.length >= generation_expression_size ||
                            sqlite_string.length >= sqlite_expression_size)) {
        set_unsupported_error(database, "generated column expression is too long");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(generation_expression, generation_string.text, generation_string.length + 1U);
        memcpy(sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
    }

    mylite_dynamic_string_deinit(&generation_string);
    mylite_dynamic_string_deinit(&sqlite_string);
    return rc;
}

static int render_generated_expression_node(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    struct generated_expression_render_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_generated_expression_render_node(context->database, &items, &item_count, node);

    while (rc == MYLITE_OK && item_count != 0U) {
        struct generated_expression_render_work_item item = items[--item_count];

        rc = render_generated_expression_work_item(context, item, &items, &item_count);
    }
    free(items);
    return rc;
}

static int render_generated_expression_work_item(
    struct generated_expression_render_context *context,
    struct generated_expression_render_work_item item,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(item.node);
    int rc = MYLITE_OK;

    switch (item.kind) {
    case GENERATED_EXPRESSION_RENDER_TEXT:
        rc = mylite_dynamic_string_append(context->generation_expression, item.text);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, item.text);
        }
        return rc;
    case GENERATED_EXPRESSION_RENDER_CHAR:
        rc = mylite_dynamic_string_append_char(context->generation_expression, item.character);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(context->sqlite_expression, item.character);
        }
        return rc;
    case GENERATED_EXPRESSION_RENDER_NODE:
        break;
    }

    if (node == NULL) {
        set_parse_error(context->database, NULL);
        return MYLITE_ERROR;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return render_generated_expression_column(context, node);
    case MYLITE_SQL_AST_LITERAL:
        return render_generated_expression_literal(context, node);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return render_generated_expression_unary(context, node, items, item_count);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return render_generated_expression_binary(context, node, items, item_count);
    default:
        break;
    }

    set_unsupported_error(
        context->database,
        "generated columns support only unqualified integer columns, integer literals, NULL, "
        "TRUE, FALSE, unary signs, and +, -, * expressions"
    );
    return MYLITE_ERROR;
}

static int render_generated_expression_column(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    const struct planned_column *column = NULL;
    int rc = copy_identifier_text(node, column_name, sizeof(column_name), context->database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_planned_column_index(
        context->plan->columns,
        context->plan->column_count,
        column_name,
        &column_index
    );
    if (rc != MYLITE_OK) {
        set_unknown_column_error(context->database, column_name);
        return MYLITE_ERROR;
    }
    if (column_index >= context->generated_column_index) {
        set_unsupported_error(
            context->database,
            "generated columns support only references to previously declared base columns"
        );
        return MYLITE_ERROR;
    }

    column = &context->plan->columns[column_index];
    if (column->is_generated || column->physical_type == NULL ||
        strcmp(column->physical_type, "INTEGER") != 0 || planned_column_is_bit(column) ||
        planned_column_is_year(column)) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only baseline integer descriptor columns"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_mysql_quoted_identifier(
        context->generation_expression,
        column->name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            context->sqlite_expression,
            column->name
        );
    }
    return rc;
}

static int render_generated_expression_literal(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(node);

    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        int rc = append_check_expression_span_text(
            context->database,
            context->generation_expression,
            node
        );

        if (rc == MYLITE_OK) {
            rc = append_check_expression_span_text(
                context->database,
                context->sqlite_expression,
                node
            );
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "TRUE");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "1");
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "FALSE");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "0");
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "NULL");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "NULL");
        }
        return rc;
    }

    set_unsupported_error(
        context->database,
        "generated column expressions support only integer, boolean, and NULL literals"
    );
    return MYLITE_ERROR;
}

static int render_generated_expression_unary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    int rc = MYLITE_OK;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only unary + and -"
        );
        return MYLITE_ERROR;
    }

    rc = append_generated_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_text(context->database, items, item_count, "-(");
    }
    return rc;
}

static int render_generated_expression_binary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    const char *operator_text = generated_expression_binary_operator_text(operator_kind);
    int rc = MYLITE_OK;

    if (operator_text == NULL) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only +, -, *"
        );
        return MYLITE_ERROR;
    }

    rc = append_generated_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 1U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_text(
            context->database,
            items,
            item_count,
            operator_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_char(context->database, items, item_count, '(');
    }
    return rc;
}

static const char *generated_expression_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return " + ";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return " - ";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return " * ";
    default:
        return NULL;
    }
}

static int append_generated_expression_render_node(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_NODE,
            .node = node,
        }
    );
}

static int append_generated_expression_render_text(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_TEXT,
            .text = text,
        }
    );
}

static int append_generated_expression_render_char(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    char character
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_CHAR,
            .character = character,
        }
    );
}

static int append_generated_expression_render_work_item(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    struct generated_expression_render_work_item item
) {
    struct generated_expression_render_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}
