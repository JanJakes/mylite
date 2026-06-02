static int plan_alter_table_add_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_ADD,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_check_definition(database, child_at(statement, 1U), out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_drop_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t check_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_DROP,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), check_name, sizeof(check_name), database);
    }
    if (rc == MYLITE_OK &&
        find_planned_check_constraint_by_name(&out_plan->rebuild, check_name, &check_index) !=
            MYLITE_OK) {
        set_check_constraint_not_found_error(database, check_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->target_check_constraint_id =
            out_plan->rebuild.check_constraints[check_index].check_constraint_id;
        out_plan->check_constraint_index = check_index;
        out_plan->has_check_constraint_index = true;
        out_plan->catalog_change_required = true;
        out_plan->physical_rebuild_required =
            out_plan->rebuild.check_constraints[check_index].is_enforced;
        remove_planned_check_constraint_at(&out_plan->rebuild, check_index);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_alter_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    const struct mylite_sql_ast_node *enforcement = child_at(statement, 2U);
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t check_index = 0U;
    bool target_enforced = true;
    bool source_enforced = false;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_ALTER,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), check_name, sizeof(check_name), database);
    }
    if (rc == MYLITE_OK &&
        find_planned_check_constraint_by_name(&out_plan->rebuild, check_name, &check_index) !=
            MYLITE_OK) {
        set_check_constraint_not_found_error(database, check_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        if (enforcement == NULL || !check_constraint_node_is_enforcement(enforcement)) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        } else {
            target_enforced = enforcement->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED;
        }
    }
    if (rc == MYLITE_OK) {
        source_enforced = out_plan->rebuild.check_constraints[check_index].is_enforced;
        out_plan->target_check_constraint_id =
            out_plan->rebuild.check_constraints[check_index].check_constraint_id;
        out_plan->check_constraint_index = check_index;
        out_plan->has_check_constraint_index = true;
        out_plan->target_enforced = target_enforced;
        out_plan->catalog_change_required = source_enforced != target_enforced;
        out_plan->physical_rebuild_required = source_enforced != target_enforced;
        out_plan->validating_rebuild = false;
        if (!source_enforced) {
            if (target_enforced) {
                out_plan->validating_rebuild = true;
            }
        }
        out_plan->rebuild.check_constraints[check_index].is_enforced = target_enforced;
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_check_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = resolve_writable_table_name(database, table_node, &out_plan->target);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "ALTER TABLE CHECK supports only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = populate_alter_table_check_rebuild_plan(database, out_plan);
    }

    return rc;
}

