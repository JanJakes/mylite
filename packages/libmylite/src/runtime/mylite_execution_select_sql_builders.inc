static int build_select_sql(const struct planned_select *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (plan->is_distinct) {
        rc = mylite_dynamic_string_append(&string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(&string, "SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_projection_column_sql(&string, plan, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
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
    const struct planned_select *plan
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
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->sources[right_source_index].table.physical_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = append_select_source_alias(string, right_source_index);
        }
        if (rc == MYLITE_OK) {
            rc = append_select_join_condition_sql(string, &plan->join_conditions[join_index]);
        }
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
    const struct planned_select_join_condition *condition
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
        rc = append_descriptor_value_sql_for_source(
            string,
            &condition->right_column,
            condition->right_source_index,
            true
        );
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
    if (rc == MYLITE_OK && plan->has_source) {
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
