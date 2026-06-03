static bool check_constraint_node_is_not_enforced(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    return node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED;
}

static bool check_constraint_node_is_enforcement(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED) {
        return true;
    }
    return node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED;
}

static int reserve_planned_check_constraints(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t requested_capacity
) {
    enum { initial_check_constraint_capacity = 4 };

    struct planned_check_constraint *checks = NULL;
    size_t capacity = 0U;

    if (requested_capacity <= plan->check_constraint_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->check_constraint_capacity == 0U ? initial_check_constraint_capacity
                                                     : plan->check_constraint_capacity;
    while (capacity < requested_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*checks)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    checks = realloc(plan->check_constraints, capacity * sizeof(*checks));
    if (checks == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->check_constraints = checks;
    plan->check_constraint_capacity = capacity;
    return MYLITE_OK;
}

static int extract_foreign_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *foreign_key,
    struct foreign_key_definition_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *child = NULL;

    *out_nodes = (struct foreign_key_definition_nodes){0};

    if (foreign_key == NULL || foreign_key->kind != MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    child = child_at(foreign_key, 0U);
    while (child != NULL) {
        if (record_foreign_key_definition_node(database, child, out_nodes) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }
    if (out_nodes->child_parts_node == NULL ||
        out_nodes->child_parts_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST ||
        out_nodes->parent_table_node == NULL || out_nodes->parent_parts_node == NULL ||
        out_nodes->parent_parts_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int record_foreign_key_definition_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return record_foreign_key_identifier_node(database, node, out_nodes);
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return record_foreign_key_qualified_identifier_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME:
        return record_foreign_key_index_name_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST:
        return record_foreign_key_part_list_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST:
        return record_foreign_key_action_list_node(database, node, out_nodes);
    default:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
}

static int record_foreign_key_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL && out_nodes->constraint_name_node == NULL) {
        out_nodes->constraint_name_node = node;
        return MYLITE_OK;
    }
    if (out_nodes->child_parts_node != NULL && out_nodes->parent_table_node == NULL) {
        out_nodes->parent_table_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_qualified_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node != NULL && out_nodes->parent_table_node == NULL) {
        out_nodes->parent_table_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_index_name_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL && out_nodes->index_name_node == NULL) {
        out_nodes->index_name_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_part_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL) {
        out_nodes->child_parts_node = node;
        return MYLITE_OK;
    }
    if (out_nodes->parent_table_node != NULL && out_nodes->parent_parts_node == NULL) {
        out_nodes->parent_parts_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_action_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->parent_parts_node != NULL && out_nodes->action_list_node == NULL) {
        out_nodes->action_list_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int copy_foreign_key_index_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *index_name_node,
    char *destination,
    size_t destination_size
) {
    const struct mylite_sql_ast_node *identifier = NULL;

    if (destination == NULL || destination_size == 0U) {
        set_runtime_error(database, "invalid foreign-key index descriptor");
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    if (index_name_node == NULL) {
        return MYLITE_OK;
    }
    if (index_name_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    identifier = child_at(index_name_node, 0U);
    return copy_identifier_text(identifier, destination, destination_size, database);
}

static int parse_foreign_key_actions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action_list_node,
    char *update_rule,
    size_t update_rule_size,
    char *delete_rule,
    size_t delete_rule_size
) {
    const struct mylite_sql_ast_node *action = NULL;
    bool saw_update = false;
    bool saw_delete = false;
    int rc = copy_foreign_key_rule_text(database, update_rule, update_rule_size, "NO ACTION");

    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_rule_text(database, delete_rule, delete_rule_size, "NO ACTION");
    }
    if (rc != MYLITE_OK || action_list_node == NULL) {
        return rc;
    }
    if (action_list_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    action = child_at(action_list_node, 0U);
    while (action != NULL) {
        bool is_update = false;
        const char *rule = NULL;

        switch (action->kind) {
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE:
            rule = "CASCADE";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT:
            rule = "RESTRICT";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION:
            rule = "NO ACTION";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL:
            rule = "SET NULL";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE:
            rule = "CASCADE";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT:
            rule = "RESTRICT";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION:
            rule = "NO ACTION";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL:
            rule = "SET NULL";
            is_update = true;
            break;
        default:
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        if (is_update) {
            if (saw_update) {
                set_unsupported_error(database, "FOREIGN KEY supports only one ON UPDATE action");
                return MYLITE_ERROR;
            }
            saw_update = true;
            rc = copy_foreign_key_rule_text(database, update_rule, update_rule_size, rule);
        } else {
            if (saw_delete) {
                set_unsupported_error(database, "FOREIGN KEY supports only one ON DELETE action");
                return MYLITE_ERROR;
            }
            saw_delete = true;
            rc = copy_foreign_key_rule_text(database, delete_rule, delete_rule_size, rule);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        action = action->next_sibling;
    }

    return MYLITE_OK;
}

static int copy_foreign_key_rule_text(
    struct mylite_db *database,
    char *destination,
    size_t destination_size,
    const char *rule
) {
    int written = 0;

    if (destination == NULL || rule == NULL) {
        set_runtime_error(database, "invalid foreign-key action descriptor");
        return MYLITE_ERROR;
    }

    written = snprintf(destination, destination_size, "%s", rule);
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "foreign-key action descriptor is too long");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool foreign_key_rule_equals(const char *rule, const char *expected_rule) {
    int comparison = 0;

    if (rule == NULL || expected_rule == NULL) {
        return false;
    }

    comparison = strcmp(rule, expected_rule);
    if (comparison == 0) {
        return true;
    }
    return false;
}

static bool foreign_key_rule_is_cascade(const char *rule) {
    return foreign_key_rule_equals(rule, "CASCADE");
}

static bool foreign_key_rule_is_set_null(const char *rule) {
    return foreign_key_rule_equals(rule, "SET NULL");
}

static int validate_planned_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
) {
    if (!foreign_key_rule_is_set_null(foreign_key->delete_rule) &&
        !foreign_key_rule_is_set_null(foreign_key->update_rule)) {
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
        const size_t child_column_index = foreign_key->parts[part_index].child_column_index;

        if (child_column_index >= plan->column_count) {
            set_runtime_error(database, "invalid foreign-key child column descriptor");
            return MYLITE_ERROR;
        }
        if (!plan->columns[child_column_index].is_nullable) {
            set_foreign_key_set_null_not_nullable_error(
                database,
                plan->columns[child_column_index].name,
                foreign_key->name
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static void foreign_key_column_names_deinit(struct foreign_key_column_names *names) {
    if (names == NULL) {
        return;
    }
    free(names->child_parts);
    free(names->parent_parts);
    *names = (struct foreign_key_column_names){0};
}

static int copy_foreign_key_part_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct foreign_key_part_name **out_parts,
    size_t *out_part_count
) {
    struct foreign_key_part_name *parts = NULL;
    size_t part_count = 0U;

    *out_parts = NULL;
    *out_part_count = 0U;

    if (part_list == NULL || part_list->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    part_count = mylite_sql_ast_node_child_count(part_list);
    if (part_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (part_count > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    parts = calloc(part_count, sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < part_count; ++index) {
        const struct mylite_sql_ast_node *part = child_at(part_list, index);
        int rc = MYLITE_OK;

        if (part == NULL || part->kind != MYLITE_SQL_AST_IDENTIFIER) {
            free(parts);
            set_unsupported_error(database, "FOREIGN KEY supports only unqualified key columns");
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(part, parts[index].name, sizeof(parts[index].name), database);
        if (rc != MYLITE_OK) {
            free(parts);
            return rc;
        }
    }

    *out_parts = parts;
    *out_part_count = part_count;
    return MYLITE_OK;
}

static int validate_foreign_key_part_counts(
    struct mylite_db *database,
    size_t child_part_count,
    size_t parent_part_count
) {
    if (child_part_count == 0U || parent_part_count == 0U ||
        child_part_count != parent_part_count) {
        set_incorrect_foreign_key_definition_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_duplicate_foreign_key_child_parts(
    struct mylite_db *database,
    const struct foreign_key_column_names *names
) {
    for (size_t left = 0U; left < names->child_part_count; ++left) {
        for (size_t right = left + 1U; right < names->child_part_count; ++right) {
            if (text_equals_ascii_case_insensitive(
                    names->child_parts[left].name,
                    names->child_parts[right].name
                )) {
                set_duplicate_column_error(database, names->child_parts[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static int plan_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *constraint_name_node,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    destination[0] = '\0';
    if (constraint_name_node == NULL) {
        return generate_create_table_foreign_key_name(
            database,
            plan,
            destination,
            destination_size
        );
    }

    rc = copy_identifier_text(constraint_name_node, destination, destination_size, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (planned_foreign_key_name_is_used(plan, destination)) {
        set_duplicate_foreign_key_error(database, destination);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generate_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    char *destination,
    size_t destination_size
) {
    unsigned int suffix = 1U;

    while (suffix < UINT_MAX) {
        char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int written =
            snprintf(candidate, sizeof(candidate), "%s_ibfk_%u", plan->target.table_name, suffix);

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "constraint");
            return MYLITE_ERROR;
        }
        if (!planned_foreign_key_name_is_used(plan, candidate)) {
            int copied = snprintf(destination, destination_size, "%s", candidate);

            if (copied < 0 || (size_t)copied >= destination_size) {
                set_identifier_too_long_error(database, "constraint");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "constraint");
    return MYLITE_ERROR;
}

static bool planned_foreign_key_name_is_used(
    const struct planned_create_table *plan,
    const char *foreign_key_name
) {
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        if (text_equals_ascii_case_insensitive(plan->foreign_keys[index].name, foreign_key_name)) {
            return true;
        }
    }

    return false;
}

static int validate_foreign_key_integer_columns(
    struct mylite_db *database,
    const char *child_logical_type,
    const char *parent_logical_type
) {
    if (child_logical_type == NULL || parent_logical_type == NULL ||
        !text_equals_ascii_case_insensitive(child_logical_type, parent_logical_type)) {
        set_foreign_key_column_incompatible_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(child_logical_type, "TINYINT") == 0 ||
        strcmp(child_logical_type, "TINYINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "SMALLINT") == 0 ||
        strcmp(child_logical_type, "SMALLINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "MEDIUMINT") == 0 ||
        strcmp(child_logical_type, "MEDIUMINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "INT") == 0 || strcmp(child_logical_type, "INT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "INTEGER") == 0 ||
        strcmp(child_logical_type, "INTEGER UNSIGNED") == 0 ||
        strcmp(child_logical_type, "BIGINT") == 0 ||
        strcmp(child_logical_type, "BIGINT UNSIGNED") == 0) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "FOREIGN KEY supports only baseline integer columns");
    return MYLITE_ERROR;
}

static void planned_foreign_key_deinit(struct planned_foreign_key *foreign_key) {
    if (foreign_key == NULL) {
        return;
    }
    free(foreign_key->parts);
    *foreign_key = (struct planned_foreign_key){0};
}

static int append_planned_foreign_key_part(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t child_column_index,
    const struct mylite_catalog_column_descriptor *parent_column
) {
    int rc = reserve_planned_foreign_key_parts(database, foreign_key, foreign_key->part_count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }

    foreign_key->parts[foreign_key->part_count] = (struct planned_foreign_key_part){
        .child_column_index = child_column_index,
        .parent_column = *parent_column,
    };
    ++foreign_key->part_count;
    return MYLITE_OK;
}

static int reserve_planned_foreign_key_parts(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t required_capacity
) {
    enum { initial_foreign_key_part_capacity = 4 };

    struct planned_foreign_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= foreign_key->part_capacity) {
        return MYLITE_OK;
    }
    capacity = foreign_key->part_capacity == 0U ? initial_foreign_key_part_capacity
                                                : foreign_key->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(foreign_key->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    foreign_key->parts = parts;
    foreign_key->part_capacity = capacity;
    return MYLITE_OK;
}

static int plan_create_table_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *parent_table_node,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
) {
    struct table_name_resolution parent_resolution = {0};
    struct mylite_catalog_column_descriptor *parent_columns = NULL;
    size_t parent_column_count = 0U;
    int rc = resolve_table_name(database, parent_table_node, &parent_resolution);

    if (rc == MYLITE_OK && parent_resolution.schema.schema_id != plan->target.schema.schema_id) {
        set_unsupported_error(database, "FOREIGN KEY does not yet support cross-schema references");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            parent_resolution.schema.schema_id,
            parent_resolution.table_name,
            &foreign_key->parent_table
        );
        if (rc != MYLITE_OK) {
            set_failed_to_open_referenced_table_error(database, parent_resolution.table_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && foreign_key->parent_table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "FOREIGN KEY references only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            foreign_key->parent_table.table_id,
            &parent_columns,
            &parent_column_count
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < names->child_part_count;
         ++part_index) {
        size_t child_column_index = 0U;
        size_t parent_column_index = 0U;

        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            names->child_parts[part_index].name,
            &child_column_index
        );
        if (rc != MYLITE_OK) {
            set_key_column_missing_error(database, names->child_parts[part_index].name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = find_column_index(
                parent_columns,
                parent_column_count,
                names->parent_parts[part_index].name,
                &parent_column_index
            );
            if (rc != MYLITE_OK) {
                set_key_column_missing_error(database, names->parent_parts[part_index].name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            rc = validate_foreign_key_integer_columns(
                database,
                plan->columns[child_column_index].logical_type,
                parent_columns[parent_column_index].logical_type
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_planned_foreign_key_part(
                database,
                foreign_key,
                child_column_index,
                &parent_columns[parent_column_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = find_foreign_key_parent_index(
            database,
            &foreign_key->parent_table,
            foreign_key,
            &foreign_key->parent_index
        );
    }

    free(parent_columns);
    return rc;
}

static int find_foreign_key_parent_index(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *parent_table,
    const struct planned_foreign_key *foreign_key,
    struct mylite_catalog_index_descriptor *out_index
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(database, parent_table->table_id, &columns, &column_count);

    *out_index = (struct mylite_catalog_index_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            parent_table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        const struct loaded_index_info *candidate = &indexes[index];
        bool matches = false;

        if (!candidate->index.is_unique || candidate->part_count != foreign_key->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < foreign_key->part_count;
             ++part_index) {
            const struct loaded_index_part *candidate_part = &candidate->parts[part_index];

            if (candidate_part->index_column.has_prefix_length ||
                candidate_part->index_column.column_id !=
                    foreign_key->parts[part_index].parent_column.column_id) {
                matches = false;
            }
        }
        if (matches) {
            *out_index = candidate->index;
            break;
        }
    }
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index->index_id <= 0) {
        set_foreign_key_missing_unique_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int find_alter_table_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct mylite_catalog_index_descriptor *out_index
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(database, plan->parent_table.table_id, &columns, &column_count);

    *out_index = (struct mylite_catalog_index_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            plan->parent_table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        const struct loaded_index_info *candidate = &indexes[index];
        bool matches = false;

        if (!candidate->index.is_unique || candidate->part_count != plan->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < plan->part_count; ++part_index) {
            const struct loaded_index_part *candidate_part = &candidate->parts[part_index];

            if (candidate_part->index_column.has_prefix_length ||
                candidate_part->index_column.column_id !=
                    plan->parts[part_index].parent_column.column_id) {
                matches = false;
            }
        }
        if (matches) {
            *out_index = candidate->index;
            break;
        }
    }
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index->index_id <= 0) {
        set_foreign_key_missing_unique_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int choose_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
) {
    if (planned_primary_key_starts_with_column(plan, foreign_key)) {
        foreign_key->child_index_is_primary = true;
        foreign_key->child_secondary_index_index = 0U;
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        const struct planned_secondary_index *candidate = &plan->secondary_indexes[index];
        bool matches = false;

        if (candidate->part_count < foreign_key->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < foreign_key->part_count;
             ++part_index) {
            if (candidate->parts[part_index].has_prefix_length ||
                candidate->parts[part_index].column_index !=
                    foreign_key->parts[part_index].child_column_index) {
                matches = false;
            }
        }
        if (matches) {
            foreign_key->child_index_is_primary = false;
            foreign_key->child_secondary_index_index = index;
            return MYLITE_OK;
        }
    }

    return add_create_table_foreign_key_child_index(database, plan, foreign_key);
}

static int add_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
) {
    struct planned_secondary_index index = {0};
    const struct planned_column *column = &plan->columns[foreign_key->parts[0].child_column_index];
    const char *default_name = column->name;
    int rc = MYLITE_OK;

    if (foreign_key->has_explicit_name) {
        default_name = foreign_key->name;
    } else if (foreign_key->child_index_name[0] != '\0') {
        default_name = foreign_key->child_index_name;
    }

    rc = plan_create_table_foreign_key_child_index_name(
        database,
        plan,
        foreign_key,
        default_name,
        index.name,
        sizeof(index.name)
    );

    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_indexes(database, plan, plan->secondary_index_count + 1U);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_index_parts(database, &index, foreign_key->part_count);
    }
    if (rc == MYLITE_OK) {
        index.kind = MYLITE_CATALOG_INDEX_KIND_SECONDARY;
        index.is_unique = false;
        index.is_visible = true;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        index.parts[part_index] = (struct planned_secondary_index_part){
            .column_index = foreign_key->parts[part_index].child_column_index,
            .has_prefix_length = false,
            .prefix_length = 0,
            .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
        };
        ++index.part_count;
    }
    if (rc == MYLITE_OK) {
        foreign_key->child_index_is_primary = false;
        foreign_key->child_secondary_index_index = plan->secondary_index_count;
        plan->secondary_indexes[plan->secondary_index_count] = index;
        index = (struct planned_secondary_index){0};
        ++plan->secondary_index_count;
    }

    free(index.parts);
    return rc;
}

static int plan_create_table_foreign_key_child_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key,
    const char *default_name,
    char *destination,
    size_t destination_size
) {
    int copied = 0;

    if (!foreign_key->has_explicit_name && foreign_key->child_index_name[0] == '\0') {
        return plan_secondary_index_name(
            database,
            plan,
            NULL,
            default_name,
            destination,
            destination_size
        );
    }
    if (text_equals_ascii_case_insensitive(default_name, "PRIMARY")) {
        set_incorrect_index_name_error(database, default_name);
        return MYLITE_ERROR;
    }
    if (planned_index_name_is_used(plan, default_name)) {
        set_duplicate_key_name_error(database, default_name);
        return MYLITE_ERROR;
    }

    copied = snprintf(destination, destination_size, "%s", default_name);
    if (copied < 0 || (size_t)copied >= destination_size) {
        set_identifier_too_long_error(database, "index");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool planned_primary_key_starts_with_column(
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
) {
    if (plan == NULL || foreign_key == NULL || !plan->has_primary_key ||
        plan->primary_key_part_count < foreign_key->part_count) {
        return false;
    }

    for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
        if (plan->primary_key_parts[part_index].column_index !=
            foreign_key->parts[part_index].child_column_index) {
            return false;
        }
    }

    return true;
}

static int reserve_planned_foreign_keys(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_foreign_key_capacity = 4 };

    struct planned_foreign_key *foreign_keys = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->foreign_key_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->foreign_key_capacity == 0U ? initial_foreign_key_capacity
                                                : plan->foreign_key_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*foreign_keys)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    foreign_keys = realloc(plan->foreign_keys, capacity * sizeof(*foreign_keys));
    if (foreign_keys == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->foreign_keys = foreign_keys;
    plan->foreign_key_capacity = capacity;
    return MYLITE_OK;
}

static int append_planned_secondary_index_part(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_secondary_index *index,
    const struct mylite_sql_ast_node *part
) {
    const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
    const struct mylite_sql_ast_node *prefix_node = secondary_index_part_prefix_node(part);
    const struct mylite_sql_ast_node *direction_node = secondary_index_part_direction_node(part);
    enum mylite_catalog_index_sort_direction sort_direction = index_part_sort_direction(part);
    bool is_fulltext = planned_secondary_index_is_fulltext(index);
    bool is_spatial = planned_secondary_index_is_spatial(index);
    bool has_prefix_length = false;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t prefix_length = 0;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            index_part_unsupported_message(
                part,
                planned_secondary_index_unqualified_column_message(index)
            )
        );
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &column_index
        );
    }
    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column_index == column_index) {
            set_duplicate_column_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_planned_secondary_index_part(
            database,
            plan,
            index,
            column_index,
            column_name,
            (struct secondary_index_part_nodes){
                .prefix_node = prefix_node,
                .direction_node = direction_node,
            },
            &prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_index_parts(database, index, index->part_count + 1U);
    }
    if (rc == MYLITE_OK) {
        enum mylite_catalog_index_sort_direction stored_sort_direction = sort_direction;
        int64_t stored_prefix_length = prefix_length;

        if (is_fulltext || is_spatial) {
            stored_sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
            stored_prefix_length = 0;
        } else if (prefix_node != NULL) {
            has_prefix_length = true;
        }
        index->parts[index->part_count] = (struct planned_secondary_index_part){
            .column_index = column_index,
            .has_prefix_length = has_prefix_length,
            .prefix_length = stored_prefix_length,
            .sort_direction = stored_sort_direction,
        };
        ++index->part_count;
    }

    return rc;
}

static const char *planned_secondary_index_unqualified_column_message(
    const struct planned_secondary_index *index
) {
    if (planned_secondary_index_is_fulltext(index)) {
        return "FULLTEXT indexes support only unqualified key columns";
    }
    if (planned_secondary_index_is_spatial(index)) {
        return "SPATIAL indexes support only unqualified key columns";
    }
    if (index != NULL && index->is_unique) {
        return "Unique indexes support only unqualified key columns";
    }

    return "Secondary indexes support only unqualified key columns";
}

static int validate_planned_secondary_index_part(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index,
    size_t column_index,
    const char *column_name,
    struct secondary_index_part_nodes nodes,
    int64_t *out_prefix_length
) {
    bool is_fulltext = planned_secondary_index_is_fulltext(index);
    bool is_spatial = planned_secondary_index_is_spatial(index);
    int rc = MYLITE_OK;

    if (plan == NULL || index == NULL || column_index >= plan->column_count ||
        out_prefix_length == NULL) {
        set_runtime_error(database, "invalid secondary-index key part");
        return MYLITE_ERROR;
    }
    if ((is_fulltext || is_spatial) && nodes.direction_node != NULL) {
        set_fulltext_explicit_order_error(database);
        return MYLITE_ERROR;
    }
    if (nodes.prefix_node != NULL) {
        uint64_t key_bytes = 0U;

        rc = parse_secondary_index_part_prefix_length(
            database,
            nodes.prefix_node,
            column_name,
            out_prefix_length
        );
        if (rc == MYLITE_OK && is_spatial) {
            set_incorrect_prefix_key_error(database);
            rc = MYLITE_ERROR;
        } else if (rc == MYLITE_OK && !is_fulltext) {
            rc = validate_secondary_index_prefix_for_planned_column(
                database,
                &plan->columns[column_index],
                *out_prefix_length,
                &key_bytes
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (is_fulltext) {
        return validate_fulltext_index_column(database, &plan->columns[column_index]);
    }
    if (is_spatial) {
        return validate_spatial_index_column(database, &plan->columns[column_index]);
    }
    if (nodes.prefix_node != NULL) {
        return MYLITE_OK;
    }
    if (index->is_unique) {
        return validate_unique_index_column(database, &plan->columns[column_index]);
    }

    return validate_secondary_index_column(database, &plan->columns[column_index]);
}

static int validate_planned_unique_index_part_list(
    struct mylite_db *database,
    const struct planned_secondary_index *index
) {
    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid unique-index key parts");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reserve_planned_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
) {
    enum { initial_part_capacity = 4 };

    struct planned_secondary_index_part *parts = NULL;
    size_t capacity = 0U;

    if (index == NULL) {
        set_runtime_error(database, "invalid secondary-index plan");
        return MYLITE_ERROR;
    }
    if (required_capacity <= index->part_capacity) {
        return MYLITE_OK;
    }
    capacity = index->part_capacity == 0U ? initial_part_capacity : index->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(index->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    index->parts = parts;
    index->part_capacity = capacity;
    return MYLITE_OK;
}

static int reserve_planned_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_secondary_index_capacity = 4 };

    struct planned_secondary_index *indexes = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->secondary_index_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->secondary_index_capacity == 0U ? initial_secondary_index_capacity
                                                    : plan->secondary_index_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(plan->secondary_indexes, capacity * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->secondary_indexes = indexes;
    plan->secondary_index_capacity = capacity;
    return MYLITE_OK;
}

static int plan_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    destination[0] = '\0';
    if (index_name_node == NULL) {
        return generate_secondary_index_name(
            database,
            plan,
            column_name,
            destination,
            destination_size
        );
    }

    rc = copy_identifier_text(index_name_node, destination, destination_size, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(destination, "PRIMARY")) {
        set_incorrect_index_name_error(database, destination);
        return MYLITE_ERROR;
    }
    if (planned_index_name_is_used(plan, destination)) {
        set_duplicate_key_name_error(database, destination);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generate_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *base_name,
    char *destination,
    size_t destination_size
) {
    char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    unsigned int suffix = 1U;

    while (suffix < UINT_MAX) {
        int written = 0;

        if (suffix == 1U) {
            written = snprintf(candidate, sizeof(candidate), "%s", base_name);
        } else {
            written = snprintf(candidate, sizeof(candidate), "%s_%u", base_name, suffix);
        }
        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "index");
            return MYLITE_ERROR;
        }
        if (!text_equals_ascii_case_insensitive(candidate, "PRIMARY") &&
            !planned_index_name_is_used(plan, candidate)) {
            int copied = snprintf(destination, destination_size, "%s", candidate);

            if (copied < 0 || (size_t)copied >= destination_size) {
                set_identifier_too_long_error(database, "index");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "index");
    return MYLITE_ERROR;
}

static bool planned_index_name_is_used(
    const struct planned_create_table *plan,
    const char *index_name
) {
    if (plan->has_primary_key && text_equals_ascii_case_insensitive(index_name, "PRIMARY")) {
        return true;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (text_equals_ascii_case_insensitive(plan->secondary_indexes[index].name, index_name)) {
            return true;
        }
    }

    return false;
}

static int text_family_full_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_key_bytes
) {
    const struct text_family_type_info *info = text_family_type_info_for_logical_type(logical_type);

    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid text-family key part");
        return MYLITE_ERROR;
    }

    *out_key_bytes = utf8mb4_max_bytes_per_character;
    return MYLITE_OK;
}

static int validate_secondary_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid secondary-index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }
    if (planned_column_is_binary_blob_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_planned_string_key_column(database, column);
    }
    if (planned_column_is_approximate(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_fulltext_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid FULLTEXT index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_fulltext_string_key_column(database, column);
    }
    if (planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }

    set_fulltext_column_error(database, column->name);
    return MYLITE_ERROR;
}

static int validate_spatial_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid SPATIAL index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (!planned_column_is_spatial(column)) {
        set_spatial_index_non_geometric_error(database);
        return MYLITE_ERROR;
    }
    if (column->is_nullable) {
        set_spatial_index_must_be_not_null_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_fulltext_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !planned_column_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid FULLTEXT string key column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "FULLTEXT indexes support only baseline CHAR descriptors",
            &length
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "FULLTEXT indexes support only baseline VARCHAR descriptors",
            &length
        );
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index) {
    if (index == NULL) {
        return false;
    }
    if (index->kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return true;
    }

    return false;
}

static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index) {
    if (index == NULL) {
        return false;
    }
    if (index->kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return true;
    }

    return false;
}

static int validate_secondary_index_prefix_for_planned_column(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    uint64_t prefix = prefix_length < 0 ? 0U : (uint64_t)prefix_length;
    uint64_t max_bytes_per_character = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index prefix column");
        return MYLITE_ERROR;
    }
    *out_key_bytes = 0U;
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "prefix keys support only baseline CHAR descriptors",
            &column_length
        );
    } else if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "prefix keys support only baseline VARCHAR descriptors",
            &column_length
        );
    } else if (planned_column_is_text_family(column)) {
        return text_family_prefix_key_bytes(database, column->logical_type, prefix, out_key_bytes);
    } else if (planned_column_is_binary_string_family(column)) {
        return binary_string_prefix_key_bytes(
            database,
            column->logical_type,
            prefix,
            out_key_bytes
        );
    } else if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    } else {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix > column_length) {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    max_bytes_per_character = logical_type_max_bytes_per_character(column->logical_type);
    if (prefix > UINT64_MAX / max_bytes_per_character) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }

    *out_key_bytes = prefix * max_bytes_per_character;
    return MYLITE_OK;
}

static int text_family_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
) {
    const struct text_family_type_info *info = text_family_type_info_for_logical_type(logical_type);
    uint64_t key_bytes = 0U;

    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid text-family prefix key part");
        return MYLITE_ERROR;
    }
    if (prefix > UINT64_MAX / utf8mb4_max_bytes_per_character) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }

    key_bytes = prefix * utf8mb4_max_bytes_per_character;
    if (key_bytes > info->maximum_length) {
        set_key_too_long_error(database, info->maximum_length);
        return MYLITE_ERROR;
    }

    *out_key_bytes = key_bytes;
    return MYLITE_OK;
}

static int binary_string_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(logical_type, &storage);

    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid binary prefix key part");
        return MYLITE_ERROR;
    }
    if (prefix > info->maximum_length) {
        if (info->blob_family) {
            set_key_too_long_error(database, info->maximum_length);
        } else {
            set_incorrect_prefix_key_error(database);
        }
        return MYLITE_ERROR;
    }

    *out_key_bytes = prefix;
    return MYLITE_OK;
}

static int binary_string_full_key_bytes(
    struct mylite_db *database,
    const struct binary_string_type_info *info,
    const char *column_name,
    uint64_t *out_key_bytes
) {
    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid binary string key part");
        return MYLITE_ERROR;
    }
    if (info->blob_family) {
        set_blob_key_without_length_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (info->maximum_length == 0U) {
        set_storage_engine_cant_index_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    *out_key_bytes = info->maximum_length;
    return MYLITE_OK;
}

static int planned_index_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index key part");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "secondary indexes support only baseline CHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "secondary indexes support only baseline VARCHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_text_family(column)) {
        return text_family_full_key_bytes(database, column->logical_type, out_key_bytes);
    }
    if (planned_column_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        return binary_string_full_key_bytes(database, info, column->name, out_key_bytes);
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "Secondary indexes do not yet support this column type",
        out_key_bytes
    );
}

static int validate_unique_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid unique-index column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_spatial_unique_error(database);
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }
    if (planned_column_is_binary_blob_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_planned_string_key_column(database, column);
    }
    if (planned_column_is_approximate(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_planned_secondary_index_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index
) {
    uint64_t total = 0U;

    if (plan == NULL || index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid secondary-index descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct planned_secondary_index_part *part = &index->parts[part_index];
        const struct planned_column *column = NULL;
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column_index >= plan->column_count) {
            set_runtime_error(database, "invalid secondary-index key part");
            return MYLITE_ERROR;
        }
        column = &plan->columns[part->column_index];
        if (part->has_prefix_length) {
            rc = validate_secondary_index_prefix_for_planned_column(
                database,
                column,
                part->prefix_length,
                &part_bytes
            );
        } else {
            rc = planned_index_part_key_bytes(database, column, &part_bytes);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int parse_secondary_index_part_prefix_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *prefix_node,
    const char *column_name,
    int64_t *out_prefix_length
) {
    uint64_t prefix = 0U;
    int rc = MYLITE_OK;

    if (out_prefix_length == NULL || column_name == NULL || prefix_node == NULL ||
        prefix_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(prefix_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = parse_unsigned_integer_literal(&prefix_node->span, &prefix);
    if (rc != MYLITE_OK || prefix > INT64_MAX) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }
    if (prefix == 0U) {
        set_key_part_length_cannot_be_zero_error(database, column_name);
        return MYLITE_ERROR;
    }

    *out_prefix_length = (int64_t)prefix;
    return MYLITE_OK;
}

static const char *index_part_unsupported_message(
    const struct mylite_sql_ast_node *part,
    const char *default_message
) {
    if (secondary_index_part_is_multi_valued(part)) {
        return "Multi-valued indexes are not yet supported";
    }
    if (secondary_index_part_is_functional(part)) {
        return "Functional key parts are not yet supported";
    }
    return default_message;
}

static bool secondary_index_part_is_multi_valued(const struct mylite_sql_ast_node *part) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return false;
    }

    first_child = child_at(part, 0U);
    return first_child != NULL && first_child->kind == MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART;
}

static bool secondary_index_part_is_functional(const struct mylite_sql_ast_node *part) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return false;
    }

    first_child = child_at(part, 0U);
    return first_child != NULL && first_child->kind != MYLITE_SQL_AST_IDENTIFIER;
}

static const struct mylite_sql_ast_node *secondary_index_part_column_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL) {
        return NULL;
    }
    if (part->kind == MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return child_at(part, 0U);
    }
    return part;
}

static const struct mylite_sql_ast_node *secondary_index_part_prefix_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return NULL;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return NULL;
        }
        if (child->kind == MYLITE_SQL_AST_LITERAL) {
            return child;
        }
    }
}

