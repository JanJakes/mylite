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
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
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

static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *row_list,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t explicit_column_count = mylite_sql_ast_node_child_count(column_list);
    size_t column_count = explicit_column_count;
    bool omitted_column_list = false;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (column_list == NULL || column_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    omitted_column_list = insert_column_list_is_omitted(column_list);
    if (explicit_column_count == 0U) {
        if (!omitted_column_list || insert_row_list_first_row_is_empty(row_list)) {
            return MYLITE_OK;
        }
        column_count = count_visible_insert_target_columns(plan);
    }
    if (column_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(column_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (explicit_column_count == 0U) {
        collect_visible_insert_target_indexes(plan, indexes);
    } else {
        int rc = collect_explicit_insert_target_indexes(
            database,
            column_list,
            plan,
            indexes,
            column_count
        );

        if (rc != MYLITE_OK) {
            free(indexes);
            return rc;
        }
    }

    *out_indexes = indexes;
    *out_index_count = column_count;

    return MYLITE_OK;
}

static bool insert_column_list_is_omitted(const struct mylite_sql_ast_node *column_list) {
    if (column_list == NULL) {
        return true;
    }
    if (column_list->span.text != NULL) {
        return false;
    }
    if (column_list->span.length != 0U) {
        return false;
    }
    return true;
}

static bool insert_row_list_first_row_is_empty(const struct mylite_sql_ast_node *row_list) {
    const struct mylite_sql_ast_node *first_row = child_at(row_list, 0U);

    if (first_row == NULL) {
        return false;
    }
    if (first_row->kind != MYLITE_SQL_AST_INSERT_ROW) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(first_row) != 0U) {
        return false;
    }
    return true;
}

static size_t count_visible_insert_target_columns(const struct planned_insert *plan) {
    size_t column_count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            ++column_count;
        }
    }

    return column_count;
}

static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
) {
    size_t target_index = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            indexes[target_index] = column_index;
            ++target_index;
        }
    }
}

static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);

        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = find_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &indexes[column_index]
        );
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        column_node = column_node->next_sibling;
    }

    return MYLITE_OK;
}

static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    const char *unsupported_qualified_target_message,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    size_t assignment_count = 0U;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    if (assignment_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (assignment_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(assignment_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    assignment = child_at(assignment_list, 0U);
    for (size_t assignment_index = 0U; assignment_index < assignment_count; ++assignment_index) {
        const struct mylite_sql_ast_node *target = NULL;
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = MYLITE_OK;

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            free(indexes);
            return MYLITE_ERROR;
        }

        target = child_at(assignment, 0U);
        if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(database, unsupported_qualified_target_message);
            free(indexes);
            return MYLITE_ERROR;
        }

        rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
        if (rc == MYLITE_OK) {
            rc = find_column_index(
                plan->columns,
                plan->column_count,
                column_name,
                &indexes[assignment_index]
            );
            if (rc != MYLITE_OK) {
                set_unknown_column_error(database, column_name);
                free(indexes);
                return MYLITE_ERROR;
            }
        } else {
            free(indexes);
            return rc;
        }
        assignment = assignment->next_sibling;
    }

    *out_indexes = indexes;
    *out_index_count = assignment_count;

    return MYLITE_OK;
}

static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t left = 0U; left < target_count; ++left) {
        for (size_t right = left + 1U; right < target_count; ++right) {
            if (target_indexes[left] == target_indexes[right]) {
                set_column_specified_twice_error(database, columns[target_indexes[right]].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool insert_allows_implicit_default_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan
) {
    if (plan == NULL) {
        return false;
    }
    return dml_allows_missing_default_adjustment(database, plan->ignore_errors);
}

static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        bool column_is_targeted = false;

        for (size_t target_index = 0U; target_index < target_count; ++target_index) {
            if (target_indexes[target_index] == column_index) {
                column_is_targeted = true;
                break;
            }
        }
        if (!column_is_targeted && plan->columns[column_index].is_generated) {
            continue;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !plan->columns[column_index].is_nullable &&
            plan->columns[column_index].default_kind ==
                MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
            if (insert_allows_implicit_default_adjustment(database, plan)) {
                continue;
            }
            set_bad_null_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !column_descriptor_uses_enum_implicit_missing_default(&plan->columns[column_index]) &&
            (plan->columns[column_index].default_kind ==
                 MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT ||
             (!plan->columns[column_index].is_nullable &&
              !column_default_kind_materializes_value(plan->columns[column_index].default_kind)))) {
            if (insert_allows_implicit_default_adjustment(database, plan)) {
                continue;
            }
            set_no_default_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_insert_row_shapes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    size_t target_count
) {
    const struct mylite_sql_ast_node *row_node = NULL;
    size_t row_count = 0U;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    row_count = mylite_sql_ast_node_child_count(row_list);
    row_node = child_at(row_list, 0U);
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        size_t row_number = row_index + 1U;

        if (row_node == NULL || row_node->kind != MYLITE_SQL_AST_INSERT_ROW) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_child_count(row_node) != target_count) {
            set_column_count_mismatch_error(database, row_number);
            return MYLITE_ERROR;
        }
        row_node = row_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *row_node = NULL;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    plan->row_count = mylite_sql_ast_node_child_count(row_list);
    if (plan->row_count == 0U || plan->row_count > SIZE_MAX / sizeof(*plan->rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->rows = calloc(plan->row_count, sizeof(*plan->rows));
    if (plan->rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    row_node = child_at(row_list, 0U);
    for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
        int rc = plan_insert_row(
            database,
            row_node,
            row_index + 1U,
            target_indexes,
            target_count,
            plan,
            &plan->rows[row_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        row_node = row_node == NULL ? NULL : row_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    const struct mylite_sql_ast_node *value_node = child_at(row_node, 0U);
    int rc = MYLITE_OK;

    if (row_node == NULL || row_node->kind != MYLITE_SQL_AST_INSERT_ROW) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(row_node) != target_count) {
        set_column_count_mismatch_error(database, row_number);
        return MYLITE_ERROR;
    }
    rc = allocate_insert_row_values(database, plan, out_row);

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        size_t column_index = target_indexes[target_index];
        rc = convert_insert_value_for_plan(
            database,
            plan,
            value_node,
            &plan->columns[column_index],
            row_number,
            plan->ignore_errors,
            dml_allows_string_truncation_adjustment(database, plan->ignore_errors),
            &out_row->values[column_index]
        );

        value_node = value_node == NULL ? NULL : value_node->next_sibling;
    }
    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, plan) &&
        row_number == 1U) {
        rc = append_insert_omitted_column_warnings(database, plan, target_indexes, target_count);
    }

    return rc;
}

static int append_insert_omitted_column_warnings(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        bool column_is_targeted = false;

        for (size_t target_index = 0U; target_index < target_count; ++target_index) {
            if (target_indexes[target_index] == column_index) {
                column_is_targeted = true;
                break;
            }
        }
        if (!column_is_targeted && plan->columns[column_index].is_generated) {
            continue;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !plan->columns[column_index].is_nullable &&
            plan->columns[column_index].default_kind ==
                MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
            int rc = append_bad_null_warning(database, plan->columns[column_index].name);

            if (rc != MYLITE_OK) {
                return rc;
            }
        } else if (
            !column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !column_descriptor_uses_enum_implicit_missing_default(&plan->columns[column_index]) &&
            (plan->columns[column_index].default_kind ==
                 MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT ||
             (!plan->columns[column_index].is_nullable &&
              !column_default_kind_materializes_value(plan->columns[column_index].default_kind)))
        ) {
            int rc = append_no_default_warning(database, plan->columns[column_index].name);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }

    return MYLITE_OK;
}

static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    plan->row_count = 1U;
    plan->rows = calloc(1U, sizeof(*plan->rows));
    if (plan->rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = allocate_insert_row_values(database, plan, &plan->rows[0]);
    assignment = child_at(assignment_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        const struct mylite_sql_ast_node *value_node = NULL;
        size_t column_index = target_indexes[target_index];

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        value_node = child_at(assignment, 1U);
        rc = convert_insert_value_for_plan(
            database,
            plan,
            value_node,
            &plan->columns[column_index],
            1U,
            plan->ignore_errors,
            dml_allows_string_truncation_adjustment(database, plan->ignore_errors),
            &plan->rows[0].values[column_index]
        );
        assignment = assignment->next_sibling;
    }
    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, plan)) {
        rc = append_insert_omitted_column_warnings(database, plan, target_indexes, target_count);
    }

    return rc;
}

static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    if (plan->column_count > SIZE_MAX / sizeof(*out_row->values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_row->values = calloc(plan->column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        int rc = allocate_insert_column_value(
            database,
            plan,
            column_index,
            &out_row->values[column_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int allocate_insert_column_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    bool adjust_implicit_default = (insert_allows_implicit_default_adjustment(database, plan) &&
                                    !column_descriptor_is_auto_increment(column)) != 0;

    if (column->is_generated) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return materialize_integer_expression_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = column->default_integer,
        };
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        return copy_decimal_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return convert_bit_literal(
            database,
            &(struct mylite_sql_ast_node){
                .kind = MYLITE_SQL_AST_LITERAL,
                .span = {.text = column->default_text, .length = strlen(column->default_text)},
                .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
            },
            column,
            1U,
            false,
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return materialize_binary_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return materialize_character_expression_default_value(
            database,
            column,
            dml_allows_string_truncation_adjustment(database, adjust_implicit_default),
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_text_value(database, column->default_text, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return make_current_timestamp_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return make_current_date_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return make_current_time_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        if (!adjust_implicit_default || column->is_nullable) {
            out_value->is_null = true;
            return MYLITE_OK;
        }
        return make_insert_ignore_implicit_value(database, column, out_value);
    }
    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (!adjust_implicit_default || column->is_nullable) {
        out_value->is_null = true;
        return MYLITE_OK;
    }

    return make_insert_ignore_implicit_value(database, column, out_value);
}

static int make_insert_ignore_implicit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_string_family(column) || column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};
        int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    }
    if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->integer = 0;
    return MYLITE_OK;
}

static bool dml_allows_missing_default_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return (ignore_errors || !session_sql_mode_is_strict(database)) != 0;
}

static bool dml_allows_string_truncation_adjustment(
    const struct mylite_db *database,
    bool ignore_errors
) {
    return dml_allows_missing_default_adjustment(database, ignore_errors);
}

static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    bool handled = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return convert_insert_unix_timestamp_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_auto_increment(column)) {
        return convert_auto_increment_insert_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        return materialize_dml_default_value(
            database,
            column,
            dml_allows_missing_default_adjustment(database, ignore_errors),
            out_value
        );
    }
    rc = convert_statement_time_value_for_column(database, value_node, column, out_value, &handled);
    if (handled) {
        return rc;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }

    return convert_insert_value_by_descriptor(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        allow_string_truncation_adjustment,
        out_value
    );
}

static bool insert_value_is_unix_timestamp_arithmetic(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return true;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return false;
    }

    left = child_at(value_node, 0U);
    right = child_at(value_node, 1U);
    return (insert_unix_timestamp_now_node_is_admitted(left) &&
            insert_unix_timestamp_delta_node_is_admitted(right)) != 0;
}

static int convert_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    static const struct unix_timestamp_arithmetic_messages messages = {
        .unsupported =
            "INSERT UNIX_TIMESTAMP arithmetic supports only UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP() +/- signed integer literals, and UNIX_TIMESTAMP() +/- NULL",
        .delta_range =
            "INSERT UNIX_TIMESTAMP arithmetic supports only signed 64-bit integer deltas",
    };
    struct integer_column_range range = {0};
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = MYLITE_OK;

    if (column_descriptor_is_auto_increment(column)) {
        set_unsupported_error(
            database,
            "INSERT UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }

    if (!column_descriptor_is_string_family(column)) {
        rc = integer_range_for_column(
            database,
            column,
            "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string targets",
            &range
        );
        (void)range;
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = evaluate_insert_unix_timestamp_arithmetic_value(database, value_node, &messages, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        return convert_null_insert_value(database, column, ignore_errors, out_value);
    }

    if (column_descriptor_is_string_family(column)) {
        return finish_insert_unix_timestamp_text_value(
            database,
            value.integer,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    return finish_insert_unix_timestamp_integer_value(
        database,
        value.integer,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int evaluate_insert_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct scalar_arithmetic_value delta = {.is_null = false, .integer = 0};
    int64_t result = 0;
    bool overflow = false;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){
        .is_null = false,
        .integer = current_timestamp_epoch(database),
    };

    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_unix_timestamp_now_node_is_admitted(value_node)) {
        return MYLITE_OK;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_child_count(value_node) != 2U ||
        !insert_unix_timestamp_now_node_is_admitted(child_at(value_node, 0U))) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    right = child_at(value_node, 1U);
    rc = evaluate_insert_unix_timestamp_delta(database, right, messages, &delta);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (delta.is_null) {
        out_value->is_null = true;
        out_value->integer = 0;
        return MYLITE_OK;
    }

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        overflow = checked_int64_add(out_value->integer, delta.integer, &result);
    } else {
        overflow = checked_int64_subtract(out_value->integer, delta.integer, &result);
    }
    if (overflow) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }

    out_value->integer = result;
    return MYLITE_OK;
}

static int evaluate_insert_unix_timestamp_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct unix_timestamp_arithmetic_messages *messages,
    struct scalar_arithmetic_value *out_value
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    if (messages == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_arithmetic_value){.is_null = false, .integer = 0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            out_value->is_null = true;
            return MYLITE_OK;
        }
        return parse_insert_unix_timestamp_delta_literal(
            database,
            value_node,
            false,
            messages->delta_range,
            out_value
        );
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return parse_insert_unix_timestamp_delta_literal(
        database,
        operand,
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        messages->delta_range,
        out_value
    );
}

static int parse_insert_unix_timestamp_delta_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    const char *delta_range_message,
    struct scalar_arithmetic_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    uint64_t limit = (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;

    if (is_negative) {
        limit = int64_min_magnitude;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        magnitude > limit) {
        set_unsupported_error(database, delta_range_message);
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == int64_min_magnitude) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int finish_insert_unix_timestamp_integer_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    bool is_negative = integer < 0;
    uint64_t magnitude = 0U;

    if (integer == INT64_MIN) {
        magnitude = int64_min_magnitude;
    } else if (is_negative) {
        magnitude = (uint64_t)-integer;
    } else {
        magnitude = (uint64_t)integer;
    }

    out_value->is_null = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
}

