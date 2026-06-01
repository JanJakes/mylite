static int build_delete_sql(const struct planned_delete *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_delete_rowid_filter_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = append_delete_rowid_limited_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int append_delete_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
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
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_joined_delete_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
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

static int append_joined_delete_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_joined_delete_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_delete_from_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }

    return rc;
}

static int append_joined_delete_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan
) {
    int rc = append_select_source_alias(string, plan->target_source_index);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }

    return rc;
}

static int append_joined_delete_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }
    if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
    } else if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " JOIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            plan->sources[1].table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_join_condition_sql(string, &plan->join_condition);
    }

    return rc;
}

static int build_update_sql(const struct planned_update *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = update_assignment_parameter_count(plan) + 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = append_single_update_target_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_assignment_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_auto_update_columns_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_row_filter_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK && planned_update_has_row_filter(plan)) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(&string, plan, &next_parameter);
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

static int append_single_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);

    if (rc == MYLITE_OK && planned_update_needs_target_alias(plan)) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK && planned_update_needs_target_alias(plan)) {
        rc = append_select_source_alias(string, 0U);
    }

    return rc;
}

static bool planned_update_needs_target_alias(const struct planned_update *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->is_joined) {
        return false;
    }
    if (!plan->predicate.qualify_column_references) {
        return false;
    }
    return true;
}

static size_t update_assignment_parameter_count(const struct planned_update *plan) {
    if (planned_update_has_multiple_assignments(plan)) {
        return planned_update_executable_assignment_count(plan);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return update_date_interval_parameter_count;
    }
    return 1U;
}

static int append_update_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        return append_update_multiple_assignments_sql(string, plan);
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return append_update_date_interval_assignment_sql(string, plan);
    }
    if (plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return mylite_dynamic_string_append(string, "?1");
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    if (rc == MYLITE_OK && plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        rc = mylite_dynamic_string_append(string, " + ?1");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " - ?1");
    }

    return rc;
}

static int append_update_date_interval_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_update_date_interval_function_sql(string, &plan->assignment_column, 1U);
}

static int append_update_multiple_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    size_t emitted_count = 0U;
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (emitted_count > 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->assignments[index].column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, emitted_count + 1U);
        }
        if (rc == MYLITE_OK) {
            ++emitted_count;
        }
    }

    return rc;
}

static int append_update_auto_update_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (!planned_update_column_has_auto_update(plan, column)) {
            continue;
        }
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }

    return rc;
}

static bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
) {
    if (plan == NULL || column == NULL || !column->on_update_current_timestamp) {
        return false;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            if (planned_update_assignment_is_noop(&plan->assignments[index])) {
                continue;
            }
            if (plan->assignments[index].column.column_id == column->column_id) {
                return false;
            }
        }
        return true;
    }
    return column->column_id != plan->assignment_column.column_id;
}

static size_t planned_update_auto_update_column_count(const struct planned_update *plan) {
    size_t count = 0U;

    if (plan == NULL) {
        return 0U;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (planned_update_column_has_auto_update(plan, &plan->columns[column_index])) {
            ++count;
        }
    }
    return count;
}

static int build_update_matched_sql(const struct planned_update *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_update_from_sql(&string, plan);
    } else if (rc == MYLITE_OK) {
        rc = append_single_update_matched_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = append_joined_update_matched_target_filter_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
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

static int append_single_update_matched_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_single_update_target_sql(string, plan);
}

static int append_joined_update_matched_target_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);

    if (rc == MYLITE_OK && planned_select_predicate_has_expression(&plan->predicate)) {
        rc = mylite_dynamic_string_append(string, " AND ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT NULL");
    }

    return rc;
}

static bool planned_update_has_row_filter(const struct planned_update *plan) {
    if (plan->is_joined || plan->limit.has_limit) {
        return true;
    }
    if (planned_select_predicate_has_expression(&plan->predicate)) {
        return true;
    }
    return false;
}

static int append_update_row_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    if (plan->is_joined) {
        return append_joined_update_rowid_filter_sql(string, plan, next_parameter);
    }
    if (plan->limit.has_limit) {
        return append_update_rowid_limited_sql(string, plan, next_parameter);
    }
    if (planned_select_predicate_has_expression(&plan->predicate)) {
        return append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    return mylite_dynamic_string_append(string, " WHERE ");
}

static int append_update_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_single_update_rowid_source_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(string, &plan->order, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_single_update_rowid_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    return append_single_update_target_sql(string, plan);
}

static int append_joined_update_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_rowid_subquery_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_joined_update_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_joined_update_target_rowid_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_from_sql(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }

    return rc;
}

static int append_joined_update_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = append_select_source_alias(string, plan->target_source_index);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->rowid_alias);
    }

    return rc;
}

static int append_joined_update_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (plan->source_count != 2U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_quoted_identifier(
        string,
        plan->sources[0].table.physical_name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 0U);
    }
    if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
        rc = mylite_dynamic_string_append(string, " LEFT JOIN ");
    } else if (rc == MYLITE_OK && plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        rc = mylite_dynamic_string_append(string, " RIGHT JOIN ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " JOIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            plan->sources[1].table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_join_condition_sql(string, &plan->join_condition);
    }

    return rc;
}

static int append_update_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        return append_update_multiple_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return append_update_arithmetic_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return append_update_date_interval_changed_condition_sql(string, plan, next_parameter);
    }
    if (plan->assignment_value.is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_multiple_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    size_t emitted_count = 0U;
    int rc = mylite_dynamic_string_append_char(string, '(');

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (emitted_count > 0U) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_assignment_changed_condition_sql(
                string,
                &plan->assignments[index].column,
                &plan->assignments[index].value,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            ++emitted_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_assignment_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *value,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (value->is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_date_interval_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_date_interval_function_sql(
            string,
            &plan->assignment_column,
            *next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        *next_parameter += update_date_interval_parameter_count;
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_update_date_interval_function_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t first_parameter
) {
    int rc = mylite_dynamic_string_append(string, "_mylite_date_interval_update(");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    for (size_t offset = 0U; rc == MYLITE_OK && offset < update_date_interval_parameter_count;
         ++offset) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, first_parameter + offset);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_update_arithmetic_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NOT NULL AND ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> (");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK && plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        rc = mylite_dynamic_string_append(string, " + ");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " - ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }

    return rc;
}

static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }

    return "=";
}
