static int build_select_sql(const struct planned_select *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = append_select_sql_query(&string, plan, &next_parameter);
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int append_select_sql_query(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
) {
    struct select_sql_work_stack stack = {0};
    int rc = MYLITE_OK;

    rc = select_sql_work_stack_push(
        &stack,
        (struct select_sql_work_item){
            .kind = SELECT_SQL_WORK_QUERY,
            .plan = plan,
        }
    );
    while (rc == MYLITE_OK && stack.count != 0U) {
        struct select_sql_work_item item = stack.items[--stack.count];

        rc = append_select_sql_work_item(string, &stack, &item, next_parameter);
    }

    select_sql_work_stack_deinit(&stack);
    return rc;
}

static int append_select_sql_work_item(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct select_sql_work_item *item,
    size_t *next_parameter
) {
    if (item == NULL) {
        return MYLITE_ERROR;
    }

    switch (item->kind) {
    case SELECT_SQL_WORK_QUERY:
        return append_select_sql_work_query(string, stack, item->plan);
    case SELECT_SQL_WORK_FROM:
        return append_select_sql_work_from(string, stack, item->plan);
    case SELECT_SQL_WORK_SOURCE:
        return append_select_sql_work_source(string, stack, item->source, item->source_index);
    case SELECT_SQL_WORK_JOIN_CONDITION:
        return append_select_join_condition_sql(string, item->join_condition, next_parameter);
    case SELECT_SQL_WORK_PREDICATE:
        return append_select_predicate_sql(string, item->predicate, next_parameter);
    case SELECT_SQL_WORK_ORDER:
        return append_select_order_sql(string, item->order, next_parameter);
    case SELECT_SQL_WORK_LIMIT:
        return append_select_limit_sql(string, item->limit, next_parameter);
    case SELECT_SQL_WORK_TEXT:
        return mylite_dynamic_string_append(string, item->text);
    case SELECT_SQL_WORK_SOURCE_ALIAS:
        return append_select_source_alias(string, item->source_index);
    }

    return MYLITE_ERROR;
}

static int append_select_sql_work_query(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_ERROR;
    }
    if (plan->is_distinct) {
        rc = mylite_dynamic_string_append(string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(string, "SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_projection_column_sql(string, plan, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_LIMIT,
                .limit = &plan->limit,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_ORDER,
                .order = &plan->order,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_PREDICATE,
                .predicate = &plan->predicate,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_FROM,
                .plan = plan,
            }
        );
    }

    return rc;
}