static int finish_insert_unix_timestamp_text_value(
    struct mylite_db *database,
    int64_t integer,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct char_text_conversion conversion = {
        .text = NULL,
        .text_length = 0U,
        .row_number = row_number,
    };
    int rc = format_insert_unix_timestamp_integer_text(database, integer, &text, &text_length);

    if (rc != MYLITE_OK) {
        return rc;
    }
    conversion.text = text;
    conversion.text_length = text_length;

    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_varchar(column)) {
        rc =
            convert_varchar_text(database, column, &conversion, allow_string_truncation_adjustment);
    } else if (column_descriptor_is_text_family(column)) {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_string_truncation_adjustment,
            false
        );
    } else {
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(text);
        if (rc == MYLITE_ERROR && !diagnostics_has_error_condition(&database->diagnostics)) {
            set_unsupported_error(
                database,
                "INSERT UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string "
                "targets"
            );
        }
        return rc;
    }

    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int format_insert_unix_timestamp_integer_text(
    struct mylite_db *database,
    int64_t integer,
    char **out_text,
    size_t *out_text_length
) {
    char buffer[integer_text_capacity];
    char *text = NULL;
    int written = 0;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    written = snprintf(buffer, sizeof(buffer), "%" PRId64, integer);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        set_runtime_error(database, "failed to format INSERT UNIX_TIMESTAMP arithmetic value");
        return MYLITE_ERROR;
    }

    text = malloc((size_t)written + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, buffer, (size_t)written + 1U);
    *out_text = text;
    *out_text_length = (size_t)written;
    return MYLITE_OK;
}

static bool insert_unix_timestamp_now_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION &&
            mylite_sql_ast_node_child_count(value_node) == 0U) != 0;
}

static bool insert_unix_timestamp_delta_node_is_admitted(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return false;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }

    operand = unwrap_parenthesized_expression(child_at(value_node, 0U));
    return (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(operand) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
}

static int convert_insert_value_for_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    struct select_source_context source_context = {0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || plan == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            planned_value_deinit(out_value);
            *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
            return MYLITE_OK;
        }
        set_generated_column_value_error(database, column->name, plan->table.name);
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return convert_insert_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            allow_string_truncation_adjustment,
            out_value
        );
    }

    (void)row_number;
    (void)ignore_errors;
    (void)allow_string_truncation_adjustment;
    source_context.source = &plan->target;
    return convert_default_function_value_for_target(
        database,
        value_node,
        &source_context,
        plan->columns,
        plan->column_count,
        column,
        ignore_errors,
        out_value
    );
}

static int convert_insert_value_by_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    bool allow_string_truncation_adjustment,
    struct planned_value *out_value
) {
    if (column_descriptor_is_char(column)) {
        return convert_char_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_varchar_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_text_family_literal(
            database,
            value_node,
            column,
            row_number,
            allow_string_truncation_adjustment,
            false,
            out_value
        );
    }
    if (column_descriptor_is_json(column)) {
        (void)row_number;
        (void)ignore_errors;
        return convert_json_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        (void)ignore_errors;
        return convert_enum_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_set(column)) {
        (void)ignore_errors;
        return convert_set_literal(database, value_node, column, row_number, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_binary_string_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_decimal(column)) {
        return convert_decimal_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_approximate(column)) {
        return convert_approximate_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return convert_time_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        (void)row_number;
        (void)ignore_errors;
        set_unsupported_error(database, "Spatial DML supports only NULL or DEFAULT values");
        return MYLITE_ERROR;
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_auto_increment_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE ||
        (value_node->kind == MYLITE_SQL_AST_LITERAL &&
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL)) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }

    return convert_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_null_insert_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (column->is_nullable) {
        return MYLITE_OK;
    }
    if (!ignore_errors) {
        if (column_descriptor_is_spatial(column)) {
            set_spatial_bad_null_error(database, column->name);
            return MYLITE_ERROR;
        }
        set_bad_null_error(database, column->name);
        return MYLITE_ERROR;
    }

    rc = append_bad_null_warning(database, column->name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};

        rc = decimal_type_info_for_logical_type(column->logical_type, &info);
        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    }
    if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->integer = 0;
    return MYLITE_OK;
}

static int materialize_dml_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool adjust_missing_default,
    struct planned_value *out_value
) {
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return materialize_integer_expression_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = column->default_integer,
        };
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        return copy_decimal_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return convert_bit_literal(
            database,
            &(struct mylite_sql_ast_node){
                .kind = MYLITE_SQL_AST_LITERAL,
                .span = {.text = column->default_text, .length = strlen(column->default_text)},
                .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
            },
            column,
            1U,
            false,
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return materialize_binary_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return materialize_character_expression_default_value(
            database,
            column,
            dml_allows_string_truncation_adjustment(database, adjust_missing_default),
            out_value
        );
    }
    if (column_descriptor_is_approximate(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_approximate_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return make_current_timestamp_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return make_current_date_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return make_current_time_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        return convert_null_insert_value(database, column, adjust_missing_default, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && column->is_nullable) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (!adjust_missing_default) {
        set_no_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return materialize_dml_missing_default_value(database, column, out_value);
}

static int materialize_integer_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_bit(column)) {
        return materialize_bit_expression_default_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_year_value(database, (uint32_t)column->default_integer, out_value);
    }

    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .integer = column->default_integer,
    };
    return MYLITE_OK;
}

static int materialize_bit_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    uint64_t magnitude = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK || column->default_integer < 0) {
        return MYLITE_MISUSE;
    }
    magnitude = (uint64_t)column->default_integer;
    if (magnitude > bit_width_max_value(width)) {
        return MYLITE_MISUSE;
    }

    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = magnitude},
        out_value
    );
}

static int materialize_character_expression_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    size_t text_length = column == NULL ? 0U : strlen(column->default_text);
    char *text = NULL;
    struct char_text_conversion conversion = {0};
    int rc = MYLITE_OK;

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    text = malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, column->default_text, text_length + 1U);

    conversion = (struct char_text_conversion){
        .text = text,
        .text_length = text_length,
        .row_number = 1U,
    };
    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
    } else if (column_descriptor_is_varchar(column)) {
        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
    } else {
        set_runtime_error(database, "invalid generated character default descriptor");
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    return assign_text_value(database, text, conversion.text_length, out_value);
}

static int materialize_binary_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (!column_descriptor_is_binary_string_family(column)) {
        set_runtime_error(database, "invalid binary default descriptor");
        return MYLITE_ERROR;
    }

    rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int validate_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct mylite_catalog_column_descriptor source_column = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = resolve_default_function_source_column(
        database,
        value_node,
        source_context,
        table_columns,
        table_column_count,
        &source_column
    );

    if (rc == MYLITE_OK) {
        rc = materialize_default_function_source_value(database, &source_column, &value);
    }
    planned_value_deinit(&value);
    return rc;
}

static int convert_default_function_value_for_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *target_column,
    bool ignore_errors,
    struct planned_value *out_value
) {
    struct mylite_catalog_column_descriptor source_column = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = resolve_default_function_source_column(
        database,
        value_node,
        source_context,
        table_columns,
        table_column_count,
        &source_column
    );

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (rc == MYLITE_OK &&
        !default_function_source_target_types_are_compatible(&source_column, target_column)) {
        set_unsupported_error(
            database,
            default_function_implicit_conversion_message(target_column)
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = materialize_default_function_source_value(database, &source_column, &value);
    }
    if (rc == MYLITE_OK && value.is_null && !target_column->is_nullable) {
        if (ignore_errors) {
            planned_value_deinit(&value);
            return convert_null_insert_value(database, target_column, true, out_value);
        }
        set_bad_null_error(database, target_column->name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_value = value;
        value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .is_blob = false,
            .is_real = false,
            .integer = 0,
            .real = 0.0,
            .text = NULL,
            .text_length = 0U,
        };
    }
    planned_value_deinit(&value);
    return rc;
}

static int resolve_default_function_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = NULL;
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION ||
        mylite_sql_ast_node_child_count(value_node) != 1U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    column_node = child_at(value_node, 0U);
    if (source_context == NULL || source_context->source == NULL) {
        rc = collect_column_reference_parts(database, column_node, parts, &part_count);
        if (rc == MYLITE_OK) {
            rc = format_column_reference_name(
                database,
                parts,
                part_count,
                column_name,
                sizeof(column_name)
            );
        }
        if (rc == MYLITE_OK) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_FIELD, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "DEFAULT() supports only descriptor column references",
        table_columns,
        table_column_count,
        out_column
    );
}

static int materialize_default_function_source_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *source_column,
    struct planned_value *out_value
) {
    if (source_column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};

    if (column_descriptor_is_auto_increment(source_column)) {
        *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (column_default_kind_is_expression(source_column->default_kind)) {
        set_default_function_expression_error(database);
        return MYLITE_ERROR;
    }
    if (source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
        source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
        source_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        set_unsupported_error(
            database,
            "DEFAULT() does not yet support current date/time column defaults"
        );
        return MYLITE_ERROR;
    }

    return materialize_dml_default_value(database, source_column, false, out_value);
}

static bool default_function_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (source_column == NULL || target_column == NULL) {
        return false;
    }
    if (source_column->column_id == target_column->column_id) {
        return true;
    }
    return (strcmp(source_column->logical_type, target_column->logical_type) == 0 &&
            strcmp(source_column->physical_type, target_column->physical_type) == 0) != 0;
}

static const char *default_function_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    (void)target_column;
    return "DEFAULT() assignment does not support implicit descriptor conversion";
}

static int materialize_dml_missing_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    rc = append_no_default_warning(database, column->name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column->is_nullable) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    } else if (column_descriptor_is_string_family(column) || column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    } else if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    } else if (column_descriptor_is_enum(column)) {
        return make_enum_first_label_value(database, column, out_value);
    } else if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    } else if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    } else if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    } else if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    } else if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};

        rc = decimal_type_info_for_logical_type(column->logical_type, &info);
        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    } else if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    } else if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    } else if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    } else if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    } else if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    } else {
        *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    }
    return MYLITE_OK;
}

static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "INSERT supports only integer, boolean, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING)) {
            set_unsupported_error(
                database,
                "INSERT supports only integer, boolean, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
            return convert_integer_string_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                out_value
            );
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            if (!ignore_errors) {
                set_out_of_range_error(database, column->name, row_number);
                return MYLITE_ERROR;
            }
            out_value->is_null = false;
            return clip_integer_for_column(
                database,
                is_negative,
                column,
                row_number,
                &out_value->integer
            );
        }
    }

    out_value->is_null = false;
    rc = convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int convert_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    bool is_negative = false;
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    uint64_t magnitude = 0U;
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "Integer values support only integer, boolean, NULL, DEFAULT, and exact quoted strings",
        "invalid integer string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = parse_dml_integer_string_text(text, text_length, &magnitude, &is_negative);
    if (parse_result == DML_NUMERIC_STRING_PARSE_INVALID) {
        if (!adjust_errors) {
            set_incorrect_integer_value_error(database, text, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_incorrect_integer_value_warning(database, text, column->name, row_number);
            if (rc == MYLITE_OK) {
                *out_value = (struct planned_value){
                    .is_null = false,
                    .is_text = false,
                    .integer = 0,
                };
            }
        }
    } else if (parse_result == DML_NUMERIC_STRING_PARSE_OVERFLOW) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            out_value->is_null = false;
            rc = clip_integer_for_column(
                database,
                is_negative,
                column,
                row_number,
                &out_value->integer
            );
        }
    } else {
        bool exceeds_range = false;

        rc = dml_integer_value_exceeds_column_range(
            database,
            magnitude,
            is_negative,
            column,
            &exceeds_range
        );
        if (rc == MYLITE_OK && parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED &&
            !exceeds_range) {
            if (!adjust_errors) {
                set_data_truncated_error(database, column->name, row_number);
                rc = MYLITE_ERROR;
            } else {
                rc = append_data_truncated_warning(database, column->name, row_number);
            }
        }
        if (rc == MYLITE_OK) {
            out_value->is_null = false;
            rc = convert_integer_for_column_with_policy(
                database,
                magnitude,
                is_negative,
                column,
                row_number,
                adjust_errors,
                &out_value->integer
            );
        }
    }

    free(text);
    return rc;
}

static enum dml_numeric_string_parse_result parse_dml_integer_string_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    struct dml_integer_decimal_scan scan = {0};
    size_t token_start = 0U;
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;

    if (text == NULL || out_magnitude == NULL || out_is_negative == NULL || text_length == 0U) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    while (token_start < text_length &&
           ascii_numeric_whitespace((unsigned char)text[token_start])) {
        ++token_start;
    }
    if (token_start == text_length ||
        scan_dml_integer_decimal_token(text, text_length, token_start, &scan) !=
            DML_NUMERIC_STRING_PARSE_OK) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    parse_result = round_dml_integer_decimal_scan(text, &scan, out_magnitude, out_is_negative);
    if (parse_result != DML_NUMERIC_STRING_PARSE_OK) {
        return parse_result;
    }
    if (dml_numeric_suffix_is_truncated(text, text_length, scan.token_end)) {
        return DML_NUMERIC_STRING_PARSE_TRUNCATED;
    }
    return DML_NUMERIC_STRING_PARSE_OK;
}

static enum dml_numeric_string_parse_result scan_dml_integer_decimal_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct dml_integer_decimal_scan *out_scan
) {
    size_t index = token_start;
    struct dml_integer_decimal_scan scan = {0};

    if (text == NULL || out_scan == NULL || token_start >= text_length) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    if (text[index] == '+' || text[index] == '-') {
        scan.is_negative = text[index] == '-';
        ++index;
    }
    scan.mantissa_start = index;
    if (index >= text_length) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    index = scan_dml_integer_decimal_mantissa(text, text_length, &scan);
    if (scan.digit_count == 0U) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }
    scan.mantissa_end = index;
    scan.token_end = index;

    if (index < text_length && (text[index] == 'e' || text[index] == 'E')) {
        scan_dml_integer_decimal_exponent_suffix(text, text_length, &scan);
    }

    *out_scan = scan;
    return DML_NUMERIC_STRING_PARSE_OK;
}

static size_t scan_dml_integer_decimal_mantissa(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
) {
    size_t index = 0U;
    bool has_dot = false;
    bool has_nonzero_digit = false;

    if (text == NULL || inout_scan == NULL) {
        return 0U;
    }

    index = inout_scan->mantissa_start;
    while (index < text_length) {
        unsigned char byte = (unsigned char)text[index];

        if (ascii_decimal_digit(byte)) {
            record_dml_integer_decimal_mantissa_digit(
                inout_scan,
                byte,
                has_dot,
                &has_nonzero_digit
            );
            ++index;
        } else if (!has_dot && byte == '.') {
            has_dot = true;
            ++index;
        } else {
            break;
        }
    }
    return index;
}

static void record_dml_integer_decimal_mantissa_digit(
    struct dml_integer_decimal_scan *inout_scan,
    unsigned char byte,
    bool has_dot,
    bool *inout_has_nonzero_digit
) {
    if (inout_scan == NULL || inout_has_nonzero_digit == NULL) {
        return;
    }

    ++inout_scan->digit_count;
    if (has_dot) {
        ++inout_scan->digits_after_dot;
    }
    if (!*inout_has_nonzero_digit && byte == '0') {
        ++inout_scan->leading_zero_count;
        return;
    }
    *inout_has_nonzero_digit = true;
}

