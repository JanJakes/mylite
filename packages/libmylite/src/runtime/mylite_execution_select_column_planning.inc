static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        return plan_select_wildcard_columns(
            database,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;
        int rc = MYLITE_OK;

        if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = plan_select_qualified_wildcard_columns(
                database,
                qualified_wildcard,
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
            continue;
        }
        rc = select_item_column_reference(item, &column_node);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "SELECT supports only descriptor table columns");
            return MYLITE_ERROR;
        }
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            column_node,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "SELECT supports only descriptor table columns",
            table_columns,
            table_column_count,
            &column,
            &source_index
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = append_select_column_from_source(
            database,
            out_plan,
            &column,
            source_index,
            child_at(item, 1U)
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_qualified_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
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
                    (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
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
            (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }

    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }

    return append_visible_select_columns_from_source(
        database,
        out_plan,
        source_index,
        columns,
        column_count
    );
}

static int collect_qualified_wildcard_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
) {
    const struct mylite_sql_ast_node *qualifier = NULL;
    int rc = MYLITE_OK;

    *out_part_count = 0U;
    if (qualified_wildcard == NULL ||
        qualified_wildcard->kind != MYLITE_SQL_AST_QUALIFIED_WILDCARD) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    qualifier = child_at(qualified_wildcard, 0U);
    rc = collect_identifier_parts(
        qualifier,
        parts,
        table_name_part_capacity,
        out_part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (*out_part_count == 0U || *out_part_count > 2U) {
        set_unsupported_error(
            database,
            "SELECT supports only one- or two-part qualified wildcards"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int set_unknown_qualified_wildcard_table_error(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    char table_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc =
        format_column_reference_name(database, parts, part_count, table_name, sizeof(table_name));

    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_table_name_error(database, table_name);
    return MYLITE_ERROR;
}

static bool select_source_matches_qualified_wildcard_parts(
    const struct select_source_context *source_context,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (source_context == NULL || source_context->source == NULL) {
        return false;
    }
    if (part_count == 1U) {
        const char *expected = source_context->source->table_name;

        if (source_context->has_alias) {
            expected = source_context->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 2U && !source_context->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source_context->source->schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source_context->source->table_name);

        if (!schema_matches) {
            return false;
        }
        return table_matches;
    }

    return false;
}

static bool joined_source_matches_qualified_wildcard_parts(
    const struct planned_select_source *source,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (source == NULL) {
        return false;
    }
    if (part_count == 1U) {
        const char *expected = source->source.table_name;

        if (source->has_alias) {
            expected = source->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 2U && !source->has_alias) {
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

static int append_visible_select_columns_from_source(
    struct mylite_db *database,
    struct planned_select *out_plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = MYLITE_OK;

        if (!columns[column_index].is_visible) {
            continue;
        }
        rc = append_select_column_from_source(
            database,
            out_plan,
            &columns[column_index],
            source_index,
            NULL
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }

    return MYLITE_OK;
}

static bool select_source_context_is_joined(const struct select_source_context *source_context) {
    if (source_context == NULL) {
        return false;
    }
    return source_context->source_count > 0U;
}

static int plan_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    if (select_source_context_is_joined(source_context)) {
        return plan_joined_select_wildcard_columns(database, source_context, out_plan);
    }
    return plan_single_source_select_wildcard_columns(
        database,
        table_columns,
        table_column_count,
        out_plan
    );
}

static int plan_joined_select_wildcard_columns(
    struct mylite_db *database,
    const struct select_source_context *source_context,
    struct planned_select *out_plan
) {
    for (size_t source_index = 0U; source_index < source_context->source_count; ++source_index) {
        const struct planned_select_source *source = &source_context->sources[source_index];
        int rc = append_visible_select_columns_from_source(
            database,
            out_plan,
            source_index,
            source->columns,
            source->column_count
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_single_source_select_wildcard_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    return append_visible_select_columns_from_source(
        database,
        out_plan,
        0U,
        table_columns,
        table_column_count
    );
}

static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list) {
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    if (mylite_sql_ast_node_child_count(select_list) == 1U && expression != NULL &&
        expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return true;
    }

    return false;
}

static int append_select_column_from_source(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *alias
) {
    char *alias_text = NULL;
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (plan->column_count == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    required_count = plan->column_count + 1U;
    rc = reserve_select_columns(database, plan, required_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    plan->columns[plan->column_count] = *column;
    plan->column_aliases[plan->column_count] = alias_text;
    plan->column_source_indexes[plan->column_count] = source_index;
    plan->column_count = required_count;

    return MYLITE_OK;
}

static int reserve_select_columns(
    struct mylite_db *database,
    struct planned_select *plan,
    size_t required_capacity
) {
    enum { initial_select_column_capacity = 4 };

    struct mylite_catalog_column_descriptor *columns = NULL;
    char **aliases = NULL;
    size_t *source_indexes = NULL;
    size_t capacity = plan->column_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_select_column_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*columns) || capacity > SIZE_MAX / sizeof(*aliases) ||
        capacity > SIZE_MAX / sizeof(*source_indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    columns = realloc(plan->columns, capacity * sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->columns = columns;
    aliases = realloc(plan->column_aliases, capacity * sizeof(*aliases));
    if (aliases == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_aliases = aliases;
    source_indexes = realloc(plan->column_source_indexes, capacity * sizeof(*source_indexes));
    if (source_indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_source_indexes = source_indexes;
    plan->column_capacity = capacity;
    return MYLITE_OK;
}

static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
) {
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    *out_column = NULL;
    expression = unwrap_parenthesized_expression(expression);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
         expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_ERROR;
    }

    *out_column = expression;

    return MYLITE_OK;
}

static bool select_item_qualified_wildcard(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_qualified_wildcard
) {
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    *out_qualified_wildcard = NULL;
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_QUALIFIED_WILDCARD || child_at(item, 1U) != NULL) {
        return false;
    }

    *out_qualified_wildcard = expression;
    return true;
}

static int append_select_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index
) {
    const char *column_name = plan->columns[column_index].name;
    const char *alias = plan->column_aliases[column_index];
    struct mylite_result_column_descriptor descriptor = {0};
    int rc = MYLITE_OK;

    if (alias != NULL) {
        column_name = alias;
    }

    rc = make_select_result_column_descriptor(
        database,
        plan,
        metadata_context,
        column_index,
        column_name,
        &descriptor
    );
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static struct result_column_metadata_context result_column_metadata_context_init(void) {
    return (struct result_column_metadata_context){
        .key_metadata = mylite_execution_loaded_table_key_metadata_reference_init(),
        .has_single_source_metadata = false,
    };
}

static void result_column_metadata_context_deinit(struct result_column_metadata_context *context) {
    if (context == NULL) {
        return;
    }

    mylite_execution_loaded_table_key_metadata_reference_deinit(&context->key_metadata);
    *context = result_column_metadata_context_init();
}

static int load_result_column_metadata_context(
    struct mylite_db *database,
    const struct planned_select *plan,
    struct result_column_metadata_context *context
) {
    if (context == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->source_count != 0U) {
        return MYLITE_OK;
    }
    if (plan->source_columns != NULL && plan->source_column_count != 0U) {
        return load_result_column_metadata_context_for_table_columns(
            database,
            plan->table.table_id,
            plan->source_columns,
            plan->source_column_count,
            context
        );
    }

    return load_result_column_metadata_context_for_table(database, plan->table.table_id, context);
}

static int load_result_column_metadata_context_for_table(
    struct mylite_db *database,
    int64_t table_id,
    struct result_column_metadata_context *context
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_execution_load_table_columns(database, table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_result_column_metadata_context_for_table_columns(
            database,
            table_id,
            columns,
            column_count,
            context
        );
    }
    free(columns);
    if (rc != MYLITE_OK) {
        result_column_metadata_context_deinit(context);
        return rc;
    }

    return MYLITE_OK;
}

static int load_result_column_metadata_context_for_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct result_column_metadata_context *context
) {
    int rc = MYLITE_OK;

    if (context == NULL || columns == NULL || column_count == 0U) {
        return MYLITE_MISUSE;
    }

    rc = mylite_execution_acquire_table_key_metadata(
        database,
        table_id,
        columns,
        column_count,
        &context->key_metadata
    );
    if (rc != MYLITE_OK) {
        result_column_metadata_context_deinit(context);
        return rc;
    }

    context->has_single_source_metadata = true;
    return MYLITE_OK;
}

static struct mylite_result_column_descriptor unknown_result_column_descriptor(const char *label) {
    return (struct mylite_result_column_descriptor){
        .label = label,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };
}

static int make_select_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_select *plan,
    const struct result_column_metadata_context *metadata_context,
    size_t column_index,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    const char *table_name = plan->source.table_name;

    if (plan->source_has_alias) {
        table_name = plan->source_alias;
    }

    *out_descriptor = (struct mylite_result_column_descriptor){
        .label = label,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };

    if (metadata_context == NULL || !metadata_context->has_single_source_metadata) {
        if (plan->source_count == 1U && plan->sources != NULL && plan->sources[0].is_json_table &&
            plan->column_source_indexes != NULL &&
            plan->column_source_indexes[column_index] == 0U) {
            const struct planned_select_source *source = &plan->sources[0];
            struct result_column_metadata_context synthetic_context =
                result_column_metadata_context_init();

            out_descriptor->schema_name = source->source.schema.name;
            out_descriptor->table_name =
                source->has_alias ? source->alias : source->source.table_name;
            out_descriptor->origin_schema_name = "";
            out_descriptor->origin_table_name = "";
            out_descriptor->origin_column_name = column->name;
            out_descriptor->nullable = column->is_nullable;
            return populate_select_result_column_descriptor(
                database,
                &source->table,
                column,
                &synthetic_context,
                out_descriptor
            );
        }
        return MYLITE_OK;
    }

    out_descriptor->schema_name = plan->source.schema.name;
    out_descriptor->table_name = table_name;
    out_descriptor->origin_schema_name = plan->source.schema.name;
    out_descriptor->origin_table_name = plan->table.name;
    out_descriptor->origin_column_name = column->name;
    out_descriptor->nullable = column->is_nullable;

    return populate_select_result_column_descriptor(
        database,
        &plan->table,
        column,
        metadata_context,
        out_descriptor
    );
}

static int populate_select_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
) {
    bool matched = false;
    int rc = populate_numeric_result_column_descriptor(database, column, descriptor, &matched);

    if (rc == MYLITE_OK && !matched) {
        rc =
            populate_string_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_enum_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_set_result_column_descriptor(database, table, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_json_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_spatial_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_binary_result_column_descriptor(database, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_temporal_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_bit_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!matched) {
        set_unsupported_error(
            database,
            "result metadata supports only current baseline column descriptors"
        );
        return MYLITE_ERROR;
    }

    apply_result_column_descriptor_flags(column, metadata_context, descriptor);
    return MYLITE_OK;
}

static int populate_numeric_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct decimal_type_info decimal_info = {0};
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t display_length = 0U;
    bool is_unsigned = false;

    *out_matched = false;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    if (result_metadata_integer_descriptor(
            column->logical_type,
            &descriptor->logical_type,
            &display_length,
            &is_unsigned
        )) {
        descriptor->display_length = display_length;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_year(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_YEAR;
        descriptor->display_length = year_text_length;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                             MYLITE_RESULT_COLUMN_FLAG_ZEROFILL | MYLITE_RESULT_COLUMN_FLAG_NUM;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_decimal(column)) {
        int rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);

        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "result metadata supports only baseline DECIMAL");
            return MYLITE_ERROR;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL;
        descriptor->display_length = decimal_info.precision;
        if (decimal_info.scale != 0U) {
            ++descriptor->display_length;
        }
        if (!decimal_info.is_unsigned) {
            ++descriptor->display_length;
        }
        descriptor->decimals = (uint16_t)decimal_info.scale;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (decimal_info.is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_approximate(column)) {
        int rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);

        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "result metadata supports only baseline approximate numeric descriptors"
            );
            return MYLITE_ERROR;
        }
        if (approximate_info.type_class == APPROXIMATE_TYPE_FLOAT) {
            descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_FLOAT;
            descriptor->display_length = mysql_float_display_length;
        } else {
            descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE;
            descriptor->display_length = mysql_double_display_length;
        }
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NUM;
        if (approximate_info.is_unsigned) {
            descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
        }
        *out_matched = true;
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int populate_string_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    const struct text_family_type_info *text_info = NULL;
    size_t length = 0U;
    bool is_national = column_descriptor_is_national_char_or_varchar(column);
    uint64_t max_bytes_per_character = 0U;
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    if (is_national) {
        descriptor->charset_id = result_metadata_collation_id(
            mylite_connection_statement_collation_connection(database)
        );
        descriptor->collation_id = descriptor->charset_id;
        max_bytes_per_character = result_metadata_collation_max_bytes_per_character(
            mylite_connection_statement_collation_connection(database)
        );
    } else {
        descriptor->charset_id =
            result_metadata_collation_id(column_effective_collation_name(table, column));
        descriptor->collation_id = descriptor->charset_id;
        max_bytes_per_character = column_effective_max_bytes_per_character(table, column);
    }
    if (!is_national && column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    if (column_descriptor_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "result metadata supports only baseline CHAR",
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
        descriptor->display_length = (uint64_t)length * max_bytes_per_character;
        *out_matched = true;
        return MYLITE_OK;
    }
    if (column_descriptor_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "result metadata supports only baseline VARCHAR",
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->display_length = (uint64_t)length * max_bytes_per_character;
        *out_matched = true;
        return MYLITE_OK;
    }

    text_info = text_family_type_info_for_logical_type(column->logical_type);
    if (text_info == NULL) {
        set_unsupported_error(database, "result metadata supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
    descriptor->display_length =
        result_metadata_display_length_cap(text_info->maximum_length * max_bytes_per_character);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_enum_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct enum_type_info info = {0};
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_enum(column)) {
        return MYLITE_OK;
    }
    rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "result metadata supports only baseline ENUM descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id =
        result_metadata_collation_id(column_effective_collation_name(table, column));
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = (uint64_t)info.max_label_character_length *
                                 column_effective_max_bytes_per_character(table, column);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_ENUM;
    if (column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_set_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct set_type_info info = {0};
    int rc = MYLITE_OK;

    *out_matched = false;
    if (!column_descriptor_is_set(column)) {
        return MYLITE_OK;
    }
    rc = set_type_info_for_logical_type(
        database,
        column->logical_type,
        "result metadata supports only baseline SET descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id =
        result_metadata_collation_id(column_effective_collation_name(table, column));
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = (uint64_t)info.max_display_character_length *
                                 column_effective_max_bytes_per_character(table, column);
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_SET;
    if (column_effective_collation_is_binary(table, column)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_json_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = false;
    if (!column_descriptor_is_json(column)) {
        return MYLITE_OK;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_JSON;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = longtext_max_length;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_spatial_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = false;
    if (!column_descriptor_is_spatial(column)) {
        return MYLITE_OK;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_GEOMETRY;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = UINT32_MAX;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY;
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_binary_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    (void)database;
    *out_matched = false;
    if (!column_descriptor_is_binary_string_family(column)) {
        return MYLITE_OK;
    }

    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    if (info == NULL) {
        set_unsupported_error(
            database,
            "result metadata supports only baseline binary descriptors"
        );
        return MYLITE_ERROR;
    }

    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = info->maximum_length;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    if (info->blob_family) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BLOB;
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB;
    } else if (info->fixed_length) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    } else {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    }
    *out_matched = true;
    return MYLITE_OK;
}

static int populate_temporal_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    *out_matched = true;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    if (column_descriptor_is_date(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATE;
        descriptor->display_length = date_text_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_time(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_TIME;
        descriptor->display_length = time_text_maximum_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATETIME;
        descriptor->display_length = datetime_text_length;
        return MYLITE_OK;
    }
    if (column_descriptor_is_timestamp(column)) {
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_TIMESTAMP;
        descriptor->display_length = datetime_text_length;
        return MYLITE_OK;
    }

    *out_matched = false;
    descriptor->flags &= ~MYLITE_RESULT_COLUMN_FLAG_BINARY;
    return MYLITE_OK;
}

static int populate_bit_result_column_descriptor(
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor,
    bool *out_matched
) {
    uint64_t width = 0U;

    *out_matched = false;
    if (!column_descriptor_is_bit(column)) {
        return MYLITE_OK;
    }
    if (bit_width_for_logical_type(column->logical_type, &width) != MYLITE_OK) {
        return MYLITE_ERROR;
    }

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_BIT;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = width;
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNSIGNED;
    *out_matched = true;
    return MYLITE_OK;
}

static void apply_result_column_descriptor_flags(
    const struct mylite_catalog_column_descriptor *column,
    const struct result_column_metadata_context *metadata_context,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct loaded_table_key_metadata *key_metadata = metadata_context->key_metadata.metadata;
    struct loaded_index_info_span indexes = {
        .indexes = NULL,
        .count = 0U,
    };
    bool is_key_part = false;

    if (key_metadata != NULL) {
        indexes = (struct loaded_index_info_span){
            .indexes = key_metadata->indexes,
            .count = key_metadata->index_count,
        };
    }

    if (!column->is_nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
    if (column->is_auto_increment) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_AUTO_INCREMENT;
    }
    if (!column->is_nullable && !column->is_auto_increment &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT;
    }
    if (key_metadata != NULL && mylite_execution_primary_key_info_contains_column_id(
                                    &key_metadata->primary_key,
                                    column->column_id
                                )) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PRI_KEY;
        is_key_part = true;
    } else if (mylite_execution_column_has_unique_secondary_index(indexes, column->column_id)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY;
        is_key_part = true;
    }
    if (mylite_execution_column_has_nonunique_secondary_index(indexes, column->column_id)) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY;
        is_key_part = true;
    }
    if (is_key_part) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PART_KEY;
    }
}

static const char *column_effective_character_set_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_national_char_or_varchar(column)) {
        return national_character_set_name;
    }
    if (column == NULL ||
        (!column_descriptor_is_string_family(column) && !column_descriptor_is_enum(column) &&
         !column_descriptor_is_set(column))) {
        return NULL;
    }
    if (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') {
        const struct mylite_execution_catalog_collation *collation = NULL;

        if (column->character_set_name[0] != '\0') {
            return column->character_set_name;
        }
        collation = collation_by_name(column->collation_name);
        if (collation != NULL) {
            return collation->character_set;
        }
        return MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    }
    if (table != NULL && table->default_charset[0] != '\0') {
        return table->default_charset;
    }
    return MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
}

static const char *column_effective_collation_name(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_national_char_or_varchar(column)) {
        return national_collation_name;
    }
    if (column == NULL ||
        (!column_descriptor_is_string_family(column) && !column_descriptor_is_enum(column) &&
         !column_descriptor_is_set(column))) {
        return NULL;
    }
    if (column->collation_name[0] != '\0') {
        return column->collation_name;
    }
    if (column->character_set_name[0] != '\0') {
        const struct mylite_execution_catalog_character_set *character_set =
            character_set_by_name(column->character_set_name);

        if (character_set != NULL) {
            return character_set->default_collation;
        }
        return MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
    }
    if (table != NULL && table->default_collation[0] != '\0') {
        return table->default_collation;
    }
    return MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
}

static bool column_effective_collation_is_binary(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *collation_name = column_effective_collation_name(table, column);

    return (collation_name != NULL &&
            (text_equals_ascii_case_insensitive(collation_name, "utf8mb4_bin") ||
             text_equals_ascii_case_insensitive(collation_name, "utf8mb4_0900_bin") ||
             text_equals_ascii_case_insensitive(collation_name, "utf8mb3_bin") ||
             text_equals_ascii_case_insensitive(collation_name, "ascii_bin"))) != 0;
}

static uint64_t column_effective_max_bytes_per_character(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *character_set_name = column_effective_character_set_name(table, column);
    uint64_t max_bytes = character_set_max_bytes_per_character(character_set_name);

    return max_bytes != 0U
               ? max_bytes
               : logical_type_max_bytes_per_character(column == NULL ? NULL : column->logical_type);
}

static uint64_t character_set_max_bytes_per_character(const char *character_set_name) {
    const struct mylite_execution_catalog_character_set *character_set =
        character_set_by_name(character_set_name);
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (character_set != NULL) {
        maxlen_span = (struct mylite_sql_source_span){
            .text = character_set->maxlen,
            .length = strlen(character_set->maxlen),
        };
        if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) == MYLITE_OK && maxlen > 0U) {
            return maxlen;
        }
    }
    return 0U;
}

static uint64_t result_metadata_collation_max_bytes_per_character(const char *collation_name) {
    const struct mylite_execution_catalog_scalar_collation *scalar_collation = NULL;
    const struct mylite_execution_catalog_collation *collation = NULL;
    uint64_t max_bytes = 0U;

    scalar_collation = mylite_execution_scalar_collation_info_by_name(collation_name);
    if (scalar_collation != NULL) {
        return scalar_collation->max_bytes_per_character;
    }

    collation = collation_by_name(collation_name);
    if (collation != NULL) {
        max_bytes = character_set_max_bytes_per_character(collation->character_set);
        if (max_bytes != 0U) {
            return max_bytes;
        }
    }

    max_bytes = character_set_max_bytes_per_character(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET);
    return max_bytes == 0U ? 4U : max_bytes;
}

static uint64_t result_metadata_display_length_cap(uint64_t display_length) {
    return display_length > UINT32_MAX ? UINT32_MAX : display_length;
}

static uint32_t result_metadata_collation_id(const char *collation_name) {
    const struct mylite_execution_catalog_collation *collation = NULL;
    const struct mylite_execution_catalog_scalar_collation *scalar_collation = NULL;
    struct mylite_sql_source_span id_span = {0};
    uint64_t id = 0U;

    scalar_collation = mylite_execution_scalar_collation_info_by_name(collation_name);
    if (scalar_collation != NULL) {
        return scalar_collation->id;
    }

    collation = collation_by_name(collation_name);
    if (collation == NULL) {
        return mysql_collation_utf8mb4_0900_ai_ci_id;
    }

    id_span = (struct mylite_sql_source_span){
        .text = collation->id,
        .length = strlen(collation->id),
    };
    if (parse_unsigned_integer_literal(&id_span, &id) != MYLITE_OK || id > UINT32_MAX) {
        return mysql_collation_utf8mb4_0900_ai_ci_id;
    }
    return (uint32_t)id;
}

static bool result_metadata_integer_descriptor(
    const char *logical_type,
    enum mylite_result_logical_type *out_type,
    uint64_t *out_display_length,
    bool *out_unsigned
) {
    if (logical_type == NULL) {
        return false;
    }
    if (strcmp(logical_type, "TINYINT(1)") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_tinyint_bool_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "TINYINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_signed_tinyint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_TINY;
        *out_display_length = mysql_unsigned_tinyint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "SMALLINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_SHORT;
        *out_display_length = mysql_signed_smallint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_SHORT;
        *out_display_length = mysql_unsigned_smallint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_INT24;
        *out_display_length = mysql_signed_mediumint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_INT24;
        *out_display_length = mysql_unsigned_mediumint_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INTEGER") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONG;
        *out_display_length = mysql_signed_int_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "INT UNSIGNED") == 0 ||
        strcmp(logical_type, "INTEGER UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONG;
        *out_display_length = mysql_unsigned_int_display_length;
        *out_unsigned = true;
        return true;
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
        *out_display_length = mysql_bigint_display_length;
        *out_unsigned = false;
        return true;
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
        *out_display_length = mysql_bigint_display_length;
        *out_unsigned = true;
        return true;
    }

    return false;
}

static int copy_select_item_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_select_item_identifier_alias_text(database, alias, out_text);
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(alias) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_table_option_string_literal(
            database,
            alias,
            out_text,
            NULL,
            (struct table_option_name_policy){
                .identifier_kind = "alias",
                .nul_message = "select-item aliases do not support NUL bytes",
            }
        );
        if (rc == MYLITE_OK) {
            rc = validate_select_item_alias_text(database, out_text);
        }
        return rc;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int copy_select_item_identifier_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    char identifier[select_item_alias_capacity];
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL || alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = alias->span.text;
    source_size = alias->span.length;
    if (source == NULL || source_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (source[0] == '`' || source[0] == '"') {
        rc = copy_quoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    } else {
        rc = copy_unquoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "alias");
        return rc;
    }

    return duplicate_text(database, identifier, out_text);
}

static int validate_select_item_alias_text(struct mylite_db *database, char **text) {
    if (text == NULL || *text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (strlen(*text) > select_item_alias_max_length) {
        free(*text);
        *text = NULL;
        set_identifier_too_long_error(database, "alias");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
