static enum placeholder_statement_kind classify_query_surface_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (classify_cte_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_scalar_expression_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_expression_clause_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_function_subquery_placeholder_statement(scan) !=
        PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_query_table_reference_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (classify_dml_variant_placeholder_statement(scan) != PLACEHOLDER_STATEMENT_NONE) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_cte_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t query_index = 0U;

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "WITH") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (!placeholder_scan_cte_clause_is_supported(scan, &query_index) ||
        !placeholder_scan_cte_query_statement_starts_at(scan, query_index)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
}

static bool placeholder_scan_cte_clause_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *out_query_index
) {
    size_t index = 1U;

    if (out_query_index != NULL) {
        *out_query_index = 0U;
    }
    if (scan == NULL || scan->token_count < cte_placeholder_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "WITH")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "RECURSIVE")) {
        ++index;
    }

    while (index < scan->token_count) {
        if (!placeholder_scan_cte_definition_is_supported(scan, &index)) {
            return false;
        }
        if (index >= scan->token_count || !mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
    }

    if (out_query_index != NULL) {
        *out_query_index = index;
    }
    return index < scan->token_count;
}

static bool placeholder_scan_cte_definition_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count ||
        !placeholder_scan_token_is_identifier_like(scan, *index)) {
        return false;
    }
    ++*index;

    if (!placeholder_scan_cte_column_list_is_supported(scan, index)) {
        return false;
    }
    if (*index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, *index, "AS") ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[*index + 1U])) {
        return false;
    }
    return placeholder_scan_cte_body_is_supported(scan, index);
}

static bool placeholder_scan_cte_column_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    size_t column_list_right_index = 0U;

    if (scan == NULL || index == NULL) {
        return false;
    }
    if (*index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[*index])) {
        return true;
    }
    if (!placeholder_scan_find_matching_right_paren(scan, *index, &column_list_right_index)) {
        return false;
    }
    *index = column_list_right_index + 1U;
    return true;
}

static bool placeholder_scan_cte_body_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    size_t body_left_index = 0U;
    size_t body_right_index = 0U;

    if (scan == NULL || index == NULL || *index + 1U >= scan->token_count) {
        return false;
    }
    body_left_index = *index + 1U;
    if (!placeholder_scan_find_matching_right_paren(scan, body_left_index, &body_right_index) ||
        body_right_index <= body_left_index + 1U) {
        return false;
    }
    if (!placeholder_scan_query_expression_starts_at(scan, body_left_index + 1U) &&
        !create_table_select_parenthesized_query_starts_at(scan, body_left_index + 1U)) {
        return false;
    }
    *index = body_right_index + 1U;
    return true;
}

static bool placeholder_scan_cte_query_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_query_expression_starts_at(scan, index) ||
        create_table_select_parenthesized_query_starts_at(scan, index)) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, index, "UPDATE") ||
           placeholder_scan_token_text_equals(scan, index, "DELETE");
}

static enum placeholder_statement_kind classify_query_scalar_expression_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_lateral_derived_table(scan) ||
        placeholder_scan_contains_grouping_function(scan) ||
        placeholder_scan_contains_sounds_like_surface(scan) ||
        placeholder_scan_contains_deprecated_logical_operator_surface(scan) ||
        placeholder_scan_contains_like_surface(scan) ||
        placeholder_scan_contains_like_escape_surface(scan) ||
        placeholder_scan_contains_not_like_surface(scan) ||
        placeholder_scan_contains_typed_temporal_literal_surface(scan) ||
        placeholder_scan_contains_interval_expression_surface(scan) ||
        placeholder_scan_contains_scalar_in_surface(scan) ||
        placeholder_scan_contains_quantified_subquery_surface(scan) ||
        placeholder_scan_contains_fulltext_match_against_surface(scan) ||
        placeholder_scan_contains_expression_residual_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_expression_clause_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_query_expression_clause_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_function_subquery_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_query_function_subquery_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_query_table_reference_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_parenthesized_table_reference_surface(scan) ||
        placeholder_scan_contains_odbc_table_reference_surface(scan) ||
        placeholder_scan_contains_mixed_comma_explicit_join_surface(scan) ||
        placeholder_scan_contains_delayed_join_condition_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_dml_variant_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_dml_variant_surface(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool placeholder_scan_starts_query_statement(const struct placeholder_statement_scan *scan) {
    if (scan == NULL || scan->token_count == 0U) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        placeholder_scan_token_text_equals(scan, 0U, "WITH") ||
        placeholder_scan_token_text_equals(scan, 0U, "TABLE") ||
        placeholder_scan_token_text_equals(scan, 0U, "VALUES") ||
        placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DELETE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DO")) {
        return true;
    }
    return placeholder_scan_parenthesized_start_is_query_statement(scan);
}

static bool placeholder_scan_parenthesized_start_is_query_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 0U;

    if (scan == NULL) {
        return false;
    }
    while (index < scan->token_count && mylite_sql_parser_token_is_left_paren(&scan->tokens[index])
    ) {
        ++index;
    }
    return index > 0U && (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
                          placeholder_scan_token_text_equals(scan, index, "WITH") ||
                          placeholder_scan_token_text_equals(scan, index, "TABLE") ||
                          placeholder_scan_token_text_equals(scan, index, "VALUES"));
}

static bool placeholder_scan_contains_parenthesized_table_reference_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_parenthesized_table_reference_context(scan, index) &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_table_reference_body_starts_at(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_odbc_table_reference_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        bool saw_join = false;

        if (!placeholder_scan_odbc_escape_starts_table_reference(scan, index) ||
            !placeholder_scan_token_text_equals(scan, index, "{") ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "OJ")) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_is_join_keyword(scan, scan_index)) {
                saw_join = true;
            }
            if (placeholder_scan_token_text_equals(scan, scan_index, "}")) {
                if (saw_join) {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

static bool placeholder_scan_odbc_escape_starts_table_reference(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t previous = 0U;

    if (scan == NULL || index == 0U) {
        return false;
    }
    previous = index - 1U;
    return placeholder_scan_token_text_equals(scan, previous, "FROM") ||
           placeholder_scan_token_is_join_keyword(scan, previous) ||
           mylite_sql_parser_token_is_comma(&scan->tokens[previous]);
}

static bool placeholder_scan_contains_mixed_comma_explicit_join_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "FROM") &&
            placeholder_scan_from_clause_contains_mixed_comma_explicit_join(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_delayed_join_condition_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "FROM") &&
            placeholder_scan_from_clause_contains_delayed_join_condition(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_from_clause_contains_mixed_comma_explicit_join(
    const struct placeholder_statement_scan *scan,
    size_t from_index
) {
    int paren_depth = 0;
    bool saw_comma = false;
    bool saw_join = false;

    if (scan == NULL || from_index + 1U >= scan->token_count) {
        return false;
    }
    for (size_t index = from_index + 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_table_reference_clause(scan, index)) {
            break;
        }
        if (mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            saw_comma = true;
        } else if (placeholder_scan_token_is_join_keyword(scan, index)) {
            saw_join = true;
        }
        if (saw_comma && saw_join) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_from_clause_contains_delayed_join_condition(
    const struct placeholder_statement_scan *scan,
    size_t from_index
) {
    int paren_depth = 0;
    size_t joins_before_condition = 0U;

    if (scan == NULL || from_index + 1U >= scan->token_count) {
        return false;
    }
    for (size_t index = from_index + 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_table_reference_clause(scan, index)) {
            break;
        }
        if (placeholder_scan_token_text_equals(scan, index, "ON") ||
            placeholder_scan_token_text_equals(scan, index, "USING")) {
            joins_before_condition = 0U;
            continue;
        }
        if (placeholder_scan_token_is_join_keyword(scan, index)) {
            ++joins_before_condition;
            if (joins_before_condition > 1U) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_token_stops_table_reference_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "WINDOW") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "FOR") ||
           placeholder_scan_token_text_equals(scan, index, "LOCK") ||
           placeholder_scan_token_text_equals(scan, index, "INTO");
}

static bool placeholder_scan_parenthesized_table_reference_body_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    size_t right_paren_index = 0U;
    size_t first_index = left_paren_index + 1U;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= first_index ||
        placeholder_scan_query_expression_starts_at(scan, first_index)) {
        return false;
    }
    return placeholder_scan_parenthesized_table_reference_body_has_surface(
        scan,
        first_index,
        right_paren_index
    );
}

static bool placeholder_scan_parenthesized_table_reference_body_has_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    bool saw_reference_name = false;

    if (scan == NULL || start_index >= end_index || end_index > scan->token_count ||
        placeholder_scan_query_expression_starts_at(scan, start_index)) {
        return false;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            size_t right_paren_index = 0U;

            if (!placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) ||
                right_paren_index >= end_index) {
                return false;
            }
            if (!placeholder_scan_parenthesized_group_has_table_reference_name(
                    scan,
                    index,
                    right_paren_index
                )) {
                return false;
            }
            saw_reference_name = true;
            index = right_paren_index;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            return false;
        }
        if (placeholder_scan_token_is_table_reference_name(scan, index)) {
            saw_reference_name = true;
        }
        if (saw_reference_name && (mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
                                   placeholder_scan_token_is_join_keyword(scan, index))) {
            return true;
        }
    }
    return saw_reference_name;
}