static void scan_dml_integer_decimal_exponent_suffix(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
) {
    struct dml_integer_decimal_exponent_scan exponent_scan = {0};
    size_t index = 0U;

    if (text == NULL || inout_scan == NULL) {
        return;
    }

    index = inout_scan->token_end + 1U;
    if (index < text_length && (text[index] == '+' || text[index] == '-')) {
        exponent_scan.is_negative = text[index] == '-';
        ++index;
    }
    exponent_scan.start = index;
    while (index < text_length && ascii_decimal_digit((unsigned char)text[index])) {
        ++index;
    }
    exponent_scan.end = index;

    if (exponent_scan.end > exponent_scan.start) {
        inout_scan->exponent = scan_dml_integer_decimal_exponent(text, &exponent_scan);
        inout_scan->token_end = exponent_scan.end;
    } else if (!dml_numeric_suffix_is_truncated(text, text_length, index)) {
        inout_scan->token_end = index;
    }
}

static int64_t scan_dml_integer_decimal_exponent(
    const char *text,
    const struct dml_integer_decimal_exponent_scan *exponent_scan
) {
    int64_t exponent = 0;

    if (text == NULL || exponent_scan == NULL) {
        return 0;
    }

    for (size_t index = exponent_scan->start; index < exponent_scan->end; ++index) {
        int digit = text[index] - '0';

        if (exponent > (dml_integer_decimal_shift_limit - digit) / decimal_base) {
            if (exponent_scan->is_negative) {
                return -dml_integer_decimal_shift_limit;
            }
            return dml_integer_decimal_shift_limit;
        }
        exponent = (exponent * decimal_base) + digit;
    }
    if (exponent_scan->is_negative) {
        return -exponent;
    }
    return exponent;
}

static int64_t dml_integer_decimal_shift(const struct dml_integer_decimal_scan *scan) {
    int64_t scale = scan->digits_after_dot > (size_t)dml_integer_decimal_shift_limit
                        ? dml_integer_decimal_shift_limit
                        : (int64_t)scan->digits_after_dot;

    if (scan->exponent >= dml_integer_decimal_shift_limit) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent <= -dml_integer_decimal_shift_limit) {
        return -dml_integer_decimal_shift_limit;
    }
    if (scan->exponent > dml_integer_decimal_shift_limit - scale) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent < -dml_integer_decimal_shift_limit + scale) {
        return -dml_integer_decimal_shift_limit;
    }
    return scan->exponent - scale;
}

static enum dml_numeric_string_parse_result round_dml_integer_decimal_scan(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    uint64_t magnitude = 0U;
    int64_t decimal_shift = 0;
    int64_t integer_digit_count = 0;
    unsigned int round_digit = 0U;
    size_t significant_digit_count = 0U;
    bool round_up = false;

    if (text == NULL || scan == NULL || out_magnitude == NULL || out_is_negative == NULL) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = scan->is_negative;
    if (dml_integer_decimal_token_has_only_zero_digits(scan)) {
        *out_is_negative = false;
        return DML_NUMERIC_STRING_PARSE_OK;
    }

    significant_digit_count = scan->digit_count - scan->leading_zero_count;
    decimal_shift = dml_integer_decimal_shift(scan);
    if (decimal_shift >= 0) {
        if (significant_digit_count > (size_t)dml_integer_decimal_shift_limit ||
            decimal_shift > dml_integer_decimal_shift_limit ||
            significant_digit_count + (size_t)decimal_shift >
                (size_t)uint64_decimal_digit_capacity) {
            return DML_NUMERIC_STRING_PARSE_OVERFLOW;
        }
        integer_digit_count = (int64_t)significant_digit_count + decimal_shift;
    } else if ((uint64_t)(-decimal_shift) >= significant_digit_count) {
        integer_digit_count = (int64_t)significant_digit_count + decimal_shift;
    } else {
        integer_digit_count = (int64_t)(significant_digit_count - (size_t)(-decimal_shift));
    }

    if (integer_digit_count > uint64_decimal_digit_capacity) {
        return DML_NUMERIC_STRING_PARSE_OVERFLOW;
    }
    if (integer_digit_count > 0 && !append_dml_integer_decimal_scan_digits(
                                       text,
                                       scan,
                                       (size_t)integer_digit_count,
                                       &magnitude
                                   )) {
        return DML_NUMERIC_STRING_PARSE_OVERFLOW;
    }

    if (integer_digit_count == 0) {
        round_up = (dml_integer_decimal_scan_digit_at(text, scan, 0U, &round_digit) &&
                    round_digit >= decimal_round_half_digit) != 0;
    } else if (integer_digit_count > 0 && (size_t)integer_digit_count < significant_digit_count) {
        round_up = (dml_integer_decimal_scan_digit_at(
                        text,
                        scan,
                        (size_t)integer_digit_count,
                        &round_digit
                    ) &&
                    round_digit >= decimal_round_half_digit) != 0;
    }

    if (round_up && !checked_uint64_increment(&magnitude)) {
        return DML_NUMERIC_STRING_PARSE_OVERFLOW;
    }

    *out_magnitude = magnitude;
    *out_is_negative = (scan->is_negative && magnitude != 0U) != 0;
    return DML_NUMERIC_STRING_PARSE_OK;
}

static bool append_dml_integer_decimal_scan_digits(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t integer_digit_count,
    uint64_t *inout_magnitude
) {
    size_t appended_count = 0U;
    bool in_significant_digits = false;

    if (text == NULL || scan == NULL || inout_magnitude == NULL) {
        return false;
    }

    for (size_t index = scan->mantissa_start;
         index < scan->mantissa_end && appended_count < integer_digit_count;
         ++index) {
        unsigned char byte = (unsigned char)text[index];
        unsigned int digit = 0U;

        if (byte < '0' || byte > '9') {
            continue;
        }
        digit = (unsigned int)(byte - '0');
        if (!in_significant_digits && digit == 0U) {
            continue;
        }
        in_significant_digits = true;
        if (!append_uint64_decimal_digit(inout_magnitude, digit)) {
            return false;
        }
        ++appended_count;
    }

    while (appended_count < integer_digit_count) {
        if (!append_uint64_decimal_digit(inout_magnitude, 0U)) {
            return false;
        }
        ++appended_count;
    }

    return true;
}

static bool dml_integer_decimal_scan_digit_at(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t digit_offset,
    unsigned int *out_digit
) {
    size_t significant_seen = 0U;
    bool in_significant_digits = false;

    if (text == NULL || scan == NULL || out_digit == NULL) {
        return false;
    }

    for (size_t index = scan->mantissa_start; index < scan->mantissa_end; ++index) {
        unsigned char byte = (unsigned char)text[index];
        unsigned int digit = 0U;

        if (byte < '0' || byte > '9') {
            continue;
        }
        digit = (unsigned int)(byte - '0');
        if (!in_significant_digits && digit == 0U) {
            continue;
        }
        in_significant_digits = true;
        if (significant_seen == digit_offset) {
            *out_digit = digit;
            return true;
        }
        ++significant_seen;
    }
    return false;
}

static bool append_uint64_decimal_digit(uint64_t *inout_value, unsigned int digit) {
    if (inout_value == NULL || digit >= (unsigned int)decimal_base) {
        return false;
    }
    if (*inout_value > (UINT64_MAX - digit) / decimal_base) {
        return false;
    }
    *inout_value = (*inout_value * decimal_base) + digit;
    return true;
}

static bool checked_uint64_increment(uint64_t *inout_value) {
    if (inout_value == NULL || *inout_value == UINT64_MAX) {
        return false;
    }
    ++*inout_value;
    return true;
}

static bool dml_integer_decimal_token_has_only_zero_digits(
    const struct dml_integer_decimal_scan *scan
) {
    if (scan == NULL) {
        return true;
    }
    return scan->leading_zero_count == scan->digit_count;
}

static bool dml_numeric_suffix_is_truncated(
    const char *text,
    size_t text_length,
    size_t token_end
) {
    if (text == NULL || token_end > text_length) {
        return false;
    }
    for (size_t index = token_end; index < text_length; ++index) {
        if (!ascii_numeric_whitespace((unsigned char)text[index])) {
            return true;
        }
    }
    return false;
}

static bool ascii_decimal_digit(unsigned char byte) {
    return (byte >= '0' && byte <= '9') != 0;
}

static bool ascii_numeric_whitespace(unsigned char byte) {
    return (byte == ' ' || (byte >= '\t' && byte <= '\r')) != 0;
}

static enum dml_numeric_string_parse_result scan_dml_numeric_string_text(
    const char *text,
    size_t text_length,
    struct dml_numeric_token_scan *out_scan
) {
    size_t token_start = 0U;
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;

    if (text == NULL || out_scan == NULL || text_length == 0U) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }
    while (token_start < text_length &&
           ascii_numeric_whitespace((unsigned char)text[token_start])) {
        ++token_start;
    }
    if (token_start == text_length) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    parse_result = scan_dml_numeric_token(text, text_length, token_start, out_scan);
    if (parse_result != DML_NUMERIC_STRING_PARSE_OK) {
        return parse_result;
    }
    if (dml_numeric_suffix_is_truncated(text, text_length, out_scan->token_end)) {
        return DML_NUMERIC_STRING_PARSE_TRUNCATED;
    }
    return DML_NUMERIC_STRING_PARSE_OK;
}

static enum dml_numeric_string_parse_result scan_dml_numeric_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct dml_numeric_token_scan *out_scan
) {
    struct dml_numeric_token_scan scan = {0};
    size_t index = token_start;

    if (text == NULL || out_scan == NULL || token_start >= text_length) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }
    scan.token_start = token_start;
    index = scan_dml_numeric_optional_sign(text, index, &scan);
    scan.mantissa_start = index;
    if (!scan_dml_numeric_mantissa(text, text_length, &index, &scan)) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }
    scan.mantissa_end = index;
    scan.token_end = index;
    scan_dml_numeric_exponent_suffix(text, text_length, index, &scan);

    *out_scan = scan;
    return DML_NUMERIC_STRING_PARSE_OK;
}

static size_t scan_dml_numeric_optional_sign(
    const char *text,
    size_t index,
    struct dml_numeric_token_scan *scan
) {
    if (text[index] == '+' || text[index] == '-') {
        scan->is_negative = text[index] == '-';
        return index + 1U;
    }
    return index;
}

static bool scan_dml_numeric_mantissa(
    const char *text,
    size_t text_length,
    size_t *inout_index,
    struct dml_numeric_token_scan *scan
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    bool saw_dot = false;
    bool saw_digit = false;

    if (text == NULL || inout_index == NULL || scan == NULL) {
        return false;
    }
    while (index < text_length) {
        unsigned char byte = (unsigned char)text[index];

        if (ascii_decimal_digit(byte)) {
            scan_dml_numeric_mantissa_digit(scan, byte, saw_dot);
            saw_digit = true;
            ++index;
        } else if (!saw_dot && byte == '.') {
            saw_dot = true;
            ++index;
        } else {
            break;
        }
    }
    *inout_index = index;
    return saw_digit;
}

static void scan_dml_numeric_mantissa_digit(
    struct dml_numeric_token_scan *scan,
    unsigned char byte,
    bool saw_dot
) {
    if (scan == NULL) {
        return;
    }
    if (!saw_dot) {
        ++scan->integer_digit_count;
    }
    if (byte != '0') {
        if (!scan->has_nonzero_digits) {
            scan->first_nonzero_digit_index = scan->digit_count;
        }
        scan->last_nonzero_digit_index = scan->digit_count;
        scan->has_nonzero_digits = true;
    }
    ++scan->digit_count;
}

static void scan_dml_numeric_exponent_suffix(
    const char *text,
    size_t text_length,
    size_t index,
    struct dml_numeric_token_scan *scan
) {
    size_t exponent_start = 0U;
    bool exponent_is_negative = false;

    if (text == NULL || scan == NULL || index >= text_length ||
        (text[index] != 'e' && text[index] != 'E')) {
        return;
    }
    ++index;
    if (index < text_length && (text[index] == '+' || text[index] == '-')) {
        exponent_is_negative = text[index] == '-';
        ++index;
    }
    exponent_start = index;
    while (index < text_length && ascii_decimal_digit((unsigned char)text[index])) {
        ++index;
    }
    if (index > exponent_start) {
        scan->exponent =
            scan_dml_numeric_exponent(text, exponent_start, index, exponent_is_negative);
        scan->token_end = index;
    }
}

static int64_t scan_dml_numeric_exponent(
    const char *text,
    size_t start,
    size_t end,
    bool is_negative
) {
    int64_t exponent = 0;

    if (text == NULL || end < start) {
        return 0;
    }
    for (size_t index = start; index < end; ++index) {
        int digit = text[index] - '0';

        if (exponent > (dml_integer_decimal_shift_limit - digit) / decimal_base) {
            if (is_negative) {
                return -(int64_t)dml_integer_decimal_shift_limit;
            }
            return (int64_t)dml_integer_decimal_shift_limit;
        }
        exponent = (exponent * decimal_base) + digit;
    }
    if (is_negative) {
        return -exponent;
    }
    return exponent;
}

static bool dml_numeric_scan_has_nonzero_digits(const struct dml_numeric_token_scan *scan) {
    return (scan != NULL && scan->has_nonzero_digits) != 0;
}

static int copy_dml_numeric_scan_digits(
    struct mylite_db *database,
    const char *text,
    const struct dml_numeric_token_scan *scan,
    char **out_digits
) {
    char *digits = NULL;
    size_t digit_index = 0U;

    if (text == NULL || scan == NULL || out_digits == NULL) {
        return MYLITE_MISUSE;
    }
    *out_digits = NULL;
    if (scan->digit_count > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    digits = calloc(scan->digit_count + 1U, sizeof(*digits));
    if (digits == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = scan->mantissa_start; index < scan->mantissa_end; ++index) {
        if (ascii_decimal_digit((unsigned char)text[index])) {
            if (digit_index >= scan->digit_count) {
                free(digits);
                set_runtime_error(database, "invalid numeric scan digit count");
                return MYLITE_ERROR;
            }
            digits[digit_index] = text[index];
            ++digit_index;
        }
    }
    if (digit_index != scan->digit_count) {
        free(digits);
        set_runtime_error(database, "invalid numeric scan digit count");
        return MYLITE_ERROR;
    }
    *out_digits = digits;
    return MYLITE_OK;
}

static enum dml_numeric_string_parse_result parse_signed_integer_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    uint64_t magnitude = 0U;
    size_t digit_start = 0U;
    bool overflow = false;

    if (text == NULL || out_magnitude == NULL || out_is_negative == NULL || text_length == 0U) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    if (text[0] == '+' || text[0] == '-') {
        *out_is_negative = text[0] == '-';
        digit_start = 1U;
    }
    if (digit_start == text_length) {
        return DML_NUMERIC_STRING_PARSE_INVALID;
    }

    for (size_t index = digit_start; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return DML_NUMERIC_STRING_PARSE_INVALID;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
            continue;
        }
        if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }

    if (overflow) {
        return DML_NUMERIC_STRING_PARSE_OVERFLOW;
    }
    *out_magnitude = magnitude;
    return DML_NUMERIC_STRING_PARSE_OK;
}

static int dml_integer_value_exceeds_column_range(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    bool *out_exceeds_range
) {
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    if (out_exceeds_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_exceeds_range = false;
    rc = integer_range_for_column(
        database,
        column,
        "DML integer string conversion supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        *out_exceeds_range = ((range.negative_abs_max == 0U && magnitude != 0U) ||
                              magnitude > range.negative_abs_max) != 0;
    } else {
        *out_exceeds_range = magnitude > range.positive_max;
    }
    return MYLITE_OK;
}

