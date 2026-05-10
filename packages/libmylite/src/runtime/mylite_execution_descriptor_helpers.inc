static int reject_inline_primary_key_column_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_definition,
    const char *message
) {
    if (child_with_kind(column_definition, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) != NULL) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_auto_increment_column_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_definition,
    const char *message
) {
    if (child_with_kind(column_definition, MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT) != NULL ||
        mylite_sql_ast_node_integer_type_is_serial_alias(child_at(column_definition, 1U)) != 0) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int find_column_index(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int resolve_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    size_t source_index = 0U;

    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        diagnostic_context,
        unsupported_message,
        table_columns,
        table_column_count,
        out_column,
        &source_index
    );
}

static int resolve_descriptor_column_reference_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (out_source_index != NULL) {
        *out_source_index = 0U;
    }
    if (source_context != NULL && source_context->source_count > 0U) {
        return resolve_joined_descriptor_column_reference(
            database,
            column_node,
            source_context,
            diagnostic_context,
            unsupported_message,
            out_column,
            out_source_index
        );
    }
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        format_column_reference_name(database, parts, part_count, column_name, sizeof(column_name));
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (source_context == NULL && part_count > 1U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (source_context != NULL &&
        !column_reference_qualifier_matches_source(parts, part_count, source_context)) {
        set_unknown_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }

    rc =
        find_column_index(table_columns, table_column_count, parts[part_count - 1U], &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }

    *out_column = table_columns[column_index];
    return MYLITE_OK;
}

static int resolve_joined_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    size_t matching_source_index = 0U;
    size_t matching_column_index = 0U;
    size_t match_count = 0U;
    int rc = MYLITE_OK;

    if (column_node == NULL || source_context == NULL ||
        (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
         column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        format_column_reference_name(database, parts, part_count, column_name, sizeof(column_name));
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 0U || part_count > table_name_part_capacity) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    for (size_t source_index = 0U; source_index < source_context->source_count; ++source_index) {
        const struct planned_select_source *source = &source_context->sources[source_index];
        size_t column_index = 0U;

        if (!column_reference_source_matches_parts(source, parts, part_count)) {
            continue;
        }
        rc = find_column_index(
            source->columns,
            source->column_count,
            parts[part_count - 1U],
            &column_index
        );
        if (rc != MYLITE_OK) {
            if (part_count > 1U) {
                set_unknown_column_reference_error(database, diagnostic_context, column_name);
                return MYLITE_ERROR;
            }
            continue;
        }
        if (match_count == 0U) {
            matching_source_index = source_index;
            matching_column_index = column_index;
        }
        ++match_count;
    }

    if (match_count > 1U) {
        set_ambiguous_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }
    if (match_count == 0U) {
        set_unknown_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }

    *out_column = source_context->sources[matching_source_index].columns[matching_column_index];
    if (out_source_index != NULL) {
        *out_source_index = matching_source_index;
    }
    return MYLITE_OK;
}

static bool column_reference_source_matches_parts(
    const struct planned_select_source *source,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (part_count == 1U) {
        return true;
    }
    if (source == NULL) {
        return false;
    }
    if (part_count == 2U) {
        const char *expected = source->source.table_name;

        if (source->has_alias) {
            expected = source->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 3U && !source->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source->source.schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source->source.table_name);

        if (!schema_matches) {
            return false;
        }
        return table_matches;
    }

    return false;
}

static int collect_column_reference_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
) {
    *out_part_count = 0U;
    return collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        out_part_count,
        database
    );
}

static int format_column_reference_name(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    char *destination,
    size_t destination_size
) {
    size_t offset = 0U;

    if (part_count == 0U || part_count > table_name_part_capacity || destination == NULL ||
        destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    destination[0] = '\0';
    for (size_t part_index = 0U; part_index < part_count; ++part_index) {
        int written = 0;

        if (part_index > 0U) {
            if (offset + 1U >= destination_size) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            destination[offset] = '.';
            ++offset;
            destination[offset] = '\0';
        }
        written =
            snprintf(destination + offset, destination_size - offset, "%s", parts[part_index]);
        if (written < 0 || (size_t)written >= destination_size - offset) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        offset += (size_t)written;
    }

    return MYLITE_OK;
}

static bool column_reference_qualifier_matches_source(
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct select_source_context *source_context
) {
    if (part_count == 1U) {
        return true;
    }
    if (source_context == NULL || source_context->source == NULL) {
        return false;
    }
    if (part_count == 2U) {
        const char *expected = source_context->source->table_name;

        if (source_context->has_alias) {
            expected = source_context->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 3U && !source_context->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source_context->source->schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source_context->source->table_name);

        if (schema_matches && table_matches) {
            return true;
        }
        return false;
    }

    return false;
}

static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
) {
    if (context == COLUMN_REFERENCE_WHERE) {
        set_unknown_where_column_error(database, column_name);
        return;
    }
    if (context == COLUMN_REFERENCE_ORDER) {
        set_unknown_order_column_error(database, column_name);
        return;
    }
    if (context == COLUMN_REFERENCE_GROUP) {
        set_unknown_group_column_error(database, column_name);
        return;
    }
    if (context == COLUMN_REFERENCE_HAVING) {
        set_unknown_having_column_error(database, column_name);
        return;
    }
    if (context == COLUMN_REFERENCE_ON) {
        set_unknown_on_column_error(database, column_name);
        return;
    }

    set_unknown_column_error(database, column_name);
}

static size_t count_visible_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    size_t visible_count = 0U;

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (columns[column_index].is_visible) {
            ++visible_count;
        }
    }

    return visible_count;
}