static bool placeholder_scan_parenthesized_group_has_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    size_t start_index = left_paren_index + 1U;
    size_t end_index = right_paren_index;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        right_paren_index > scan->token_count || right_paren_index <= left_paren_index ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }

    while (start_index < end_index &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index])) {
        size_t nested_right_index = 0U;

        if (!placeholder_scan_find_matching_right_paren(scan, start_index, &nested_right_index) ||
            nested_right_index != end_index - 1U) {
            break;
        }
        ++start_index;
        --end_index;
    }

    if (start_index >= end_index ||
        placeholder_scan_query_expression_starts_at(scan, start_index)) {
        return false;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (placeholder_scan_token_is_table_reference_name(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_find_matching_right_paren(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            if (paren_depth == 0) {
                if (out_right_paren_index != NULL) {
                    *out_right_paren_index = index;
                }
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_query_expression_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "WITH") ||
           placeholder_scan_token_text_equals(scan, index, "TABLE") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES");
}

static bool placeholder_scan_token_starts_parenthesized_table_reference_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "JOIN") ||
           placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN");
}

static bool placeholder_scan_token_is_join_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "JOIN") ||
           placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN");
}

static bool placeholder_scan_token_is_table_reference_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           placeholder_scan_token_text_equals(scan, index, "DUAL");
}

static bool placeholder_scan_contains_query_function_subquery_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    return placeholder_scan_contains_any_function_call_surface(scan) ||
           placeholder_scan_contains_quoted_function_call_surface(scan) ||
           placeholder_scan_contains_nested_function_call_surface(scan) ||
           placeholder_scan_contains_named_window_function_surface(scan) ||
           placeholder_scan_contains_parenthesized_query_expression_surface(scan);
}

static bool placeholder_scan_contains_any_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_left_paren_starts_function_arguments(scan, index) &&
            placeholder_scan_token_is_generic_function_placeholder_name(scan, index - 1U) &&
            placeholder_scan_function_call_is_complete(scan, index - 1U, index, true)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_quoted_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) && index > 0U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER &&
            placeholder_scan_quoted_function_call_has_expression_context(scan, index - 1U) &&
            placeholder_scan_function_call_is_complete(scan, index - 1U, index, true)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_quoted_function_call_has_expression_context(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index
) {
    size_t previous_index = 0U;

    if (scan == NULL || function_name_index == 0U) {
        return false;
    }
    previous_index = function_name_index - 1U;
    return placeholder_scan_token_text_equals(scan, previous_index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, previous_index, "DISTINCT") ||
           placeholder_scan_token_text_equals(scan, previous_index, "DISTINCTROW") ||
           placeholder_scan_token_text_equals(scan, previous_index, "ALL") ||
           placeholder_scan_token_text_equals(scan, previous_index, "BY") ||
           placeholder_scan_token_starts_predicate_clause(scan, previous_index) ||
           mylite_sql_parser_token_is_comma(&scan->tokens[previous_index]) ||
           mylite_sql_parser_token_is_left_paren(&scan->tokens[previous_index]) ||
           placeholder_scan_token_is_assignment_operator(scan, previous_index) ||
           scan->tokens[previous_index].kind == MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_contains_parameter_marker(const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_PARAMETER) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_generic_function_placeholder_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "ROW")) {
        return false;
    }
    return !placeholder_scan_token_is_builtin_window_function_name(scan, index);
}

static bool placeholder_scan_contains_nested_function_call_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || !placeholder_scan_has_table_backed_or_dml_context(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_left_paren_starts_function_arguments(scan, index) &&
            placeholder_scan_nonempty_function_call_is_complete(scan, index - 1U, index) &&
            placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) &&
            placeholder_scan_function_call_contains_nested_call(scan, index, right_paren_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_named_window_function_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || !placeholder_scan_has_table_backed_or_dml_context(scan) ||
        !placeholder_scan_contains_text(scan, "WINDOW")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (!mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_token_is_aggregate_window_function_name(scan, index - 1U) ||
            !placeholder_scan_nonempty_function_call_is_complete(scan, index - 1U, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
            placeholder_scan_token_is_identifier_like(scan, right_paren_index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_nonempty_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index
) {
    return placeholder_scan_function_call_is_complete(
        scan,
        function_name_index,
        left_paren_index,
        false
    );
}

static bool placeholder_scan_function_call_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t function_name_index,
    size_t left_paren_index,
    bool allow_empty_arguments
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || function_name_index + 1U != left_paren_index ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index)) {
        return false;
    }
    if (right_paren_index == left_paren_index + 1U) {
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
            placeholder_scan_token_is_aggregate_window_function_name(scan, function_name_index)) {
            return true;
        }
        return allow_empty_arguments &&
               placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index);
    }
    if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "OVER") &&
        placeholder_scan_token_is_aggregate_window_function_name(scan, function_name_index)) {
        return true;
    }
    return placeholder_scan_function_call_arguments_are_well_formed(
               scan,
               left_paren_index,
               right_paren_index
           ) &&
           placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index);
}

static bool placeholder_scan_function_call_contains_nested_call(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        size_t nested_right_paren_index = 0U;

        if (!mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &nested_right_paren_index) ||
            nested_right_paren_index >= right_paren_index ||
            nested_right_paren_index == index + 1U) {
            continue;
        }
        if (placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index,
                nested_right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_function_call_arguments_are_well_formed(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;
    bool previous_top_level_token_was_separator = true;

    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            previous_top_level_token_was_separator = false;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            if (paren_depth == 0) {
                previous_top_level_token_was_separator = false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            previous_top_level_token_was_separator = false;
            continue;
        }
        if (previous_top_level_token_was_separator) {
            return false;
        }
        previous_top_level_token_was_separator = true;
    }
    return paren_depth == 0 && !previous_top_level_token_was_separator;
}

static bool placeholder_scan_token_can_follow_function_call_surface(
    const struct placeholder_statement_scan *scan,
    size_t right_paren_index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || right_paren_index + 1U >= scan->token_count) {
        return true;
    }
    if (mylite_sql_parser_token_is_comma(&scan->tokens[right_paren_index + 1U]) ||
        mylite_sql_parser_token_is_right_paren(&scan->tokens[right_paren_index + 1U]) ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AS") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "FROM") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WHERE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "GROUP") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "HAVING") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ORDER") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WINDOW") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "LIMIT") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "WITH") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AND") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ASC") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "DESC") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "UNION") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "ON") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IS") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "LIKE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "REGEXP") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "RLIKE") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "BETWEEN")) {
        return true;
    }
    if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U)) {
        return true;
    }
    token = &scan->tokens[right_paren_index + 1U];
    if (token->kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    return right_paren_index + 2U < scan->token_count &&
           !placeholder_scan_token_stops_expression_clause_search(scan, right_paren_index + 2U);
}

static bool placeholder_scan_token_is_aggregate_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "AVG") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_AND") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_OR") ||
        placeholder_scan_token_text_equals(scan, index, "BIT_XOR") ||
        placeholder_scan_token_text_equals(scan, index, "COUNT") ||
        placeholder_scan_token_text_equals(scan, index, "GROUP_CONCAT") ||
        placeholder_scan_token_text_equals(scan, index, "JSON_ARRAYAGG") ||
        placeholder_scan_token_text_equals(scan, index, "JSON_OBJECTAGG") ||
        placeholder_scan_token_text_equals(scan, index, "MAX") ||
        placeholder_scan_token_text_equals(scan, index, "MIN") ||
        placeholder_scan_token_text_equals(scan, index, "STD") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV_POP") ||
        placeholder_scan_token_text_equals(scan, index, "STDDEV_SAMP") ||
        placeholder_scan_token_text_equals(scan, index, "SUM") ||
        placeholder_scan_token_text_equals(scan, index, "VAR_POP") ||
        placeholder_scan_token_text_equals(scan, index, "VAR_SAMP") ||
        placeholder_scan_token_text_equals(scan, index, "VARIANCE")) {
        return true;
    }
    return false;
}

static bool placeholder_scan_token_is_builtin_window_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "CUME_DIST") ||
        placeholder_scan_token_text_equals(scan, index, "DENSE_RANK") ||
        placeholder_scan_token_text_equals(scan, index, "FIRST_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "LAG") ||
        placeholder_scan_token_text_equals(scan, index, "LAST_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "LEAD") ||
        placeholder_scan_token_text_equals(scan, index, "NTH_VALUE") ||
        placeholder_scan_token_text_equals(scan, index, "NTILE") ||
        placeholder_scan_token_text_equals(scan, index, "PERCENT_RANK") ||
        placeholder_scan_token_text_equals(scan, index, "RANK") ||
        placeholder_scan_token_text_equals(scan, index, "ROW_NUMBER")) {
        return true;
    }
    return false;
}