static int convert_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_enum_string_literal(
            database,
            value_node,
            column,
            row_number,
            true,
            true,
            ENUM_STRING_TRIM_TRAILING_SPACES,
            out_value
        );
    }
    return convert_enum_integer_literal(database, value_node, column, row_number, true, out_value);
}

static int convert_enum_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_ordinal_fallback,
    bool missing_is_error,
    enum enum_string_trailing_space_policy trailing_space_policy,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    const struct enum_label_descriptor *label = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "ENUM values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        value_node,
        "ENUM values support only string literals",
        "ENUM values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "ENUM values must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && trailing_space_policy == ENUM_STRING_TRIM_TRAILING_SPACES) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK) {
        rc = enum_type_info_for_logical_type(
            database,
            column->logical_type,
            "statement supports only baseline ENUM descriptors",
            &info
        );
    }
    if (rc == MYLITE_OK && enum_find_label(&info, text, text_length, &label)) {
        rc = copy_enum_label_value(database, label, out_value);
    } else if (rc == MYLITE_OK && allow_numeric_ordinal_fallback) {
        uint64_t ordinal = 0U;

        if (enum_text_is_unsigned_integer(text, text_length, &ordinal) &&
            enum_find_ordinal(&info, ordinal, &label)) {
            rc = copy_enum_label_value(database, label, out_value);
        } else if (missing_is_error) {
            set_data_truncated_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = assign_text_value(database, text, text_length, out_value);
            text = NULL;
        }
    } else if (rc == MYLITE_OK && missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        rc = MYLITE_ERROR;
    } else if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, out_value);
        text = NULL;
    }

    free(text);
    (void)character_length;
    return rc;
}

static void trim_trailing_ascii_spaces(char *text, size_t *text_length) {
    if (text == NULL || text_length == NULL) {
        return;
    }
    while (*text_length > 0U && text[*text_length - 1U] == ' ') {
        *text_length -= 1U;
    }
    text[*text_length] = '\0';
}

static int convert_enum_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    const struct enum_label_descriptor *label = NULL;
    uint64_t ordinal = 0U;
    bool is_negative = false;
    int rc = enum_literal_ordinal(value_node, &ordinal, &is_negative);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "ENUM values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline ENUM descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_negative && enum_find_ordinal(&info, ordinal, &label)) {
        return copy_enum_label_value(database, label, out_value);
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_enum_no_match_value(database, out_value);
}

static int enum_literal_ordinal(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_ordinal,
    bool *out_is_negative
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;

    if (value_node == NULL || out_ordinal == NULL || out_is_negative == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_ordinal) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    *out_is_negative = is_negative;
    return MYLITE_OK;
}

static int copy_enum_label_value(
    struct mylite_db *database,
    const struct enum_label_descriptor *label,
    struct planned_value *out_value
) {
    char *copy = NULL;

    if (label == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(label->text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (label->text_length != 0U) {
        memcpy(copy, label->text, label->text_length);
    }
    copy[label->text_length] = '\0';
    return assign_text_value(database, copy, label->text_length, out_value);
}

static int copy_enum_no_match_value(struct mylite_db *database, struct planned_value *out_value) {
    char *copy = malloc(1U);

    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy[0] = '\0';
    return assign_text_value(database, copy, 1U, out_value);
}

static int make_enum_first_label_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline ENUM descriptors",
        &info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_enum_label_value(database, &info.labels[0], out_value);
}

static int convert_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_set_string_literal(
            database,
            literal,
            column,
            row_number,
            true,
            true,
            true,
            out_value
        );
    }
    return convert_set_integer_literal(database, value_node, column, row_number, true, out_value);
}

static int convert_set_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    struct set_type_info info = {0};
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_length = 0U;
    bool has_comma = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "SET values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        value_node,
        "SET values support only string literals",
        "SET values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_length) != MYLITE_OK) {
        set_unsupported_error(database, "SET values must be valid UTF-8");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        trim_trailing_ascii_spaces(text, &text_length);
    }
    if (rc == MYLITE_OK) {
        rc = set_type_info_for_logical_type(
            database,
            column->logical_type,
            "statement supports only baseline SET descriptors",
            &info
        );
    }
    if (rc == MYLITE_OK) {
        has_comma = memchr(text, ',', text_length) != NULL;
        if (text_length == 0U) {
            rc = convert_empty_set_string_literal(
                database,
                &info,
                canonical_definition_order,
                out_value
            );
        } else {
            rc = convert_set_member_list_literal(
                database,
                text,
                text_length,
                has_comma,
                &info,
                column,
                row_number,
                allow_numeric_bitmap_fallback,
                missing_is_error,
                canonical_definition_order,
                out_value
            );
        }
    }

    free(text);
    (void)character_length;
    return rc;
}

static int convert_empty_set_string_literal(
    struct mylite_db *database,
    const struct set_type_info *info,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    if (canonical_definition_order) {
        return copy_set_bitmap_value(database, info, 0U, out_value);
    }
    return make_empty_text_value(database, out_value);
}

static int convert_set_member_list_literal(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    struct planned_value *out_value
) {
    struct mylite_dynamic_string display;
    size_t token_start = 0U;
    uint64_t bitmap = 0U;
    bool all_members_known = true;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&display);
    while (rc == MYLITE_OK) {
        size_t token_end = token_start;
        size_t member_index = 0U;
        bool found_member = false;

        while (token_end < text_length && text[token_end] != ',') {
            ++token_end;
        }

        found_member =
            set_find_member(info, text + token_start, token_end - token_start, &member_index);
        if (!found_member) {
            all_members_known = false;
        }
        if (canonical_definition_order) {
            if (!found_member) {
                break;
            }
            bitmap |= UINT64_C(1) << member_index;
        } else {
            rc = append_set_predicate_member_text((struct set_predicate_member_text_request){
                .display = &display,
                .info = info,
                .text = text,
                .token_start = token_start,
                .token_end = token_end,
                .member_index = member_index,
                .found_member = found_member,
            });
        }
        if (token_end == text_length) {
            break;
        }
        token_start = token_end + 1U;
    }
    if (rc == MYLITE_OK) {
        rc = finish_set_member_list_conversion(
            database,
            text,
            text_length,
            has_comma,
            info,
            column,
            row_number,
            allow_numeric_bitmap_fallback,
            missing_is_error,
            canonical_definition_order,
            all_members_known,
            bitmap,
            &display,
            out_value
        );
    }
    mylite_dynamic_string_deinit(&display);
    return rc;
}

static int append_set_predicate_member_text(struct set_predicate_member_text_request request) {
    int rc = MYLITE_OK;

    if (request.display == NULL || request.token_start > request.token_end) {
        return MYLITE_ERROR;
    }
    if (request.found_member) {
        if (request.info == NULL || request.member_index >= request.info->member_count) {
            return MYLITE_ERROR;
        }
    } else if (request.text == NULL) {
        return MYLITE_ERROR;
    }

    if (request.display->length > 0U || request.token_start > 0U) {
        rc = mylite_dynamic_string_append_char(request.display, ',');
    }
    if (rc == MYLITE_OK && request.found_member) {
        rc = mylite_dynamic_string_append(
            request.display,
            request.info->members[request.member_index].text
        );
    } else if (rc == MYLITE_OK) {
        char saved = request.text[request.token_end];

        request.text[request.token_end] = '\0';
        rc = mylite_dynamic_string_append(request.display, request.text + request.token_start);
        request.text[request.token_end] = saved;
    }
    return rc;
}

static int finish_set_member_list_conversion(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    bool has_comma,
    const struct set_type_info *info,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_numeric_bitmap_fallback,
    bool missing_is_error,
    bool canonical_definition_order,
    bool all_members_known,
    uint64_t bitmap,
    struct mylite_dynamic_string *display,
    struct planned_value *out_value
) {
    if (!canonical_definition_order) {
        char *display_text = mylite_dynamic_string_take(display);

        if (display_text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        return assign_text_value(database, display_text, strlen(display_text), out_value);
    }
    if (all_members_known) {
        return copy_set_bitmap_value(database, info, bitmap, out_value);
    }
    if (allow_numeric_bitmap_fallback && !has_comma) {
        uint64_t numeric_bitmap = 0U;

        if (enum_text_is_unsigned_integer(text, text_length, &numeric_bitmap) &&
            set_bitmap_is_valid(info, numeric_bitmap)) {
            return copy_set_bitmap_value(database, info, numeric_bitmap, out_value);
        }
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_set_no_match_value(database, out_value);
}

static int convert_set_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool missing_is_error,
    struct planned_value *out_value
) {
    struct set_type_info info = {0};
    uint64_t bitmap = 0U;
    bool is_negative = false;
    int rc = enum_literal_ordinal(value_node, &bitmap, &is_negative);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SET values support only string or integer literals");
        return MYLITE_ERROR;
    }
    rc = set_type_info_for_logical_type(
        database,
        column->logical_type,
        "statement supports only baseline SET descriptors",
        &info
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_negative && set_bitmap_is_valid(&info, bitmap)) {
        return copy_set_bitmap_value(database, &info, bitmap, out_value);
    }
    if (missing_is_error) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    return copy_set_no_match_value(database, out_value);
}

static int copy_set_bitmap_value(
    struct mylite_db *database,
    const struct set_type_info *info,
    uint64_t bitmap,
    struct planned_value *out_value
) {
    struct mylite_dynamic_string string;
    bool needs_comma = false;
    int rc = MYLITE_OK;

    if (info == NULL || out_value == NULL || !set_bitmap_is_valid(info, bitmap)) {
        return MYLITE_MISUSE;
    }
    if (bitmap == 0U) {
        return make_empty_text_value(database, out_value);
    }

    mylite_dynamic_string_init(&string);
    for (size_t member_index = 0U; rc == MYLITE_OK && member_index < info->member_count;
         ++member_index) {
        if ((bitmap & (UINT64_C(1) << member_index)) == 0U) {
            continue;
        }
        if (needs_comma) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, info->members[member_index].text);
        }
        needs_comma = true;
    }
    if (rc == MYLITE_OK) {
        char *text = mylite_dynamic_string_take(&string);

        if (text == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        } else {
            rc = assign_text_value(database, text, strlen(text), out_value);
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int copy_set_no_match_value(struct mylite_db *database, struct planned_value *out_value) {
    char *copy = malloc(2U);

    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy[0] = '\x01';
    copy[1] = '\0';
    return assign_text_value(database, copy, 1U, out_value);
}

static int convert_char_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "CHAR values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "CHAR values support only string literals",
        "CHAR values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
        text_length = conversion.text_length;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_varchar_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "VARCHAR values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "VARCHAR values support only string literals",
        "VARCHAR values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
        text_length = conversion.text_length;
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_text_family_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TEXT values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "TEXT values support only string literals",
        "TEXT values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };

        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_nonspace_truncation,
            warn_on_trailing_space_truncation
        );
        if (rc == MYLITE_OK) {
            text_length = conversion.text_length;
        }
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }

    out_value->is_null = false;
    out_value->is_text = true;
    out_value->integer = 0;
    out_value->text = text;
    out_value->text_length = text_length;
    return MYLITE_OK;
}

static int convert_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_string_literal(database, value_node, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = convert_binary_bytes_for_column(
            database,
            column,
            row_number,
            ignore_errors,
            &bytes,
            &byte_count
        );
    }
    if (rc != MYLITE_OK) {
        free(bytes);
        return rc;
    }

    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int decode_binary_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "Binary string values support only string, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal_with_policy(
            database,
            value_node,
            "Binary string values support only string and hex literals",
            "invalid binary string literal",
            true,
            out_bytes,
            out_byte_count
        );
    }
    if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX) {
        return decode_binary_hex_literal(database, value_node, out_bytes, out_byte_count);
    }

    set_unsupported_error(
        database,
        "Binary string values support only string, hex, NULL, and DEFAULT values"
    );
    return MYLITE_ERROR;
}

static int decode_binary_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_bytes,
    size_t *out_byte_count
) {
    const struct mylite_sql_source_span *span = NULL;

    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_HEX) {
        set_unsupported_error(database, "Binary string values support only hex literals");
        return MYLITE_ERROR;
    }

    span = &literal_node->span;
    if (span->text == NULL || span->length < 3U) {
        set_unsupported_error(database, "Binary string values support only valid hex literals");
        return MYLITE_ERROR;
    }
    if (span->text[0] == '0' && (span->text[1] == 'x' || span->text[1] == 'X')) {
        return decode_0x_hex_literal(database, span, out_bytes, out_byte_count);
    }
    return decode_quoted_hex_literal(database, span, out_bytes, out_byte_count);
}

