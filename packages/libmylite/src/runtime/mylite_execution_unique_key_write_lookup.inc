static int unique_key_tuple_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(statement, index, values, value_count);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);

    return rc;
}

static int build_unique_key_lookup_sql(
    const char *physical_table_name,
    const struct loaded_index_info *index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &index->parts[part_index],
                part_index + 1U
            );
        }
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

static bool loaded_index_contains_column_id(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    if (index == NULL) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static int unique_key_single_value_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *value,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_planned_value_parameter(statement, 1, value);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);

    return rc;
}

static int find_update_unique_key_conflict_tuple(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char *destination,
    size_t destination_size,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_found = false;
    destination[0] = '\0';
    rc = build_update_unique_key_conflict_sql(executable_plan, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        const struct update_unique_key_conflict_bind_request request = {
            .executable_plan = executable_plan,
            .plan = plan,
            .index = index,
        };
        rc = bind_update_unique_key_conflict_parameters(statement, &request);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                index->parts,
                index->part_count,
                destination,
                destination_size
            );
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);
    statement = NULL;
    sql = NULL;
    if (rc == MYLITE_OK && !*out_found) {
        rc = build_update_unique_key_internal_conflict_sql(executable_plan, index, &sql);
        if (rc == MYLITE_OK) {
            rc = prepare_cached_sqlite_statement(database, sql, &statement);
        }
        if (rc == MYLITE_OK) {
            const struct update_unique_key_conflict_bind_request request = {
                .executable_plan = executable_plan,
                .plan = plan,
                .index = index,
            };
            rc = bind_update_unique_key_internal_conflict_parameters(statement, &request);
        }
        if (rc == MYLITE_OK) {
            sqlite_rc = sqlite3_step(statement);
            if (sqlite_rc == SQLITE_ROW) {
                rc = format_sqlite_key_tuple(
                    statement,
                    index->parts,
                    index->part_count,
                    destination,
                    destination_size
                );
                if (rc == MYLITE_OK) {
                    *out_found = true;
                }
            } else if (sqlite_rc != SQLITE_DONE) {
                rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        }
        rc = finish_cached_sqlite_statement(database, statement, rc);
        free(sql);
    }

    return rc;
}

static int build_update_unique_key_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char conflict_alias[] = "_mylite_conflict";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_select_list(&string, plan, index, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_exists_clause(
            &string,
            plan,
            index,
            conflict_alias,
            &next_parameter
        );
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

static int build_update_unique_key_internal_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char projected_alias[] = "_mylite_projected";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_select_list(
            &string,
            plan,
            index,
            &next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, projected_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_not_null_filter(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 LIMIT 1");
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

static int append_update_unique_key_internal_conflict_alias_list(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_not_null_filter(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_candidate_source(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = mylite_dynamic_string_append(string, " FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }

    return rc;
}

static int append_update_unique_key_conflict_target_filter(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = append_joined_update_rowid_filter_sql(string, plan, next_parameter);
    } else if (plan->limit.has_limit) {
        rc = append_update_rowid_limited_sql(string, plan, next_parameter);
    } else {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        if (plan->is_joined || plan->limit.has_limit ||
            planned_select_predicate_has_expression(&plan->predicate)) {
            rc = mylite_dynamic_string_append(string, " AND ");
        } else {
            rc = mylite_dynamic_string_append(string, " WHERE ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(string, plan, next_parameter);
    }

    return rc;
}

static int append_update_unique_key_conflict_exists_clause(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    const char *conflict_alias,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " AND EXISTS (SELECT 1 FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, conflict_alias);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &index->parts[part_index], conflict_alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_unique_key_conflict_expression(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_part *part,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (update_unique_key_assignment_value_for_part(plan, plan, part) != NULL) {
        rc = append_loaded_key_part_parameter_sql(string, part, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    }

    rc = append_loaded_key_part_sql(string, part, plan->table.physical_name);

    return rc;
}

static int bind_update_unique_key_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK && request->plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, request->plan);
    }
    if (rc == MYLITE_OK && request->plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &request->plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && planned_update_has_multiple_assignments(request->plan)) {
        rc = bind_update_multiple_changed_condition_parameters(
            statement,
            &parameter_index,
            request->executable_plan
        );
    } else if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);
    }

    return rc;
}

static int bind_update_unique_key_internal_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK && request->plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, request->plan);
    }
    if (rc == MYLITE_OK && request->plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &request->plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && planned_update_has_multiple_assignments(request->plan)) {
        rc = bind_update_multiple_changed_condition_parameters(
            statement,
            &parameter_index,
            request->executable_plan
        );
    } else if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
    }

    return rc;
}

static int bind_update_unique_key_assignment_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < request->index->part_count;
         ++part_index) {
        const struct planned_value *value = update_unique_key_assignment_value_for_part(
            request->executable_plan,
            request->plan,
            &request->index->parts[part_index]
        );

        if (value == NULL) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, value);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}
