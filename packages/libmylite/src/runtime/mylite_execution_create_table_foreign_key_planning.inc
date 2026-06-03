
static int apply_create_table_foreign_key_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = MYLITE_OK;

    if (item_list == NULL || plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        if (item->kind == MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION) {
            rc = apply_create_table_foreign_key_definition(database, plan, item);
        }
        item = item->next_sibling;
    }

    return rc;
}

static int apply_create_table_foreign_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *foreign_key
) {
    struct foreign_key_definition_nodes nodes = {0};
    struct foreign_key_column_names names = {0};
    struct planned_foreign_key planned = {0};
    int rc = extract_foreign_key_definition_nodes(database, foreign_key, &nodes);

    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            nodes.child_parts_node,
            &names.child_parts,
            &names.child_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            nodes.parent_parts_node,
            &names.parent_parts,
            &names.parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_part_counts(
            database,
            names.child_part_count,
            names.parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_foreign_key_child_parts(database, &names);
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_foreign_key_name(
            database,
            plan,
            nodes.constraint_name_node,
            planned.name,
            sizeof(planned.name)
        );
        planned.has_explicit_name = nodes.constraint_name_node != NULL;
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_index_name(
            database,
            nodes.index_name_node,
            planned.child_index_name,
            sizeof(planned.child_index_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = parse_foreign_key_actions(
            database,
            nodes.action_list_node,
            planned.update_rule,
            sizeof(planned.update_rule),
            planned.delete_rule,
            sizeof(planned.delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_foreign_key_parent(
            database,
            plan,
            nodes.parent_table_node,
            &names,
            &planned
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_planned_foreign_key_set_null_columns(database, plan, &planned);
    }
    if (rc == MYLITE_OK) {
        rc = choose_create_table_foreign_key_child_index(database, plan, &planned);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_foreign_keys(database, plan, plan->foreign_key_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->foreign_keys[plan->foreign_key_count] = planned;
        planned = (struct planned_foreign_key){0};
        ++plan->foreign_key_count;
    }

    foreign_key_column_names_deinit(&names);
    planned_foreign_key_deinit(&planned);
    return rc;
}