static bool placeholder_scan_contains_parenthesized_query_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "WITH")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) &&
            placeholder_scan_query_expression_starts_at(scan, index + 1U) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_query_expression_clause_surface(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_contains_query_final_tail_surface(scan) ||
        placeholder_scan_contains_odbc_expression_escape_surface(scan) ||
        placeholder_scan_contains_postfix_is_predicate_surface(scan) ||
        placeholder_scan_contains_between_expression_surface(scan) ||
        placeholder_scan_contains_row_constructor_predicate_surface(scan) ||
        placeholder_scan_contains_string_order_key_surface(scan) ||
        placeholder_scan_contains_constant_order_key_surface(scan) ||
        placeholder_scan_contains_having_residual_surface(scan)) {
        return true;
    }
    if (!placeholder_scan_has_table_backed_or_dml_context(scan)) {
        return false;
    }
    return placeholder_scan_contains_json_arrow_surface(scan) ||
           placeholder_scan_contains_expression_operator_surface(scan) ||
           placeholder_scan_contains_row_tuple_predicate_surface(scan) ||
           placeholder_scan_contains_descriptor_in_list_surface(scan) ||
           placeholder_scan_contains_literal_left_predicate_surface(scan) ||
           placeholder_scan_contains_group_by_user_variable_assignment_surface(scan) ||
           placeholder_scan_contains_bare_truth_clause_surface(scan);
}

static bool placeholder_scan_contains_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_query_statement(scan)) {
        return false;
    }
    return placeholder_scan_contains_compound_query_final_tail_surface(scan) ||
           placeholder_scan_contains_select_limit_into_locking_permutation(scan);
}

static bool placeholder_scan_contains_compound_query_final_tail_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;
    bool saw_set_operation = false;

    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_top_level_token_starts_set_operation(scan, index)) {
            saw_set_operation = true;
            continue;
        }
        if (saw_set_operation &&
            placeholder_scan_top_level_token_starts_final_query_tail(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_select_limit_into_locking_permutation(
    const struct placeholder_statement_scan *scan
) {
    size_t limit_index = 0U;
    size_t into_index = 0U;
    size_t locking_index = 0U;
    size_t locking_end_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_find_top_level_keyword(scan, "LIMIT", &limit_index)) {
        return false;
    }
    if (placeholder_scan_find_top_level_keyword_after(
            scan,
            "INTO",
            limit_index + 1U,
            &into_index
        ) &&
        placeholder_scan_into_user_variable_list_is_complete(scan, into_index)) {
        return true;
    }
    for (size_t index = limit_index + 1U; index < scan->token_count; ++index) {
        if (!placeholder_scan_token_starts_select_locking_clause(scan, index) ||
            !scan_select_locking_clause_end(scan, index, &locking_end_index)) {
            continue;
        }
        locking_index = index;
        if (locking_end_index < scan->token_count &&
            placeholder_scan_token_text_equals(scan, locking_end_index, "INTO") &&
            placeholder_scan_into_user_variable_list_is_complete(scan, locking_end_index)) {
            return true;
        }
        if (placeholder_scan_find_top_level_keyword_after(
                scan,
                "INTO",
                locking_index + 1U,
                &into_index
            ) &&
            placeholder_scan_into_user_variable_list_is_complete(scan, into_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_top_level_token_starts_set_operation(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "EXCEPT") ||
           placeholder_scan_token_text_equals(scan, index, "INTERSECT");
}

static bool placeholder_scan_top_level_token_starts_final_query_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t locking_end_index = 0U;

    if (placeholder_scan_token_text_equals(scan, index, "ORDER") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
        !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U)) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LIMIT") &&
        !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U)) {
        return true;
    }
    if (placeholder_scan_into_user_variable_list_is_complete(scan, index)) {
        return true;
    }
    if (!scan_select_locking_clause_end(scan, index, &locking_end_index) ||
        locking_end_index <= index) {
        return false;
    }
    return locking_end_index >= scan->token_count ||
           placeholder_scan_into_user_variable_list_is_complete(scan, locking_end_index);
}

static bool placeholder_scan_into_user_variable_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t into_index
) {
    bool need_variable = true;
    bool saw_variable = false;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, into_index, "INTO")) {
        return false;
    }
    for (size_t index = into_index + 1U; index < scan->token_count; ++index) {
        if (!need_variable && (placeholder_scan_token_starts_select_locking_clause(scan, index) ||
                               placeholder_scan_token_text_equals(scan, index, "ORDER") ||
                               placeholder_scan_token_text_equals(scan, index, "LIMIT"))) {
            return saw_variable;
        }
        if (need_variable) {
            if (scan->tokens[index].kind != MYLITE_SQL_TOKEN_USER_VARIABLE) {
                return false;
            }
            need_variable = false;
            saw_variable = true;
            continue;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_variable = true;
    }
    return saw_variable && !need_variable;
}

static bool placeholder_scan_contains_json_arrow_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (token->operator_kind == MYLITE_SQL_OPERATOR_JSON_EXTRACT ||
             token->operator_kind == MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT) &&
            placeholder_scan_json_arrow_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_json_arrow_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return scan != NULL && index > 0U && index + 1U < scan->token_count &&
           placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
           scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool placeholder_scan_contains_odbc_expression_escape_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        size_t right_brace_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "{") ||
            placeholder_scan_token_text_equals(scan, index + 1U, "OJ") ||
            !placeholder_scan_token_is_odbc_expression_escape_name(scan, index + 1U) ||
            !placeholder_scan_find_matching_right_brace(scan, index, &right_brace_index) ||
            right_brace_index <= index + 2U ||
            placeholder_scan_token_is_incomplete_statement_tail(scan, right_brace_index - 1U)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool placeholder_scan_token_is_odbc_expression_escape_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FN") ||
           placeholder_scan_token_text_equals(scan, index, "D") ||
           placeholder_scan_token_text_equals(scan, index, "T") ||
           placeholder_scan_token_text_equals(scan, index, "TS");
}

static bool placeholder_scan_find_matching_right_brace(
    const struct placeholder_statement_scan *scan,
    size_t left_brace_index,
    size_t *out_right_brace_index
) {
    int brace_depth = 0;

    if (scan == NULL || left_brace_index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, left_brace_index, "{")) {
        return false;
    }
    for (size_t index = left_brace_index; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "{")) {
            ++brace_depth;
        } else if (placeholder_scan_token_text_equals(scan, index, "}")) {
            --brace_depth;
            if (brace_depth < 0) {
                return false;
            }
            if (brace_depth == 0) {
                if (out_right_brace_index != NULL) {
                    *out_right_brace_index = index;
                }
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_postfix_is_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        size_t value_index = index + 1U;

        if (!placeholder_scan_token_text_equals(scan, index, "IS") ||
            !placeholder_scan_token_can_end_expression_operand(scan, index - 1U)) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, value_index, "NOT")) {
            ++value_index;
        }
        if ((placeholder_scan_token_text_equals(scan, value_index, "NULL") ||
             placeholder_scan_token_text_equals(scan, value_index, "TRUE") ||
             placeholder_scan_token_text_equals(scan, value_index, "FALSE") ||
             placeholder_scan_token_text_equals(scan, value_index, "UNKNOWN")) &&
            !placeholder_scan_token_text_equals(scan, value_index + 1U, "IS") &&
            !(index >= 2U && placeholder_scan_token_text_equals(scan, index - 2U, "IS")) &&
            !(index >= 3U && placeholder_scan_token_text_equals(scan, index - 2U, "NOT") &&
              placeholder_scan_token_text_equals(scan, index - 3U, "IS"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_between_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "BETWEEN") &&
            placeholder_scan_between_expression_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_between_expression_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t between_index
) {
    int paren_depth = 0;
    bool left_operand_is_complete = false;

    if (scan == NULL || between_index == 0U || between_index + 2U >= scan->token_count ||
        !placeholder_scan_token_can_start_expression_operand(scan, between_index + 1U)) {
        return false;
    }
    left_operand_is_complete =
        placeholder_scan_token_can_end_expression_operand(scan, between_index - 1U) ||
        (between_index > 1U &&
         placeholder_scan_token_text_equals(scan, between_index - 1U, "NOT") &&
         placeholder_scan_token_can_end_expression_operand(scan, between_index - 2U));
    if (!left_operand_is_complete) {
        return false;
    }
    for (size_t index = between_index + 1U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > between_index + 1U &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "AND")) {
            return index > between_index + 1U &&
                   placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
                   placeholder_scan_token_can_start_expression_operand(scan, index + 1U) &&
                   placeholder_scan_between_bounds_contain_qualified_identifier(
                       scan,
                       between_index + 1U,
                       index
                   );
        }
    }
    return false;
}

static bool placeholder_scan_between_bounds_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t lower_start_index,
    size_t and_index
) {
    int paren_depth = 0;
    size_t upper_end_index = 0U;

    if (scan == NULL || lower_start_index >= and_index || and_index + 1U >= scan->token_count) {
        return false;
    }
    upper_end_index = scan->token_count;
    for (size_t index = and_index + 1U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > and_index + 1U &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            upper_end_index = index;
            break;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return placeholder_scan_tokens_contain_qualified_identifier(
               scan,
               lower_start_index,
               and_index
           ) ||
           placeholder_scan_tokens_contain_qualified_identifier(
               scan,
               and_index + 1U,
               upper_end_index
           );
}

static bool placeholder_scan_contains_descriptor_in_list_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "IN") &&
            placeholder_scan_scalar_in_is_descriptor_predicate(scan, index) &&
            placeholder_scan_in_predicate_has_operand_context(scan, index) &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) &&
            placeholder_scan_tokens_contain_qualified_identifier(
                scan,
                index + 2U,
                right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_in_predicate_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    if (scan == NULL || in_index == 0U) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, in_index - 1U, "NOT")) {
        return in_index > 1U &&
               placeholder_scan_token_can_end_expression_operand(scan, in_index - 2U);
    }
    return placeholder_scan_token_can_end_expression_operand(scan, in_index - 1U);
}