static const struct mylite_sql_ast_node *secondary_index_part_direction_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return NULL;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return NULL;
        }
        if (child->kind == MYLITE_SQL_AST_ORDER_DIRECTION) {
            return child;
        }
    }
}

static enum mylite_catalog_index_sort_direction index_part_sort_direction(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
        }
        if (child->kind != MYLITE_SQL_AST_ORDER_DIRECTION) {
            continue;
        }
        if (mylite_sql_ast_node_order_direction(child) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
            return MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC;
        }
        return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
    }
}

static int mark_primary_key_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    int rc = MYLITE_OK;

    if (plan == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid primary-key column");
        return MYLITE_ERROR;
    }
    if (plan->columns[column_index].is_primary_key) {
        set_multiple_primary_key_error(database);
        return MYLITE_ERROR;
    }

    rc = validate_primary_key_column(database, &plan->columns[column_index]);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reserve_planned_primary_key_parts(database, plan, plan->primary_key_part_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    plan->has_primary_key = true;
    plan->primary_key_parts[plan->primary_key_part_count] = (struct planned_primary_key_part){
        .column_index = column_index,
        .sort_direction = sort_direction,
    };
    ++plan->primary_key_part_count;
    plan->columns[column_index].is_primary_key = true;
    plan->columns[column_index].is_nullable = false;

    return MYLITE_OK;
}