static int decode_quoted_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_count = span->length - 3U;
    size_t byte_count = hex_count / 2U;
    char *bytes = NULL;

    if ((span->text[0] != 'x' && span->text[0] != 'X') || span->text[1] != '\'' ||
        span->text[span->length - 1U] != '\'' || (hex_count % 2U) != 0U) {
        set_unsupported_error(database, "Binary string values support only valid hex literals");
        return MYLITE_ERROR;
    }
    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        unsigned char high = 0U;
        unsigned char low = 0U;
        size_t hex_index = 2U + (byte_index * 2U);

        if (hex_nibble_value(span->text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(span->text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)((high << 4U) | low);
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static int decode_0x_hex_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_count = span->length - 2U;
    size_t byte_count = (hex_count + 1U) / 2U;
    size_t hex_index = 2U;
    size_t byte_index = 0U;
    char *bytes = NULL;

    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if ((hex_count % 2U) != 0U) {
        unsigned char low = 0U;

        if (hex_nibble_value(span->text[hex_index], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)low;
        ++byte_index;
        ++hex_index;
    }
    while (hex_index < span->length) {
        unsigned char high = 0U;
        unsigned char low = 0U;

        if (hex_nibble_value(span->text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(span->text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            set_unsupported_error(database, "Binary string values support only valid hex literals");
            return MYLITE_ERROR;
        }
        bytes[byte_index] = (char)((high << 4U) | low);
        ++byte_index;
        hex_index += 2U;
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static int hex_nibble_value(char byte, unsigned char *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (byte >= '0' && byte <= '9') {
        *out_value = (unsigned char)(byte - '0');
        return MYLITE_OK;
    }
    if (byte >= 'a' && byte <= 'f') {
        *out_value = (unsigned char)(byte - 'a' + decimal_base);
        return MYLITE_OK;
    }
    if (byte >= 'A' && byte <= 'F') {
        *out_value = (unsigned char)(byte - 'A' + decimal_base);
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int convert_binary_bytes_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    char **bytes,
    size_t *byte_count
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;

    if (column == NULL || bytes == NULL || *bytes == NULL || byte_count == NULL) {
        return MYLITE_MISUSE;
    }

    info = binary_string_type_info_for_logical_type(column->logical_type, &storage);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline binary descriptors");
        return MYLITE_ERROR;
    }
    if (*byte_count > info->maximum_length) {
        if (!ignore_errors) {
            set_data_too_long_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        int rc = append_data_truncated_warning(database, column->name, row_number);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *byte_count = (size_t)info->maximum_length;
        (*bytes)[*byte_count] = '\0';
    }
    if (info->fixed_length && *byte_count < info->maximum_length) {
        char *padded = malloc((size_t)info->maximum_length + 1U);

        if (padded == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(padded, *bytes, *byte_count);
        memset(padded + *byte_count, 0, (size_t)info->maximum_length - *byte_count);
        padded[info->maximum_length] = '\0';
        free(*bytes);
        *bytes = padded;
        *byte_count = (size_t)info->maximum_length;
    }

    return MYLITE_OK;
}

static int convert_bit_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint64_t magnitude = 0U;
    bool is_negative = false;
    bool parse_overflow = false;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING ||
         mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_HEX)) {
        char *bytes = NULL;
        size_t byte_count = 0U;

        if (mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal_with_policy(
                database,
                value_node,
                "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT "
                "values",
                "invalid BIT string literal",
                true,
                &bytes,
                &byte_count
            );
        } else {
            rc = decode_binary_hex_literal(database, value_node, &bytes, &byte_count);
        }
        if (rc == MYLITE_OK) {
            rc = bit_string_bytes_to_magnitude(bytes, byte_count, &magnitude, &parse_overflow);
        }
        free(bytes);
        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT "
                "values"
            );
            return MYLITE_ERROR;
        }
        if (parse_overflow) {
            if (!ignore_errors) {
                set_data_too_long_error(database, column->name, row_number);
                return MYLITE_ERROR;
            }
            return clip_bit_value_for_column(database, column, row_number, false, out_value);
        }
        return convert_bit_value_magnitude(
            database,
            column,
            magnitude,
            false,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = parse_bit_value_magnitude(value_node, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "BIT values support only integer, boolean, bit, string, hex, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (parse_overflow) {
        if (!ignore_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return clip_bit_value_for_column(database, column, row_number, true, out_value);
    }

    return convert_bit_value_magnitude(
        database,
        column,
        magnitude,
        is_negative,
        row_number,
        ignore_errors,
        out_value
    );
}

static int make_implicit_bit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK) {
        return MYLITE_MISUSE;
    }
    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = 0U},
        out_value
    );
}

static int convert_bit_value_magnitude(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t magnitude,
    bool is_negative,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    uint64_t max_value = 0U;
    int rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "BIT values require baseline BIT descriptors");
        return MYLITE_ERROR;
    }
    max_value = bit_width_max_value(width);
    if ((is_negative && magnitude != 0U) || magnitude > max_value) {
        if (!ignore_errors) {
            set_data_too_long_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_too_long_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        magnitude = max_value;
    }

    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = magnitude},
        out_value
    );
}

static int clip_bit_value_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool include_out_of_range_warning,
    struct planned_value *out_value
) {
    uint64_t width = 0U;
    int rc = MYLITE_OK;

    if (include_out_of_range_warning) {
        rc = append_out_of_range_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK) {
        rc = append_data_too_long_warning(database, column->name, row_number);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = bit_width_for_logical_type(column == NULL ? NULL : column->logical_type, &width);
    if (rc != MYLITE_OK) {
        return MYLITE_MISUSE;
    }
    return make_bit_blob_value(
        database,
        &(struct bit_blob_value){.width = width, .magnitude = bit_width_max_value(width)},
        out_value
    );
}

static int parse_bit_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
) {
    const struct mylite_sql_ast_node *literal = value_node;
    uint64_t boolean_magnitude = 0U;

    if (value_node == NULL || out_magnitude == NULL || out_is_negative == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_magnitude = 0U;
    *out_is_negative = false;
    *out_parse_overflow = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        *out_magnitude = boolean_magnitude;
        return MYLITE_OK;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (parse_unsigned_integer_literal(&literal->span, out_magnitude) != MYLITE_OK) {
            *out_parse_overflow = true;
        }
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_BIT) {
        return parse_bit_literal_magnitude(&literal->span, out_magnitude, out_parse_overflow);
    }
    return MYLITE_ERROR;
}

static int parse_bit_literal_magnitude(
    const struct mylite_sql_source_span *span,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
) {
    size_t bit_start = 0U;
    size_t bit_end = 0U;
    bool significant = false;
    size_t significant_bits = 0U;
    uint64_t magnitude = 0U;

    if (span == NULL || span->text == NULL || out_magnitude == NULL || out_parse_overflow == NULL ||
        span->length < 2U) {
        return MYLITE_ERROR;
    }
    *out_magnitude = 0U;
    *out_parse_overflow = false;
    if (span->length >= 3U && (span->text[0] == 'b' || span->text[0] == 'B') &&
        span->text[1] == '\'' && span->text[span->length - 1U] == '\'') {
        bit_start = 2U;
        bit_end = span->length - 1U;
    } else if (
        span->length >= 3U && span->text[0] == '0' && (span->text[1] == 'b' || span->text[1] == 'B')
    ) {
        bit_start = 2U;
        bit_end = span->length;
    } else {
        return MYLITE_ERROR;
    }

    for (size_t index = bit_start; index < bit_end; ++index) {
        char byte = span->text[index];

        if (byte != '0' && byte != '1') {
            return MYLITE_ERROR;
        }
        if (!significant && byte == '0') {
            continue;
        }
        significant = true;
        if (significant_bits >= bit_max_length) {
            *out_parse_overflow = true;
            return MYLITE_OK;
        }
        magnitude = (magnitude << 1U) | (byte == '1' ? 1U : 0U);
        ++significant_bits;
    }

    *out_magnitude = magnitude;
    return MYLITE_OK;
}

static int bit_string_bytes_to_magnitude(
    const char *bytes,
    size_t byte_count,
    uint64_t *out_magnitude,
    bool *out_parse_overflow
) {
    uint64_t magnitude = 0U;

    if ((bytes == NULL && byte_count != 0U) || out_magnitude == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_magnitude = 0U;
    *out_parse_overflow = false;
    if (byte_count > sizeof(uint64_t)) {
        *out_parse_overflow = true;
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        magnitude = (magnitude << CHAR_BIT) | (uint64_t)(unsigned char)bytes[index];
    }
    *out_magnitude = magnitude;
    return MYLITE_OK;
}

static int make_bit_blob_value(
    struct mylite_db *database,
    const struct bit_blob_value *bit_value,
    struct planned_value *out_value
) {
    size_t byte_count = bit_value == NULL ? 0U : bit_byte_width(bit_value->width);
    char *bytes = NULL;

    if (bit_value == NULL || bit_value->width == 0U || bit_value->width > bit_max_length ||
        out_value == NULL) {
        return MYLITE_MISUSE;
    }
    bytes = calloc(byte_count + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < byte_count; ++index) {
        size_t shift = (byte_count - index - 1U) * byte_bit_count;

        bytes[index] = (char)((bit_value->magnitude >> shift) & byte_all_bits_mask);
    }
    return assign_blob_value(database, bytes, byte_count, out_value);
}

static int format_bit_default_text(
    struct mylite_db *database,
    const char *column_name,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
) {
    size_t position = 0U;
    bool wrote_one = false;

    if (column_name == NULL || buffer == NULL || buffer_size < bit_literal_default_capacity) {
        set_invalid_default_error(database, column_name == NULL ? "" : column_name);
        return MYLITE_ERROR;
    }

    buffer[position++] = 'b';
    buffer[position++] = '\'';
    for (int bit_index = bit_max_length - 1; bit_index >= 0; --bit_index) {
        bool bit_set = ((magnitude >> (unsigned int)bit_index) & 1U) != 0U;

        if (!wrote_one && !bit_set && bit_index != 0) {
            continue;
        }
        if (bit_set) {
            buffer[position++] = '1';
        } else {
            buffer[position++] = '0';
        }
        wrote_one = true;
    }
    buffer[position++] = '\'';
    buffer[position] = '\0';
    return MYLITE_OK;
}

static int convert_decimal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const char true_text[] = "1";
    static const char false_text[] = "0";

    const struct mylite_sql_ast_node *literal = value_node;
    struct decimal_type_info info = {0};
    bool is_negative = false;
    uint64_t boolean_magnitude = 0U;
    int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "DECIMAL values require baseline decimal descriptors");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "DECIMAL values support only numeric, boolean, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "DECIMAL values support only numeric, boolean, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        return canonicalize_decimal_text(
            database,
            boolean_magnitude == 0U ? false_text : true_text,
            1U,
            is_negative,
            &info,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING)) {
        set_unsupported_error(
            database,
            "DECIMAL values support only numeric, boolean, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_decimal_string_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            &info,
            out_value
        );
    }

    return canonicalize_decimal_text(
        database,
        literal->span.text,
        literal->span.length,
        is_negative,
        &info,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_decimal_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct dml_numeric_token_scan scan = {0};
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "DECIMAL values support only numeric, boolean, NULL, DEFAULT, and quoted numeric string "
        "values",
        "invalid DECIMAL string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = scan_dml_numeric_string_text(text, text_length, &scan);
    if (parse_result == DML_NUMERIC_STRING_PARSE_INVALID) {
        if (!adjust_errors) {
            set_incorrect_decimal_value_error(database, text, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_incorrect_decimal_value_warning(database, text, column->name, row_number);
            if (rc == MYLITE_OK) {
                rc = build_decimal_zero_value(database, info, out_value);
            }
        }
    } else if (parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED && !adjust_errors) {
        set_incorrect_decimal_value_error(database, text, column->name, row_number);
        rc = MYLITE_ERROR;
    } else {
        rc = convert_decimal_scanned_string_literal(
            database,
            text,
            &scan,
            parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED,
            column,
            row_number,
            adjust_errors,
            info,
            out_value
        );
    }
    free(text);
    return rc;
}

static int convert_decimal_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    char *decimal_text = NULL;
    bool is_out_of_range = false;
    bool value_done = false;
    int rc = MYLITE_OK;

    if (text == NULL || scan == NULL || column == NULL || info == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (truncated) {
        rc = append_data_truncated_note(database, column->name, row_number);
    }
    if (rc == MYLITE_OK && !dml_numeric_scan_has_nonzero_digits(scan)) {
        rc = build_decimal_zero_value(database, info, out_value);
        value_done = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK && !value_done) {
        rc = build_decimal_text_from_numeric_scan(
            database,
            text,
            scan,
            info,
            &decimal_text,
            &is_out_of_range
        );
        if (rc == MYLITE_OK && is_out_of_range) {
            rc = handle_decimal_out_of_range(
                database,
                info,
                column->name,
                row_number,
                scan->is_negative,
                adjust_errors,
                out_value
            );
        } else if (rc == MYLITE_OK) {
            rc = canonicalize_decimal_text(
                database,
                decimal_text,
                strlen(decimal_text),
                scan->is_negative,
                info,
                column->name,
                row_number,
                adjust_errors,
                out_value
            );
        }
    }
    free(decimal_text);
    return rc;
}

static int build_decimal_text_from_numeric_scan(
    struct mylite_db *database,
    const char *text,
    const struct dml_numeric_token_scan *scan,
    const struct decimal_type_info *info,
    char **out_text,
    bool *out_is_out_of_range
) {
    int64_t decimal_position = 0;
    int64_t fraction_start_digit = 0;
    size_t output_size = 0U;
    size_t write_index = 0U;
    uint64_t fraction_digit_count = 0U;
    char *digits = NULL;
    char *output = NULL;

    if (text == NULL || scan == NULL || info == NULL || out_text == NULL ||
        out_is_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_out_of_range = false;
    if (dml_numeric_scan_exceeds_decimal_range(scan, info)) {
        *out_is_out_of_range = true;
        return MYLITE_OK;
    }

    decimal_position = dml_numeric_scan_decimal_position(scan);
    {
        int rc = copy_dml_numeric_scan_digits(database, text, scan, &digits);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    output_size = dml_numeric_scan_decimal_text_size(scan, info);
    output = calloc(output_size + 1U, sizeof(*output));
    if (output == NULL) {
        free(digits);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (decimal_position <= (int64_t)scan->first_nonzero_digit_index) {
        output[write_index++] = '0';
    } else {
        for (int64_t digit_index = (int64_t)scan->first_nonzero_digit_index;
             digit_index < decimal_position;
             ++digit_index) {
            output[write_index++] = dml_numeric_scan_virtual_digit(digits, scan, digit_index);
        }
    }

    fraction_digit_count = info->scale + 1U;
    if (fraction_digit_count > 0U) {
        output[write_index++] = '.';
    }
    fraction_start_digit = decimal_position;
    for (uint64_t index = 0U; index < fraction_digit_count; ++index) {
        int64_t digit_index = fraction_start_digit + (int64_t)index;
        char digit = dml_numeric_scan_virtual_digit(digits, scan, digit_index);

        if (index == info->scale && digit == '0' &&
            dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(scan, digit_index + 1)) {
            digit = '1';
        }
        output[write_index++] = digit;
    }
    output[write_index] = '\0';
    free(digits);
    *out_text = output;
    return MYLITE_OK;
}

static int64_t dml_numeric_scan_decimal_position(const struct dml_numeric_token_scan *scan) {
    int64_t integer_digits = 0;

    if (scan == NULL) {
        return 0;
    }
    integer_digits = scan->integer_digit_count > (size_t)dml_integer_decimal_shift_limit
                         ? dml_integer_decimal_shift_limit
                         : (int64_t)scan->integer_digit_count;
    if (scan->exponent > dml_integer_decimal_shift_limit - integer_digits) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent < -dml_integer_decimal_shift_limit + integer_digits) {
        return -dml_integer_decimal_shift_limit;
    }
    return integer_digits + scan->exponent;
}

static bool dml_numeric_scan_exceeds_decimal_range(
    const struct dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
) {
    int64_t decimal_position = dml_numeric_scan_decimal_position(scan);
    int64_t integer_digit_count = 0;
    uint64_t integer_capacity = info == NULL ? 0U : info->precision - info->scale;

    if (scan == NULL || info == NULL || !scan->has_nonzero_digits) {
        return false;
    }
    if (info->is_unsigned && scan->is_negative) {
        return true;
    }
    if (decimal_position <= (int64_t)scan->first_nonzero_digit_index) {
        return false;
    }
    integer_digit_count = decimal_position - (int64_t)scan->first_nonzero_digit_index;
    return integer_digit_count > (int64_t)integer_capacity;
}

static size_t dml_numeric_scan_decimal_text_size(
    const struct dml_numeric_token_scan *scan,
    const struct decimal_type_info *info
) {
    int64_t decimal_position = dml_numeric_scan_decimal_position(scan);
    size_t integer_output_digits =
        scan == NULL || decimal_position <= (int64_t)scan->first_nonzero_digit_index
            ? 1U
            : (size_t)(decimal_position - (int64_t)scan->first_nonzero_digit_index);
    size_t fraction_output_digits = info == NULL ? 0U : (size_t)info->scale + 1U;

    return integer_output_digits + (fraction_output_digits == 0U ? 0U : 1U) +
           fraction_output_digits;
}

static char dml_numeric_scan_virtual_digit(
    const char *digits,
    const struct dml_numeric_token_scan *scan,
    int64_t digit_index
) {
    if (digit_index < 0 || digits == NULL || scan == NULL ||
        (size_t)digit_index >= scan->digit_count) {
        return '0';
    }
    return digits[digit_index];
}

static bool dml_numeric_scan_has_nonzero_virtual_digit_at_or_after(
    const struct dml_numeric_token_scan *scan,
    int64_t digit_index
) {
    if (scan == NULL || !scan->has_nonzero_digits) {
        return false;
    }
    if (digit_index <= 0) {
        return true;
    }
    return (uint64_t)digit_index <= scan->last_nonzero_digit_index;
}

static int convert_approximate_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const char true_text[] = "1";
    static const char false_text[] = "0";

    const struct mylite_sql_ast_node *literal = value_node;
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    bool is_negative = false;
    uint64_t boolean_magnitude = 0U;
    double value = 0.0;
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "FLOAT and DOUBLE values require baseline descriptors");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "FLOAT and DOUBLE values support only numeric, boolean, NULL, and DEFAULT values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (boolean_literal_magnitude(literal, &boolean_magnitude)) {
        rc = parse_approximate_literal_value(
            database,
            boolean_magnitude == 0U ? false_text : true_text,
            1U,
            is_negative,
            &info,
            column->name,
            row_number,
            &value
        );
        if (rc == MYLITE_OK) {
            rc = assign_approximate_value(value, out_value);
        }
        return rc;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_FLOAT &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING)) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_approximate_string_literal(
            database,
            literal,
            column,
            row_number,
            ignore_errors,
            &info,
            out_value
        );
    }

    rc = parse_approximate_literal_value(
        database,
        literal->span.text,
        literal->span.length,
        is_negative,
        &info,
        column->name,
        row_number,
        &value
    );
    if (rc == MYLITE_OK) {
        rc = assign_approximate_value(value, out_value);
    }
    return rc;
}