static bool placeholder_scan_contains_literal_left_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if (!mylite_sql_parser_token_is_string_literal(&scan->tokens[index]) ||
            !placeholder_scan_literal_predicate_has_clause_context(scan, index)) {
            continue;
        }
        if (placeholder_scan_literal_left_comparison_is_complete(scan, index) ||
            placeholder_scan_literal_left_between_is_complete(scan, index) ||
            placeholder_scan_literal_left_in_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_literal_left_comparison_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    size_t right_index = literal_index + 2U;

    if (scan == NULL || right_index >= scan->token_count ||
        !placeholder_scan_token_is_comparison_operator(scan, literal_index + 1U) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, right_index) ||
        placeholder_scan_token_stops_expression_clause_search(scan, right_index)) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, right_index);
}

static bool placeholder_scan_literal_left_between_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    int paren_depth = 0;
    size_t lower_start_index = literal_index + 2U;

    if (scan == NULL || lower_start_index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, literal_index + 1U, "BETWEEN") ||
        !placeholder_scan_token_can_start_expression_operand(scan, lower_start_index)) {
        return false;
    }
    for (size_t index = lower_start_index + 1U; index + 1U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (placeholder_scan_token_text_equals(scan, index, "AND") &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U) &&
            (placeholder_scan_tokens_contain_identifier_like(scan, lower_start_index, index) ||
             placeholder_scan_token_can_name_loose_identifier(scan, index + 1U))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_literal_left_in_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    size_t left_paren_index = literal_index + 2U;
    size_t right_paren_index = 0U;

    return scan != NULL && left_paren_index < scan->token_count &&
           placeholder_scan_token_text_equals(scan, literal_index + 1U, "IN") &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) &&
           placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, left_paren_index) &&
           placeholder_scan_tokens_contain_identifier_like(
               scan,
               left_paren_index + 1U,
               right_paren_index
           );
}

static bool placeholder_scan_literal_predicate_has_clause_context(
    const struct placeholder_statement_scan *scan,
    size_t literal_index
) {
    if (scan == NULL || literal_index == 0U) {
        return false;
    }
    for (size_t scan_index = literal_index; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_tokens_contain_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index >= end_index || end_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index + 2U < end_index; ++index) {
        if (placeholder_scan_token_can_name_loose_identifier(scan, index) &&
            placeholder_scan_token_text_equals(scan, index + 1U, ".") &&
            placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_tokens_contain_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index >= end_index || end_index > scan->token_count) {
        return false;
    }
    if (placeholder_scan_tokens_contain_qualified_identifier(scan, start_index, end_index)) {
        return true;
    }
    for (size_t index = start_index; index < end_index; ++index) {
        if (placeholder_scan_token_is_identifier_like(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_row_constructor_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_row_constructor_starts_at(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, right_paren_index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
            (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") &&
             placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "IN"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_row_constructor_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t row_index,
    size_t *out_right_paren_index
) {
    size_t left_paren_index = row_index + 1U;
    size_t right_paren_index = 0U;

    if (!placeholder_scan_token_text_equals(scan, row_index, "ROW") ||
        left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        !placeholder_scan_parenthesized_list_contains_top_level_comma(
            scan,
            left_paren_index,
            right_paren_index
        )) {
        return false;
    }
    if (out_right_paren_index != NULL) {
        *out_right_paren_index = right_paren_index;
    }
    return true;
}

static bool placeholder_scan_parenthesized_list_contains_top_level_comma(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= right_paren_index ||
        right_paren_index >= scan->token_count) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_string_order_key_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    if (!placeholder_scan_token_text_equals(scan, 0U, "VALUES") &&
        !placeholder_scan_token_text_equals(scan, 0U, "VALUE")) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index + 2U];

        if (placeholder_scan_token_text_equals(scan, index, "ORDER") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            (token->kind == MYLITE_SQL_TOKEN_STRING ||
             token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_constant_order_key_surface(
    const struct placeholder_statement_scan *scan
) {
    int paren_depth = 0;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_query_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "ORDER") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            placeholder_scan_token_is_constant_order_key_surface(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_constant_order_key_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING ||
           token->kind == MYLITE_SQL_TOKEN_USER_VARIABLE ||
           token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
           placeholder_scan_token_text_equals(scan, index, "NULL") ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE");
}

static bool placeholder_scan_contains_having_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t having_index = 0U;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_find_top_level_keyword(scan, "HAVING", &having_index)) {
        return false;
    }
    return placeholder_scan_having_clause_contains_residual(scan, having_index + 1U);
}

static bool placeholder_scan_having_clause_contains_residual(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > start_index &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, index) && index > start_index &&
            placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
        if (placeholder_scan_having_in_predicate_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_having_in_predicate_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    size_t operand_index = 0U;
    size_t right_paren_index = 0U;

    if (scan == NULL || in_index == 0U || in_index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, in_index, "IN")) {
        return false;
    }
    operand_index = in_index - 1U;
    if (placeholder_scan_token_text_equals(scan, operand_index, "NOT")) {
        if (operand_index == 0U) {
            return false;
        }
        --operand_index;
    }
    return placeholder_scan_token_can_end_expression_operand(scan, operand_index) &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[in_index + 1U]) &&
           placeholder_scan_find_matching_right_paren(scan, in_index + 1U, &right_paren_index) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, in_index + 1U);
}

static bool placeholder_scan_has_table_backed_or_dml_context(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE") ||
        placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return true;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "FROM") ||
            placeholder_scan_token_text_equals(scan, index, "JOIN")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_expression_operator_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_predicate_clause(scan, index) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if ((placeholder_scan_token_text_equals(scan, index, "GROUP") ||
             placeholder_scan_token_text_equals(scan, index, "ORDER")) &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "SET") &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if ((placeholder_scan_token_text_equals(scan, index, "VALUES") ||
             placeholder_scan_token_text_equals(scan, index, "VALUE")) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
        if (placeholder_scan_token_starts_duplicate_update_assignment_clause(scan, index) &&
            placeholder_scan_expression_clause_contains_operator_surface(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_expression_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_contains_parameter_marker(scan) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        placeholder_scan_contains_malformed_function_argument_surface(scan)) {
        return false;
    }
    return placeholder_scan_contains_select_symbolic_not_arithmetic_surface(scan) ||
           placeholder_scan_contains_targeted_comparison_surface(scan) ||
           placeholder_scan_contains_identifier_between_residual_surface(scan) ||
           placeholder_scan_contains_identifier_in_list_residual_surface(scan) ||
           placeholder_scan_contains_insert_keyword_group_function_surface(scan) ||
           placeholder_scan_contains_group_by_user_variable_assignment_surface(scan) ||
           placeholder_scan_contains_charset_introducer_string_surface(scan) ||
           placeholder_scan_contains_current_temporal_keyword_surface(scan);
}

static bool placeholder_scan_contains_select_symbolic_not_arithmetic_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    if (!placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        bool saw_arithmetic = false;

        if (placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR ||
            scan->tokens[index].operator_kind != MYLITE_SQL_OPERATOR_NOT ||
            !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_text_equals(scan, scan_index, "FROM")) {
                return saw_arithmetic &&
                       placeholder_scan_token_text_equals(scan, scan_index + 1U, "DUAL");
            }
            if (scan->tokens[scan_index].kind == MYLITE_SQL_TOKEN_OPERATOR &&
                scan->tokens[scan_index].operator_kind == MYLITE_SQL_OPERATOR_STAR &&
                placeholder_scan_expression_operator_surface_has_operand_context(
                    scan,
                    scan_index
                )) {
                saw_arithmetic = true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_targeted_comparison_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (!placeholder_scan_token_is_comparison_operator(scan, index) ||
            !placeholder_scan_comparison_operator_has_predicate_context(scan, index) ||
            index + 1U >= scan->token_count ||
            !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            continue;
        }
        if (scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_DECIMAL &&
            index + 4U <= scan->token_count &&
            placeholder_scan_tokens_contain_qualified_identifier(scan, index + 1U, index + 4U)) {
            return true;
        }
        if (placeholder_scan_parenthesized_left_operand_contains_comparison(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_comparison_operator_has_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index == 0U) {
        return false;
    }
    for (size_t scan_index = index; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_left_operand_contains_comparison(
    const struct placeholder_statement_scan *scan,
    size_t comparison_index
) {
    int paren_depth = 1;

    if (scan == NULL || comparison_index < 2U ||
        !mylite_sql_parser_token_is_right_paren(&scan->tokens[comparison_index - 1U])) {
        return false;
    }
    for (size_t scan_index = comparison_index - 1U; scan_index > 0U; --scan_index) {
        size_t previous_index = scan_index - 1U;

        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[previous_index])) {
            ++paren_depth;
            continue;
        }
        if (!mylite_sql_parser_token_is_left_paren(&scan->tokens[previous_index])) {
            continue;
        }
        --paren_depth;
        if (paren_depth == 0) {
            return placeholder_scan_parenthesized_range_contains_comparison_operator(
                scan,
                previous_index,
                comparison_index - 1U
            );
        }
        if (paren_depth < 0) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_range_contains_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    if (scan == NULL || left_paren_index + 2U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index + 1U < right_paren_index; ++index) {
        if (placeholder_scan_token_is_comparison_operator(scan, index) &&
            placeholder_scan_token_can_end_expression_operand(scan, index - 1U) &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_identifier_between_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (!placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
            !placeholder_scan_token_is_identifier_like(scan, index - 1U) ||
            !placeholder_scan_token_is_identifier_like(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index + 1U < scan->token_count; ++scan_index) {
            if (placeholder_scan_token_stops_expression_clause_search(scan, scan_index)) {
                break;
            }
            if (placeholder_scan_token_text_equals(scan, scan_index, "AND") &&
                placeholder_scan_token_is_identifier_like(scan, scan_index + 1U)) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_identifier_in_list_residual_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 2U; index + 3U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "IN") ||
            !placeholder_scan_token_is_identifier_like(scan, index - 1U) ||
            !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) ||
            !placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) ||
            !placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) ||
            !placeholder_scan_tokens_contain_identifier_like(scan, index + 2U, right_paren_index)) {
            continue;
        }
        for (size_t scan_index = index + 2U; scan_index < right_paren_index; ++scan_index) {
            if (mylite_sql_parser_token_is_comma(&scan->tokens[scan_index])) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_keyword_group_function_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 1U; index + 3U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_token_text_equals(scan, index, "GROUP") ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "BY") ||
            !placeholder_scan_token_text_equals(scan, index + 2U, "INSERT") ||
            !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 3U]) ||
            !placeholder_scan_find_matching_right_paren(scan, index + 3U, &right_paren_index) ||
            !placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index + 3U,
                right_paren_index
            ) ||
            !placeholder_scan_function_call_has_four_arguments(
                scan,
                index + 3U,
                right_paren_index
            ) ||
            !placeholder_scan_token_can_follow_function_call_surface(scan, right_paren_index)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool placeholder_scan_contains_group_by_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t group_index = 0U;
    int paren_depth = 0;

    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_find_top_level_keyword(scan, "GROUP", &group_index) ||
        !placeholder_scan_token_text_equals(scan, group_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = group_index + 2U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, "HAVING") ||
            placeholder_scan_token_text_equals(scan, index, "ORDER") ||
            placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
            placeholder_scan_token_text_equals(scan, index, "UNION")) {
            return false;
        }
        if (placeholder_scan_token_is_assignment_operator(scan, index) &&
            index > group_index + 2U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_USER_VARIABLE &&
            placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_function_call_has_four_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t right_paren_index
) {
    int paren_depth = 0;
    size_t argument_count = 1U;

    if (scan == NULL || left_paren_index + 1U >= right_paren_index) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            ++argument_count;
        }
    }
    return paren_depth == 0 && argument_count == 4U;
}