static int populate_alter_table_check_rebuild_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t check_constraint_count = 0U;
    int rc = MYLITE_OK;

    out_plan->rebuild.target = out_plan->target;
    out_plan->rebuild.auto_increment_next = out_plan->table.auto_increment_next;
    snprintf(
        out_plan->rebuild.default_charset,
        sizeof(out_plan->rebuild.default_charset),
        "%s",
        out_plan->table.default_charset
    );
    snprintf(
        out_plan->rebuild.default_collation,
        sizeof(out_plan->rebuild.default_collation),
        "%s",
        out_plan->table.default_collation
    );

    rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_columns(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_indexes(database, indexes, index_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_check_constraint_infos(
            database,
            out_plan->table.table_id,
            &check_constraints,
            &check_constraint_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_constraints(
            database,
            check_constraints,
            check_constraint_count,
            out_plan
        );
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int copy_alter_table_check_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    if (column_count > SIZE_MAX / sizeof(*out_plan->rebuild.columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->rebuild.columns = calloc(column_count, sizeof(*out_plan->rebuild.columns));
    if (out_plan->rebuild.columns == NULL && column_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->rebuild.column_count = column_count;
    for (size_t index = 0U; index < column_count; ++index) {
        planned_column_from_catalog_descriptor(
            &columns[index],
            NULL,
            &out_plan->rebuild.columns[index]
        );
    }

    return MYLITE_OK;
}

static int copy_alter_table_check_indexes(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = copy_alter_table_check_primary_index(database, &indexes[index], out_plan);
        } else if (
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL
        ) {
            rc = copy_alter_table_check_secondary_index(database, &indexes[index], out_plan);
        }
    }

    return rc;
}

static int copy_alter_table_check_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = reserve_planned_primary_key_parts(database, &out_plan->rebuild, index->part_count);

    if (rc == MYLITE_OK) {
        out_plan->rebuild.has_primary_key = true;
        out_plan->rebuild.primary_key_index_id = index->index.index_id;
        snprintf(
            out_plan->rebuild.primary_key_physical_name,
            sizeof(out_plan->rebuild.primary_key_physical_name),
            "%s",
            index->index.physical_name
        );
        out_plan->rebuild.primary_key_show_create_explicit_btree =
            index->index.show_create_explicit_btree;
        snprintf(
            out_plan->rebuild.primary_key_comment,
            sizeof(out_plan->rebuild.primary_key_comment),
            "%s",
            index->index.comment
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        out_plan->rebuild.primary_key_parts[out_plan->rebuild.primary_key_part_count] =
            (struct planned_primary_key_part){
                .column_index = column_index,
                .sort_direction = index->parts[part_index].index_column.sort_direction,
            };
        ++out_plan->rebuild.primary_key_part_count;
        out_plan->rebuild.columns[column_index].is_primary_key = true;
        out_plan->rebuild.columns[column_index].is_nullable = false;
    }

    return rc;
}

static int copy_alter_table_check_secondary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
) {
    struct planned_secondary_index *target = NULL;
    int rc = reserve_planned_secondary_indexes(
        database,
        &out_plan->rebuild,
        out_plan->rebuild.secondary_index_count + 1U
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    target = &out_plan->rebuild.secondary_indexes[out_plan->rebuild.secondary_index_count];
    *target = (struct planned_secondary_index){0};
    target->index_id = index->index.index_id;
    target->kind = index->index.kind;
    target->is_unique = index->index.is_unique;
    target->is_visible = index->index.is_visible;
    target->show_create_explicit_btree = index->index.show_create_explicit_btree;
    snprintf(target->name, sizeof(target->name), "%s", index->index.name);
    snprintf(target->comment, sizeof(target->comment), "%s", index->index.comment);
    snprintf(
        target->physical_name,
        sizeof(target->physical_name),
        "%s",
        index->index.physical_name
    );
    rc = reserve_planned_secondary_index_parts(database, target, index->part_count);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        target->parts[target->part_count] = (struct planned_secondary_index_part){
            .column_index = index->parts[part_index].column_index,
            .has_prefix_length = index->parts[part_index].index_column.has_prefix_length,
            .prefix_length = index->parts[part_index].index_column.prefix_length,
            .sort_direction = index->parts[part_index].index_column.sort_direction,
        };
        ++target->part_count;
    }
    if (rc == MYLITE_OK) {
        ++out_plan->rebuild.secondary_index_count;
    } else {
        free(target->parts);
        *target = (struct planned_secondary_index){0};
    }

    return rc;
}

static int copy_alter_table_check_constraints(
    struct mylite_db *database,
    const struct loaded_check_constraint_info *check_constraints,
    size_t check_constraint_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    if (check_constraint_count == 0U) {
        return MYLITE_OK;
    }
    rc = reserve_planned_check_constraints(database, &out_plan->rebuild, check_constraint_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        const struct mylite_catalog_check_constraint_descriptor *source =
            &check_constraints[index].check_constraint;
        struct planned_check_constraint *target =
            &out_plan->rebuild.check_constraints[out_plan->rebuild.check_constraint_count];

        *target = (struct planned_check_constraint){
            .check_constraint_id = source->check_constraint_id,
            .generated_ordinal = source->generated_ordinal,
            .ordinal_position = source->ordinal_position,
            .is_enforced = source->is_enforced,
            .name_is_generated = source->name_is_generated,
        };
        snprintf(target->name, sizeof(target->name), "%s", source->name);
        snprintf(target->physical_name, sizeof(target->physical_name), "%s", source->physical_name);
        snprintf(target->check_clause, sizeof(target->check_clause), "%s", source->check_clause);
        snprintf(
            target->sqlite_expression,
            sizeof(target->sqlite_expression),
            "%s",
            source->sqlite_expression
        );
        ++out_plan->rebuild.check_constraint_count;
    }

    return rc;
}

static int plan_alter_table_add_check_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *check_constraint,
    struct planned_alter_table_check_constraint *out_plan
) {
    const struct mylite_sql_ast_node *expression = child_at(check_constraint, 0U);
    const struct mylite_sql_ast_node *child = child_at(check_constraint, 1U);
    const struct mylite_sql_ast_node *name_node = NULL;
    struct planned_check_constraint planned = {
        .generated_ordinal = next_planned_check_constraint_generated_ordinal(&out_plan->rebuild),
        .ordinal_position = (int64_t)out_plan->rebuild.check_constraint_count + 1,
        .is_enforced = true,
    };
    int rc = MYLITE_OK;

    if (check_constraint == NULL ||
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

    if (name_node == NULL) {
        rc = next_available_planned_check_constraint_generated_ordinal(
            database,
            &out_plan->rebuild,
            &planned.generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_check_constraint_name(
            database,
            &out_plan->rebuild,
            name_node,
            &planned
        );
    }
    if (rc == MYLITE_OK) {
        struct mylite_dynamic_string check_string;
        struct mylite_dynamic_string sqlite_string;
        struct check_expression_render_context context = {
            .database = database,
            .plan = &out_plan->rebuild,
            .inline_column = NULL,
            .inline_column_index = 0U,
            .alter_check_constraint_name = planned.name,
            .check_clause = &check_string,
            .sqlite_expression = &sqlite_string,
        };
        bool is_boolean = false;

        mylite_dynamic_string_init(&check_string);
        mylite_dynamic_string_init(&sqlite_string);
        rc = mylite_dynamic_string_reserve(&check_string, 1U);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_reserve(&sqlite_string, 1U);
        }
        if (rc == MYLITE_OK) {
            rc = render_check_expression_node(&context, expression, true, &is_boolean);
        }
        if (rc == MYLITE_OK && !is_boolean) {
            set_check_constraint_non_boolean_error(database);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK && (check_string.length >= sizeof(planned.check_clause) ||
                                sqlite_string.length >= sizeof(planned.sqlite_expression))) {
            set_unsupported_error(database, "CHECK constraint expression is too long");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            memcpy(planned.check_clause, check_string.text, check_string.length + 1U);
            memcpy(planned.sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
        }
        mylite_dynamic_string_deinit(&check_string);
        mylite_dynamic_string_deinit(&sqlite_string);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_check_constraints(
            database,
            &out_plan->rebuild,
            out_plan->rebuild.check_constraint_count + 1U
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->check_constraint_index = out_plan->rebuild.check_constraint_count;
        out_plan->has_check_constraint_index = true;
        out_plan->catalog_change_required = true;
        out_plan->physical_rebuild_required = planned.is_enforced;
        out_plan->validating_rebuild = planned.is_enforced;
        out_plan->rebuild.check_constraints[out_plan->rebuild.check_constraint_count] = planned;
        ++out_plan->rebuild.check_constraint_count;
    }

    return rc;
}

static int next_available_planned_check_constraint_generated_ordinal(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t *out_ordinal
) {
    if (out_ordinal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_ordinal = 0;
    if (plan == NULL) {
        *out_ordinal = 1;
        return MYLITE_OK;
    }

    for (int64_t ordinal = 1; ordinal < INT64_MAX; ++ordinal) {
        bool ordinal_is_used = false;

        for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
            const struct planned_check_constraint *check_constraint =
                &plan->check_constraints[index];

            if (check_constraint->name_is_generated &&
                check_constraint->generated_ordinal == ordinal) {
                ordinal_is_used = true;
                break;
            }
        }
        if (!ordinal_is_used) {
            *out_ordinal = ordinal;
            return MYLITE_OK;
        }
    }

    set_runtime_error(database, "generated CHECK constraint ordinal is too large");
    return MYLITE_ERROR;
}

static int find_planned_check_constraint_by_name(
    const struct planned_create_table *plan,
    const char *check_constraint_name,
    size_t *out_index
) {
    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = 0U;
    if (plan == NULL || check_constraint_name == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                plan->check_constraints[index].name,
                check_constraint_name
            )) {
            *out_index = index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static void remove_planned_check_constraint_at(struct planned_create_table *plan, size_t index) {
    if (plan == NULL || index >= plan->check_constraint_count) {
        return;
    }
    if (index + 1U < plan->check_constraint_count) {
        memmove(
            &plan->check_constraints[index],
            &plan->check_constraints[index + 1U],
            (plan->check_constraint_count - index - 1U) * sizeof(plan->check_constraints[0])
        );
    }
    --plan->check_constraint_count;
}

static int alter_table_check_constraint_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->catalog_change_required) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_ADD) {
        rc = insert_alter_table_check_constraint_catalog_row(database, &mutation, plan);
    } else if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_DROP) {
        rc = mylite_catalog_delete_check_constraint_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->target_check_constraint_id
        );
    } else if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_ALTER) {
        rc = mylite_catalog_update_check_constraint_enforcement_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->target_check_constraint_id,
            plan->target_enforced
        );
    }
    if (rc == MYLITE_OK && plan->physical_rebuild_required) {
        rc = execute_physical_alter_table_check_constraint(
            database,
            plan,
            mylite_catalog_mutation_generation(&mutation),
            &plan->affected_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to alter CHECK constraint");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (plan->physical_rebuild_required) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int insert_alter_table_check_constraint_catalog_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_alter_table_check_constraint *plan
) {
    struct planned_check_constraint *check_constraint =
        &plan->rebuild.check_constraints[plan->check_constraint_index];
    int64_t check_constraint_id = 0;
    int rc = mylite_catalog_allocate_check_constraint_id_in_mutation(
        database,
        mutation,
        &check_constraint_id
    );

    if (rc == MYLITE_OK) {
        check_constraint->check_constraint_id = check_constraint_id;
        rc = build_physical_check_constraint_name(
            check_constraint_id,
            check_constraint->physical_name,
            sizeof(check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_check_constraint_in_mutation(
            database,
            mutation,
            check_constraint->check_constraint_id,
            plan->table.table_id,
            check_constraint->name,
            check_constraint->physical_name,
            check_constraint->check_clause,
            check_constraint->sqlite_expression,
            check_constraint->is_enforced,
            check_constraint->name_is_generated,
            check_constraint->generated_ordinal,
            check_constraint->ordinal_position,
            NULL
        );
    }

    return rc;
}

static int execute_physical_alter_table_check_constraint(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    int64_t *out_affected_rows
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool copy_failed = false;
    int64_t affected_rows = 0;
    int rc = build_alter_table_check_temporary_physical_name(
        plan,
        sqlite_schema_generation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = build_alter_table_check_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_check_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_alter_table_check_copy_rows(database, plan, sql, &affected_rows, &copy_failed);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_create_table_indexes_sql(&plan->rebuild, plan->table.physical_name, &sql);
    }
    if (rc == MYLITE_OK && sql != NULL) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK && plan->validating_rebuild) {
        *out_affected_rows = affected_rows;
    }

    return rc;
}

static int execute_alter_table_check_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    const char *sql,
    int64_t *out_affected_rows,
    bool *out_copy_failed
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_sqlite_statement(database, sql, &statement);

    *out_affected_rows = 0;
    *out_copy_failed = false;
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_DONE) {
            *out_affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            bool was_check_violation = false;

            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_status_is_constraint(sqlite_rc)) {
                int check_rc = handle_check_constraint_violation(
                    database,
                    plan->table.table_id,
                    false,
                    &was_check_violation
                );

                if (check_rc != MYLITE_OK || was_check_violation) {
                    rc = check_rc;
                }
            }
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc != MYLITE_OK) {
        *out_copy_failed = true;
    }

    return rc;
}

