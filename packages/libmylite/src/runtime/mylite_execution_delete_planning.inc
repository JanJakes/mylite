static int plan_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
) {
    if (statement != NULL && statement->kind == MYLITE_SQL_AST_JOINED_DELETE_STATEMENT) {
        return plan_joined_delete(database, statement, out_plan);
    }
    return plan_single_table_delete(database, statement, out_plan);
}

static int plan_single_table_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
) {
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *target_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *table_node = target_node;
    struct select_source_context source_context = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct loaded_table_columns_cache_entry *table_columns_cache_entry = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_delete){0};
    optional_clause = child_at(statement, 1U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "DELETE supports only WHERE, ORDER BY, and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (target_node != NULL && target_node->kind == MYLITE_SQL_AST_FROM_TABLE) {
        table_node = child_at(target_node, 0U);
    }
    rc = resolve_visible_writable_table_reference(
        database,
        table_node,
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK && target_node != table_node) {
        rc = init_select_source_context(database, target_node, &out_plan->target, &source_context);
    }
    if (rc == MYLITE_OK && target_node == table_node) {
        source_context = (struct select_source_context){
            .source = &out_plan->target,
        };
    }
    if (rc == MYLITE_OK && (where_clause != NULL || order_clause != NULL || limit_clause != NULL ||
                            sql_safe_updates_is_enabled(database))) {
        rc = mylite_execution_acquire_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count,
            &table_columns_cache_entry
        );
    }
    if (rc == MYLITE_OK) {
        const struct select_predicate_plan_options predicate_options = {
            .allow_exists = true,
            .allow_in_subquery = true,
            .allow_sql_auto_is_null_lookup = true,
        };

        rc = plan_select_predicate_with_options(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &predicate_options,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_validate_single_table_dml_order_clause(database, order_clause);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_plan_select_order(
            database,
            order_clause,
            NULL,
            NULL,
            table_columns,
            table_column_count,
            false,
            false,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_plan_delete_limit(database, limit_clause, &out_plan->limit);
    }
    if (rc == MYLITE_OK && out_plan->limit.has_limit) {
        rc = mylite_execution_choose_sqlite_rowid_alias(
            database,
            table_columns,
            table_column_count,
            "DELETE LIMIT requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_single_table_delete_safe_updates(
            database,
            table_columns,
            table_column_count,
            out_plan
        );
    }

    mylite_execution_release_table_columns(table_columns_cache_entry, table_columns);
    if (rc != MYLITE_OK) {
        planned_delete_deinit(out_plan);
    }

    return rc;
}

static int validate_single_table_delete_safe_updates(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_delete *plan
) {
    struct loaded_index_info *indexes = NULL;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (plan == NULL || !sql_safe_updates_is_enabled(database) || plan->limit.has_limit) {
        return MYLITE_OK;
    }

    rc = mylite_execution_load_table_index_infos(
        database,
        plan->table.table_id,
        table_columns,
        table_column_count,
        &indexes,
        &index_count
    );
    if (rc == MYLITE_OK) {
        rc = validate_sql_safe_updates_predicate(database, &plan->predicate, indexes, index_count);
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    return rc;
}

static int plan_joined_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
) {
    const struct mylite_sql_ast_node *target_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *join_condition = child_at(from_clause, 2U);
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_delete){.is_joined = true};
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_JOIN) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (where_clause != NULL && where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(
            database,
            "joined DELETE supports only WHERE after the joined source"
        );
        return MYLITE_ERROR;
    }

    out_plan->join_kind = mylite_sql_ast_node_join_kind(from_clause);
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_NATURAL_INNER ||
        out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_NATURAL_LEFT_OUTER ||
        out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_NATURAL_RIGHT_OUTER) {
        set_unsupported_error(database, "joined DELETE does not support NATURAL joins");
        return MYLITE_ERROR;
    }
    if (join_condition != NULL && join_condition->kind == MYLITE_SQL_AST_JOIN_USING_CLAUSE) {
        set_unsupported_error(database, "joined DELETE does not support USING join conditions");
        return MYLITE_ERROR;
    }
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        set_unsupported_error(database, "joined DELETE does not support RIGHT JOIN");
        return MYLITE_ERROR;
    }
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER && join_condition == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = reject_builtin_schema_joined_delete_source(database, child_at(from_clause, 0U));
    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_joined_delete_source(database, child_at(from_clause, 1U));
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->sources = calloc(2U, sizeof(*out_plan->sources));
    if (out_plan->sources == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->source_count = 2U;

    rc = plan_joined_select_source(database, child_at(from_clause, 0U), &out_plan->sources[0]);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_source(database, child_at(from_clause, 1U), &out_plan->sources[1]);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_condition(
            database,
            join_condition,
            out_plan->join_kind,
            &source_context,
            &out_plan->join_condition
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            NULL,
            0U,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_joined_delete_targets(
            database,
            target_node,
            out_plan->sources,
            out_plan->source_count,
            out_plan->target_source_indexes,
            sizeof(out_plan->target_source_indexes) / sizeof(out_plan->target_source_indexes[0]),
            &out_plan->target_source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_joined_delete_mixed_physical_targets(
            database,
            out_plan->sources,
            out_plan->target_source_indexes,
            out_plan->target_source_count
        );
    }
    if (rc == MYLITE_OK) {
        const struct planned_select_source *target_source =
            &out_plan->sources[out_plan->target_source_indexes[0]];

        out_plan->target_source_index = out_plan->target_source_indexes[0];
        out_plan->target = target_source->source;
        out_plan->table = target_source->table;
        rc = mylite_execution_choose_sqlite_rowid_alias(
            database,
            target_source->columns,
            target_source->column_count,
            "joined DELETE requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }

    if (rc != MYLITE_OK) {
        planned_delete_deinit(out_plan);
    }
    return rc;
}

static int reject_builtin_schema_joined_delete_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node
) {
    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }
    return reject_builtin_schema_write_target(database, child_at(source_node, 0U));
}

static int resolve_joined_delete_targets(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    const struct planned_select_source *sources,
    size_t source_count,
    size_t *out_source_indexes,
    size_t source_index_capacity,
    size_t *out_source_index_count
) {
    const struct mylite_sql_ast_node *target = NULL;
    int rc = MYLITE_OK;

    *out_source_index_count = 0U;
    if (target_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (target_node->kind != MYLITE_SQL_AST_TABLE_NAME_LIST) {
        size_t source_index = 0U;

        rc = resolve_joined_delete_target(
            database,
            target_node,
            sources,
            source_count,
            &source_index
        );
        if (rc == MYLITE_OK) {
            rc = append_joined_delete_target_source_index(
                database,
                source_index,
                out_source_indexes,
                source_index_capacity,
                out_source_index_count
            );
        }
        return rc;
    }

    target = child_at(target_node, 0U);
    while (rc == MYLITE_OK && target != NULL) {
        size_t source_index = 0U;

        rc = resolve_joined_delete_target(database, target, sources, source_count, &source_index);
        if (rc == MYLITE_OK) {
            rc = append_joined_delete_target_source_index(
                database,
                source_index,
                out_source_indexes,
                source_index_capacity,
                out_source_index_count
            );
        }
        target = target->next_sibling;
    }
    if (rc == MYLITE_OK && *out_source_index_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return rc;
}

static int resolve_joined_delete_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    const struct planned_select_source *sources,
    size_t source_count,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char target_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    struct mylite_catalog_schema_descriptor selected_schema = {0};
    size_t part_count = 0U;
    size_t match_count = 0U;
    size_t matching_source_index = 0U;
    int rc = MYLITE_OK;

    *out_source_index = 0U;
    rc = reject_builtin_schema_write_target(database, target_node);
    if (rc == MYLITE_OK) {
        rc = collect_identifier_parts(
            target_node,
            parts,
            table_name_part_capacity,
            &part_count,
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_reserved_joined_delete_target(database, parts, part_count);
    }
    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            target_name,
            sizeof(target_name)
        );
    }
    if (rc == MYLITE_OK && part_count == 1U) {
        rc = resolve_selected_schema(database, &selected_schema);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t source_index = 0U; source_index < source_count; ++source_index) {
        if (!joined_delete_target_matches_source(
                parts,
                part_count,
                part_count == 1U ? &selected_schema : NULL,
                &sources[source_index]
            )) {
            continue;
        }
        if (match_count == 0U) {
            matching_source_index = source_index;
        }
        ++match_count;
    }

    if (match_count == 0U) {
        set_unknown_multi_delete_table_error(database, target_name);
        return MYLITE_ERROR;
    }
    *out_source_index = matching_source_index;
    return MYLITE_OK;
}

static int append_joined_delete_target_source_index(
    struct mylite_db *database,
    size_t source_index,
    size_t *out_source_indexes,
    size_t source_index_capacity,
    size_t *out_source_index_count
) {
    for (size_t index = 0U; index < *out_source_index_count; ++index) {
        if (out_source_indexes[index] == source_index) {
            return MYLITE_OK;
        }
    }
    if (*out_source_index_count >= source_index_capacity) {
        set_unsupported_error(database, "joined DELETE supports only two target table aliases");
        return MYLITE_ERROR;
    }
    out_source_indexes[*out_source_index_count] = source_index;
    ++(*out_source_index_count);
    return MYLITE_OK;
}

static int reject_joined_delete_mixed_physical_targets(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    const size_t *target_source_indexes,
    size_t target_source_count
) {
    int64_t table_id = 0;

    if (target_source_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    table_id = sources[target_source_indexes[0]].table.table_id;
    for (size_t index = 1U; index < target_source_count; ++index) {
        if (sources[target_source_indexes[index]].table.table_id != table_id) {
            set_unsupported_error(
                database,
                "multi-target joined DELETE supports only aliases of the same physical table"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int reject_reserved_joined_delete_target(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
) {
    if (part_count == 0U || part_count > table_name_part_capacity) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (part_count == 2U && mylite_catalog_name_is_reserved(parts[0])) {
        set_reserved_name_error(database, "database", parts[0]);
        return MYLITE_ERROR;
    }
    if (mylite_catalog_name_is_reserved(parts[part_count - 1U])) {
        set_reserved_name_error(database, "table", parts[part_count - 1U]);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool joined_delete_target_matches_source(
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct mylite_catalog_schema_descriptor *selected_schema,
    const struct planned_select_source *source
) {
    if (source == NULL || part_count == 0U || part_count > 2U) {
        return false;
    }
    if (source->has_alias) {
        return (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], source->alias)) !=
               0;
    }
    if (part_count == 1U) {
        return (selected_schema != NULL &&
                text_equals_ascii_case_insensitive(
                    selected_schema->name,
                    source->source.schema.name
                ) &&
                text_equals_ascii_case_insensitive(parts[0], source->source.table_name)) != 0;
    }
    return (text_equals_ascii_case_insensitive(parts[0], source->source.schema.name) &&
            text_equals_ascii_case_insensitive(parts[1], source->source.table_name)) != 0;
}

static void planned_delete_deinit(struct planned_delete *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        planned_select_source_deinit(&plan->sources[source_index]);
    }
    free(plan->sources);
    planned_select_join_condition_deinit(&plan->join_condition);
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_delete){0};
}

static int execute_delete_from_plan(
    struct mylite_db *database,
    const struct planned_delete *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    struct mylite_statement_transaction transaction = {0};
    struct loaded_foreign_key_info *parent_foreign_keys = NULL;
    size_t parent_foreign_key_count = 0U;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_delete_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = apply_parent_delete_actions(
            database,
            plan,
            &parent_foreign_keys,
            &parent_foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_delete_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(database, &plan->table, affected_rows, false);
    }
    if (rc == MYLITE_OK) {
        rc = validate_self_referencing_parent_foreign_keys_after_delete(
            database,
            plan,
            parent_foreign_keys,
            parent_foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rc = rollback_statement_transaction(database, &transaction, rc);
    }
    mylite_execution_loaded_foreign_key_infos_deinit(
        &parent_foreign_keys,
        &parent_foreign_key_count
    );
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, affected_rows);

    return MYLITE_OK;
}

static int apply_parent_delete_actions(
    struct mylite_db *database,
    const struct planned_delete *plan,
    struct loaded_foreign_key_info **out_foreign_keys,
    size_t *out_foreign_key_count
) {
    int rc = MYLITE_OK;

    *out_foreign_keys = NULL;
    *out_foreign_key_count = 0U;
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    rc = mylite_execution_load_parent_foreign_key_infos(
        database,
        plan->table.table_id,
        out_foreign_keys,
        out_foreign_key_count
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < *out_foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &(*out_foreign_keys)[index];

        if (!foreign_key_rule_is_cascade(foreign_key->foreign_key.delete_rule) &&
            !foreign_key_rule_is_set_null(foreign_key->foreign_key.delete_rule)) {
            if (foreign_key->child_table.table_id != foreign_key->parent_table.table_id) {
                rc = reject_parent_delete_with_references(database, plan, foreign_key);
            }
            continue;
        }
        rc = reject_unsupported_recursive_foreign_key_action(database, foreign_key);
        if (rc == MYLITE_OK) {
            if (foreign_key_rule_is_set_null(foreign_key->foreign_key.delete_rule)) {
                rc = execute_parent_delete_set_null(database, plan, foreign_key);
            } else {
                rc = execute_parent_delete_cascade(database, plan, foreign_key);
            }
        }
    }

    return rc;
}

static int validate_self_referencing_parent_foreign_keys_after_delete(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
) {
    int rc = MYLITE_OK;

    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        if (foreign_keys[index].child_table.table_id == plan->table.table_id) {
            rc = validate_foreign_key_references(database, &foreign_keys[index], true);
        }
    }

    return rc;
}

static int reject_parent_delete_with_references(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_parent_delete_reference_check_sql(plan, foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_delete_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = set_row_is_referenced_foreign_key_error(database, foreign_key);
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    return rc;
}

static int build_parent_delete_reference_check_sql(
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_delete_child_key_filter_sql(&string, plan, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int execute_parent_delete_cascade(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_parent_delete_cascade_sql(plan, foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_delete_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &foreign_key->child_table,
            affected_rows,
            false
        );
    }
    free(sql);
    return rc;
}

static int build_parent_delete_cascade_sql(
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_delete_child_key_filter_sql(&string, plan, foreign_key);
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

static int execute_parent_delete_set_null(
    struct mylite_db *database,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_parent_delete_set_null_sql(plan, foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_delete_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else if (sqlite_status_is_constraint(sqlite_rc)) {
            rc = handle_foreign_key_action_set_null_constraint(
                database,
                foreign_key->child_table.table_id
            );
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &foreign_key->child_table,
            affected_rows,
            false
        );
    }
    free(sql);
    return rc;
}

static int build_parent_delete_set_null_sql(
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_set_null_assignments_sql(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_delete_child_key_filter_sql(&string, plan, foreign_key);
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

static int append_parent_delete_child_key_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    const struct loaded_foreign_key_info *foreign_key
) {
    bool has_parent_condition = false;
    size_t next_parameter = 1U;
    int rc = append_foreign_key_column_tuple_sql(string, foreign_key, true);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_column_tuple_sql(string, foreign_key, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            foreign_key->parent_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, "p");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_delete_target_sql(string, plan, &next_parameter, &has_parent_condition);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_foreign_key_column_tuple_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
) {
    const char *alias = child_columns ? "c" : "p";
    int rc = MYLITE_OK;

    if (child_columns && foreign_key->part_count > 1U) {
        rc = mylite_dynamic_string_append_char(string, '(');
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key->part_count; ++index) {
        const char *column_name = child_columns ? foreign_key->parts[index].child_column.name
                                                : foreign_key->parts[index].parent_column.name;

        if (index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, alias, column_name);
        }
    }
    if (rc == MYLITE_OK && child_columns && foreign_key->part_count > 1U) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_parent_delete_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter,
    bool *out_has_condition
) {
    int rc = MYLITE_OK;

    *out_has_condition = false;
    if (plan->is_joined) {
        rc = append_joined_parent_delete_target_sql(string, plan, next_parameter);
        *out_has_condition = rc == MYLITE_OK;
        return rc;
    }
    if (plan->limit.has_limit) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "p", plan->rowid_alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IN (SELECT ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, plan->rowid_alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " FROM ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
        }
        if (rc == MYLITE_OK) {
            rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = append_select_order_sql(string, &plan->order, next_parameter);
        }
        if (rc == MYLITE_OK) {
            rc = append_select_limit_sql(string, &plan->limit);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        *out_has_condition = rc == MYLITE_OK;
        return rc;
    }
    if (planned_select_predicate_has_expression(&plan->predicate)) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
        *out_has_condition = rc == MYLITE_OK;
    }
    return rc;
}

static int append_joined_parent_delete_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = append_foreign_key_column_reference(string, "p", plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_delete_rowid_subquery_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_parent_child_match_sql(
    struct mylite_dynamic_string *string,
    bool has_parent_condition,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *prefix = " WHERE ";
    int rc = MYLITE_OK;

    if (has_parent_condition) {
        prefix = " AND ";
    }
    rc = mylite_dynamic_string_append(string, prefix);

    if (rc == MYLITE_OK) {
        rc = append_foreign_key_parent_match_checks(string, foreign_key);
    }
    return rc;
}

static int resolve_writable_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return resolve_table_name(database, node, out_resolution);
}

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = resolve_schema_name(database, parts[0], &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        }
        return rc;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int resolve_visible_writable_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return resolve_visible_table_reference(database, node, out_resolution, out_table);
}

static int resolve_visible_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool missing_schema = false;
    int rc =
        resolve_table_name_allow_missing_schema(database, node, out_resolution, &missing_schema);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_resolution->table_name)) {
        set_reserved_name_error(database, "table", out_resolution->table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_table(database, out_resolution, missing_schema, out_table);
    }
    return rc;
}

static int resolve_writable_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
) {
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return resolve_table_name_allow_missing_schema(
        database,
        node,
        out_resolution,
        out_missing_schema
    );
}

static int resolve_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    *out_missing_schema = false;
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        bool schema_found = false;

        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            parts[0],
            &out_resolution->schema,
            &schema_found
        );
        memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
            return rc;
        }
        if (!schema_found) {
            memcpy(out_resolution->schema.name, parts[0], sizeof(out_resolution->schema.name));
            *out_missing_schema = true;
        }
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int resolve_readable_table(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found_temporary = false;
    int rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        resolution->schema.name,
        resolution->table_name,
        out_table,
        &found_temporary
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found_temporary) {
        return MYLITE_OK;
    }
    if (missing_schema) {
        set_unknown_database_error(database, resolution->schema.name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_read_table_by_name(
        database,
        resolution->schema.schema_id,
        resolution->table_name,
        out_table
    );
    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, resolution->schema.name, resolution->table_name);
        return MYLITE_ERROR;
    }
    if (out_table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "statement supports only persistent base tables");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found_temporary = false;
    int rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        resolution->schema.name,
        resolution->table_name,
        out_table,
        &found_temporary
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found_temporary) {
        return MYLITE_OK;
    }
    if (missing_schema) {
        set_unknown_database_error(database, resolution->schema.name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_read_table_by_name(
        database,
        resolution->schema.schema_id,
        resolution->table_name,
        out_table
    );
    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, resolution->schema.name, resolution->table_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_persistent_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = MYLITE_OK;

    if (missing_schema) {
        set_unknown_database_error(database, resolution->schema.name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_read_table_by_name(
        database,
        resolution->schema.schema_id,
        resolution->table_name,
        out_table
    );
    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, resolution->schema.name, resolution->table_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_show_columns_table_name(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    *out_missing_schema = false;
    if (nodes.schema == NULL) {
        rc = resolve_table_name_allow_missing_schema(
            database,
            nodes.table,
            out_resolution,
            out_missing_schema
        );
    } else {
        bool schema_found = false;

        rc = copy_identifier_text(nodes.schema, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_try_read_schema_by_name(
                database,
                schema_name,
                &out_resolution->schema,
                &schema_found
            );
            if (rc != MYLITE_OK) {
                set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
                return rc;
            }
            if (!schema_found) {
                memcpy(
                    out_resolution->schema.name,
                    schema_name,
                    sizeof(out_resolution->schema.name)
                );
                *out_missing_schema = true;
            }
        }
        if (rc == MYLITE_OK) {
            rc = copy_show_columns_explicit_table_name(
                database,
                nodes.table,
                out_resolution->table_name,
                sizeof(out_resolution->table_name)
            );
        }
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_resolution->table_name)) {
        set_reserved_name_error(database, "table", out_resolution->table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int copy_show_columns_explicit_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *destination,
    size_t destination_size
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (destination == NULL || destination_size < MYLITE_CATALOG_IDENTIFIER_CAPACITY) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        table_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 0U || part_count > 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    memcpy(destination, parts[part_count - 1U], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    return MYLITE_OK;
}

static int resolve_writable_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    int rc = reject_builtin_schema_write_target(database, node);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return resolve_truncate_table_name(database, node, out_resolution);
}

static int resolve_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_read_schema_by_name(database, parts[0], &out_resolution->schema);
        if (rc != MYLITE_OK) {
            memcpy(out_resolution->schema.name, parts[0], sizeof(out_resolution->schema.name));
            set_table_does_not_exist_error(database, parts[0], parts[1]);
            return MYLITE_ERROR;
        }
        memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int reject_builtin_schema_write_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_builtin_schema *schema = NULL;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema = find_builtin_schema_descriptor(parts[0]);
    } else if (part_count == 1U && selected_schema_is_builtin_schema(database)) {
        schema = find_builtin_schema_descriptor(database->session.selected_schema);
    }
    if (schema != NULL) {
        set_builtin_schema_write_error(database, schema->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_builtin_schema_schema_write_name(
    struct mylite_db *database,
    const char *schema_name
) {
    const struct mylite_execution_catalog_builtin_schema *schema =
        find_builtin_schema_descriptor(schema_name);

    if (schema != NULL) {
        set_builtin_schema_write_error(database, schema->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_by_name(schema_name);
}

static bool selected_schema_is_builtin_schema(const struct mylite_db *database) {
    if (database == NULL || !database->session.has_selected_schema) {
        return false;
    }

    return find_builtin_schema_descriptor(database->session.selected_schema) != NULL;
}

static const char *builtin_schema_error_name(const char *schema_name) {
    const struct mylite_execution_catalog_builtin_schema *schema =
        find_builtin_schema_descriptor(schema_name);

    return schema == NULL ? schema_name : schema->name;
}

static void set_builtin_schema_write_error(struct mylite_db *database, const char *schema_name) {
    const char *canonical_name = builtin_schema_error_name(schema_name);

    if (schema_name_is_information_schema(canonical_name) ||
        strcmp(canonical_name, "performance_schema") == 0) {
        set_database_access_denied_error(database, canonical_name);
        return;
    }

    set_system_schema_access_error(database, canonical_name);
}

static bool selected_schema_is_information_schema(const struct mylite_db *database) {
    if (database == NULL || !database->session.has_selected_schema) {
        return false;
    }

    return schema_name_is_information_schema(database->session.selected_schema);
}

static bool schema_name_is_information_schema(const char *schema_name) {
    return text_equals_ascii_case_insensitive(schema_name, "information_schema");
}

static bool selected_schema_is_mysql_system_schema(const struct mylite_db *database) {
    if (database == NULL || !database->session.has_selected_schema) {
        return false;
    }

    return schema_name_is_mysql_system_schema(database->session.selected_schema);
}

static bool schema_name_is_mysql_system_schema(const char *schema_name) {
    return schema_name != NULL && strcmp(schema_name, "mysql") == 0;
}

static int require_selected_schema_for_unqualified_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && (database == NULL || !database->session.has_selected_schema)) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_read_schema_by_name(database, schema_name, out_schema);

    if (rc != MYLITE_OK) {
        set_unknown_database_error(database, schema_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return resolve_schema_name(database, database->session.selected_schema, out_schema);
}

static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
) {
    const struct mylite_sql_ast_node *tail_nodes[table_name_part_capacity];
    const struct mylite_sql_ast_node *current = node;
    size_t tail_count = 0U;
    int rc = MYLITE_OK;

    if (current == NULL || part_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    while (current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (tail_count >= part_capacity || tail_count >= table_name_part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        tail_nodes[tail_count] = child_at(current, 1U);
        ++tail_count;
        current = child_at(current, 0U);
        if (current == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    if (current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*part_count >= part_capacity) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(
        current,
        parts[*part_count],
        MYLITE_CATALOG_IDENTIFIER_CAPACITY,
        database
    );
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    ++(*part_count);

    while (tail_count > 0U) {
        --tail_count;
        if (*part_count >= part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(
            tail_nodes[tail_count],
            parts[*part_count],
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        ++(*part_count);
    }

    return MYLITE_OK;
}
