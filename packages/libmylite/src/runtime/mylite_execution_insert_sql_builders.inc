static int build_insert_sql(const struct planned_insert *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    size_t physical_column_count = count_physical_insert_columns(plan);
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") VALUES (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_parameters(&string, physical_column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int append_insert_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;
    bool has_previous = false;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (plan->columns[column_index].is_generated) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
        has_previous = true;
    }

    return rc;
}

static size_t count_physical_insert_columns(const struct planned_insert *plan) {
    size_t count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (!plan->columns[column_index].is_generated) {
            ++count;
        }
    }
    return count;
}

static int append_insert_parameters(struct mylite_dynamic_string *string, size_t column_count) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, column_index + 1U);
        }
    }

    return rc;
}

static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index) {
    char parameter[integer_text_capacity];
    int written = snprintf(parameter, sizeof(parameter), "?%zu", parameter_index);

    if (written < 0 || (size_t)written >= sizeof(parameter)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, parameter);
}

static int append_size_literal(struct mylite_dynamic_string *string, size_t value) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%zu", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, text);
}

static int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%" PRIu64, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append(string, text);
}

static int build_insert_select_temp_table_name(
    const struct mylite_db *database,
    char *destination,
    size_t destination_size
) {
    int written = snprintf(
        destination,
        destination_size,
        "_mylite_insert_select_%" PRIu64,
        database->session.connection_id
    );

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_insert_select_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return build_insert_select_compound_materialize_sql(plan, temporary_table_name, out_sql);
    }

    return build_insert_select_table_materialize_sql(plan, temporary_table_name, out_sql);
}

static int build_insert_select_table_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TEMP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        if (plan->source.is_distinct) {
            rc = mylite_dynamic_string_append(&string, " AS SELECT DISTINCT ");
        } else {
            rc = mylite_dynamic_string_append(&string, " AS SELECT ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_source_projection(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->source.table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->source.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->source.order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->source.limit, &next_parameter);
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

static int build_insert_select_compound_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TEMP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " UNION ALL ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_branch_sql(
                &string,
                branch,
                branch_index,
                branch_index == 0U,
                &next_parameter
            );
        }
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

static int append_insert_select_compound_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR) {
        return append_insert_select_compound_row_scalar_branch_sql(
            string,
            branch,
            branch_index,
            alias_columns,
            next_parameter
        );
    }

    return append_insert_select_compound_table_branch_sql(
        string,
        branch,
        branch_index,
        alias_columns,
        next_parameter
    );
}

static int append_insert_select_compound_table_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    const struct planned_select *source = &branch->source;
    int rc = MYLITE_OK;

    if (source->is_distinct) {
        rc = mylite_dynamic_string_append(string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(string, "SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_branch_marker_sql(string, branch_index, alias_columns);
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < source->column_count;
         ++column_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_column_projection_sql(string, source, column_index);
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = append_insert_select_temp_column_name(string, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(string, source, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &source->predicate, next_parameter);
    }

    return rc;
}

static int append_insert_select_compound_branch_marker_sql(
    struct mylite_dynamic_string *string,
    size_t branch_index,
    bool alias_columns
) {
    int rc = append_size_literal(string, branch_index);

    if (rc == MYLITE_OK && alias_columns) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK && alias_columns) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_branch_column_name
        );
    }

    return rc;
}

static int append_insert_select_compound_column_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *source,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &source->columns[column_index];
    int rc = append_descriptor_column_name_sql_for_source(
        string,
        column,
        source->column_source_indexes[column_index],
        planned_select_qualifies_source_references(source)
    );

    if (rc == MYLITE_OK && column_descriptor_is_string_family(column)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static int append_insert_select_compound_row_scalar_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
) {
    const struct planned_row_scalar_select *source = &branch->row_source;
    int rc = mylite_dynamic_string_append(string, "SELECT ");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_branch_marker_sql(string, branch_index, alias_columns);
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < source->item_count; ++item_index) {
        rc = mylite_dynamic_string_append(string, ", ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_row_scalar_projection_sql(
                string,
                &source->items[item_index].expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK && alias_columns) {
            rc = append_insert_select_temp_column_name(string, item_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_tableless_filter_sql(string, source, next_parameter);
    }

    return rc;
}

static int append_insert_select_compound_row_scalar_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
) {
    int rc = append_row_scalar_expression_sql(string, expression, next_parameter);

    if (rc == MYLITE_OK && row_scalar_expression_uses_string_collation(expression)) {
        rc = append_string_key_collation_sql(string);
    }

    return rc;
}

static bool row_scalar_expression_uses_string_collation(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return expression->value.is_text;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return column_descriptor_is_string_family(&expression->column);
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
        return true;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
                expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) != 0;
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        return expression->field_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        break;
    }

    return false;
}

static int build_insert_select_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return build_insert_select_compound_validation_sql(plan, temporary_table_name, out_sql);
    }

    return build_insert_select_table_validation_sql(plan, temporary_table_name, out_sql);
}

