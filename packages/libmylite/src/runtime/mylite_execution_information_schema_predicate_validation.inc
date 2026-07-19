static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct information_schema_query *query
) {
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports metadata predicates");
        return MYLITE_ERROR;
    }
    return information_schema_compile_predicate(database, query, child_at(where_clause, 0U));
}

static int information_schema_compile_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_plan_stack stack = {0};
    struct information_schema_predicate_plan_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_PLAN_VISIT,
    };
    int rc = information_schema_predicate_plan_stack_push(database, &stack, frame);

    while (rc == MYLITE_OK && information_schema_predicate_plan_stack_pop(&stack, &frame)) {
        if (frame.action == INFORMATION_SCHEMA_PREDICATE_PLAN_APPEND) {
            struct information_schema_predicate_instruction instruction = {
                .kind = frame.instruction_kind,
            };

            rc = information_schema_append_predicate_instruction(
                database,
                &query->predicate,
                &instruction
            );
        } else {
            rc = information_schema_compile_predicate_visit(database, query, frame.node, &stack);
        }
    }
    information_schema_predicate_plan_stack_deinit(&stack);
    if (rc != MYLITE_OK) {
        mylite_execution_information_schema_predicate_plan_deinit(&query->predicate);
    }
    return rc;
}

static int information_schema_compile_predicate_visit(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    struct information_schema_predicate_plan_stack *stack
) {
    const struct mylite_sql_ast_node *current =
        mylite_execution_unwrap_parenthesized_expression(predicate_node);
    enum information_schema_predicate_instruction_kind instruction_kind =
        INFORMATION_SCHEMA_PREDICATE_COMPARISON;
    int rc = MYLITE_OK;

    if (current == NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports metadata predicates");
        return MYLITE_ERROR;
    }
    if (current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return information_schema_compile_comparison_predicate(database, query, current);
    }
    if (current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        return information_schema_compile_is_null_predicate(database, query, current);
    }
    if (current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        return information_schema_compile_between_predicate(database, query, current);
    }
    if (current->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        return information_schema_compile_in_predicate(database, query, current);
    }
    if (current->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        instruction_kind = INFORMATION_SCHEMA_PREDICATE_NOT;
    } else if (current->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        instruction_kind = INFORMATION_SCHEMA_PREDICATE_AND;
    } else if (current->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        instruction_kind = INFORMATION_SCHEMA_PREDICATE_OR;
    } else if (current->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        instruction_kind = INFORMATION_SCHEMA_PREDICATE_XOR;
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports metadata predicates");
        return MYLITE_ERROR;
    }

    rc = information_schema_predicate_plan_stack_push(
        database,
        stack,
        (struct information_schema_predicate_plan_frame){
            .action = INFORMATION_SCHEMA_PREDICATE_PLAN_APPEND,
            .instruction_kind = instruction_kind,
        }
    );
    if (rc == MYLITE_OK && current->kind != MYLITE_SQL_AST_NOT_PREDICATE) {
        rc = information_schema_predicate_plan_stack_push(
            database,
            stack,
            (struct information_schema_predicate_plan_frame){
                .node = child_at(current, 1U),
                .action = INFORMATION_SCHEMA_PREDICATE_PLAN_VISIT,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_plan_stack_push(
            database,
            stack,
            (struct information_schema_predicate_plan_frame){
                .node = child_at(current, 0U),
                .action = INFORMATION_SCHEMA_PREDICATE_PLAN_VISIT,
            }
        );
    }
    return rc;
}

static int information_schema_compile_comparison_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_instruction instruction = {
        .kind = INFORMATION_SCHEMA_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .value_count = 1U,
    };
    enum mylite_execution_information_schema_predicate_value_kind value_kind =
        MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_VALUE;
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &instruction.column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    instruction.values = calloc(1U, sizeof(*instruction.values));
    if (instruction.values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (instruction.operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        value_kind = MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LIKE_PATTERN;
    }
    rc = mylite_execution_information_schema_copy_predicate_value(
        database,
        child_at(predicate_node, 1U),
        value_kind,
        &instruction.values[0]
    );
    if (rc == MYLITE_OK && instruction.operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        rc = mylite_execution_information_schema_predicate_escape_character(
            database,
            predicate_node,
            &instruction.values[0].like_escape_character
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_predicate_instruction(
            database,
            &query->predicate,
            &instruction
        );
    }
    information_schema_predicate_instruction_deinit(&instruction);
    return rc;
}

static int information_schema_compile_is_null_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_instruction instruction = {
        .kind = INFORMATION_SCHEMA_PREDICATE_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
    };
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &instruction.column_index
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_append_predicate_instruction(
            database,
            &query->predicate,
            &instruction
        );
    }
    return rc;
}

static int information_schema_compile_between_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_instruction instruction = {
        .kind = INFORMATION_SCHEMA_PREDICATE_BETWEEN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .value_count = 2U,
    };
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &instruction.column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    instruction.values = calloc(instruction.value_count, sizeof(*instruction.values));
    if (instruction.values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < instruction.value_count; ++index) {
        rc = mylite_execution_information_schema_copy_predicate_value(
            database,
            child_at(predicate_node, index + 1U),
            MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LITERAL,
            &instruction.values[index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_predicate_instruction(
            database,
            &query->predicate,
            &instruction
        );
    }
    information_schema_predicate_instruction_deinit(&instruction);
    return rc;
}

static int information_schema_compile_in_predicate(
    struct mylite_db *database,
    struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    struct information_schema_predicate_instruction instruction = {
        .kind = INFORMATION_SCHEMA_PREDICATE_IN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
    };
    int rc = mylite_execution_information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &instruction.column_index
    );

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
    instruction.value_count = mylite_sql_ast_node_child_count(values);
    if (instruction.value_count > SIZE_MAX / sizeof(*instruction.values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (instruction.value_count != 0U) {
        instruction.values = calloc(instruction.value_count, sizeof(*instruction.values));
        if (instruction.values == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    value = child_at(values, 0U);
    for (size_t index = 0U; rc == MYLITE_OK && index < instruction.value_count && value != NULL;
         ++index) {
        rc = mylite_execution_information_schema_copy_predicate_value(
            database,
            value,
            MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_LITERAL,
            &instruction.values[index]
        );
        value = value->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_predicate_instruction(
            database,
            &query->predicate,
            &instruction
        );
    }
    information_schema_predicate_instruction_deinit(&instruction);
    return rc;
}

static int information_schema_append_predicate_instruction(
    struct mylite_db *database,
    struct information_schema_predicate_plan *plan,
    struct information_schema_predicate_instruction *instruction
) {
    struct information_schema_predicate_instruction *instructions = NULL;
    size_t capacity = plan->instruction_capacity;

    if (plan->instruction_count == capacity) {
        capacity = capacity == 0U ? if_stack_initial_capacity : capacity * 2U;
        if (capacity < plan->instruction_capacity || capacity > SIZE_MAX / sizeof(*instructions)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        instructions = realloc(plan->instructions, capacity * sizeof(*instructions));
        if (instructions == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        plan->instructions = instructions;
        plan->instruction_capacity = capacity;
    }
    plan->instructions[plan->instruction_count++] = *instruction;
    *instruction = (struct information_schema_predicate_instruction){0};
    return MYLITE_OK;
}

static void information_schema_predicate_instruction_deinit(
    struct information_schema_predicate_instruction *instruction
) {
    if (instruction == NULL) {
        return;
    }
    for (size_t index = 0U; index < instruction->value_count; ++index) {
        free(instruction->values[index].text);
    }
    free(instruction->values);
    *instruction = (struct information_schema_predicate_instruction){0};
}

static int information_schema_predicate_plan_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_plan_stack *stack,
    struct information_schema_predicate_plan_frame frame
) {
    struct information_schema_predicate_plan_frame *grown_items = NULL;
    size_t new_capacity = 0U;

    if (stack->items == NULL) {
        stack->items = stack->stack_items;
        stack->capacity = if_stack_initial_capacity;
    }
    if (stack->count < stack->capacity) {
        stack->items[stack->count++] = frame;
        return MYLITE_OK;
    }
    if (stack->capacity > SIZE_MAX / 2U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    new_capacity = stack->capacity * 2U;
    if (new_capacity > SIZE_MAX / sizeof(*grown_items)) {
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
    stack->items[stack->count++] = frame;
    return MYLITE_OK;
}

static bool information_schema_predicate_plan_stack_pop(
    struct information_schema_predicate_plan_stack *stack,
    struct information_schema_predicate_plan_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    *out_frame = stack->items[--stack->count];
    return true;
}

static void information_schema_predicate_plan_stack_deinit(
    struct information_schema_predicate_plan_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    if (stack->uses_heap) {
        free(stack->items);
    }
    *stack = (struct information_schema_predicate_plan_stack){0};
}
