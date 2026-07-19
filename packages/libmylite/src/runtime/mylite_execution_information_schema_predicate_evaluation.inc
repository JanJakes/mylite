
static int information_schema_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
    bool *out_matches
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_truth_stack truth_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    *out_matches = false;
    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
        } else if (frame.action == INFORMATION_SCHEMA_PREDICATE_VISIT) {
            rc = information_schema_visit_predicate(database, &frame_stack, current);
        } else {
            rc = information_schema_evaluate_predicate(database, query, current, row, &truth_stack);
        }
    }
    if (rc == MYLITE_OK) {
        enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (truth_stack.count != 1U || !information_schema_truth_stack_pop(&truth_stack, &truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate state");
            rc = MYLITE_ERROR;
        } else {
            *out_matches = information_schema_truth_is_true(truth);
        }
    }

    information_schema_truth_stack_deinit(&truth_stack);
    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static void information_schema_predicate_frame_stack_deinit(
    struct information_schema_predicate_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    if (stack->uses_heap) {
        free(stack->items);
    }
    *stack = (struct information_schema_predicate_frame_stack){0};
}

static int information_schema_predicate_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame frame
) {
    struct information_schema_predicate_eval_frame *grown_items = NULL;
    size_t new_capacity = 0U;

    if (stack->items == NULL) {
        stack->items = stack->stack_items;
        stack->capacity = if_stack_initial_capacity;
    }
    if (stack->count < stack->capacity) {
        stack->items[stack->count] = frame;
        ++stack->count;
        return MYLITE_OK;
    }
    new_capacity = stack->capacity * 2U;
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (stack->uses_heap) {
        grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    } else {
        grown_items = malloc(new_capacity * sizeof(*grown_items));
        if (grown_items != NULL) {
            memcpy(grown_items, stack->items, stack->count * sizeof(*grown_items));
        }
    }
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->uses_heap = true;
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_predicate_frame_stack_pop(
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_frame = stack->items[stack->count];
    return true;
}

static void information_schema_truth_stack_deinit(struct information_schema_truth_stack *stack) {
    if (stack == NULL) {
        return;
    }
    if (stack->uses_heap) {
        free(stack->items);
    }
    *stack = (struct information_schema_truth_stack){0};
}

static int information_schema_truth_stack_push(
    struct mylite_db *database,
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value value
) {
    enum information_schema_truth_value *grown_items = NULL;
    size_t new_capacity = 0U;

    if (stack->items == NULL) {
        stack->items = stack->stack_items;
        stack->capacity = if_stack_initial_capacity;
    }
    if (stack->count < stack->capacity) {
        stack->items[stack->count] = value;
        ++stack->count;
        return MYLITE_OK;
    }
    new_capacity = stack->capacity * 2U;
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (stack->uses_heap) {
        grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    } else {
        grown_items = malloc(new_capacity * sizeof(*grown_items));
        if (grown_items != NULL) {
            memcpy(grown_items, stack->items, stack->count * sizeof(*grown_items));
        }
    }
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->uses_heap = true;
    stack->items[stack->count] = value;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_truth_stack_pop(
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value *out_value
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_value = stack->items[stack->count];
    return true;
}

static int information_schema_visit_predicate(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate_node
) {
    int rc = information_schema_predicate_frame_stack_push(
        database,
        frame_stack,
        (struct information_schema_predicate_eval_frame){
            .node = predicate_node,
            .action = INFORMATION_SCHEMA_PREDICATE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 0U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
    }
    if (predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        rc = information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 1U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_predicate_frame_stack_push(
                database,
                frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(predicate_node, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int information_schema_evaluate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
    struct information_schema_truth_stack *truth_stack
) {
    enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    int rc = MYLITE_OK;

    if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = information_schema_comparison_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = information_schema_is_null_predicate_matches(
            database,
            query,
            predicate_node,
            row,
            &truth
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = information_schema_between_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = information_schema_in_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum information_schema_truth_value child_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (!information_schema_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
            return MYLITE_ERROR;
        }
        if (child_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (child_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
               predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
               predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        rc = information_schema_compound_predicate_matches(
            database,
            predicate_node->kind,
            truth_stack,
            &truth
        );
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports metadata predicates");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = information_schema_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    const char *const *row,
    enum information_schema_truth_value *out_truth
) {
    size_t column_index = 0U;
    char predicate_value_text[integer_text_capacity];
    const char *predicate_value = NULL;
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_row_value(
            database,
            query,
            row,
            column_index,
            predicate_value_text,
            sizeof(predicate_value_text),
            &predicate_value
        );
    }
    if (rc == MYLITE_OK) {
        bool is_null = predicate_value == NULL;
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        *out_truth = information_schema_truth_from_bool(is_null);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NULL) {
            if (is_null) {
                *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
            } else {
                *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
            }
        }
    }
    return rc;
}

static int information_schema_predicate_row_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct table_status_values status = {0};
    bool found = false;
    int rc = MYLITE_OK;

    if (database == NULL || query == NULL || row == NULL || buffer == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = row[column_index];
    if (*out_value != NULL || query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES ||
        column_index != information_schema_tables_auto_increment_column ||
        row[information_schema_tables_table_schema_column] == NULL ||
        row[information_schema_tables_table_name_column] == NULL) {
        return MYLITE_OK;
    }
    rc = mylite_catalog_try_read_schema_by_name(
        database,
        row[information_schema_tables_table_schema_column],
        &schema,
        &found
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA predicate schema");
        return MYLITE_ERROR;
    }
    if (!found) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        schema.schema_id,
        row[information_schema_tables_table_name_column],
        &table,
        &found
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA predicate table");
        return MYLITE_ERROR;
    }
    if (!found || table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    status.auto_increment = row[column_index];
    return table_status_auto_increment_predicate_value(
        database,
        &table,
        &status,
        buffer,
        buffer_size,
        out_value
    );
}

static int information_schema_compound_predicate_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind predicate_kind,
    struct information_schema_truth_stack *truth_stack,
    enum information_schema_truth_value *out_truth
) {
    enum information_schema_truth_value right_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    enum information_schema_truth_value left_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

    if (!information_schema_truth_stack_pop(truth_stack, &right_truth) ||
        !information_schema_truth_stack_pop(truth_stack, &left_truth)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
        return MYLITE_ERROR;
    }
    if (predicate_kind == MYLITE_SQL_AST_AND_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_FALSE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (left_truth == INFORMATION_SCHEMA_TRUTH_TRUE &&
                   right_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (predicate_kind == MYLITE_SQL_AST_OR_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_TRUE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else if (left_truth == INFORMATION_SCHEMA_TRUTH_FALSE &&
                   right_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (left_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN ||
               right_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        *out_truth = information_schema_truth_from_bool(left_truth != right_truth);
    }
    return MYLITE_OK;
}

static enum information_schema_truth_value information_schema_truth_from_bool(bool condition) {
    if (condition) {
        return INFORMATION_SCHEMA_TRUTH_TRUE;
    }
    return INFORMATION_SCHEMA_TRUTH_FALSE;
}

static bool information_schema_truth_is_true(enum information_schema_truth_value truth) {
    return truth == INFORMATION_SCHEMA_TRUTH_TRUE;
}
