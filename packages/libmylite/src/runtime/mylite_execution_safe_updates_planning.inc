static bool sql_safe_updates_is_enabled(const struct mylite_db *database) {
    const char *value = session_system_variable_override_value(
        database,
        MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES
    );

    return value != NULL && strcmp(value, "0") != 0;
}

static int validate_sql_safe_updates_predicate(
    struct mylite_db *database,
    const struct planned_select_predicate *predicate,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    if (predicate == NULL || !predicate->has_root ||
        !sql_safe_updates_predicate_uses_key(
            predicate,
            predicate->root_index,
            indexes,
            index_count
        )) {
        mylite_execution_diagnostics_set_safe_update_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool sql_safe_updates_predicate_uses_key(
    const struct planned_select_predicate *predicate,
    size_t node_index,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    const struct planned_select_predicate_node *node = NULL;

    if (predicate == NULL || node_index >= predicate->node_count) {
        return false;
    }

    node = &predicate->nodes[node_index];
    switch (node->kind) {
        case PLANNED_SELECT_PREDICATE_AND:
            return sql_safe_updates_predicate_uses_key(
                       predicate,
                       node->left_index,
                       indexes,
                       index_count
                   ) ||
                   sql_safe_updates_predicate_uses_key(
                       predicate,
                       node->right_index,
                       indexes,
                       index_count
                   );

        case PLANNED_SELECT_PREDICATE_OR:
            return sql_safe_updates_predicate_uses_key(
                       predicate,
                       node->left_index,
                       indexes,
                       index_count
                   ) &&
                   sql_safe_updates_predicate_uses_key(
                       predicate,
                       node->right_index,
                       indexes,
                       index_count
                   );

        default:
            return sql_safe_updates_leaf_node_uses_key(node, indexes, index_count);
    }
}

static bool sql_safe_updates_leaf_node_uses_key(
    const struct planned_select_predicate_node *node,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    if (node == NULL) {
        return false;
    }

    switch (node->kind) {
        case PLANNED_SELECT_PREDICATE_COMPARISON:
        case PLANNED_SELECT_PREDICATE_IS_NULL:
        case PLANNED_SELECT_PREDICATE_BETWEEN:
        case PLANNED_SELECT_PREDICATE_IN:
        case PLANNED_SELECT_PREDICATE_QUANTIFIED_SUBQUERY:
        case PLANNED_SELECT_PREDICATE_IS_BOOLEAN:
            if (sql_safe_updates_column_id_is_keyed(
                    node->column.column_id,
                    indexes,
                    index_count
                )) {
                return true;
            }
            return node->value_is_column_reference &&
                   sql_safe_updates_column_id_is_keyed(
                       node->value_column.column_id,
                       indexes,
                       index_count
                   );

        default:
            return false;
    }
}

static bool sql_safe_updates_column_id_is_keyed(
    int64_t column_id,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (!indexes[index].index.is_visible || indexes[index].part_count == 0U) {
            continue;
        }
        if (indexes[index].parts[0].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}
