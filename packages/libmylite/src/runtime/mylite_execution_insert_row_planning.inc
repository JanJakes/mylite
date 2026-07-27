static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    const struct mylite_sql_ast_node *row_list,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t explicit_column_count = mylite_sql_ast_node_child_count(column_list);
    size_t column_count = explicit_column_count;
    bool omitted_column_list = false;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (column_list == NULL || column_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    omitted_column_list = insert_column_list_is_omitted(column_list);
    if (explicit_column_count == 0U) {
        if (!omitted_column_list || insert_row_list_first_row_is_empty(row_list)) {
            return MYLITE_OK;
        }
        column_count = count_visible_insert_target_columns(plan);
    }
    if (column_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(column_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (explicit_column_count == 0U) {
        collect_visible_insert_target_indexes(plan, indexes);
    } else {
        int rc = collect_explicit_insert_target_indexes(
            database,
            column_list,
            plan,
            indexes,
            column_count
        );

        if (rc != MYLITE_OK) {
            free(indexes);
            return rc;
        }
    }

    *out_indexes = indexes;
    *out_index_count = column_count;

    return MYLITE_OK;
}

static bool insert_column_list_is_omitted(const struct mylite_sql_ast_node *column_list) {
    if (column_list == NULL) {
        return true;
    }
    if (column_list->span.text != NULL) {
        return false;
    }
    if (column_list->span.length != 0U) {
        return false;
    }
    return true;
}

static bool insert_row_list_first_row_is_empty(const struct mylite_sql_ast_node *row_list) {
    const struct mylite_sql_ast_node *first_row = child_at(row_list, 0U);

    if (first_row == NULL) {
        return false;
    }
    if (first_row->kind != MYLITE_SQL_AST_INSERT_ROW) {
        return false;
    }
    if (mylite_sql_ast_node_child_count(first_row) != 0U) {
        return false;
    }
    return true;
}

static size_t count_visible_insert_target_columns(const struct planned_insert *plan) {
    size_t column_count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            ++column_count;
        }
    }

    return column_count;
}

static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
) {
    size_t target_index = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            indexes[target_index] = column_index;
            ++target_index;
        }
    }
}

static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);

        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = find_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &indexes[column_index]
        );
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        column_node = column_node->next_sibling;
    }

    return MYLITE_OK;
}

static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    const char *unsupported_qualified_target_message,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    size_t assignment_count = 0U;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    if (assignment_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (assignment_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(assignment_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    assignment = child_at(assignment_list, 0U);
    for (size_t assignment_index = 0U; assignment_index < assignment_count; ++assignment_index) {
        const struct mylite_sql_ast_node *target = NULL;
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = MYLITE_OK;

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            free(indexes);
            return MYLITE_ERROR;
        }

        target = child_at(assignment, 0U);
        if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(database, unsupported_qualified_target_message);
            free(indexes);
            return MYLITE_ERROR;
        }

        rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
        if (rc == MYLITE_OK) {
            rc = find_column_index(
                plan->columns,
                plan->column_count,
                column_name,
                &indexes[assignment_index]
            );
            if (rc != MYLITE_OK) {
                set_unknown_column_error(database, column_name);
                free(indexes);
                return MYLITE_ERROR;
            }
        } else {
            free(indexes);
            return rc;
        }
        assignment = assignment->next_sibling;
    }

    *out_indexes = indexes;
    *out_index_count = assignment_count;

    return MYLITE_OK;
}