static int build_insert_select_table_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_temp_column_name(&string, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
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

static int build_insert_select_compound_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t last_distinct_branch = 0U;
    bool has_distinct_branch =
        insert_select_compound_last_distinct_branch(plan, &last_distinct_branch);
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_qualified_temp_column_name(
                &string,
                insert_select_union_current_alias,
                column_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ", ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_branch_column_name(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK && has_distinct_branch) {
        rc = append_insert_select_compound_distinct_filter_sql(
            &string,
            plan,
            temporary_table_name,
            last_distinct_branch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ".rowid");
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

static bool insert_select_compound_last_distinct_branch(
    const struct planned_insert_select *plan,
    size_t *out_branch_index
) {
    bool found = false;

    *out_branch_index = 0U;
    for (size_t branch_index = 1U; branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        if (plan->compound_source.branches[branch_index].modifier !=
            MYLITE_SQL_AST_UNION_MODIFIER_ALL) {
            *out_branch_index = branch_index;
            found = true;
        }
    }

    return found;
}

static int append_insert_select_compound_distinct_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    size_t last_distinct_branch
) {
    int rc = mylite_dynamic_string_append(string, " WHERE (");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_branch_column_name(
            string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " > ");
    }
    if (rc == MYLITE_OK) {
        rc = append_size_literal(string, last_distinct_branch);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " OR NOT EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(string, temporary_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_dynamic_string_append_quoted_identifier(string, insert_select_union_prior_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_dynamic_string_append_quoted_identifier(string, insert_select_union_prior_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ".rowid < ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_current_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ".rowid");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target_count;
         ++column_index) {
        rc = mylite_dynamic_string_append(string, " AND ");
        if (rc == MYLITE_OK) {
            rc = append_insert_select_compound_distinct_equality_sql(string, plan, column_index);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "))");
    }

    return rc;
}

static int append_insert_select_compound_distinct_equality_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    size_t column_index
) {
    bool use_string_collation =
        insert_select_compound_output_uses_string_collation(plan, column_index);
    int rc = mylite_dynamic_string_append(string, "((");

    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_prior_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_current_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK && use_string_collation) {
        rc = append_string_key_collation_sql(string);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") OR (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_prior_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_compound_qualified_temp_column_name(
            string,
            insert_select_union_current_alias,
            column_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL))");
    }

    return rc;
}

static int append_insert_select_compound_qualified_temp_column_name(
    struct mylite_dynamic_string *string,
    const char *alias,
    size_t column_index
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_column_name(string, column_index);
    }

    return rc;
}

static int append_insert_select_compound_qualified_branch_column_name(
    struct mylite_dynamic_string *string,
    const char *alias
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            string,
            insert_select_union_branch_column_name
        );
    }

    return rc;
}

static bool insert_select_compound_output_uses_string_collation(
    const struct planned_insert_select *plan,
    size_t column_index
) {
    for (size_t branch_index = 0U; branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE) {
            if (column_index < branch->source.column_count &&
                column_descriptor_is_string_family(&branch->source.columns[column_index])) {
                return true;
            }
        } else if (
            column_index < branch->row_source.item_count &&
            row_scalar_expression_uses_string_collation(
                &branch->row_source.items[column_index].expression
            )
        ) {
            return true;
        }
    }

    return false;
}

static int build_drop_temp_table_sql(const char *temporary_table_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP TABLE IF EXISTS ");
    if (rc == MYLITE_OK) {
        rc = append_insert_select_temp_table_name(&string, temporary_table_name);
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

static int append_insert_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->source.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->source.columns[column_index].name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = append_insert_select_temp_column_name(string, column_index);
        }
    }

    return rc;
}

static int append_insert_select_temp_column_name(
    struct mylite_dynamic_string *string,
    size_t column_index
) {
    char column_name[integer_text_capacity + sizeof("_mylite_value_")];
    int written = snprintf(column_name, sizeof(column_name), "_mylite_value_%zu", column_index);

    if (written < 0 || (size_t)written >= sizeof(column_name)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append_quoted_identifier(string, column_name);
}

static int append_insert_select_temp_table_name(
    struct mylite_dynamic_string *string,
    const char *temporary_table_name
) {
    int rc = mylite_dynamic_string_append(string, "temp.");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, temporary_table_name);
    }

    return rc;
}

static bool find_insert_select_target_position(
    const struct planned_insert_select *plan,
    size_t column_index,
    size_t *out_target_position
) {
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        if (plan->target_indexes[target_position] == column_index) {
            *out_target_position = target_position;
            return true;
        }
    }

    return false;
}

static int build_create_table_select_insert_sql(
    const struct planned_create_table_select *plan,
    const char *physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_select_target_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        if (plan->source.is_distinct) {
            rc = mylite_dynamic_string_append(&string, ") SELECT DISTINCT ");
        } else {
            rc = mylite_dynamic_string_append(&string, ") SELECT ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_create_table_select_source_projection(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->source.table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->source.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->source.order, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->source.limit, &next_parameter);
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

static int append_create_table_select_target_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < plan->create_table.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->create_table.columns[column_index].name
            );
        }
    }

    return rc;
}

static int append_create_table_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->source.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->source.columns[column_index].name
            );
        }
    }

    return rc;
}
