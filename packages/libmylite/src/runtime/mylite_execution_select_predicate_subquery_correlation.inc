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
        column_descriptor_is_enum(&node->column)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_set(&node->column)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE LIKE predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_regexp(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE REGEXP predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(&node->column) &&
        !comparison_operator_is_string_predicate(node->operator_kind)) {
        set_unsupported_error(
            database,
            "WHERE string predicates support only =, <=>, <>, !=, <, <=, >, >=, LIKE, REGEXP, "
            "and RLIKE"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_enum(&node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_set(&node->column) &&
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
    int rc = MYLITE_OK;

    if (options == NULL || !options->allow_exists) {
        set_unsupported_error(database, "EXISTS subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }
    if (source_context == NULL || select_source_context_is_joined(source_context)) {
        set_unsupported_error(database, "EXISTS subqueries support only one outer table source");
        return MYLITE_ERROR;
    }

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
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
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_exists_subquery){.has_table_source = false};
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
            outer_source_context,
            outer_columns,
            outer_column_count,
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

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "EXISTS subqueries support only one table source");
        return MYLITE_ERROR;
    }

    rc = plan_joined_select_source(database, from_clause, &out_subquery->source);
    if (rc == MYLITE_OK) {
        out_subquery->has_table_source = true;
    }
    return rc;
}

static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
) {
    struct select_source_context source_context = {0};

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (subquery->has_table_source) {
        source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
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
            subquery->source.columns,
            subquery->source.column_count,
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
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    int rc = MYLITE_OK;

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
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }

    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
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
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        set_unsupported_error(database, "EXISTS correlated predicates support only = and <=>");
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
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!exists_correlated_column_comparison_is_supported(&node)) {
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
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    bool resolved = false;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
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
        parts,
        part_count,
        column_name,
        inner_source_context,
        inner_columns,
        inner_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 1U;
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        parts,
        part_count,
        column_name,
        outer_source_context,
        outer_columns,
        outer_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 0U;
        return rc;
    }

    set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
    return MYLITE_ERROR;
}

static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_resolved = false;
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

    *out_column = columns[column_index];
    *out_resolved = true;
    return MYLITE_OK;
}

static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    bool left_is_outer = node->column_source_index == 0U;
    bool right_is_outer = node->value_column_source_index == 0U;

    if (left_is_outer == right_is_outer) {
        return false;
    }
    if (!join_condition_column_is_integer_family(&node->column)) {
        return false;
    }
    return join_condition_column_is_integer_family(&node->value_column);
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
    return operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE;
}

static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) != 0;
}