static bool placeholder_scan_contains_charset_introducer_string_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_CHARSET_INTRODUCER &&
            (scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING ||
             scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_HEX_LITERAL)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_current_temporal_keyword_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "CURRENT_DATE") ||
             placeholder_scan_token_text_equals(scan, index, "CURRENT_TIME")) &&
            (index + 1U >= scan->token_count ||
             !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U])) &&
            index > 0U && placeholder_scan_token_is_comparison_operator(scan, index - 1U) &&
            placeholder_scan_comparison_operator_has_predicate_context(scan, index - 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_malformed_function_argument_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
            !placeholder_scan_left_paren_starts_function_arguments(scan, index) ||
            !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index) ||
            right_paren_index == index + 1U) {
            continue;
        }
        if (!placeholder_scan_function_call_arguments_are_well_formed(
                scan,
                index,
                right_paren_index
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_expression_clause_contains_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;
    bool waiting_for_between_and = false;

    if (scan == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 &&
            placeholder_scan_token_stops_expression_clause_search(scan, index)) {
            return false;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "BETWEEN")) {
            waiting_for_between_and = true;
            continue;
        }
        if (paren_depth == 0 && waiting_for_between_and &&
            placeholder_scan_token_text_equals(scan, index, "AND")) {
            waiting_for_between_and = false;
            continue;
        }
        if (placeholder_scan_token_is_expression_operator_surface(scan, index) &&
            placeholder_scan_expression_operator_surface_has_operand_context(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "AND") ||
        placeholder_scan_token_text_equals(scan, index, "OR") ||
        placeholder_scan_token_text_equals(scan, index, "XOR") ||
        placeholder_scan_token_text_equals(scan, index, "DIV") ||
        placeholder_scan_token_text_equals(scan, index, "MOD") ||
        placeholder_scan_token_text_equals(scan, index, "NOT")) {
        return true;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           (token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_STAR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_SLASH ||
            token->operator_kind == MYLITE_SQL_OPERATOR_PERCENT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_NOT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_AND ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_OR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_XOR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_AND ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_OR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LEFT_SHIFT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_RIGHT_SHIFT);
}

static bool placeholder_scan_expression_operator_surface_has_operand_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        !placeholder_scan_token_can_start_expression_operand(scan, index + 1U)) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
        (index == 0U || !placeholder_scan_token_can_end_expression_operand(scan, index - 1U))) {
        return true;
    }
    if (placeholder_scan_token_is_unary_expression_operator_surface(scan, index) &&
        (index == 0U || !placeholder_scan_token_can_end_expression_operand(scan, index - 1U))) {
        return true;
    }
    return index > 0U && placeholder_scan_token_can_end_expression_operand(scan, index - 1U);
}

static bool placeholder_scan_token_is_unary_expression_operator_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    token = &scan->tokens[index];
    return token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
           token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
           token->operator_kind == MYLITE_SQL_OPERATOR_NOT ||
           token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT;
}

static bool placeholder_scan_token_can_start_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
        mylite_sql_parser_token_is_right_paren(&scan->tokens[index]) ||
        placeholder_scan_token_stops_expression_clause_search(scan, index)) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->kind == MYLITE_SQL_TOKEN_OPERATOR) {
        return placeholder_scan_token_is_unary_expression_operator_surface(scan, index);
    }
    return !placeholder_scan_token_is_incomplete_statement_tail(scan, index) ||
           mylite_sql_parser_token_is_left_paren(token);
}

static bool placeholder_scan_token_can_end_expression_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
        mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
        placeholder_scan_token_stops_expression_clause_search(scan, index) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index)) {
        return false;
    }
    return scan->tokens[index].kind != MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_statement_tail_is_obviously_incomplete(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count == 0U) {
        return true;
    }
    return placeholder_scan_token_is_incomplete_statement_tail(scan, scan->token_count - 1U);
}

static bool placeholder_scan_token_is_incomplete_statement_tail(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return true;
    }
    if (mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
        mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "FROM") ||
        placeholder_scan_token_text_equals(scan, index, "WHERE") ||
        placeholder_scan_token_text_equals(scan, index, "GROUP") ||
        placeholder_scan_token_text_equals(scan, index, "ORDER") ||
        placeholder_scan_token_text_equals(scan, index, "BY") ||
        placeholder_scan_token_text_equals(scan, index, "HAVING") ||
        placeholder_scan_token_text_equals(scan, index, "ON") ||
        placeholder_scan_token_text_equals(scan, index, "JOIN") ||
        placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
        placeholder_scan_token_text_equals(scan, index, "UNION") ||
        placeholder_scan_token_text_equals(scan, index, "VALUES") ||
        placeholder_scan_token_text_equals(scan, index, "SET") ||
        placeholder_scan_token_text_equals(scan, index, "AS") ||
        placeholder_scan_token_text_equals(scan, index, "COLLATE") ||
        placeholder_scan_token_text_equals(scan, index, "USING") ||
        placeholder_scan_token_text_equals(scan, index, "ESCAPE") ||
        placeholder_scan_token_text_equals(scan, index, "INTERVAL") ||
        placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
        placeholder_scan_token_text_equals(scan, index, "AND") ||
        placeholder_scan_token_text_equals(scan, index, "OR") ||
        placeholder_scan_token_text_equals(scan, index, "XOR") ||
        placeholder_scan_token_text_equals(scan, index, "DIV") ||
        placeholder_scan_token_text_equals(scan, index, "MOD") ||
        placeholder_scan_token_text_equals(scan, index, "NOT") ||
        placeholder_scan_token_text_equals(scan, index, "IN") ||
        placeholder_scan_token_text_equals(scan, index, "IS") ||
        placeholder_scan_token_text_equals(scan, index, "LIKE") ||
        placeholder_scan_token_text_equals(scan, index, "REGEXP") ||
        placeholder_scan_token_text_equals(scan, index, "RLIKE")) {
        return true;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR;
}

static bool placeholder_scan_token_stops_expression_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "SET");
}

static bool placeholder_scan_token_starts_duplicate_update_assignment_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "UPDATE") && index >= 3U &&
           placeholder_scan_token_text_equals(scan, index - 1U, "KEY") &&
           placeholder_scan_token_text_equals(scan, index - 2U, "DUPLICATE") &&
           placeholder_scan_token_text_equals(scan, index - 3U, "ON");
}