static void planned_alter_table_check_constraint_deinit(
    struct planned_alter_table_check_constraint *plan
) {
    if (plan == NULL) {
        return;
    }
    planned_create_table_deinit(&plan->rebuild);
    *plan = (struct planned_alter_table_check_constraint){0};
}

static bool loaded_index_name_is_used_by_other(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const struct loaded_index_info *skipped_index,
    const char *index_name
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (&indexes[index] == skipped_index) {
            continue;
        }
        if (text_equals_ascii_case_insensitive(indexes[index].index.name, index_name)) {
            return true;
        }
    }

    return false;
}

static int find_loaded_index_by_name(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *index_name,
    size_t *out_index
) {
    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = 0U;

    for (size_t index = 0U; index < index_count; ++index) {
        if (text_equals_ascii_case_insensitive(indexes[index].index.name, index_name)) {
            *out_index = index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int validate_drop_index_auto_increment(
    struct mylite_db *database,
    const struct loaded_index_info *target,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t part_index = 0U; part_index < target->part_count; ++part_index) {
        const struct mylite_catalog_column_descriptor *column = &target->parts[part_index].column;
        const struct loaded_index_column_lookup lookup = {
            .indexes = indexes,
            .index_count = index_count,
            .skipped_index_id = target->index.index_id,
            .column_id = column->column_id,
        };

        if (!column->is_auto_increment) {
            continue;
        }
        if (!loaded_other_index_contains_column(&lookup)) {
            set_wrong_auto_key_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool loaded_other_index_contains_column(const struct loaded_index_column_lookup *lookup) {
    for (size_t index = 0U; index < lookup->index_count; ++index) {
        const struct loaded_index_info *candidate = &lookup->indexes[index];

        if (candidate->index.index_id == lookup->skipped_index_id) {
            continue;
        }
        for (size_t part_index = 0U; part_index < candidate->part_count; ++part_index) {
            if (candidate->parts[part_index].column.column_id == lookup->column_id) {
                return true;
            }
        }
    }

    return false;
}

static int execute_physical_drop_index(
    struct mylite_db *database,
    const struct planned_drop_index *plan
) {
    return execute_physical_drop_index_by_name(database, plan->index.index.physical_name);
}

static int execute_physical_drop_index_by_name(struct mylite_db *database, const char *name) {
    char *sql = NULL;
    int rc = build_drop_index_sql(name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}