static int convert_approximate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    struct dml_numeric_token_scan scan = {0};
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "FLOAT and DOUBLE values support only numeric, boolean, NULL, DEFAULT, and quoted "
        "numeric string values",
        "invalid FLOAT or DOUBLE string literal",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = scan_dml_numeric_string_text(text, text_length, &scan);
    if (parse_result == DML_NUMERIC_STRING_PARSE_INVALID) {
        if (!adjust_errors) {
            set_data_truncated_error(database, column->name, row_number);
            rc = MYLITE_ERROR;
        } else {
            rc = append_data_truncated_warning(database, column->name, row_number);
            if (rc == MYLITE_OK) {
                rc = assign_approximate_value(0.0, out_value);
            }
        }
    } else {
        rc = convert_approximate_scanned_string_literal(
            database,
            text,
            &scan,
            parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED,
            column,
            row_number,
            adjust_errors,
            info,
            out_value
        );
    }
    free(text);
    return rc;
}

static int convert_approximate_scanned_string_literal(
    struct mylite_db *database,
    const char *text,
    const struct dml_numeric_token_scan *scan,
    bool truncated,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool adjust_errors,
    const struct approximate_type_info *info,
    struct planned_value *out_value
) {
    bool token_is_out_of_range = false;
    bool value_is_out_of_range = false;
    double value = 0.0;
    int rc = parse_approximate_scanned_value(database, text, scan, &value, &token_is_out_of_range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (token_is_out_of_range) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return convert_adjusted_approximate_range_value(
            database,
            info,
            scan->is_negative,
            column->name,
            row_number,
            out_value
        );
    }

    value_is_out_of_range = approximate_value_is_out_of_range(value, info);
    if (truncated) {
        if (!adjust_errors) {
            set_data_truncated_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK && value_is_out_of_range) {
        if (!adjust_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = convert_adjusted_approximate_range_value(
            database,
            info,
            value < 0.0,
            column->name,
            row_number,
            out_value
        );
    } else if (rc == MYLITE_OK) {
        rc = assign_approximate_value(approximate_storage_value(value, info), out_value);
    }
    return rc;
}

static int parse_approximate_scanned_value(
    struct mylite_db *database,
    const char *text,
    const struct dml_numeric_token_scan *scan,
    double *out_value,
    bool *out_is_out_of_range
) {
    size_t token_length = 0U;
    char *token = NULL;
    char *end = NULL;
    double parsed = 0.0;
    int rc = MYLITE_OK;

    if (text == NULL || scan == NULL || out_value == NULL || out_is_out_of_range == NULL ||
        scan->token_end <= scan->token_start) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;
    *out_is_out_of_range = false;
    token_length = scan->token_end - scan->token_start;
    token = calloc(token_length + 1U, sizeof(*token));
    if (token == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(token, text + scan->token_start, token_length);
    token[token_length] = '\0';

    rc = parse_c_locale_double(token, &end, &parsed);
    if (rc == MYLITE_OK && (end == token || end == NULL || *end != '\0')) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !isfinite(parsed)) {
        *out_is_out_of_range = true;
    } else if (rc == MYLITE_OK) {
        *out_value = parsed == 0.0 ? 0.0 : parsed;
    }
    free(token);
    return rc;
}

static int convert_adjusted_approximate_range_value(
    struct mylite_db *database,
    const struct approximate_type_info *info,
    bool is_negative,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info->is_unsigned && is_negative) {
        return assign_approximate_value(0.0, out_value);
    }
    return assign_approximate_value(approximate_endpoint_value(info, is_negative), out_value);
}

static bool approximate_value_is_out_of_range(
    double value,
    const struct approximate_type_info *info
) {
    if (info == NULL || !isfinite(value)) {
        return true;
    }
    if (info->is_unsigned && value < 0.0) {
        return true;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)value;

        return isfinite(single) == 0;
    }
    return false;
}

static double approximate_storage_value(double value, const struct approximate_type_info *info) {
    if (info != NULL && info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)value;

        return single == 0.0F ? 0.0 : (double)single;
    }
    return value == 0.0 ? 0.0 : value;
}

static double approximate_endpoint_value(const struct approximate_type_info *info, bool negative) {
    double value =
        info != NULL && info->type_class == APPROXIMATE_TYPE_FLOAT ? (double)FLT_MAX : DBL_MAX;

    if (negative) {
        return -value;
    }
    return value;
}

static int parse_approximate_literal_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct approximate_type_info *info,
    const char *column_name,
    size_t row_number,
    double *out_value
) {
    char buffer[approximate_numeric_text_capacity];
    char *end = NULL;
    double parsed = 0.0;

    if (text == NULL || info == NULL || column_name == NULL || out_value == NULL ||
        text_length == 0U || text_length >= sizeof(buffer)) {
        set_unsupported_error(
            database,
            "FLOAT and DOUBLE values support only numeric, boolean, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    memcpy(buffer, text, text_length);
    buffer[text_length] = '\0';

    if (parse_c_locale_double(buffer, &end, &parsed) != MYLITE_OK || end == buffer || end == NULL ||
        *end != '\0' || !isfinite(parsed)) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }
    if (is_negative) {
        parsed = -parsed;
    }
    if (parsed == 0.0) {
        parsed = 0.0;
    }
    if (info->is_unsigned && parsed < 0.0) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        float single = (float)parsed;

        if (!isfinite(single)) {
            set_out_of_range_error(database, column_name, row_number);
            return MYLITE_ERROR;
        }
        *out_value = single == 0.0F ? 0.0 : (double)single;
        return MYLITE_OK;
    }

    *out_value = parsed;
    return MYLITE_OK;
}

static int assign_approximate_value(double value, struct planned_value *out_value) {
    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_real = true,
        .integer = 0,
        .real = value == 0.0 ? 0.0 : value,
        .text = NULL,
        .text_length = 0U,
    };
    return MYLITE_OK;
}

static int copy_approximate_default_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    double value = 0.0;
    size_t text_length = column == NULL ? 0U : strlen(column->default_text);
    int rc = column == NULL ? MYLITE_ERROR
                            : approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc == MYLITE_OK) {
        rc = parse_approximate_literal_value(
            database,
            column->default_text,
            text_length,
            false,
            &info,
            column->name,
            1U,
            &value
        );
    }
    if (rc == MYLITE_OK) {
        rc = assign_approximate_value(value, out_value);
    }

    return rc;
}

static int make_zero_approximate_value(struct planned_value *out_value) {
    return assign_approximate_value(0.0, out_value);
}

static int format_approximate_value_text(
    struct mylite_db *database,
    double value,
    const struct approximate_type_info *info,
    const char *context_name,
    char *buffer,
    size_t buffer_size
) {
    if (info == NULL) {
        return MYLITE_MISUSE;
    }
    if (info->type_class == APPROXIMATE_TYPE_FLOAT) {
        return format_float_value_text(database, value, context_name, buffer, buffer_size);
    }

    return format_double_text(
        database,
        value == 0.0 ? 0.0 : value,
        context_name,
        buffer,
        buffer_size
    );
}

static int format_float_value_text(
    struct mylite_db *database,
    double value,
    const char *context_name,
    char *buffer,
    size_t buffer_size
) {
    char candidate[approximate_numeric_text_capacity];
    int written = 0;

    if (context_name == NULL || buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    written = format_c_locale_text(
        candidate,
        sizeof(candidate),
        "%.*g",
        float_text_max_significant_digits,
        value
    );
    if (written < 0 || (size_t)written >= sizeof(candidate)) {
        set_double_format_error(database, context_name);
        return MYLITE_ERROR;
    }
    if (strchr(candidate, 'e') != NULL || strchr(candidate, 'E') != NULL) {
        return copy_normalized_scientific_double_text(
            database,
            candidate,
            context_name,
            buffer,
            buffer_size
        );
    }
    return copy_normalized_double_text(database, candidate, context_name, buffer, buffer_size);
}

static int canonicalize_decimal_text(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_negative,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    struct decimal_literal_parts parts = {0};
    struct decimal_digit_buffer buffer = {0};
    int rc = MYLITE_OK;

    if (text == NULL || info == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    rc = parse_decimal_literal_parts(database, text, text_length, &parts);
    if (rc == MYLITE_OK) {
        rc = build_decimal_digit_buffer(database, text, info, &parts, &buffer);
    }
    if (rc == MYLITE_OK &&
        decimal_value_is_out_of_range(info, is_negative, buffer.digits, buffer.digit_count)) {
        rc = handle_decimal_out_of_range(
            database,
            info,
            column_name,
            row_number,
            is_negative,
            ignore_errors,
            out_value
        );
        free(buffer.digits);
        return rc;
    }
    if (rc == MYLITE_OK && buffer.discarded_nonzero) {
        rc = append_decimal_truncated_note(database, column_name, row_number);
    }
    if (rc == MYLITE_OK) {
        bool keep_negative_sign =
            (is_negative && !decimal_digits_are_zero(buffer.digits, buffer.digit_count)) != 0;

        rc = format_decimal_digits(
            database,
            buffer.digits,
            buffer.digit_count,
            keep_negative_sign,
            info->scale,
            out_value
        );
    }
    free(buffer.digits);
    return rc;
}

static int parse_decimal_literal_parts(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct decimal_literal_parts *out_parts
) {
    size_t integer_end = 0U;
    size_t fraction_start = 0U;
    size_t fraction_end = 0U;
    bool saw_dot = false;
    bool saw_digit = false;

    if (text == NULL || out_parts == NULL) {
        return MYLITE_MISUSE;
    }

    while (integer_end < text_length && text[integer_end] >= '0' && text[integer_end] <= '9') {
        ++integer_end;
        saw_digit = true;
    }
    fraction_start = integer_end;
    fraction_end = fraction_start;
    if (fraction_start < text_length && text[fraction_start] == '.') {
        saw_dot = true;
        ++fraction_start;
        fraction_end = fraction_start;
        while (fraction_end < text_length && text[fraction_end] >= '0' &&
               text[fraction_end] <= '9') {
            ++fraction_end;
            saw_digit = true;
        }
    }
    if (!saw_digit || fraction_end != text_length || (!saw_dot && integer_end == 0U)) {
        set_unsupported_error(
            database,
            "DECIMAL values support only fixed decimal and integer literals"
        );
        return MYLITE_ERROR;
    }

    *out_parts = (struct decimal_literal_parts){
        .integer_end = integer_end,
        .fraction_start = fraction_start,
        .fraction_end = fraction_end,
        .integer_digit_start = 0U,
    };
    while (out_parts->integer_digit_start < integer_end &&
           text[out_parts->integer_digit_start] == '0') {
        ++out_parts->integer_digit_start;
    }

    return MYLITE_OK;
}

static int build_decimal_digit_buffer(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *out_buffer
) {
    size_t digit_capacity = 0U;
    size_t digit_count = 0U;
    size_t integer_digit_count = 0U;
    size_t scale = 0U;

    if (text == NULL || info == NULL || parts == NULL || out_buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if (parts->integer_digit_start > parts->integer_end ||
        parts->integer_end > parts->fraction_start || parts->fraction_start > parts->fraction_end ||
        info->scale > (uint64_t)(SIZE_MAX - 2U)) {
        set_runtime_error(database, "invalid DECIMAL digit buffer bounds");
        return MYLITE_ERROR;
    }
    integer_digit_count = parts->integer_end - parts->integer_digit_start;
    scale = (size_t)info->scale;
    if (integer_digit_count > SIZE_MAX - scale - 2U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    digit_capacity = integer_digit_count + scale + 2U;

    *out_buffer = (struct decimal_digit_buffer){
        .digits = malloc(digit_capacity),
        .digit_count = 0U,
        .digit_capacity = digit_capacity,
        .discarded_nonzero = false,
    };
    if (out_buffer->digits == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = parts->integer_digit_start; index < parts->integer_end; ++index) {
        if (digit_count >= out_buffer->digit_capacity) {
            set_runtime_error(database, "DECIMAL digit buffer overflow");
            return MYLITE_ERROR;
        }
        out_buffer->digits[digit_count] = text[index];
        ++digit_count;
    }
    for (size_t scale_index = 0U; scale_index < scale; ++scale_index) {
        size_t fraction_index = parts->fraction_start + scale_index;

        if (digit_count >= out_buffer->digit_capacity) {
            set_runtime_error(database, "DECIMAL digit buffer overflow");
            return MYLITE_ERROR;
        }
        if (fraction_index < parts->fraction_end) {
            out_buffer->digits[digit_count] = text[fraction_index];
        } else {
            out_buffer->digits[digit_count] = '0';
        }
        ++digit_count;
    }
    out_buffer->digit_count = digit_count;

    return round_decimal_digit_buffer_if_needed(database, text, info, parts, out_buffer);
}

static int round_decimal_digit_buffer_if_needed(
    struct mylite_db *database,
    const char *text,
    const struct decimal_type_info *info,
    const struct decimal_literal_parts *parts,
    struct decimal_digit_buffer *buffer
) {
    size_t round_index =
        parts == NULL || info == NULL ? 0U : parts->fraction_start + (size_t)info->scale;
    int rc = MYLITE_OK;

    if (text == NULL || info == NULL || parts == NULL || buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if ((uint64_t)(parts->fraction_end - parts->fraction_start) <= info->scale) {
        return MYLITE_OK;
    }

    for (size_t index = round_index; index < parts->fraction_end; ++index) {
        if (text[index] != '0') {
            buffer->discarded_nonzero = true;
            break;
        }
    }
    if (text[round_index] < '5') {
        return MYLITE_OK;
    }

    rc = increment_decimal_digits(buffer->digits, &buffer->digit_count, buffer->digit_capacity);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool decimal_value_is_out_of_range(
    const struct decimal_type_info *info,
    bool is_negative,
    const char *digits,
    size_t digit_count
) {
    uint64_t integer_digits = 0U;
    uint64_t integer_capacity = info == NULL ? 0U : info->precision - info->scale;

    if (info == NULL || decimal_digits_are_zero(digits, digit_count)) {
        return false;
    }
    if (digit_count > (size_t)info->scale) {
        integer_digits = (uint64_t)(digit_count - (size_t)info->scale);
    }

    return (integer_digits > integer_capacity || (info->is_unsigned && is_negative)) != 0;
}

static int handle_decimal_out_of_range(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    const char *column_name,
    size_t row_number,
    bool is_negative,
    bool ignore_errors,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (!ignore_errors) {
        set_out_of_range_error(database, column_name, row_number);
        return MYLITE_ERROR;
    }

    rc = append_out_of_range_warning(database, column_name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (info->is_unsigned && is_negative) {
        return build_decimal_zero_value(database, info, out_value);
    }

    return build_decimal_endpoint_value(database, info, is_negative, out_value);
}

static int build_decimal_zero_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    struct planned_value *out_value
) {
    return format_decimal_digits(database, "", 0U, false, info->scale, out_value);
}

static int build_decimal_endpoint_value(
    struct mylite_db *database,
    const struct decimal_type_info *info,
    bool is_negative,
    struct planned_value *out_value
) {
    char digits[decimal_max_precision];

    if (info == NULL || info->precision == 0U || info->precision > decimal_max_precision) {
        return MYLITE_ERROR;
    }
    memset(digits, '9', (size_t)info->precision);
    return format_decimal_digits(
        database,
        digits,
        (size_t)info->precision,
        (is_negative && !info->is_unsigned) != 0,
        info->scale,
        out_value
    );
}

static int assign_decimal_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
) {
    return assign_text_value(database, text, text_length, out_value);
}

static int make_zero_date_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "0000-00-00", out_value);
}

static int make_zero_time_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "00:00:00", out_value);
}

static int make_zero_datetime_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "0000-00-00 00:00:00", out_value);
}