static bool placeholder_scan_contains_row_tuple_predicate_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (!placeholder_scan_row_tuple_starts_at(scan, index, &right_paren_index)) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U) ||
            placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN") ||
            (placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") &&
             placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "IN"))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_row_tuple_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index
) {
    size_t right_paren_index = 0U;
    int paren_depth = 0;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 2U ||
        placeholder_scan_left_paren_starts_function_arguments(scan, left_paren_index) ||
        placeholder_scan_query_expression_starts_at(scan, left_paren_index + 1U)) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            if (out_right_paren_index != NULL) {
                *out_right_paren_index = right_paren_index;
            }
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_left_paren_starts_function_arguments(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    if (scan == NULL || left_paren_index == 0U || left_paren_index >= scan->token_count ||
        (scan->tokens[left_paren_index].flags & MYLITE_SQL_TOKEN_HAS_LEADING_SPACE) != 0U) {
        return false;
    }
    return placeholder_scan_token_can_name_immediate_function(scan, left_paren_index - 1U);
}

static bool placeholder_scan_token_can_name_immediate_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    return !placeholder_scan_token_text_equals(scan, index, "SELECT") &&
           !placeholder_scan_token_text_equals(scan, index, "FROM") &&
           !placeholder_scan_token_text_equals(scan, index, "WHERE") &&
           !placeholder_scan_token_text_equals(scan, index, "HAVING") &&
           !placeholder_scan_token_text_equals(scan, index, "ON") &&
           !placeholder_scan_token_text_equals(scan, index, "GROUP") &&
           !placeholder_scan_token_text_equals(scan, index, "ORDER") &&
           !placeholder_scan_token_text_equals(scan, index, "BY") &&
           !placeholder_scan_token_text_equals(scan, index, "LIMIT") &&
           !placeholder_scan_token_text_equals(scan, index, "UNION") &&
           !placeholder_scan_token_text_equals(scan, index, "IN") &&
           !placeholder_scan_token_text_equals(scan, index, "NOT") &&
           !placeholder_scan_token_text_equals(scan, index, "VALUES") &&
           !placeholder_scan_token_text_equals(scan, index, "SET") &&
           !placeholder_scan_token_text_equals(scan, index, "UPDATE") &&
           !placeholder_scan_token_text_equals(scan, index, "DELETE") &&
           !placeholder_scan_token_text_equals(scan, index, "INSERT") &&
           !placeholder_scan_token_text_equals(scan, index, "REPLACE") &&
           !placeholder_scan_token_text_equals(scan, index, "JOIN");
}

static bool placeholder_scan_contains_dml_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return placeholder_scan_contains_delete_variant_surface(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "UPDATE")) {
        return placeholder_scan_contains_update_variant_surface(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REPLACE")) {
        return placeholder_scan_contains_insert_replace_variant_surface(scan);
    }
    return false;
}

static bool placeholder_scan_contains_delete_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_contains_delete_ignore_modifier_surface(scan) ||
           placeholder_scan_contains_delete_multitable_target_surface(scan) ||
           placeholder_scan_contains_delete_using_surface(scan) ||
           placeholder_scan_contains_multikey_dml_order_surface(scan) ||
           placeholder_scan_contains_user_variable_assignment_surface(scan);
}

static bool placeholder_scan_contains_delete_ignore_modifier_surface(
    const struct placeholder_statement_scan *scan
) {
    bool saw_ignore = false;
    size_t prefix_end = 0U;
    size_t from_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    prefix_end = placeholder_scan_delete_modifier_prefix_end(scan, &saw_ignore);
    return saw_ignore && prefix_end < scan->token_count &&
           !placeholder_scan_token_is_delete_modifier_keyword(scan, prefix_end) &&
           placeholder_scan_find_top_level_keyword_after(scan, "FROM", prefix_end, &from_index);
}

static bool placeholder_scan_contains_delete_multitable_target_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t target_start = 0U;
    size_t from_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE") ||
        !placeholder_scan_find_top_level_keyword(scan, "FROM", &from_index)) {
        return false;
    }
    target_start = placeholder_scan_delete_modifier_prefix_end(scan, NULL);
    if (from_index <= target_start || from_index + 1U >= scan->token_count ||
        placeholder_scan_token_is_delete_modifier_keyword(scan, target_start)) {
        return false;
    }
    return target_start < scan->token_count;
}

static bool placeholder_scan_contains_delete_using_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t from_index = 0U;
    size_t using_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    from_index = placeholder_scan_delete_modifier_prefix_end(scan, NULL);
    return placeholder_scan_token_text_equals(scan, from_index, "FROM") &&
           placeholder_scan_find_top_level_keyword(scan, "USING", &using_index) &&
           using_index > from_index + 1U && using_index + 1U < scan->token_count;
}

static bool placeholder_scan_contains_multikey_dml_order_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t order_index = 0U;
    int paren_depth = 0;

    if (scan == NULL ||
        (!placeholder_scan_token_text_equals(scan, 0U, "DELETE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "UPDATE")) ||
        !placeholder_scan_find_top_level_keyword(scan, "ORDER", &order_index) ||
        !placeholder_scan_token_text_equals(scan, order_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = order_index + 2U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "LIMIT")) {
            return false;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return index > order_index + 2U && index + 1U < scan->token_count &&
                   !placeholder_scan_token_text_equals(scan, index + 1U, "LIMIT");
        }
    }
    return false;
}

static bool placeholder_scan_contains_user_variable_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_is_assignment_operator(scan, index) && index > 0U &&
            scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_USER_VARIABLE) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_update_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t set_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        !placeholder_scan_find_top_level_keyword(scan, "SET", &set_index) || set_index <= 1U ||
        set_index + 1U >= scan->token_count) {
        return false;
    }
    return placeholder_scan_update_has_joined_source_before_set(scan, set_index) ||
           placeholder_scan_update_has_using_join_before_set(scan, set_index) ||
           placeholder_scan_contains_multikey_dml_order_surface(scan);
}

static bool placeholder_scan_update_has_joined_source_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
) {
    size_t source_start = 1U;

    if (placeholder_scan_token_text_equals(scan, source_start, "LOW_PRIORITY")) {
        ++source_start;
    }
    if (placeholder_scan_token_text_equals(scan, source_start, "IGNORE")) {
        ++source_start;
    }
    for (size_t index = source_start + 1U; index < set_index; ++index) {
        if (mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return index > source_start && index + 1U < set_index &&
                   !mylite_sql_parser_token_is_comma(&scan->tokens[index - 1U]) &&
                   !mylite_sql_parser_token_is_comma(&scan->tokens[index + 1U]);
        }
        if (placeholder_scan_token_text_equals(scan, index, "JOIN") ||
            placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_update_has_using_join_before_set(
    const struct placeholder_statement_scan *scan,
    size_t set_index
) {
    bool saw_join = false;

    for (size_t index = 1U; index < set_index; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "JOIN") ||
            placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN")) {
            saw_join = true;
        }
        if (saw_join && placeholder_scan_token_text_equals(scan, index, "USING")) {
            return index + 1U < set_index;
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_replace_variant_surface(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_contains_insert_row_alias_surface(scan) ||
           placeholder_scan_contains_insert_identifier_value_surface(scan) ||
           placeholder_scan_contains_insert_set_identifier_value_surface(scan) ||
           placeholder_scan_contains_duplicate_identifier_assignment_surface(scan) ||
           placeholder_scan_contains_duplicate_qualified_assignment_surface(scan) ||
           placeholder_scan_contains_replace_compound_select_surface(scan);
}

static bool placeholder_scan_contains_insert_row_alias_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        size_t alias_index = 0U;
        size_t row_list_end_index = 0U;

        if ((!placeholder_scan_token_text_equals(scan, index, "VALUES") &&
             !placeholder_scan_token_text_equals(scan, index, "VALUE")) ||
            !placeholder_scan_insert_row_constructor_list_end(scan, index, &row_list_end_index)) {
            continue;
        }
        alias_index = row_list_end_index;
        if (placeholder_scan_token_text_equals(scan, alias_index, "AS")) {
            ++alias_index;
        }
        if (placeholder_scan_insert_row_alias_tail_is_complete(scan, alias_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_insert_row_constructor_list_end(
    const struct placeholder_statement_scan *scan,
    size_t values_index,
    size_t *out_end_index
) {
    bool saw_constructor = false;
    size_t index = values_index + 1U;

    if (scan == NULL || out_end_index == NULL || index >= scan->token_count) {
        return false;
    }
    for (;;) {
        size_t left_paren_index = index;
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "ROW")) {
            left_paren_index = index + 1U;
        }
        if (left_paren_index >= scan->token_count ||
            !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
            !placeholder_scan_find_matching_right_paren(
                scan,
                left_paren_index,
                &right_paren_index
            )) {
            return false;
        }
        saw_constructor = true;
        index = right_paren_index + 1U;
        if (index >= scan->token_count) {
            break;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
        if (index >= scan->token_count) {
            return false;
        }
    }

    *out_end_index = index;
    return saw_constructor;
}

static bool placeholder_scan_insert_row_alias_tail_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t alias_index
) {
    size_t tail_index = alias_index + 1U;

    if (!placeholder_scan_token_can_name_insert_row_alias(scan, alias_index)) {
        return false;
    }
    if (tail_index >= scan->token_count) {
        return true;
    }
    if (mylite_sql_parser_token_is_left_paren(&scan->tokens[tail_index]) &&
        !placeholder_scan_insert_row_alias_column_list_is_complete(scan, tail_index, &tail_index)) {
        return false;
    }
    if (tail_index >= scan->token_count) {
        return true;
    }
    return placeholder_scan_token_starts_duplicate_update_assignment_clause(scan, tail_index + 3U);
}

static bool placeholder_scan_token_can_name_insert_row_alias(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index)) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "ON") ||
        placeholder_scan_token_text_equals(scan, index, "WHERE") ||
        placeholder_scan_token_text_equals(scan, index, "ORDER") ||
        placeholder_scan_token_text_equals(scan, index, "LIMIT")) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, index);
}

