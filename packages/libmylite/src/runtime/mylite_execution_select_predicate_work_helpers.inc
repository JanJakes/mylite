static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
) {
    struct planned_select_predicate_node stored_node = *node;
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (predicate->node_count == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    required_count = predicate->node_count + 1U;
    rc = reserve_planned_select_predicate_nodes(database, predicate, required_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (node->column != NULL) {
        rc = retain_planned_select_predicate_column(
            database,
            predicate,
            node->column,
            &stored_node.column
        );
    }
    if (rc == MYLITE_OK && node->value_column != NULL) {
        rc = retain_planned_select_predicate_column(
            database,
            predicate,
            node->value_column,
            &stored_node.value_column
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    predicate->nodes[predicate->node_count] = stored_node;
    if (out_node_index != NULL) {
        *out_node_index = predicate->node_count;
    }
    predicate->node_count = required_count;

    return MYLITE_OK;
}

static int retain_planned_select_predicate_column(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_catalog_column_descriptor **out_column
) {
    struct mylite_catalog_column_descriptor *copy = NULL;
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    for (size_t index = 0U; index < predicate->owned_column_count; ++index) {
        const struct mylite_catalog_column_descriptor *owned = predicate->owned_columns[index];

        if (planned_select_predicate_columns_match(&(const struct catalog_column_descriptor_pair
            ){.left = owned, .right = column})) {
            *out_column = owned;
            return MYLITE_OK;
        }
    }
    if (predicate->owned_column_count == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    required_count = predicate->owned_column_count + 1U;
    rc = reserve_planned_select_predicate_columns(database, predicate, required_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    copy = malloc(sizeof(*copy));
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *copy = *column;
    predicate->owned_columns[predicate->owned_column_count] = copy;
    predicate->owned_column_count = required_count;
    *out_column = copy;
    return MYLITE_OK;
}

static bool planned_select_predicate_columns_match(
    const struct catalog_column_descriptor_pair *columns
) {
    const struct mylite_catalog_column_descriptor *left = columns == NULL ? NULL : columns->left;
    const struct mylite_catalog_column_descriptor *right = columns == NULL ? NULL : columns->right;

    if (left == NULL || right == NULL) {
        return false;
    }
    if (right->column_id > 0 && right->table_id > 0) {
        return left->column_id == right->column_id && left->table_id == right->table_id;
    }
    return left->column_id == right->column_id && left->table_id == right->table_id &&
           left->ordinal_position == right->ordinal_position &&
           strcmp(left->name, right->name) == 0 &&
           strcmp(left->logical_type, right->logical_type) == 0 &&
           strcmp(left->physical_type, right->physical_type) == 0 &&
           left->is_nullable == right->is_nullable && left->is_visible == right->is_visible &&
           left->is_auto_increment == right->is_auto_increment &&
           left->default_kind == right->default_kind &&
           left->default_integer == right->default_integer &&
           (left->default_text == right->default_text ||
            (left->default_text != NULL && right->default_text != NULL &&
             strcmp(left->default_text, right->default_text) == 0)) &&
           left->on_update_current_timestamp == right->on_update_current_timestamp &&
           strcmp(left->character_set_name, right->character_set_name) == 0 &&
           strcmp(left->collation_name, right->collation_name) == 0 &&
           (left->comment == right->comment || (left->comment != NULL && right->comment != NULL &&
                                                strcmp(left->comment, right->comment) == 0)) &&
           left->is_generated == right->is_generated &&
           left->generated_kind == right->generated_kind &&
           (left->generation_expression == right->generation_expression ||
            (left->generation_expression != NULL && right->generation_expression != NULL &&
             strcmp(left->generation_expression, right->generation_expression) == 0)) &&
           (left->sqlite_generation_expression == right->sqlite_generation_expression ||
            (left->sqlite_generation_expression != NULL &&
             right->sqlite_generation_expression != NULL &&
             strcmp(left->sqlite_generation_expression, right->sqlite_generation_expression) == 0)
           ) &&
           left->descriptor_version == right->descriptor_version &&
           left->created_catalog_generation == right->created_catalog_generation &&
           left->updated_catalog_generation == right->updated_catalog_generation;
}

static int reserve_planned_select_predicate_nodes(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t required_capacity
) {
    enum { initial_predicate_node_capacity = 4 };

    struct planned_select_predicate_node *nodes = NULL;
    size_t capacity = predicate->node_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_predicate_node_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*nodes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    nodes = realloc(predicate->nodes, capacity * sizeof(*nodes));
    if (nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    predicate->nodes = nodes;
    predicate->node_capacity = capacity;
    return MYLITE_OK;
}

static int reserve_planned_select_predicate_columns(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    size_t required_capacity
) {
    enum { initial_predicate_column_capacity = 2 };

    struct mylite_catalog_column_descriptor **columns = NULL;
    size_t capacity = predicate->owned_column_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_predicate_column_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    columns = (struct mylite_catalog_column_descriptor **)
        realloc((void *)predicate->owned_columns, capacity * sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    predicate->owned_columns = columns;
    predicate->owned_column_capacity = capacity;
    return MYLITE_OK;
}

static int append_deprecated_logical_and_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_and,
        "HY000",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_deprecated_logical_or_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_or,
        "HY000",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool planned_select_predicate_has_expression(const struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return false;
    }

    return predicate->has_root;
}

static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    const struct mylite_sql_ast_node *node
) {
    return append_predicate_work_item(
        database,
        stack,
        (struct predicate_work_item){.kind = PREDICATE_WORK_NODE, .node = node}
    );
}

static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_stack *stack
) {
    return append_predicate_work_item(
        database,
        stack,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_AND_WARNING}
    );
}

static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_stack *stack
) {
    return append_predicate_work_item(
        database,
        stack,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_OR_WARNING}
    );
}

static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_work_item(
        database,
        stack,
        (struct predicate_work_item){
            .kind = PREDICATE_WORK_FINISH_LOGICAL,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_stack *stack
) {
    return append_predicate_work_item(
        database,
        stack,
        (struct predicate_work_item){.kind = PREDICATE_WORK_FINISH_NOT}
    );
}

static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_stack *stack,
    struct predicate_work_item item
) {
    struct predicate_work_item *grown_items = NULL;
    size_t capacity = stack->capacity;

    if (stack->count == stack->capacity) {
        if (capacity == 0U) {
            capacity = predicate_planning_stack_initial_capacity;
        } else if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        } else {
            capacity *= 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*grown_items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }

        grown_items = realloc(stack->items, capacity * sizeof(*grown_items));
        if (grown_items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = grown_items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = item;
    ++stack->count;

    return MYLITE_OK;
}

static int append_predicate_result_index(
    struct mylite_db *database,
    struct predicate_result_stack *stack,
    size_t index
) {
    size_t *grown_indexes = NULL;
    size_t capacity = stack->capacity;

    if (stack->count == stack->capacity) {
        if (capacity == 0U) {
            capacity = predicate_planning_stack_initial_capacity;
        } else if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        } else {
            capacity *= 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*grown_indexes)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }

        grown_indexes = realloc(stack->indexes, capacity * sizeof(*grown_indexes));
        if (grown_indexes == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->indexes = grown_indexes;
        stack->capacity = capacity;
    }

    stack->indexes[stack->count] = index;
    ++stack->count;

    return MYLITE_OK;
}

static int pop_predicate_result_index(struct predicate_result_stack *stack, size_t *out_index) {
    if (stack == NULL || stack->indexes == NULL || stack->count == 0U || out_index == NULL) {
        return MYLITE_ERROR;
    }

    --stack->count;
    *out_index = stack->indexes[stack->count];
    return MYLITE_OK;
}

static int resolve_predicate_column_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_pointer_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_WHERE,
        "WHERE supports only unqualified predicate columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}