static int make_zero_timestamp_value(struct mylite_db *database, struct planned_value *out_value) {
    return make_zero_datetime_value(database, out_value);
}

static int make_current_timestamp_value(
    struct mylite_db *database,
    struct planned_value *out_value
) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_current_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_sysdate_value(struct mylite_db *database, struct planned_value *out_value) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_sysdate_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_current_date_value(struct mylite_db *database, struct planned_value *out_value) {
    char date_text[date_text_length + 1U];
    int rc = format_current_date_text(database, date_text, sizeof(date_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, date_text, out_value);
}

static int make_current_time_value(struct mylite_db *database, struct planned_value *out_value) {
    char time_text[time_text_minimum_length + 1U];
    int rc = format_current_time_text(database, time_text, sizeof(time_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, time_text, out_value);
}

static int make_utc_timestamp_value(struct mylite_db *database, struct planned_value *out_value) {
    char timestamp_text[datetime_text_length + 1U];
    int rc = format_utc_timestamp_text(database, timestamp_text, sizeof(timestamp_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, timestamp_text, out_value);
}

static int make_utc_date_value(struct mylite_db *database, struct planned_value *out_value) {
    char date_text[date_text_length + 1U];
    int rc = format_utc_date_text(database, date_text, sizeof(date_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, date_text, out_value);
}

static int make_utc_time_value(struct mylite_db *database, struct planned_value *out_value) {
    char time_text[time_text_minimum_length + 1U];
    int rc = format_utc_time_text(database, time_text, sizeof(time_text));

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_text_value(database, time_text, out_value);
}

static int64_t current_timestamp_epoch(const struct mylite_db *database) {
    time_t fallback = 0;

    if (database != NULL && database->session.has_timestamp_override) {
        return database->session.timestamp_override;
    }
    if (database != NULL && database->session.active_statement_time > 0) {
        return database->session.active_statement_time;
    }
    fallback = time(NULL);
    if (fallback < (time_t)0) {
        return 0;
    }
    return (int64_t)fallback;
}

static int format_current_timestamp_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current timestamp buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_sysdate_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid SYSDATE buffer");
        return MYLITE_ERROR;
    }
    if (sysdate_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format SYSDATE value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_current_date_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current date buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current date");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_current_time_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid current time buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_session_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%02d:%02d:%02d",
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format current time");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_timestamp_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC timestamp buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_date_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC date buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC date");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_utc_time_text(struct mylite_db *database, char *buffer, size_t buffer_size) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid UTC time buffer");
        return MYLITE_ERROR;
    }
    if (current_timestamp_utc_parts(database, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%02d:%02d:%02d",
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format UTC time");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int current_timestamp_session_parts(struct mylite_db *database, struct tm *out_time_parts) {
    return session_time_parts_from_epoch(
        database,
        current_timestamp_epoch(database),
        out_time_parts
    );
}

static int sysdate_session_parts(struct mylite_db *database, struct tm *out_time_parts) {
    return session_time_parts_from_epoch(database, current_wall_clock_epoch(), out_time_parts);
}

static int current_timestamp_utc_parts(struct mylite_db *database, struct tm *out_time_parts) {
    return utc_time_parts_from_epoch(database, current_timestamp_epoch(database), out_time_parts);
}

static int format_session_timestamp_epoch_text(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size
) {
    struct tm time_parts;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        set_runtime_error(database, "invalid timestamp metadata buffer");
        return MYLITE_ERROR;
    }
    if (session_time_parts_from_epoch(database, epoch, &time_parts) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        time_parts.tm_year + tm_year_calendar_offset,
        time_parts.tm_mon + 1,
        time_parts.tm_mday,
        time_parts.tm_hour,
        time_parts.tm_min,
        time_parts.tm_sec
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format timestamp metadata");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int session_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
) {
    int offset_seconds = database == NULL ? 0
                                          : database->session.time_zone_offset_minutes *
                                                time_zone_seconds_per_minute;
    time_t time_value = (time_t)0;

    if (out_time_parts == NULL) {
        return MYLITE_MISUSE;
    }
    if ((offset_seconds > 0 && epoch > INT64_MAX - (int64_t)offset_seconds) ||
        (offset_seconds < 0 && epoch < INT64_MIN - (int64_t)offset_seconds)) {
        set_runtime_error(database, "failed to apply session time zone");
        return MYLITE_ERROR;
    }
    epoch += (int64_t)offset_seconds;
    time_value = (time_t)epoch;
    if ((int64_t)time_value != epoch) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#ifdef _WIN32
    if (gmtime_s(out_time_parts, &time_value) != 0) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#else
    if (gmtime_r(&time_value, out_time_parts) == NULL) {
        set_runtime_error(database, "failed to format current timestamp");
        return MYLITE_ERROR;
    }
#endif
    return MYLITE_OK;
}

static int utc_time_parts_from_epoch(
    struct mylite_db *database,
    int64_t epoch,
    struct tm *out_time_parts
) {
    time_t time_value = (time_t)0;

    if (out_time_parts == NULL) {
        return MYLITE_MISUSE;
    }
    time_value = (time_t)epoch;
    if ((int64_t)time_value != epoch) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#ifdef _WIN32
    if (gmtime_s(out_time_parts, &time_value) != 0) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#else
    if (gmtime_r(&time_value, out_time_parts) == NULL) {
        set_runtime_error(database, "failed to format UTC timestamp");
        return MYLITE_ERROR;
    }
#endif
    return MYLITE_OK;
}

static int64_t current_wall_clock_epoch(void) {
    time_t now = time(NULL);

    if (now < (time_t)0) {
        return 0;
    }
    return (int64_t)now;
}

static int assign_text_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    struct planned_value *out_value
) {
    if (text == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = true,
        .is_blob = false,
        .integer = 0,
        .text = text,
        .text_length = text_length,
    };
    (void)database;
    return MYLITE_OK;
}

static int make_json_null_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_text_value(database, "null", out_value);
}

static int make_empty_blob_value(struct mylite_db *database, struct planned_value *out_value) {
    return copy_blob_value(database, NULL, 0U, out_value);
}

static int convert_json_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "JSON values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "JSON values support only string literals",
        "JSON values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return normalize_json_value(database, text, text_length, column->name, out_value);
}

static int normalize_json_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    struct planned_value *out_value
) {
    char *normalized = NULL;
    size_t normalized_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = mylite_json_normalize(text, text_length, &normalized, &normalized_length, &result);

    free(text);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        if (result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            set_unsupported_error(
                database,
                "JSON values support only objects, arrays, strings, signed integer numbers, "
                "booleans, and null in this baseline"
            );
        } else {
            set_invalid_json_text_error(database, result.position, column_name);
        }
        return MYLITE_ERROR;
    }

    return assign_text_value(database, normalized, normalized_length, out_value);
}

static int assign_blob_value(
    struct mylite_db *database,
    char *bytes,
    size_t byte_count,
    struct planned_value *out_value
) {
    if (bytes == NULL || out_value == NULL) {
        free(bytes);
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_blob = true,
        .integer = 0,
        .text = bytes,
        .text_length = byte_count,
    };
    (void)database;
    return MYLITE_OK;
}

static int copy_decimal_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    char *copy = NULL;

    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length + 1U);
    return assign_text_value(database, copy, text_length, out_value);
}

static int copy_text_value(
    struct mylite_db *database,
    const char *text,
    struct planned_value *out_value
) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    char *copy = NULL;

    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length + 1U);
    return assign_text_value(database, copy, text_length, out_value);
}

static int copy_blob_value(
    struct mylite_db *database,
    const void *bytes,
    size_t byte_count,
    struct planned_value *out_value
) {
    char *copy = NULL;

    if (bytes == NULL && byte_count != 0U) {
        return MYLITE_MISUSE;
    }
    copy = malloc(byte_count + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (byte_count != 0U) {
        memcpy(copy, bytes, byte_count);
    }
    copy[byte_count] = '\0';
    return assign_blob_value(database, copy, byte_count, out_value);
}

static bool decimal_digits_are_zero(const char *digits, size_t digit_count) {
    for (size_t index = 0U; index < digit_count; ++index) {
        if (digits[index] != '0') {
            return false;
        }
    }
    return true;
}

static int increment_decimal_digits(char *digits, size_t *digit_count, size_t capacity) {
    size_t index = digit_count == NULL ? 0U : *digit_count;

    if (digits == NULL || digit_count == NULL || capacity == 0U) {
        return MYLITE_MISUSE;
    }
    while (index > 0U) {
        --index;
        if (digits[index] < '9') {
            ++digits[index];
            return MYLITE_OK;
        }
        digits[index] = '0';
    }
    if (*digit_count + 1U > capacity) {
        return MYLITE_NOMEM;
    }
    memmove(digits + 1, digits, *digit_count);
    digits[0] = '1';
    ++(*digit_count);
    return MYLITE_OK;
}

static int format_decimal_digits(
    struct mylite_db *database,
    const char *digits,
    size_t digit_count,
    bool is_negative,
    uint64_t scale,
    struct planned_value *out_value
) {
    size_t scale_size = (size_t)scale;
    size_t integer_digit_count = digit_count > scale_size ? digit_count - scale_size : 0U;
    size_t leading_fraction_zeroes = digit_count < scale_size ? scale_size - digit_count : 0U;
    size_t sign_length = 0U;
    size_t output_length = 0U;
    char *text = NULL;
    size_t output_index = 0U;

    if (is_negative) {
        sign_length = 1U;
    }
    output_length = sign_length + (integer_digit_count == 0U ? 1U : integer_digit_count);
    if (scale_size > 0U) {
        output_length += 1U + scale_size;
    }
    text = malloc(output_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[output_index] = '-';
        ++output_index;
    }
    if (integer_digit_count == 0U) {
        text[output_index] = '0';
        ++output_index;
    } else {
        memcpy(text + output_index, digits, integer_digit_count);
        output_index += integer_digit_count;
    }
    if (scale_size > 0U) {
        text[output_index] = '.';
        ++output_index;
        memset(text + output_index, '0', leading_fraction_zeroes);
        output_index += leading_fraction_zeroes;
        if (digit_count > integer_digit_count) {
            size_t fraction_digit_count = digit_count - integer_digit_count;

            memcpy(text + output_index, digits + integer_digit_count, fraction_digit_count);
            output_index += fraction_digit_count;
        }
    }
    text[output_index] = '\0';
    return assign_decimal_text_value(database, text, output_index, out_value);
}

static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal_with_policy(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        false,
        out_text,
        out_text_length
    );
}