static int reserve_planned_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_primary_key_part_capacity = 4 };

    struct planned_primary_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->primary_key_part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->primary_key_part_capacity == 0U ? initial_primary_key_part_capacity
                                                     : plan->primary_key_part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->primary_key_parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->primary_key_parts = parts;
    plan->primary_key_part_capacity = capacity;
    return MYLITE_OK;
}

static bool planned_primary_key_contains_column(
    const struct planned_create_table *plan,
    size_t column_index
) {
    for (size_t part_index = 0U; part_index < plan->primary_key_part_count; ++part_index) {
        if (plan->primary_key_parts[part_index].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int validate_primary_key_column(struct mylite_db *database, struct planned_column *column) {
    if (column == NULL) {
        set_runtime_error(database, "invalid primary-key column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_spatial_unique_error(database);
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        int rc = validate_planned_string_key_column(database, column);

        if (rc != MYLITE_OK) {
            return rc;
        }
    } else if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    } else if (
        planned_column_is_string_family(column) || planned_column_is_binary_string_family(column) ||
        planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column) || planned_column_is_decimal(column) ||
        planned_column_is_approximate(column) || planned_column_is_date(column) ||
        planned_column_is_time(column) || planned_column_is_datetime(column) ||
        planned_column_is_timestamp(column) || planned_column_is_year(column)
    ) {
        if (column->is_auto_increment) {
            return MYLITE_OK;
        }
        set_unsupported_error(database, "PRIMARY KEY supports only integer columns");
        return MYLITE_ERROR;
    }
    if (column->nullability == MYLITE_SQL_AST_NULLABILITY_NULL) {
        set_primary_key_part_null_error(database);
        return MYLITE_ERROR;
    }
    if (column->default_node != NULL &&
        column->default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_auto_increment_columns(
    struct mylite_db *database,
    struct planned_create_table *plan,
    bool allow_secondary_key
) {
    size_t auto_increment_count = 0U;
    size_t auto_increment_column_index = 0U;

    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment table plan");
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_auto_increment) {
            ++auto_increment_count;
            auto_increment_column_index = column_index;
        }
    }
    if (auto_increment_count == 0U) {
        return MYLITE_OK;
    }
    if (auto_increment_count > 1U) {
        set_wrong_auto_key_error(database);
        return MYLITE_ERROR;
    }

    return validate_auto_increment_column(
        database,
        plan,
        auto_increment_column_index,
        allow_secondary_key
    );
}

static int validate_auto_increment_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t column_index,
    bool allow_secondary_key
) {
    const struct planned_column *column = NULL;

    if (plan == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid auto-increment column");
        return MYLITE_ERROR;
    }

    column = &plan->columns[column_index];
    if (planned_column_is_string_family(column) || planned_column_is_binary_string_family(column) ||
        planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column) || planned_column_is_decimal(column) ||
        planned_column_is_approximate(column) || planned_column_is_date(column) ||
        planned_column_is_time(column) || planned_column_is_datetime(column) ||
        planned_column_is_timestamp(column)) {
        set_incorrect_column_specifier_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column->default_node != NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (plan->has_primary_key && plan->primary_key_part_count == 1U &&
        column_index == plan->primary_key_parts[0].column_index && column->is_primary_key) {
        return MYLITE_OK;
    }
    if (allow_secondary_key && planned_secondary_index_starts_with_column(plan, column_index)) {
        return MYLITE_OK;
    }

    set_wrong_auto_key_error(database);
    return MYLITE_ERROR;
}

static bool planned_secondary_index_starts_with_column(
    const struct planned_create_table *plan,
    size_t column_index
) {
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].part_count > 0U &&
            plan->secondary_indexes[index].parts[0].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int find_planned_column_index(
    const struct planned_column *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
) {
    *out_index = 0U;
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}