static bool placeholder_scan_insert_row_alias_column_list_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_end_index
) {
    size_t right_paren_index = 0U;
    bool need_column = true;

    if (scan == NULL || out_end_index == NULL ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (need_column) {
            if (!placeholder_scan_token_can_name_loose_identifier(scan, index)) {
                return false;
            }
            need_column = false;
            continue;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_column = true;
    }
    if (need_column) {
        return false;
    }
    *out_end_index = right_paren_index + 1U;
    return true;
}

static bool placeholder_scan_contains_insert_identifier_value_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "VALUES") ||
             placeholder_scan_token_text_equals(scan, index, "VALUE")) &&
            index + 1U < scan->token_count &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_values_contain_identifier(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_values_contain_identifier(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    size_t right_paren_index = 0U;
    int paren_depth = 0;

    if (!placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        mylite_sql_parser_token_is_comma(&scan->tokens[right_paren_index - 1U])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < right_paren_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index]) &&
            index + 1U == right_paren_index) {
            return false;
        }
        if (paren_depth == 0 &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_insert_set_identifier_value_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t set_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "SET", &set_index)) {
        return false;
    }
    for (size_t index = set_index + 1U; index + 1U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_equal_sign(&scan->tokens[index]) &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_duplicate_identifier_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t update_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "UPDATE", &update_index) ||
        update_index < 3U || !placeholder_scan_token_text_equals(scan, update_index - 1U, "KEY") ||
        !placeholder_scan_token_text_equals(scan, update_index - 2U, "DUPLICATE") ||
        !placeholder_scan_token_text_equals(scan, update_index - 3U, "ON")) {
        return false;
    }
    for (size_t index = update_index + 1U; index + 1U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_equal_sign(&scan->tokens[index]) &&
            placeholder_scan_token_can_start_dml_identifier_value(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_duplicate_qualified_assignment_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t update_index = 0U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        !placeholder_scan_find_top_level_keyword(scan, "UPDATE", &update_index) ||
        update_index < 3U || !placeholder_scan_token_text_equals(scan, update_index - 1U, "KEY") ||
        !placeholder_scan_token_text_equals(scan, update_index - 2U, "DUPLICATE") ||
        !placeholder_scan_token_text_equals(scan, update_index - 3U, "ON")) {
        return false;
    }
    for (size_t index = update_index + 1U; index + 1U < scan->token_count; ++index) {
        size_t lhs_start_index = update_index + 1U;
        size_t lhs_end_index = 0U;
        size_t rhs_end_index = 0U;

        if (!mylite_sql_parser_token_is_equal_sign(&scan->tokens[index])) {
            continue;
        }
        for (size_t scan_index = index; scan_index > update_index + 1U; --scan_index) {
            size_t previous_index = scan_index - 1U;

            if (mylite_sql_parser_token_is_comma(&scan->tokens[previous_index])) {
                lhs_start_index = scan_index;
                break;
            }
        }
        if (index > update_index + 1U &&
            placeholder_scan_qualified_identifier_starts_at(
                scan,
                lhs_start_index,
                &lhs_end_index
            ) &&
            lhs_end_index == index) {
            return true;
        }
        if (placeholder_scan_qualified_identifier_starts_at(scan, index + 1U, &rhs_end_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_qualified_identifier_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_end_index
) {
    if (out_end_index != NULL) {
        *out_end_index = index;
    }
    if (scan == NULL || index + 2U >= scan->token_count ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index) ||
        !placeholder_scan_token_text_equals(scan, index + 1U, ".") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
        return false;
    }
    if (out_end_index != NULL) {
        *out_end_index = index + 3U;
    }
    return true;
}

static bool placeholder_scan_contains_replace_compound_select_surface(
    const struct placeholder_statement_scan *scan
) {
    size_t select_index = 0U;
    size_t union_index = 0U;

    return scan != NULL && placeholder_scan_token_text_equals(scan, 0U, "REPLACE") &&
           placeholder_scan_find_top_level_keyword(scan, "SELECT", &select_index) &&
           placeholder_scan_find_top_level_keyword_after(
               scan,
               "UNION",
               select_index + 1U,
               &union_index
           ) &&
           union_index + 1U < scan->token_count;
}

static bool placeholder_scan_find_top_level_keyword(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t *out_index
) {
    return placeholder_scan_find_top_level_keyword_after(scan, keyword, 0U, out_index);
}

static bool placeholder_scan_find_top_level_keyword_after(
    const struct placeholder_statement_scan *scan,
    const char *keyword,
    size_t start_index,
    size_t *out_index
) {
    int paren_depth = 0;

    if (scan == NULL || keyword == NULL || out_index == NULL || start_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
            continue;
        }
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, keyword)) {
            *out_index = index;
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_assignment_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           token->operator_kind == MYLITE_SQL_OPERATOR_ASSIGN;
}

static size_t placeholder_scan_delete_modifier_prefix_end(
    const struct placeholder_statement_scan *scan,
    bool *out_saw_ignore
) {
    size_t index = 1U;

    if (out_saw_ignore != NULL) {
        *out_saw_ignore = false;
    }
    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "DELETE")) {
        return 0U;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LOW_PRIORITY")) {
        ++index;
    }
    if (placeholder_scan_token_text_equals(scan, index, "QUICK")) {
        ++index;
    }
    if (placeholder_scan_token_text_equals(scan, index, "IGNORE")) {
        if (out_saw_ignore != NULL) {
            *out_saw_ignore = true;
        }
        ++index;
    }
    return index;
}

static bool placeholder_scan_token_is_delete_modifier_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "LOW_PRIORITY") ||
           placeholder_scan_token_text_equals(scan, index, "QUICK") ||
           placeholder_scan_token_text_equals(scan, index, "IGNORE");
}

static bool placeholder_scan_token_can_start_dml_identifier_value(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || !placeholder_scan_token_is_identifier_like(scan, index)) {
        return false;
    }
    if (index + 1U >= scan->token_count) {
        return true;
    }
    return !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]);
}

static bool placeholder_scan_contains_bare_truth_clause_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_starts_predicate_clause(scan, index) &&
            placeholder_scan_token_can_start_bare_truth_expression(scan, index + 1U) &&
            placeholder_scan_bare_truth_expression_is_simple(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_can_start_bare_truth_expression(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
        return false;
    }
    token = &scan->tokens[index];
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           token->kind == MYLITE_SQL_TOKEN_INTEGER || token->kind == MYLITE_SQL_TOKEN_DECIMAL ||
           token->kind == MYLITE_SQL_TOKEN_FLOAT || token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_USER_VARIABLE ||
           token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE") ||
           placeholder_scan_token_text_equals(scan, index, "NULL") ||
           mylite_sql_parser_token_is_left_paren(token);
}

