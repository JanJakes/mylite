
static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct information_schema_query *query
) {
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports metadata predicates");
        return MYLITE_ERROR;
    }
    return information_schema_validate_predicate(database, query, child_at(where_clause, 0U));
}

static int information_schema_validate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            continue;
        }

        switch (current->kind) {
        case MYLITE_SQL_AST_NOT_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            break;
        case MYLITE_SQL_AST_AND_PREDICATE:
        case MYLITE_SQL_AST_OR_PREDICATE:
        case MYLITE_SQL_AST_XOR_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 1U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            if (rc == MYLITE_OK) {
                rc = information_schema_predicate_frame_stack_push(
                    database,
                    &frame_stack,
                    (struct information_schema_predicate_eval_frame){
                        .node = child_at(current, 0U),
                        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                    }
                );
            }
            break;
        case MYLITE_SQL_AST_COMPARISON_PREDICATE:
            rc = information_schema_validate_comparison_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IS_NULL_PREDICATE:
            rc = information_schema_validate_is_null_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_BETWEEN_PREDICATE:
            rc = information_schema_validate_between_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IN_PREDICATE:
            rc = information_schema_validate_in_predicate(database, query, current);
            break;
        default:
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            break;
        }
    }

    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static int information_schema_validate_comparison_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value right = {
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    size_t column_index = 0U;
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
            char escape_character = '\0';

            rc = like_escape_character_from_predicate(database, predicate_node, &escape_character);
            if (rc == MYLITE_OK) {
                rc = information_schema_predicate_like_pattern_text(
                    database,
                    child_at(predicate_node, 1U),
                    &right
                );
            }
        } else {
            rc = information_schema_predicate_value_text(
                database,
                child_at(predicate_node, 1U),
                &right
            );
        }
    }
    free(right.text);
    return rc;
}

static int information_schema_validate_is_null_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    size_t column_index = 0U;

    return mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );
}

static int information_schema_validate_between_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value lower = {0};
    struct information_schema_predicate_value upper = {0};
    size_t column_index = 0U;
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 1U),
            &lower
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 2U),
            &upper
        );
    }
    free(lower.text);
    free(upper.text);
    return rc;
}

static int information_schema_validate_in_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_index = 0U;
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || values->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE does not support IN subqueries");
        return MYLITE_ERROR;
    }
    if (values->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports literal IN lists");
        return MYLITE_ERROR;
    }

    value = child_at(values, 0U);
    while (rc == MYLITE_OK && value != NULL) {
        struct information_schema_predicate_value item = {0};

        rc = information_schema_predicate_literal_value_text(database, value, &item);
        free(item.text);
        value = value->next_sibling;
    }
    return rc;
}
