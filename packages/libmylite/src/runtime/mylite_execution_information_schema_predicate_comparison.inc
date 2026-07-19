
static int information_schema_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
) {
    const char *left_text = NULL;
    char left_buffer[integer_text_capacity];
    int rc = MYLITE_OK;

    if (instruction->value_count != 1U || instruction->values == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA comparison plan");
        return MYLITE_ERROR;
    }
    rc = information_schema_predicate_row_value(
        database,
        query,
        row,
        instruction->column_index,
        left_buffer,
        sizeof(left_buffer),
        &left_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return information_schema_comparison_truth(
        database,
        query,
        instruction->column_index,
        left_text,
        &instruction->values[0],
        instruction->operator_kind,
        out_truth
    );
}

static int information_schema_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return information_schema_like_comparison_truth(
            query,
            column_index,
            left_text,
            right,
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return information_schema_null_safe_comparison_truth(
            database,
            query,
            column_index,
            left_text,
            right,
            operator_kind,
            out_truth
        );
    }
    return information_schema_regular_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_like_comparison_truth(
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum information_schema_truth_value *out_truth
) {
    bool like_matches = false;
    int rc = MYLITE_OK;

    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    rc = information_schema_like_comparison_matches(
        &query->definition->columns[column_index],
        left_text,
        right->text,
        right->like_escape_character,
        &like_matches
    );
    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(like_matches);
    }
    return rc;
}

static int information_schema_null_safe_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        bool both_null = ((left_text == NULL) && right->is_null) != 0;

        *out_truth = information_schema_truth_from_bool(both_null);
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_regular_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_nonnull_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    bool comparison_matches = false;
    int rc = information_schema_compare_nonnull_value_matches(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        &comparison_matches
    );

    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(comparison_matches);
    }
    return rc;
}

static int information_schema_like_comparison_matches(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    char escape_character,
    bool *out_matches
) {
    bool case_sensitive = true;

    if (information_schema_column_uses_case_insensitive_collation(column)) {
        case_sensitive = false;
    }

    *out_matches = show_like_pattern_matches_with_escape(
        right_text,
        strlen(right_text),
        left_text,
        strlen(left_text),
        case_sensitive,
        escape_character
    );
    return MYLITE_OK;
}

static int information_schema_compare_nonnull_value_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    return information_schema_text_comparison_matches(
        database,
        &query->definition->columns[column_index],
        operator_kind,
        left_text,
        right->text,
        right->is_numeric,
        out_matches
    );
}

static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
) {
    const char *left_text = NULL;
    char left_buffer[integer_text_capacity];
    int rc = MYLITE_OK;

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (instruction->value_count != 2U || instruction->values == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA BETWEEN plan");
        return MYLITE_ERROR;
    }
    rc = information_schema_predicate_row_value(
        database,
        query,
        row,
        instruction->column_index,
        left_buffer,
        sizeof(left_buffer),
        &left_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left_text == NULL || instruction->values[0].is_null || instruction->values[1].is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        bool lower_matches = false;
        bool upper_matches = false;

        rc = information_schema_between_bound_matches(
            database,
            query,
            instruction->column_index,
            left_text,
            &instruction->values[0],
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            &lower_matches
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_between_bound_matches(
                database,
                query,
                instruction->column_index,
                left_text,
                &instruction->values[1],
                MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
                &upper_matches
            );
        }
        if (rc == MYLITE_OK) {
            *out_truth = information_schema_truth_from_bool((lower_matches && upper_matches) != 0);
        }
    }

    return rc;
}

static int information_schema_between_bound_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    int comparison = 0;

    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    comparison = information_schema_compare_column_text(
        &query->definition->columns[column_index],
        left_text,
        right->text
    );
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE BETWEEN supports bound ordering");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
) {
    const char *left_text = NULL;
    char left_buffer[integer_text_capacity];
    bool saw_null = false;
    int rc = MYLITE_OK;

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (instruction->value_count != 0U && instruction->values == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA IN plan");
        return MYLITE_ERROR;
    }
    rc = information_schema_predicate_row_value(
        database,
        query,
        row,
        instruction->column_index,
        left_buffer,
        sizeof(left_buffer),
        &left_text
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left_text == NULL) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < instruction->value_count; ++index) {
        const struct information_schema_predicate_value *item = &instruction->values[index];

        if (item->is_null) {
            saw_null = true;
        } else {
            bool item_matches = false;

            rc = information_schema_compare_nonnull_value_matches(
                database,
                query,
                instruction->column_index,
                left_text,
                item,
                MYLITE_SQL_AST_OPERATOR_EQUAL,
                &item_matches
            );
            if (rc == MYLITE_OK && item_matches) {
                *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
                return MYLITE_OK;
            }
        }
    }
    if (rc == MYLITE_OK && saw_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    }
    return rc;
}

static int information_schema_numeric_comparison_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool *out_matches
) {
    int64_t left_value = 0;
    int64_t right_value = 0;

    if (information_schema_text_to_i64(left_text, &left_value) != MYLITE_OK ||
        information_schema_text_to_i64(right_text, &right_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison requires integer metadata values"
        );
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = ((left_value == right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = ((left_value != right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((left_value < right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((left_value <= right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((left_value > right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((left_value >= right_value) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_text_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool right_is_numeric,
    bool *out_matches
) {
    int comparison = 0;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) == 0;
        return MYLITE_OK;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) != 0;
        return MYLITE_OK;
    }
    if (!right_is_numeric) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE string predicates support only equality comparisons"
        );
        return MYLITE_ERROR;
    }

    comparison = information_schema_compare_column_text(column, left_text, right_text);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((comparison < 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((comparison > 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
