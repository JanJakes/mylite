static bool comparison_predicate_rhs_is_column_reference(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *right = child_at(predicate_node, 1U);

    return (right != NULL && (right->kind == MYLITE_SQL_AST_IDENTIFIER ||
                              right->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static int validate_comparison_predicate_column(
    struct mylite_db *database,
    const struct planned_select_predicate_node *node
) {
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_enum(node->column)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_set(node->column)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        !column_descriptor_is_string_family(node->column)) {
        set_unsupported_error(database, "WHERE LIKE predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_regexp(node->operator_kind) &&
        !column_descriptor_is_string_family(node->column) &&
        !join_condition_column_is_integer_family(node->column)) {
        set_unsupported_error(database, "WHERE REGEXP predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(node->column) &&
        !comparison_operator_is_string_predicate(node->operator_kind)) {
        set_unsupported_error(
            database,
            "WHERE string predicates support only =, <=>, <>, !=, <, <=, >, >=, LIKE, REGEXP, "
            "and RLIKE"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_enum(node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_set(node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_exists_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_EXISTS,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    struct planned_exists_subquery *subquery = NULL;
    size_t outer_source_count = 1U;
    int rc = MYLITE_OK;

    if (options == NULL || !options->allow_exists) {
        set_unsupported_error(database, "EXISTS subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(database, "EXISTS subqueries support only an outer table source");
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate_node, 0U),
        &(const struct exists_subquery_outer_context){
            .source_context = source_context,
            .columns = table_columns,
            .column_count = table_column_count,
            .source_count = outer_source_count,
        },
        subquery
    );
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node.exists_subquery = subquery;
    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_exists_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct exists_subquery_outer_context *outer_context,
    struct planned_exists_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_exists_subquery){
        .has_table_source = false,
        .inner_source_index = outer_context->source_count,
    };
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "EXISTS supports only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "EXISTS subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "EXISTS subqueries support only WHERE and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        rc = plan_exists_table_source(database, from_clause, out_subquery);
    }
    if (rc == MYLITE_OK) {
        rc = validate_exists_select_list(database, select_list, out_subquery);
    }
    if (rc == MYLITE_OK && where_clause != NULL && !out_subquery->has_table_source) {
        set_unsupported_error(database, "EXISTS tableless subqueries do not support WHERE");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_subquery->has_table_source) {
        rc = plan_exists_inner_predicate(
            database,
            where_clause,
            outer_context->source_context,
            outer_context->columns,
            outer_context->column_count,
            out_subquery
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_delete_limit(database, limit_clause, &out_subquery->limit);
    }

    return rc;
}

static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
) {
    int rc = MYLITE_OK;

    if (from_clause == NULL) {
        set_unsupported_error(database, "EXISTS subqueries support only descriptor table sources");
        return MYLITE_ERROR;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        return plan_exists_join_source(database, from_clause, out_subquery);
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "EXISTS subqueries support only descriptor table sources");
        return MYLITE_ERROR;
    }

    rc = plan_joined_select_source(database, from_clause, &out_subquery->source);
    if (rc == MYLITE_OK) {
        out_subquery->has_table_source = true;
    }
    return rc;
}

static int plan_exists_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
) {
    struct planned_select joined_plan = {0};
    struct joined_select_temp_nodes temp_nodes = {0};
    int rc = prepare_joined_select_plan(database, from_clause, &joined_plan, &temp_nodes);

    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            joined_plan.sources,
            joined_plan.source_count
        );
    }
    if (rc == MYLITE_OK) {
        for (size_t join_index = 0U; join_index < joined_plan.join_count; ++join_index) {
            if (joined_plan.join_kinds[join_index] != MYLITE_SQL_AST_JOIN_KIND_INNER) {
                set_unsupported_error(
                    database,
                    "EXISTS joined subqueries support only INNER JOIN sources"
                );
                rc = MYLITE_ERROR;
                break;
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_conditions(
            database,
            temp_nodes.join_condition_nodes,
            temp_nodes.join_condition_node_count,
            &joined_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_subquery->sources = joined_plan.sources;
        out_subquery->source_count = joined_plan.source_count;
        out_subquery->join_kinds = joined_plan.join_kinds;
        out_subquery->join_conditions = joined_plan.join_conditions;
        out_subquery->join_count = joined_plan.join_count;
        out_subquery->has_table_source = true;

        joined_plan.sources = NULL;
        joined_plan.source_count = 0U;
        joined_plan.join_kinds = NULL;
        joined_plan.join_conditions = NULL;
        joined_plan.join_count = 0U;
    }
    joined_select_temp_nodes_deinit(&temp_nodes);
    if (rc != MYLITE_OK) {
        planned_select_deinit(&joined_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_exists_join_source(database, out_subquery);
    }
    return rc;
}

static int validate_exists_join_source(
    struct mylite_db *database,
    const struct planned_exists_subquery *subquery
) {
    if (subquery == NULL || subquery->source_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t join_index = 0U; join_index < subquery->join_count; ++join_index) {
        const struct planned_select_join_condition *condition =
            &subquery->join_conditions[join_index];

        if (!condition->has_condition) {
            set_unsupported_error(
                database,
                "EXISTS joined subqueries require descriptor equality join conditions"
            );
            return MYLITE_ERROR;
        }
        if (condition->right_value_is_row_scalar_expression) {
            set_unsupported_error(
                database,
                "EXISTS joined subqueries support only descriptor equality join conditions"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
) {
    struct select_source_context source_context = {0};
    const struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (subquery->source_count > 0U) {
        int rc = init_join_select_source_context(
            subquery->sources,
            subquery->source_count,
            &source_context
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    } else if (subquery->has_table_source) {
        source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
        columns = subquery->source.columns;
        column_count = subquery->source.column_count;
    }

    for (const struct mylite_sql_ast_node *item = child_at(select_list, 0U); item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        struct mylite_catalog_column_descriptor column = {0};

        if (expression == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (expression->kind == MYLITE_SQL_AST_WILDCARD ||
            exists_select_item_is_supported_literal(expression)) {
            continue;
        }
        if (!subquery->has_table_source) {
            set_unsupported_error(database, "EXISTS tableless subqueries support only literals");
            return MYLITE_ERROR;
        }
        if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
            expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
            set_unsupported_error(
                database,
                "EXISTS subqueries support only literal or descriptor-column select items"
            );
            return MYLITE_ERROR;
        }
        int rc = resolve_descriptor_column_reference(
            database,
            expression,
            &source_context,
            COLUMN_REFERENCE_FIELD,
            "EXISTS subqueries support only literal or descriptor-column select items",
            columns,
            column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static bool exists_select_item_is_supported_literal(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    kind = mylite_sql_ast_node_literal_kind(expression);
    return (kind == MYLITE_SQL_AST_LITERAL_INTEGER || kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            kind == MYLITE_SQL_AST_LITERAL_FALSE || kind == MYLITE_SQL_AST_LITERAL_NULL ||
            kind == MYLITE_SQL_AST_LITERAL_STRING) != 0;
}

static int plan_exists_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *subquery
) {
    struct select_source_context inner_source_context = {0};
    const struct mylite_catalog_column_descriptor *inner_columns = NULL;
    size_t inner_column_count = 0U;
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .allow_sql_auto_is_null_lookup = true,
        .exists_inner_source_index = subquery->inner_source_index,
        .exists_inner_source_count = subquery->source_count == 0U ? 1U : subquery->source_count,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };
    int rc = MYLITE_OK;

    if (subquery->source_count > 0U) {
        rc = init_join_select_source_context(
            subquery->sources,
            subquery->source_count,
            &inner_source_context
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    } else {
        inner_source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(
            inner_source_context.alias,
            subquery->source.alias,
            sizeof(inner_source_context.alias)
        );
        inner_columns = subquery->source.columns;
        inner_column_count = subquery->source.column_count;
    }

    subquery->predicate = (struct planned_select_predicate){0};
    subquery->predicate.qualify_column_references =
        select_source_context_is_joined(&inner_source_context);
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node_without_exists(
        database,
        child_at(where_clause, 0U),
        &inner_source_context,
        inner_columns,
        inner_column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }

    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(
            &subquery->predicate,
            subquery->inner_source_index
        );
    }
    return rc;
}

static void offset_exists_inner_predicate_source_indexes(
    struct planned_select_predicate *predicate,
    size_t source_index_offset
) {
    if (predicate == NULL || source_index_offset == 0U) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        struct planned_select_predicate_node *node = &predicate->nodes[node_index];

        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON && node->value_is_column_reference) {
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_NULL ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN ||
            node->kind == PLANNED_SELECT_PREDICATE_BETWEEN ||
            node->kind == PLANNED_SELECT_PREDICATE_IN) {
            node->column_source_index += source_index_offset;
        }
    }
}

static int plan_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NOT_EQUAL &&
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        set_unsupported_error(database, "EXISTS correlated predicates support only =, <=>, and <>");
        return MYLITE_ERROR;
    }

    rc = resolve_exists_column_reference(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        options->outer_source_context,
        options->outer_columns,
        options->outer_column_count,
        options,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_exists_column_reference(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            options->outer_source_context,
            options->outer_columns,
            options->outer_column_count,
            options,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!exists_correlated_column_comparison_is_supported(
            &node,
            options->exists_inner_source_index,
            options->exists_inner_source_count
        )) {
        set_unsupported_error(
            database,
            "EXISTS correlated predicates support one inner and one outer integer column"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int resolve_exists_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *inner_source_context,
    const struct mylite_catalog_column_descriptor *inner_columns,
    size_t inner_column_count,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    const struct select_predicate_plan_options *options,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    bool resolved = false;
    int rc = MYLITE_OK;

    *out_column = NULL;
    *out_source_index = 0U;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "EXISTS correlated predicates require descriptor columns");
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

    rc = resolve_exists_column_reference_in_source(
        database,
        column_node,
        parts,
        part_count,
        column_name,
        inner_source_context,
        inner_columns,
        inner_column_count,
        out_column,
        out_source_index,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        if (resolved) {
            *out_source_index += options->exists_inner_source_index;
        }
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        column_node,
        parts,
        part_count,
        column_name,
        outer_source_context,
        outer_columns,
        outer_column_count,
        out_column,
        out_source_index,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        return rc;
    }

    set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
    return MYLITE_ERROR;
}

static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index,
    bool *out_resolved
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    (void)column_node;
    *out_resolved = false;
    if (select_source_context_is_joined(source_context)) {
        return resolve_exists_column_reference_in_joined_source(
            database,
            parts,
            part_count,
            column_name,
            source_context,
            out_column,
            out_source_index,
            out_resolved
        );
    }
    if (!column_reference_qualifier_matches_source(parts, part_count, source_context)) {
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, parts[part_count - 1U], &column_index);
    if (rc != MYLITE_OK) {
        if (part_count > 1U) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    *out_column = &columns[column_index];
    *out_source_index = 0U;
    *out_resolved = true;
    return MYLITE_OK;
}

static int resolve_exists_column_reference_in_joined_source(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index,
    bool *out_resolved
) {
    size_t matching_source_index = 0U;
    size_t matching_column_index = 0U;
    size_t matched_source_count = 0U;
    size_t match_count = 0U;

    *out_resolved = false;
    for (size_t source_index = 0U; source_index < source_context->source_count; ++source_index) {
        const struct planned_select_source *source = &source_context->sources[source_index];
        size_t column_index = 0U;

        if (!column_reference_source_matches_parts(source, parts, part_count)) {
            continue;
        }
        ++matched_source_count;
        if (find_column_index(
                source->columns,
                source->column_count,
                parts[part_count - 1U],
                &column_index
            ) != MYLITE_OK) {
            if (part_count > 1U) {
                set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
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
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
        return MYLITE_ERROR;
    }
    if (match_count == 0U) {
        if (part_count > 1U && matched_source_count > 0U) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    *out_column = &source_context->sources[matching_source_index].columns[matching_column_index];
    *out_source_index = matching_source_index;
    *out_resolved = true;
    return MYLITE_OK;
}

static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node,
    size_t inner_source_index,
    size_t inner_source_count
) {
    bool left_is_inner = exists_source_index_is_inner(
        node->column_source_index,
        inner_source_index,
        inner_source_count
    );
    bool right_is_inner = exists_source_index_is_inner(
        node->value_column_source_index,
        inner_source_index,
        inner_source_count
    );

    if (left_is_inner == right_is_inner) {
        return false;
    }
    if (!join_condition_column_is_integer_family(node->column)) {
        return false;
    }
    return join_condition_column_is_integer_family(node->value_column);
}

static bool exists_source_index_is_inner(
    size_t source_index,
    size_t inner_source_index,
    size_t inner_source_count
) {
    return source_index >= inner_source_index &&
           source_index - inner_source_index < inner_source_count;
}

static bool comparison_operator_is_string_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_LIKE_BINARY:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_enum_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE_BINARY) != 0;
}

static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) != 0;
}
