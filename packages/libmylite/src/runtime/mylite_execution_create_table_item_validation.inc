
static int count_create_table_column_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    size_t *out_column_count
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = validate_create_table_item_list(database, item_list);

    *out_column_count = 0U;
    if (rc != MYLITE_OK) {
        return rc;
    }

    item = child_at(item_list, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            ++(*out_column_count);
        }
        item = item->next_sibling;
    }
    if (*out_column_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (item_list == NULL || item_list->kind != MYLITE_SQL_AST_COLUMN_DEFINITION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (item != NULL) {
        if (item->kind != MYLITE_SQL_AST_COLUMN_DEFINITION &&
            item->kind != MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION &&
            item->kind != MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION &&
            item->kind != MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}