static int decode_sql_string_literal_with_policy(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    bool allow_nul,
    char **out_text,
    size_t *out_text_length
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    bool allow_backslash = false;
    int rc = MYLITE_OK;

    if (unsupported_message == NULL || nul_message == NULL || out_text == NULL ||
        out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (literal_node == NULL || literal_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    text = literal_node->span.text;
    length = literal_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid string literal");
        return MYLITE_ERROR;
    }

    quote = text[0];
    allow_backslash = true;
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        allow_backslash = false;
    }
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (allow_backslash && byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_sql_string_escape(
                database,
                &string,
                text[index],
                nul_message,
                allow_nul
            );
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_text_length = string.length;
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_sql_string_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    const char *nul_message,
    bool allow_nul
) {
    switch (escaped_byte) {
    case '0':
        if (allow_nul) {
            return mylite_dynamic_string_append_char(string, '\0');
        }
        set_unsupported_error(database, nul_message);
        return MYLITE_ERROR;
    case 'n':
        return mylite_dynamic_string_append_char(string, '\n');
    case 'r':
        return mylite_dynamic_string_append_char(string, '\r');
    case 't':
        return mylite_dynamic_string_append_char(string, '\t');
    case 'b':
        return mylite_dynamic_string_append_char(string, '\b');
    case 'Z':
        return mylite_dynamic_string_append_char(string, '\032');
    case '\\':
        return mylite_dynamic_string_append_char(string, '\\');
    case '\'':
        return mylite_dynamic_string_append_char(string, '\'');
    case '"':
        return mylite_dynamic_string_append_char(string, '"');
    case '%':
    case '_': {
        int rc = mylite_dynamic_string_append_char(string, '\\');

        if (rc != MYLITE_OK) {
            return rc;
        }
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static int validate_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid VARCHAR value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "VARCHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "VARCHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }

    rc = varchar_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count > declared_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_varchar_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid VARCHAR value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "VARCHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "VARCHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_varchar(column)) {
        set_unsupported_error(database, "statement supports only baseline VARCHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = varchar_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = utf8_prefix_byte_length_for_character_count(
        (struct utf8_prefix_request){
            .text = conversion->text,
            .text_length = conversion->text_length,
            .character_limit = declared_length,
        },
        &truncated_length,
        &character_count
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count <= declared_length) {
        return MYLITE_OK;
    }
    if (text_is_ascii_spaces(
            conversion->text + truncated_length,
            conversion->text_length - truncated_length
        )) {
        rc = append_data_truncated_note(database, column->name, conversion->row_number);
    } else if (allow_nonspace_truncation) {
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
    } else {
        set_data_too_long_error(database, column->name, conversion->row_number);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    conversion->text[truncated_length] = '\0';
    conversion->text_length = truncated_length;
    return MYLITE_OK;
}

static int convert_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid CHAR value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "CHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "CHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = char_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(conversion->text, conversion->text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = conversion->text_length;
    while (trimmed_length > 0U && conversion->text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count > character_count ||
        character_count - trailing_space_count > declared_length) {
        if (!allow_nonspace_truncation) {
            set_data_too_long_error(database, column->name, conversion->row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = utf8_prefix_byte_length_for_character_count(
            (struct utf8_prefix_request){
                .text = conversion->text,
                .text_length = conversion->text_length,
                .character_limit = declared_length,
            },
            &truncated_length,
            &character_count
        );
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "CHAR values must be valid UTF-8");
            return MYLITE_ERROR;
        }
        conversion->text[truncated_length] = '\0';
        conversion->text_length = truncated_length;
        trim_trailing_ascii_spaces(conversion->text, &conversion->text_length);
        return MYLITE_OK;
    }

    conversion->text[trimmed_length] = '\0';
    conversion->text_length = trimmed_length;
    return MYLITE_OK;
}

static int validate_canonical_char_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid CHAR value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "CHAR values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "CHAR values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    rc = char_length_for_column(database, column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = validation.text_length;
    while (trimmed_length > 0U && validation.text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count > character_count ||
        character_count - trailing_space_count > declared_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }
    if (trimmed_length != validation.text_length) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT into CHAR supports only already-canonical CHAR values"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct varchar_text_validation validation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (validation.text == NULL) {
        set_runtime_error(database, "invalid TEXT value");
        return MYLITE_ERROR;
    }
    if (validation.text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(validation.text, '\0', validation.text_length) != NULL) {
        set_unsupported_error(database, "TEXT values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_text_family(column)) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }

    rc = validate_utf8_text(validation.text, validation.text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    info = text_family_type_info_for_logical_type(column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (validation.text_length > info->maximum_length) {
        set_data_too_long_error(database, column->name, validation.row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_text_family_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct char_text_conversion *conversion,
    bool allow_nonspace_truncation,
    bool warn_on_trailing_space_truncation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    if (conversion == NULL || conversion->text == NULL) {
        set_runtime_error(database, "invalid TEXT value");
        return MYLITE_ERROR;
    }
    if (conversion->text_length > (size_t)INT_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }
    if (memchr(conversion->text, '\0', conversion->text_length) != NULL) {
        set_unsupported_error(database, "TEXT values do not support NUL bytes");
        return MYLITE_ERROR;
    }
    if (column == NULL || !column_descriptor_is_text_family(column)) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }

    rc = validate_utf8_text(conversion->text, conversion->text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    (void)character_count;
    info = text_family_type_info_for_logical_type(column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (conversion->text_length <= info->maximum_length) {
        return MYLITE_OK;
    }
    if (info->maximum_length > SIZE_MAX) {
        set_unsupported_error(database, "TEXT values are too large for this build");
        return MYLITE_ERROR;
    }

    rc = utf8_prefix_byte_length_for_byte_limit(
        (struct utf8_byte_prefix_request){
            .text = conversion->text,
            .text_length = conversion->text_length,
            .byte_limit = (size_t)info->maximum_length,
        },
        &truncated_length
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    if (text_is_ascii_spaces(
            conversion->text + truncated_length,
            conversion->text_length - truncated_length
        )) {
        if (warn_on_trailing_space_truncation) {
            rc = append_data_truncated_warning(database, column->name, conversion->row_number);
        } else {
            rc = append_data_truncated_note(database, column->name, conversion->row_number);
        }
    } else if (allow_nonspace_truncation) {
        rc = append_data_truncated_warning(database, column->name, conversion->row_number);
    } else {
        set_data_too_long_error(database, column->name, conversion->row_number);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    conversion->text[truncated_length] = '\0';
    conversion->text_length = truncated_length;
    return MYLITE_OK;
}

static int validate_canonical_decimal_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *text,
    size_t text_length,
    size_t row_number
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
    bool is_negative = false;
    size_t start = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || column == NULL || !column_descriptor_is_decimal(column)) {
        set_unsupported_error(database, "statement supports only baseline DECIMAL descriptors");
        return MYLITE_ERROR;
    }
    if (text_length > 0U && text[0] == '-') {
        is_negative = true;
        start = 1U;
    }
    rc = decimal_type_info_for_logical_type(column->logical_type, &info);
    if (rc == MYLITE_OK) {
        rc = canonicalize_decimal_text(
            database,
            text + start,
            text_length - start,
            is_negative,
            &info,
            column->name,
            row_number,
            false,
            &value
        );
    }
    if (rc == MYLITE_OK &&
        (value.text_length != text_length || memcmp(value.text, text, text_length) != 0)) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT into DECIMAL supports only already-canonical DECIMAL values"
        );
        rc = MYLITE_ERROR;
    }
    planned_value_deinit(&value);
    return rc;
}

static int validate_utf8_text(const char *text, size_t text_length, size_t *out_character_count) {
    size_t index = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || out_character_count == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(text, text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }

        index += width;
        ++character_count;
    }

    *out_character_count = character_count;
    return MYLITE_OK;
}

static int utf8_prefix_byte_length_for_character_count(
    struct utf8_prefix_request request,
    size_t *out_prefix_length,
    size_t *out_character_count
) {
    size_t index = 0U;
    size_t character_count = 0U;
    size_t prefix_length = 0U;
    int rc = MYLITE_OK;

    if (request.text == NULL || out_prefix_length == NULL || out_character_count == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < request.text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(request.text, request.text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (character_count < request.character_limit) {
            prefix_length = index + width;
        }

        index += width;
        ++character_count;
    }

    if (character_count <= request.character_limit) {
        prefix_length = request.text_length;
    }
    *out_prefix_length = prefix_length;
    *out_character_count = character_count;
    return MYLITE_OK;
}

static int utf8_prefix_byte_length_for_byte_limit(
    struct utf8_byte_prefix_request request,
    size_t *out_prefix_length
) {
    size_t index = 0U;
    size_t prefix_length = 0U;
    int rc = MYLITE_OK;

    if (request.text == NULL || out_prefix_length == NULL) {
        return MYLITE_MISUSE;
    }

    while (index < request.text_length) {
        size_t width = 0U;

        rc = utf8_sequence_width(request.text, request.text_length, index, &width);
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (index > request.byte_limit || width > request.byte_limit - index) {
            break;
        }

        prefix_length = index + width;
        index += width;
    }

    *out_prefix_length = prefix_length;
    return MYLITE_OK;
}

static bool text_is_ascii_spaces(const char *text, size_t text_length) {
    if (text == NULL && text_length != 0U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] != ' ') {
            return false;
        }
    }
    return true;
}

static int utf8_sequence_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    unsigned char byte = (unsigned char)text[index];

    if (byte <= utf8_ascii_max) {
        *out_width = 1U;
        return MYLITE_OK;
    }
    if (byte >= utf8_two_byte_min && byte <= utf8_two_byte_max) {
        return utf8_two_byte_width(text, text_length, index, out_width);
    }
    if (byte == utf8_three_byte_e0) {
        return utf8_three_byte_e0_width(text, text_length, index, out_width);
    }
    if ((byte >= utf8_three_byte_min && byte < utf8_three_byte_first_gap_start) ||
        (byte > utf8_three_byte_first_gap_end && byte <= utf8_three_byte_max)) {
        return utf8_three_byte_plain_width(text, text_length, index, out_width);
    }
    if (byte == utf8_three_byte_first_gap_start) {
        return utf8_three_byte_surrogate_gap_width(text, text_length, index, out_width);
    }
    if (byte == utf8_four_byte_f0) {
        return utf8_four_byte_f0_width(text, text_length, index, out_width);
    }
    if (byte >= utf8_four_byte_min && byte <= utf8_four_byte_max_before_last) {
        return utf8_four_byte_plain_width(text, text_length, index, out_width);
    }
    if (byte == utf8_four_byte_last) {
        return utf8_four_byte_last_width(text, text_length, index, out_width);
    }

    return MYLITE_ERROR;
}

static int utf8_two_byte_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 1U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U])) {
        return MYLITE_ERROR;
    }
    *out_width = 2U;
    return MYLITE_OK;
}

static int utf8_three_byte_e0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length ||
        (unsigned char)text[index + 1U] < utf8_three_byte_e0_second_min ||
        (unsigned char)text[index + 1U] > utf8_three_byte_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_three_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_three_byte_surrogate_gap_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 2U >= text_length || (unsigned char)text[index + 1U] < utf8_continuation_tag ||
        (unsigned char)text[index + 1U] > utf8_surrogate_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U])) {
        return MYLITE_ERROR;
    }
    *out_width = 3U;
    return MYLITE_OK;
}

static int utf8_four_byte_f0_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length ||
        (unsigned char)text[index + 1U] < utf8_four_byte_f0_second_min ||
        (unsigned char)text[index + 1U] > utf8_three_byte_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static int utf8_four_byte_plain_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length || !utf8_byte_is_continuation((unsigned char)text[index + 1U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static int utf8_four_byte_last_width(
    const char *text,
    size_t text_length,
    size_t index,
    size_t *out_width
) {
    if (index + 3U >= text_length || (unsigned char)text[index + 1U] < utf8_continuation_tag ||
        (unsigned char)text[index + 1U] > utf8_four_byte_last_second_max ||
        !utf8_byte_is_continuation((unsigned char)text[index + 2U]) ||
        !utf8_byte_is_continuation((unsigned char)text[index + 3U])) {
        return MYLITE_ERROR;
    }
    *out_width = 4U;
    return MYLITE_OK;
}

static bool utf8_byte_is_continuation(unsigned char byte) {
    return (byte & utf8_continuation_mask) == utf8_continuation_tag;
}

static int char_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
) {
    if (column == NULL || out_length == NULL || !column_descriptor_is_char(column)) {
        set_unsupported_error(database, "statement supports only baseline CHAR descriptors");
        return MYLITE_ERROR;
    }

    return parse_char_descriptor_length(
        database,
        column->logical_type,
        "statement supports only baseline CHAR descriptors",
        out_length
    );
}

static int varchar_length_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t *out_length
) {
    if (column == NULL || out_length == NULL || !column_descriptor_is_varchar(column)) {
        set_unsupported_error(database, "statement supports only baseline VARCHAR descriptors");
        return MYLITE_ERROR;
    }

    return parse_varchar_descriptor_length(
        database,
        column->logical_type,
        "statement supports only baseline VARCHAR descriptors",
        out_length
    );
}

static int parse_char_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t prefix_length = char_descriptor_prefix_length(logical_type);
    size_t syntax_overhead = char_descriptor_syntax_overhead(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        prefix_length == 0U || logical_length <= prefix_length + 1U ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[prefix_length],
        .length = logical_length - syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > char_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int parse_varchar_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
) {
    struct mylite_sql_source_span length_span = {0};
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t prefix_length = varchar_descriptor_prefix_length(logical_type);
    size_t syntax_overhead = varchar_descriptor_syntax_overhead(logical_type);
    uint64_t length = 0U;
    int rc = MYLITE_OK;

    if (out_length == NULL || logical_type == NULL || unsupported_message == NULL ||
        prefix_length == 0U || logical_length <= prefix_length + 1U ||
        logical_type[logical_length - 1U] != ')') {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    length_span = (struct mylite_sql_source_span){
        .text = &logical_type[prefix_length],
        .length = logical_length - syntax_overhead,
    };
    rc = parse_unsigned_integer_literal(&length_span, &length);
    if (rc != MYLITE_OK || length > varchar_max_length) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)length;
    return MYLITE_OK;
}

static int make_empty_text_value(struct mylite_db *database, struct planned_value *out_value) {
    char *text = NULL;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }

    planned_value_deinit(out_value);
    text = malloc(1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text[0] = '\0';
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = true,
        .integer = 0,
        .text = text,
        .text_length = 0U,
    };
    return MYLITE_OK;
}

static int make_implicit_binary_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        column == NULL ? NULL
                       : binary_string_type_info_for_logical_type(column->logical_type, &storage);
    size_t byte_count = 0U;
    char *bytes = NULL;

    if (out_value == NULL || info == NULL) {
        return MYLITE_MISUSE;
    }

    if (info->fixed_length) {
        byte_count = (size_t)info->maximum_length;
    }
    planned_value_deinit(out_value);
    bytes = calloc(byte_count + 1U, sizeof(*bytes));
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .is_blob = true,
        .integer = 0,
        .text = bytes,
        .text_length = byte_count,
    };
    return MYLITE_OK;
}

static int clip_integer_for_column(
    struct mylite_db *database,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "INSERT supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = append_out_of_range_warning(database, column->name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (is_negative) {
        if (range.negative_abs_max == 0U) {
            *out_value = 0;
        } else if (range.negative_abs_max == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)range.negative_abs_max;
        }
        return MYLITE_OK;
    }

    *out_value = (int64_t)range.positive_max;
    return MYLITE_OK;
}

static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL || out_value == NULL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1U;
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0U;
        return true;
    }

    return false;
}

static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || out_value == NULL) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return MYLITE_ERROR;
        }
        value = (value * decimal_base) + digit;
    }

    *out_value = value;

    return MYLITE_OK;
}

static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
) {
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        false,
        out_value
    );
}

static int convert_integer_for_column_with_policy(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "INSERT supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            if (ignore_errors) {
                return clip_integer_for_column(database, true, column, row_number, out_value);
            }
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }

    if (magnitude > range.positive_max) {
        if (ignore_errors) {
            return clip_integer_for_column(database, false, column, row_number, out_value);
        }
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)magnitude;

    return MYLITE_OK;
}

static int plan_row_scalar_select_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
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
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct planned_row_scalar_select_item *items = NULL;
    struct planned_row_scalar_select_item item = {0};
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item.source_expression = child_at(select_item, 0U);
    item.alias = child_at(select_item, 1U);
    rc = plan_row_scalar_expression(
        database,
        item.source_expression,
        plan->has_source,
        source_context,
        table,
        table_columns,
        table_column_count,
        &item.expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(&item.expression);
        return rc;
    }

    required_count = plan->item_count + 1U;
    if (required_count > SIZE_MAX / sizeof(*items)) {
        planned_row_scalar_expression_deinit(&item.expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    items = realloc(plan->items, required_count * sizeof(*items));
    if (items == NULL) {
        planned_row_scalar_expression_deinit(&item.expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->items = items;
    plan->items[plan->item_count] = item;
    plan->item_count = required_count;

    return MYLITE_OK;
}
