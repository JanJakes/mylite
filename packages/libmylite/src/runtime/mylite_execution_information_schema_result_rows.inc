static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
) {
    int rc = MYLITE_OK;

    if (query->is_count_star) {
        char *column_name = NULL;

        if (query->count_alias != NULL) {
            rc = copy_select_item_alias_text(database, query->count_alias, &column_name);
        } else {
            rc = copy_aggregate_result_column_name(
                database,
                &query->count_expression->span,
                &column_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(column_name);
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        rc = mylite_result_append_column(result, query->projection_names[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    size_t *indexes = NULL;
    size_t index_count = 0U;
    size_t visible_count = 0U;
    const char **values = NULL;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (rc == MYLITE_OK && index_count == 0U) {
        free(indexes);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && rows->rows == NULL) {
        free(indexes);
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_sort_row_indexes(query, rows, indexes, index_count);
    }
    if (rc == MYLITE_OK && query->projection_count != 0U) {
        values = (const char **)calloc(query->projection_count, sizeof(*values));
        if (values == NULL) {
            free(indexes);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < index_count; ++output_index) {
        size_t row_index = indexes[output_index];

        if (query->limit.has_limit && visible_count >= (size_t)query->limit.row_count) {
            break;
        }
        for (size_t column_index = 0U; column_index < query->projection_count; ++column_index) {
            values[column_index] = rows->rows[row_index][query->projection_indexes[column_index]];
        }
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        ++visible_count;
    }
    free((void *)values);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    char count_text[integer_text_capacity];
    const char *values[1] = {count_text};
    size_t *indexes = NULL;
    size_t index_count = 0U;
    bool emits_count_row = true;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (query->limit.has_limit && query->limit.row_count == 0) {
        emits_count_row = false;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)index_count,
            count_text,
            sizeof(count_text)
        );
    }
    if (rc == MYLITE_OK && emits_count_row) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && emits_count_row && out_read_row_count != NULL) {
        *out_read_row_count = index_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    size_t *indexes = NULL;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_indexes = NULL;
    *out_index_count = 0U;
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
            break;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (rows->row_count == 0U) {
        return MYLITE_OK;
    }
    if (rows->row_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    indexes = malloc(rows->row_count * sizeof(*indexes));
    if (indexes == NULL && rows->row_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        bool matches = true;

        if (where_clause != NULL) {
            rc = information_schema_predicate_matches(
                database,
                query,
                child_at(where_clause, 0U),
                rows->rows[row_index],
                &matches
            );
        }
        if (rc == MYLITE_OK && matches) {
            indexes[index_count] = row_index;
            ++index_count;
        }
    }
    if (rc != MYLITE_OK) {
        free(indexes);
        return rc;
    }

    *out_indexes = indexes;
    *out_index_count = index_count;
    return MYLITE_OK;
}

static int information_schema_sort_row_indexes(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    if (!query->has_order) {
        return MYLITE_OK;
    }
    for (size_t index = 1U; index < index_count; ++index) {
        size_t row_index = indexes[index];
        size_t scan = index;

        while (scan > 0U && information_schema_compare_rows(
                                query,
                                rows,
                                (struct information_schema_row_order_pair){
                                    .left_row = row_index,
                                    .right_row = indexes[scan - 1U],
                                }
                            ) < 0) {
            indexes[scan] = indexes[scan - 1U];
            --scan;
        }
        indexes[scan] = row_index;
    }
    return MYLITE_OK;
}

static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
) {
    const char *left = rows->rows[pair.left_row][query->order_index];
    const char *right = rows->rows[pair.right_row][query->order_index];
    int comparison = 0;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        comparison = -1;
    } else if (right == NULL) {
        comparison = 1;
    } else if (information_schema_column_is_numeric(query->definition, query->order_index)) {
        int64_t left_value = 0;
        int64_t right_value = 0;

        if (information_schema_text_to_i64(left, &left_value) == MYLITE_OK &&
            information_schema_text_to_i64(right, &right_value) == MYLITE_OK) {
            comparison = (left_value > right_value) - (left_value < right_value);
        } else {
            comparison =
                information_schema_compare_text(query->definition, query->order_index, left, right);
        }
    } else {
        comparison =
            information_schema_compare_text(query->definition, query->order_index, left, right);
    }
    if (query->order_direction == PLANNED_SELECT_ORDER_DESC) {
        comparison = -comparison;
    }
    return comparison;
}