static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t left = 0U; left < target_count; ++left) {
        for (size_t right = left + 1U; right < target_count; ++right) {
            if (target_indexes[left] == target_indexes[right]) {
                set_column_specified_twice_error(database, columns[target_indexes[right]].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool insert_allows_implicit_default_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan
) {
    if (plan == NULL) {
        return false;
    }
    return dml_allows_missing_default_adjustment(database, plan->ignore_errors);
}

static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        bool column_is_targeted = false;

        for (size_t target_index = 0U; target_index < target_count; ++target_index) {
            if (target_indexes[target_index] == column_index) {
                column_is_targeted = true;
                break;
            }
        }
        if (!column_is_targeted && plan->columns[column_index].is_generated) {
            continue;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !plan->columns[column_index].is_nullable &&
            plan->columns[column_index].default_kind ==
                MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
            if (insert_allows_implicit_default_adjustment(database, plan)) {
                continue;
            }
            set_bad_null_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !column_descriptor_uses_enum_implicit_missing_default(&plan->columns[column_index]) &&
            (plan->columns[column_index].default_kind ==
                 MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT ||
             (!plan->columns[column_index].is_nullable &&
              !column_default_kind_materializes_value(plan->columns[column_index].default_kind)))) {
            if (insert_allows_implicit_default_adjustment(database, plan)) {
                continue;
            }
            set_no_default_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_insert_row_shapes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    size_t target_count
) {
    const struct mylite_sql_ast_node *row_node = NULL;
    size_t row_count = 0U;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    row_count = mylite_sql_ast_node_child_count(row_list);
    row_node = child_at(row_list, 0U);
    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        size_t row_number = row_index + 1U;

        if (row_node == NULL || row_node->kind != MYLITE_SQL_AST_INSERT_ROW) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_child_count(row_node) != target_count) {
            set_column_count_mismatch_error(database, row_number);
            return MYLITE_ERROR;
        }
        row_node = row_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *row_node = NULL;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    {
        size_t row_count = mylite_sql_ast_node_child_count(row_list);

        if (plan->rows != NULL && plan->row_count != row_count) {
            set_runtime_error(database, "prepared INSERT row shape changed");
            return MYLITE_ERROR;
        }
        plan->row_count = row_count;
    }
    if (plan->row_count == 0U || plan->row_count > SIZE_MAX / sizeof(*plan->rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (plan->rows == NULL) {
        plan->rows = calloc(plan->row_count, sizeof(*plan->rows));
        if (plan->rows == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }

    row_node = child_at(row_list, 0U);
    for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
        int rc = plan_insert_row(
            database,
            row_node,
            row_index + 1U,
            target_indexes,
            target_count,
            plan,
            &plan->rows[row_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        row_node = row_node == NULL ? NULL : row_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    const struct mylite_sql_ast_node *value_node = child_at(row_node, 0U);
    int rc = MYLITE_OK;

    if (row_node == NULL || row_node->kind != MYLITE_SQL_AST_INSERT_ROW) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(row_node) != target_count) {
        set_column_count_mismatch_error(database, row_number);
        return MYLITE_ERROR;
    }
    rc = allocate_insert_row_values(database, plan, out_row);

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        size_t column_index = target_indexes[target_index];
        rc = convert_insert_value_for_plan(
            database,
            plan,
            value_node,
            &plan->columns[column_index],
            row_number,
            plan->ignore_errors,
            dml_allows_string_truncation_adjustment(database, plan->ignore_errors),
            &out_row->values[column_index]
        );

        value_node = value_node == NULL ? NULL : value_node->next_sibling;
    }
    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, plan) &&
        row_number == 1U) {
        rc = append_insert_omitted_column_warnings(database, plan, target_indexes, target_count);
    }

    return rc;
}

static int append_insert_omitted_column_warnings(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        bool column_is_targeted = false;

        for (size_t target_index = 0U; target_index < target_count; ++target_index) {
            if (target_indexes[target_index] == column_index) {
                column_is_targeted = true;
                break;
            }
        }
        if (!column_is_targeted && plan->columns[column_index].is_generated) {
            continue;
        }
        if (!column_is_targeted &&
            !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
            !plan->columns[column_index].is_nullable &&
            plan->columns[column_index].default_kind ==
                MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
            int rc = append_bad_null_warning(database, plan->columns[column_index].name);

            if (rc != MYLITE_OK) {
                return rc;
            }
        } else if (!column_is_targeted &&
                   !column_descriptor_is_auto_increment(&plan->columns[column_index]) &&
                   !column_descriptor_uses_enum_implicit_missing_default(
                       &plan->columns[column_index]
                   ) &&
                   (plan->columns[column_index].default_kind ==
                        MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT ||
                    (!plan->columns[column_index].is_nullable &&
                     !column_default_kind_materializes_value(
                         plan->columns[column_index].default_kind
                     )))) {
            int rc = append_no_default_warning(database, plan->columns[column_index].name);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }

    return MYLITE_OK;
}

static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    plan->row_count = 1U;
    if (plan->rows == NULL) {
        plan->rows = calloc(1U, sizeof(*plan->rows));
        if (plan->rows == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }

    rc = allocate_insert_row_values(database, plan, &plan->rows[0]);
    assignment = child_at(assignment_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        const struct mylite_sql_ast_node *value_node = NULL;
        size_t column_index = target_indexes[target_index];

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        value_node = child_at(assignment, 1U);
        rc = convert_insert_value_for_plan(
            database,
            plan,
            value_node,
            &plan->columns[column_index],
            1U,
            plan->ignore_errors,
            dml_allows_string_truncation_adjustment(database, plan->ignore_errors),
            &plan->rows[0].values[column_index]
        );
        assignment = assignment->next_sibling;
    }
    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, plan)) {
        rc = append_insert_omitted_column_warnings(database, plan, target_indexes, target_count);
    }

    return rc;
}

static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    if (plan->column_count > SIZE_MAX / sizeof(*out_row->values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (out_row->values == NULL) {
        out_row->values = calloc(plan->column_count, sizeof(*out_row->values));
        if (out_row->values == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    } else {
        for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
            planned_value_deinit(&out_row->values[column_index]);
        }
    }
    out_row->generated_auto_increment = false;
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        int rc = allocate_insert_column_value(
            database,
            plan,
            column_index,
            &out_row->values[column_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int allocate_insert_column_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
    bool adjust_implicit_default = (insert_allows_implicit_default_adjustment(database, plan) &&
                                    !column_descriptor_is_auto_increment(column)) != 0;

    if (column->is_generated) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return materialize_integer_expression_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = column->default_integer,
        };
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        return copy_decimal_text_value(database, column->default_text, out_value);
    }
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return convert_bit_literal(
            database,
            &(struct mylite_sql_ast_node){
                .kind = MYLITE_SQL_AST_LITERAL,
                .span = {.text = column->default_text, .length = strlen(column->default_text)},
                .payload.literal = {.kind = MYLITE_SQL_AST_LITERAL_BIT},
            },
            column,
            1U,
            false,
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return materialize_binary_default_value(database, column, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return materialize_character_expression_default_value(
            database,
            column,
            dml_allows_string_truncation_adjustment(database, adjust_implicit_default),
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_SCALAR_EXPRESSION) {
        return materialize_scalar_expression_default_value(
            database,
            column,
            dml_allows_string_truncation_adjustment(database, adjust_implicit_default),
            out_value
        );
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return copy_text_value(database, column->default_text, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return make_current_timestamp_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return make_current_date_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return make_current_time_value(database, out_value);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        if (!adjust_implicit_default || column->is_nullable) {
            out_value->is_null = true;
            return MYLITE_OK;
        }
        return make_insert_ignore_implicit_value(database, column, out_value);
    }
    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (!adjust_implicit_default || column->is_nullable) {
        out_value->is_null = true;
        return MYLITE_OK;
    }

    return make_insert_ignore_implicit_value(database, column, out_value);
}

static int make_insert_ignore_implicit_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_string_family(column) || column_descriptor_is_set(column)) {
        return make_empty_text_value(database, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return make_enum_first_label_value(database, column, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return make_json_null_value(database, out_value);
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        return make_empty_blob_value(database, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return make_implicit_binary_value(database, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return make_implicit_bit_value(database, column, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return make_zero_year_value(database, out_value);
    }
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};
        int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

        if (rc == MYLITE_OK) {
            rc = build_decimal_zero_value(database, &info, out_value);
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        return make_zero_approximate_value(out_value);
    }
    if (column_descriptor_is_date(column)) {
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return make_zero_timestamp_value(database, out_value);
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->integer = 0;
    return MYLITE_OK;
}