static int append_select_sql_work_from(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_ERROR;
    }
    if (plan->source_count == 0U) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = append_select_source_alias(string, 0U);
        }
        return rc;
    }
    if (plan->join_count != plan->source_count - 1U || plan->join_kinds == NULL ||
        plan->join_conditions == NULL) {
        return MYLITE_ERROR;
    }

    for (size_t remaining = plan->join_count; rc == MYLITE_OK && remaining != 0U; --remaining) {
        size_t join_index = remaining - 1U;
        size_t right_source_index = join_index + 1U;
        const char *join_sql = " JOIN ";

        if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
            join_sql = " LEFT JOIN ";
        } else if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            join_sql = " RIGHT JOIN ";
        }
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_JOIN_CONDITION,
                .join_condition = &plan->join_conditions[join_index],
            }
        );
        if (rc == MYLITE_OK) {
            rc = select_sql_work_stack_push(
                stack,
                (struct select_sql_work_item){
                    .kind = SELECT_SQL_WORK_SOURCE,
                    .source = &plan->sources[right_source_index],
                    .source_index = right_source_index,
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = select_sql_work_stack_push(
                stack,
                (struct select_sql_work_item){
                    .kind = SELECT_SQL_WORK_TEXT,
                    .text = join_sql,
                }
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = select_sql_work_stack_push(
            stack,
            (struct select_sql_work_item){
                .kind = SELECT_SQL_WORK_SOURCE,
                .source = &plan->sources[0],
                .source_index = 0U,
            }
        );
    }

    return rc;
}

static int append_select_sql_work_source(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select_source *source,
    size_t source_index
) {
    int rc = MYLITE_OK;

    if (source == NULL) {
        return MYLITE_ERROR;
    }
    if (source->is_derived) {
        if (source->derived_select == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append_char(string, '(');
        if (rc == MYLITE_OK) {
            rc = select_sql_work_stack_push(
                stack,
                (struct select_sql_work_item){
                    .kind = SELECT_SQL_WORK_SOURCE_ALIAS,
                    .source_index = source_index,
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = select_sql_work_stack_push(
                stack,
                (struct select_sql_work_item){
                    .kind = SELECT_SQL_WORK_TEXT,
                    .text = ") AS ",
                }
            );
        }
        if (rc == MYLITE_OK) {
            rc = select_sql_work_stack_push(
                stack,
                (struct select_sql_work_item){
                    .kind = SELECT_SQL_WORK_QUERY,
                    .plan = source->derived_select,
                }
            );
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, source->table.physical_name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, source_index);
    }

    return rc;
}

static int select_sql_work_stack_push(
    struct select_sql_work_stack *stack,
    struct select_sql_work_item item
) {
    struct select_sql_work_item *items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return MYLITE_ERROR;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = item;
    ++stack->count;
    return MYLITE_OK;
}

static void select_sql_work_stack_deinit(struct select_sql_work_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct select_sql_work_stack){0};
}

static int append_select_projection_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    int rc = append_descriptor_column_name_sql_for_source(
        string,
        column,
        plan->column_source_indexes[column_index],
        planned_select_qualifies_source_references(plan)
    );

    if (rc == MYLITE_OK && plan->is_distinct && column_descriptor_is_string_family(column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_select_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->source_count == 0U) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && plan->requires_source_alias) {
            rc = append_select_source_alias(string, 0U);
        }
        return rc;
    }
    if (plan->join_count != plan->source_count - 1U || plan->join_kinds == NULL ||
        plan->join_conditions == NULL) {
        return MYLITE_ERROR;
    }

    rc = append_select_source_sql(string, &plan->sources[0], 0U, next_parameter);

    for (size_t join_index = 0U; rc == MYLITE_OK && join_index < plan->join_count; ++join_index) {
        size_t right_source_index = join_index + 1U;

        if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
            rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
        } else if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
        } else {
            rc = mylite_dynamic_string_append(string, " JOIN ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_source_sql(
                string,
                &plan->sources[right_source_index],
                right_source_index,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_select_join_condition_sql(
                string,
                &plan->join_conditions[join_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_select_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (source == NULL) {
        return MYLITE_ERROR;
    }
    if (source->is_derived) {
        if (source->derived_select == NULL) {
            return MYLITE_ERROR;
        }
        rc = mylite_dynamic_string_append_char(string, '(');
        if (rc == MYLITE_OK) {
            rc = append_select_sql_query(string, source->derived_select, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, ") AS ");
        }
    } else {
        rc = mylite_dynamic_string_append_quoted_identifier(string, source->table.physical_name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, source_index);
    }

    return rc;
}

static bool planned_select_qualifies_source_references(const struct planned_select *plan) {
    if (plan == NULL) {
        return false;
    }
    return (plan->source_count > 0U || plan->requires_source_alias) != 0;
}

static int append_select_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_join_condition *condition,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!condition->has_condition) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ON ");
    if (rc == MYLITE_OK) {
        rc = append_descriptor_value_sql_for_source(
            string,
            &condition->left_column,
            condition->left_source_index,
            true
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc == MYLITE_OK) {
        if (condition->right_value_is_row_scalar_expression) {
            if (next_parameter == NULL) {
                return MYLITE_ERROR;
            }
            rc = append_row_scalar_expression_sql(
                string,
                condition->right_row_scalar_expression,
                next_parameter
            );
        } else {
            rc = append_descriptor_value_sql_for_source(
                string,
                &condition->right_column,
                condition->right_source_index,
                true
            );
        }
    }

    return rc;
}

static int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index) {
    char alias[select_source_alias_capacity];
    int written = snprintf(alias, sizeof(alias), "_mylite_s%zu", source_index);

    if (written < 0 || (size_t)written >= sizeof(alias)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append_quoted_identifier(string, alias);
}

static int build_row_scalar_select_sql(
    const struct planned_row_scalar_select *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK && plan->has_distinct) {
        rc = mylite_dynamic_string_append(&string, "DISTINCT ");
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                &string,
                &plan->items[item_index].expression,
                &next_parameter
            );
        }
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK && plan->has_source && plan->source_count != 0U) {
        struct planned_select source_plan = {
            .sources = plan->sources,
            .source_count = plan->source_count,
            .join_kinds = plan->join_kinds,
            .join_conditions = plan->join_conditions,
            .join_count = plan->join_count,
        };

        rc = append_select_from_sql(&string, &source_plan, &next_parameter);
    } else if (rc == MYLITE_OK && plan->has_source) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_order_sql(&string, &plan->order, &next_parameter);
    }
    if (rc == MYLITE_OK && plan->has_source) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
    }
    if (rc == MYLITE_OK && !plan->has_source) {
        rc = append_row_scalar_tableless_filter_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK && !plan->has_source) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int append_row_scalar_tableless_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_select *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!plan->tableless_filter.has_filter) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK && plan->tableless_filter.has_scalar_predicate) {
        return append_select_predicate_expression_sql_without_exists(
            string,
            &plan->tableless_filter.predicate,
            next_parameter
        );
    }
    if (rc == MYLITE_OK && plan->tableless_filter.negate) {
        rc = mylite_dynamic_string_append(string, "NOT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_exists_predicate_sql(
            string,
            &plan->tableless_filter.subquery,
            next_parameter
        );
    }

    return rc;
}
