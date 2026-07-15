static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
) {
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
    predicate->nodes[predicate->node_count] = *node;
    if (out_node_index != NULL) {
        *out_node_index = predicate->node_count;
    }
    predicate->node_count = required_count;

    return MYLITE_OK;
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

static bool planned_select_predicate_has_expression(
    const struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return false;
    }

    return predicate->has_root;
}

static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_NODE, .node = node}
    );
}

static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_AND_WARNING}
    );
}

static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_OR_WARNING}
    );
}

static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){
            .kind = PREDICATE_WORK_FINISH_LOGICAL,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_FINISH_NOT}
    );
}

static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    struct predicate_work_item item
) {
    struct predicate_work_item *grown_items = NULL;
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

static int append_predicate_result_index(
    struct mylite_db *database,
    size_t **indexes,
    size_t *index_count,
    size_t index
) {
    size_t *grown_indexes = NULL;
    size_t required_count = *index_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_indexes = realloc(*indexes, required_count * sizeof(*grown_indexes));
    if (grown_indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *indexes = grown_indexes;
    (*indexes)[*index_count] = index;
    *index_count = required_count;

    return MYLITE_OK;
}

static int pop_predicate_result_index(
    const size_t *indexes,
    size_t *index_count,
    size_t *out_index
) {
    if (indexes == NULL || index_count == NULL || *index_count == 0U || out_index == NULL) {
        return MYLITE_ERROR;
    }

    --(*index_count);
    *out_index = indexes[*index_count];
    return MYLITE_OK;
}

static int resolve_predicate_column_with_source_index(
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
        COLUMN_REFERENCE_WHERE,
        "WHERE supports only unqualified predicate columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}