static bool placeholder_scan_bare_truth_expression_is_simple(
    const struct placeholder_statement_scan *scan,
    size_t expression_index
) {
    int paren_depth = 0;
    size_t expression_token_count = 0U;

    if (scan == NULL || expression_index >= scan->token_count) {
        return false;
    }
    if (expression_index + 1U < scan->token_count &&
        mylite_sql_parser_token_is_left_paren(&scan->tokens[expression_index + 1U])) {
        return false;
    }
    for (size_t index = expression_index; index < scan->token_count; ++index) {
        if (paren_depth == 0 && index > expression_index &&
            placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
            return expression_token_count == 1U ||
                   placeholder_scan_bare_truth_expression_is_qualified_identifier(
                       scan,
                       expression_index,
                       index
                   );
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
        if (paren_depth == 0 && (placeholder_scan_token_is_comparison_operator(scan, index) ||
                                 placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
                                 placeholder_scan_token_text_equals(scan, index, "IN") ||
                                 placeholder_scan_token_text_equals(scan, index, "IS") ||
                                 placeholder_scan_token_text_equals(scan, index, "LIKE") ||
                                 placeholder_scan_token_text_equals(scan, index, "REGEXP") ||
                                 placeholder_scan_token_text_equals(scan, index, "RLIKE"))) {
            return false;
        }
        ++expression_token_count;
    }
    return expression_token_count == 1U ||
           placeholder_scan_bare_truth_expression_is_qualified_identifier(
               scan,
               expression_index,
               scan->token_count
           );
}

static bool placeholder_scan_bare_truth_expression_is_qualified_identifier(
    const struct placeholder_statement_scan *scan,
    size_t expression_index,
    size_t stop_index
) {
    bool need_identifier = true;
    size_t token_count = 0U;

    if (scan == NULL || stop_index <= expression_index + 2U || stop_index > scan->token_count) {
        return false;
    }
    for (size_t index = expression_index; index < stop_index; ++index) {
        if (need_identifier) {
            if (!placeholder_scan_token_is_identifier_like(scan, index)) {
                return false;
            }
            need_identifier = false;
        } else {
            if (!placeholder_scan_token_text_equals(scan, index, ".")) {
                return false;
            }
            need_identifier = true;
        }
        ++token_count;
    }
    return token_count >= 3U && !need_identifier;
}

static bool placeholder_scan_contains_lateral_derived_table(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "LATERAL") &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_grouping_function(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "GROUPING") &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_sounds_like_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "SOUNDS") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LIKE") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_deprecated_logical_operator_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_token_text_equals(scan, 0U, "DO") ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (scan->tokens[index].operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_AND ||
             scan->tokens[index].operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_OR) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_like_surface(const struct placeholder_statement_scan *scan) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        bool right_operand_is_parenthesized =
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U);
        bool right_operand_is_plain =
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 1U);

        if (placeholder_scan_token_text_equals(scan, index, "LIKE") &&
            !placeholder_scan_token_text_equals(scan, index - 1U, "SOUNDS") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            (right_operand_is_plain || right_operand_is_parenthesized)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_like_escape_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 1U < scan->token_count; ++index) {
        if (!placeholder_scan_token_text_equals(scan, index, "ESCAPE") ||
            placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) ||
            placeholder_scan_token_stops_expression_clause_search(scan, index + 1U)) {
            continue;
        }
        for (size_t scan_index = index; scan_index > 0U; --scan_index) {
            size_t previous_index = scan_index - 1U;

            if (placeholder_scan_token_stops_expression_clause_search(scan, previous_index)) {
                break;
            }
            if (placeholder_scan_token_text_equals(scan, previous_index, "LIKE") &&
                previous_index > 1U && previous_index + 1U < index &&
                !placeholder_scan_token_is_incomplete_statement_tail(scan, previous_index - 1U) &&
                !placeholder_scan_token_stops_expression_clause_search(scan, previous_index - 1U) &&
                !placeholder_scan_token_is_incomplete_statement_tail(scan, previous_index + 1U) &&
                !placeholder_scan_token_stops_expression_clause_search(scan, previous_index + 1U)) {
                return true;
            }
        }
    }
    return false;
}

static bool placeholder_scan_contains_not_like_surface(const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 2U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LIKE") &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index - 1U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index - 1U) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 2U) &&
            !placeholder_scan_token_stops_expression_clause_search(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_typed_temporal_literal_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER &&
            scan->tokens[index + 1U].kind == MYLITE_SQL_TOKEN_STRING) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_interval_expression_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        size_t right_paren_index = 0U;

        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_interval_expression_follows_parenthesized_separator(scan, index) &&
            !placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) &&
            placeholder_scan_token_is_date_interval_unit(scan, index + 2U)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            index + 4U < scan->token_count &&
            !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_interval_expression_follows_parenthesized_separator(scan, index) &&
            placeholder_scan_interval_value_binary_expression_is_complete(scan, index)) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U) &&
            right_paren_index + 1U < scan->token_count &&
            placeholder_scan_token_is_date_interval_unit(scan, right_paren_index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_interval_expression_follows_parenthesized_separator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    int paren_depth = 0;

    if (scan == NULL || index == 0U ||
        !mylite_sql_parser_token_is_comma(&scan->tokens[index - 1U])) {
        return false;
    }
    for (size_t scan_index = 0U; scan_index < index; ++scan_index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth > 0;
}

static bool placeholder_scan_interval_value_binary_expression_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *operator_token = NULL;

    if (scan == NULL || index + 4U >= scan->token_count ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index + 1U) ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, index + 3U) ||
        !placeholder_scan_token_is_date_interval_unit(scan, index + 4U)) {
        return false;
    }

    operator_token = &scan->tokens[index + 2U];
    if (operator_token->kind != MYLITE_SQL_TOKEN_OPERATOR) {
        return false;
    }
    return operator_token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_XOR ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_LEFT_SHIFT ||
           operator_token->operator_kind == MYLITE_SQL_OPERATOR_RIGHT_SHIFT;
}

static bool placeholder_scan_token_is_date_interval_unit(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "YEAR") ||
           placeholder_scan_token_text_equals(scan, index, "QUARTER") ||
           placeholder_scan_token_text_equals(scan, index, "MONTH") ||
           placeholder_scan_token_text_equals(scan, index, "WEEK") ||
           placeholder_scan_token_text_equals(scan, index, "DAY") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "YEAR_MONTH") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_HOUR") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_MINUTE") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE_SECOND") ||
           placeholder_scan_token_text_equals(scan, index, "DAY_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "HOUR_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "MINUTE_MICROSECOND") ||
           placeholder_scan_token_text_equals(scan, index, "SECOND_MICROSECOND");
}

static bool placeholder_scan_contains_scalar_in_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "IN") &&
            !placeholder_scan_scalar_in_is_descriptor_predicate(scan, index) &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_scalar_in_is_descriptor_predicate(
    const struct placeholder_statement_scan *scan,
    size_t in_index
) {
    if (scan == NULL || in_index == 0U ||
        mylite_sql_parser_token_is_right_paren(&scan->tokens[in_index - 1U])) {
        return false;
    }
    for (size_t index = in_index; index > 0U; --index) {
        size_t previous_index = index - 1U;

        if (placeholder_scan_token_starts_predicate_clause(scan, previous_index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, previous_index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_token_starts_predicate_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "HAVING") ||
           placeholder_scan_token_text_equals(scan, index, "ON");
}

static bool placeholder_scan_token_stops_predicate_clause_search(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "GROUP") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "SET");
}

static bool placeholder_scan_contains_quantified_subquery_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 1U; index + 1U < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "ANY") ||
             placeholder_scan_token_text_equals(scan, index, "SOME") ||
             placeholder_scan_token_text_equals(scan, index, "ALL")) &&
            placeholder_scan_token_is_comparison_operator(scan, index - 1U) &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_contains_fulltext_match_against_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "MATCH") &&
            (placeholder_scan_match_column_list_without_parentheses_is_supported(scan, index) ||
             placeholder_scan_match_column_list_with_parentheses_is_supported(scan, index))) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_match_column_list_without_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
) {
    size_t index = match_index + 1U;
    bool saw_column = false;
    bool need_column = true;

    if (scan == NULL || index >= scan->token_count ||
        mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
        return false;
    }
    while (index < scan->token_count) {
        size_t next_index = index;

        if (need_column) {
            if (!placeholder_scan_match_column_name_starts_at(scan, index, &next_index)) {
                return false;
            }
            saw_column = true;
            need_column = false;
            index = next_index;
            continue;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            break;
        }
        need_column = true;
        ++index;
    }
    return saw_column && !need_column && index + 1U < scan->token_count &&
           placeholder_scan_token_text_equals(scan, index, "AGAINST") &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, index + 1U);
}

static bool placeholder_scan_match_column_list_with_parentheses_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t match_index
) {
    size_t left_paren_index = match_index + 1U;
    size_t right_paren_index = 0U;
    size_t index = left_paren_index + 1U;
    bool need_column = true;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U) {
        return false;
    }
    while (index < right_paren_index) {
        size_t next_index = index;

        if (need_column) {
            if (!placeholder_scan_match_column_name_starts_at(scan, index, &next_index) ||
                next_index > right_paren_index) {
                return false;
            }
            need_column = false;
            index = next_index;
            continue;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return false;
        }
        need_column = true;
        ++index;
    }
    return !need_column && right_paren_index + 2U < scan->token_count &&
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "AGAINST") &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[right_paren_index + 2U]) &&
           placeholder_scan_parenthesized_operand_is_complete(scan, right_paren_index + 2U);
}

static bool placeholder_scan_match_column_name_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_next_index
) {
    if (out_next_index != NULL) {
        *out_next_index = index;
    }
    if (!placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        return false;
    }
    ++index;
    while (placeholder_scan_token_text_equals(scan, index, ".") &&
           placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        index += 2U;
    }
    if (out_next_index != NULL) {
        *out_next_index = index;
    }
    return true;
}

static bool placeholder_scan_token_can_name_loose_identifier(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_is_identifier_like(scan, index) ||
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_KEYWORD;
}

static bool placeholder_scan_parentheses_are_balanced(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return paren_depth == 0;
}

static bool placeholder_scan_parenthesized_operand_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (!mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            continue;
        }
        --paren_depth;
        if (paren_depth < 0) {
            return false;
        }
        if (paren_depth == 0) {
            return index > left_paren_index + 1U &&
                   !mylite_sql_parser_token_is_comma(&scan->tokens[index - 1U]);
        }
    }
    return false;
}

static bool placeholder_scan_token_is_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return mylite_sql_parser_token_is_equal_sign(token) ||
           (token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            (token->operator_kind == MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_NOT_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_LESS ||
             token->operator_kind == MYLITE_SQL_OPERATOR_LESS_EQUAL ||
             token->operator_kind == MYLITE_SQL_OPERATOR_GREATER ||
             token->operator_kind == MYLITE_SQL_OPERATOR_GREATER_EQUAL));
}
