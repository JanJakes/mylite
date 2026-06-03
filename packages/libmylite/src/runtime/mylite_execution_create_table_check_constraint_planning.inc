
static int apply_create_table_check_constraint_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (item_list == NULL || plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            if (column_index >= plan->column_count) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            rc = apply_create_table_column_check_constraint_definitions(
                database,
                plan,
                item,
                column_index
            );
            ++column_index;
        } else if (item->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            const struct create_table_check_constraint_definition definition = {
                .check_constraint = item,
                .inline_enforcement = NULL,
                .inline_column = NULL,
                .inline_column_index = 0U,
            };

            rc = apply_create_table_check_constraint_definition(database, plan, &definition);
        }
        item = item->next_sibling;
    }

    return rc;
}

static int apply_create_table_column_check_constraint_definitions(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_definition,
    size_t column_index
) {
    const struct mylite_sql_ast_node *attribute = child_at(column_definition, 2U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && attribute != NULL) {
        if (attribute->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            const struct mylite_sql_ast_node *next_attribute = attribute->next_sibling;
            const bool next_attribute_is_enforcement =
                check_constraint_node_is_enforcement(next_attribute);
            const struct mylite_sql_ast_node *inline_enforcement = NULL;
            struct create_table_check_constraint_definition definition = {
                .check_constraint = attribute,
                .inline_enforcement = NULL,
                .inline_column = &plan->columns[column_index],
                .inline_column_index = column_index,
            };

            if (next_attribute_is_enforcement) {
                inline_enforcement = next_attribute;
                definition.inline_enforcement = inline_enforcement;
            }

            rc = apply_create_table_check_constraint_definition(database, plan, &definition);
            if (inline_enforcement != NULL) {
                attribute = inline_enforcement;
            }
        } else if (check_constraint_node_is_enforcement(attribute)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        attribute = attribute->next_sibling;
    }

    return rc;
}

static int apply_create_table_check_constraint_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct create_table_check_constraint_definition *definition
) {
    const struct mylite_sql_ast_node *check_constraint = NULL;
    const struct mylite_sql_ast_node *expression = child_at(check_constraint, 0U);
    const struct mylite_sql_ast_node *child = child_at(check_constraint, 1U);
    const struct mylite_sql_ast_node *name_node = NULL;
    int64_t generated_ordinal = next_planned_check_constraint_generated_ordinal(plan);
    struct planned_check_constraint planned = {
        .generated_ordinal = generated_ordinal,
        .ordinal_position = (int64_t)plan->check_constraint_count + 1,
        .is_enforced = true,
        .name_is_generated = false,
    };
    int rc = MYLITE_OK;

    if (definition != NULL) {
        check_constraint = definition->check_constraint;
        expression = child_at(check_constraint, 0U);
        child = child_at(check_constraint, 1U);
    }

    if (definition == NULL || check_constraint == NULL ||
        check_constraint->kind != MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION ||
        expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            name_node = child;
        } else if (check_constraint_node_is_not_enforced(child)) {
            planned.is_enforced = false;
        }
        child = child->next_sibling;
    }
    if (check_constraint_node_is_not_enforced(definition->inline_enforcement)) {
        planned.is_enforced = false;
    }

    rc = plan_create_table_check_constraint_name(database, plan, name_node, &planned);
    if (rc == MYLITE_OK) {
        rc = render_check_constraint_expression(
            database,
            plan,
            definition->inline_column,
            definition->inline_column_index,
            expression,
            planned.check_clause,
            sizeof(planned.check_clause),
            planned.sqlite_expression,
            sizeof(planned.sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_check_constraints(database, plan, plan->check_constraint_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->check_constraints[plan->check_constraint_count] = planned;
        ++plan->check_constraint_count;
    }

    return rc;
}

static int64_t next_planned_check_constraint_generated_ordinal(
    const struct planned_create_table *plan
) {
    int64_t ordinal = 1;

    if (plan == NULL) {
        return ordinal;
    }
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (plan->check_constraints[index].name_is_generated &&
            plan->check_constraints[index].generated_ordinal >= ordinal) {
            ordinal = plan->check_constraints[index].generated_ordinal + 1;
        }
    }
    return ordinal;
}

static int plan_create_table_check_constraint_name(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *name_node,
    struct planned_check_constraint *check_constraint
) {
    int rc = MYLITE_OK;

    check_constraint->name_is_generated = name_node == NULL;
    if (name_node == NULL) {
        rc = generate_create_table_check_constraint_name(
            database,
            plan,
            check_constraint->generated_ordinal,
            check_constraint->name,
            sizeof(check_constraint->name)
        );
    } else {
        rc = copy_identifier_text(
            name_node,
            check_constraint->name,
            sizeof(check_constraint->name),
            database
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (planned_check_constraint_name_is_used(plan, check_constraint->name)) {
        set_duplicate_check_constraint_error(database, check_constraint->name);
        return MYLITE_ERROR;
    }

    return reject_duplicate_schema_check_constraint_name(database, plan, check_constraint->name);
}

static int generate_create_table_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t generated_ordinal,
    char *destination,
    size_t destination_size
) {
    int written = snprintf(
        destination,
        destination_size,
        "%s_chk_%" PRId64,
        plan->target.table_name,
        generated_ordinal
    );

    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "generated CHECK constraint name is too long");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool planned_check_constraint_name_is_used(
    const struct planned_create_table *plan,
    const char *check_constraint_name
) {
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                plan->check_constraints[index].name,
                check_constraint_name
            )) {
            return true;
        }
    }
    return false;
}

static int reject_duplicate_schema_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *check_constraint_name
) {
    struct find_check_constraint_name_context context = {
        .name = check_constraint_name,
        .found = false,
    };
    int rc = mylite_catalog_for_each_check_constraint_in_schema(
        database,
        plan->target.schema.schema_id,
        reject_duplicate_schema_check_constraint_name_callback,
        &context
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptors");
        return rc;
    }
    if (context.found) {
        set_duplicate_check_constraint_error(database, check_constraint_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_duplicate_schema_check_constraint_name_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct find_check_constraint_name_context *context = user_data;

    if (check_constraint == NULL || context == NULL || context->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (text_equals_ascii_case_insensitive(check_constraint->name, context->name)) {
        context->found = true;
    }
    return MYLITE_OK;
}
