static enum placeholder_statement_kind classify_create_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (spatial_reference_system_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE") ||
        logfile_group_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (create_view_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_zerofill_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_extended_option_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_residual_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (create_table_select_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }

    for (size_t index = 1U;
         index < scan->token_count && index < placeholder_create_scan_token_limit;
         ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
        }
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_alter_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE") ||
        placeholder_scan_token_text_equals(scan, 1U, "INSTANCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE") ||
        logfile_group_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (alter_view_placeholder_statement_is_supported(scan) ||
        alter_table_tablespace_file_operation_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_zerofill_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (alter_table_engine_first_multi_action_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ALTER_TABLE_MULTI_ACTION_UNSUPPORTED;
    }
    if (ddl_extended_option_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (ddl_residual_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
        alter_table_partition_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (alter_schema_unsupported_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "PROCEDURE") ||
        placeholder_scan_token_text_equals(scan, 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, 1U, "EVENT")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool foreign_server_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 1U, "SERVER")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return foreign_server_create_placeholder_statement_is_supported(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "ALTER")) {
        return foreign_server_alter_placeholder_statement_is_supported(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DROP")) {
        return foreign_server_drop_placeholder_statement_is_supported(scan);
    }
    return false;
}

static bool foreign_server_create_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_create_min_tokens = 8 };

    bool saw_foreign_data_wrapper = false;
    bool saw_options = false;

    if (scan == NULL || scan->token_count < foreign_server_create_min_tokens) {
        return false;
    }
    for (size_t index = 3U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "FOREIGN") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "DATA") &&
            placeholder_scan_token_text_equals(scan, index + 2U, "WRAPPER")) {
            saw_foreign_data_wrapper = true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "OPTIONS")) {
            saw_options = true;
        }
    }
    return saw_foreign_data_wrapper && saw_options;
}

static bool foreign_server_alter_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_alter_min_tokens = 5 };

    if (scan == NULL || scan->token_count < foreign_server_alter_min_tokens) {
        return false;
    }
    for (size_t index = 3U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "OPTIONS")) {
            return true;
        }
    }
    return false;
}

static bool foreign_server_drop_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { foreign_server_drop_if_exists_tokens = 5 };

    if (scan == NULL || scan->token_count < 3U) {
        return false;
    }
    if (scan->token_count == 3U) {
        return foreign_server_drop_name_is_supported(scan, 2U);
    }
    return scan->token_count == foreign_server_drop_if_exists_tokens &&
           placeholder_scan_token_text_equals(scan, 2U, "IF") &&
           placeholder_scan_token_text_equals(scan, 3U, "EXISTS") &&
           foreign_server_drop_name_is_supported(scan, 4U);
}

static bool foreign_server_drop_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_text_equals(scan, index, "IF") ||
        placeholder_scan_token_text_equals(scan, index, "EXISTS")) {
        return false;
    }
    return placeholder_scan_token_can_name_loose_identifier(scan, index) ||
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool spatial_reference_system_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t spatial_index = 1U;
    size_t system_index = 0U;

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        if (placeholder_scan_token_text_equals(scan, 1U, "OR") &&
            placeholder_scan_token_text_equals(scan, 2U, "REPLACE")) {
            spatial_index = 3U;
        }
    } else if (!placeholder_scan_token_text_equals(scan, 0U, "DROP")) {
        return false;
    }

    system_index = spatial_index + 2U;
    if (!placeholder_scan_token_text_equals(scan, spatial_index, "SPATIAL") ||
        !placeholder_scan_token_text_equals(scan, spatial_index + 1U, "REFERENCE") ||
        !placeholder_scan_token_text_equals(scan, system_index, "SYSTEM")) {
        return false;
    }
    return system_index + 1U < scan->token_count;
}

static bool alter_schema_unsupported_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        (!placeholder_scan_token_text_equals(scan, 1U, "DATABASE") &&
         !placeholder_scan_token_text_equals(scan, 1U, "SCHEMA"))) {
        return false;
    }
    for (size_t index = 2U; index < scan->token_count; ++index) {
        if (alter_schema_unsupported_encryption_option_starts_at(scan, index) ||
            alter_schema_unsupported_read_only_option_starts_at(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool alter_schema_unsupported_encryption_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t value_index = index + 1U;

    if (scan == NULL || index + 1U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, index, "ENCRYPTION")) {
        return false;
    }
    if (mylite_sql_parser_token_is_equal_sign(&scan->tokens[value_index])) {
        ++value_index;
    }
    if (value_index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, value_index, "DEFAULT") ||
           scan->tokens[value_index].kind == MYLITE_SQL_TOKEN_STRING;
}

static bool alter_schema_unsupported_read_only_option_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t value_index = index + 2U;

    if (scan == NULL || index + 2U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, index, "READ") ||
        !placeholder_scan_token_text_equals(scan, index + 1U, "ONLY")) {
        return false;
    }
    if (mylite_sql_parser_token_is_equal_sign(&scan->tokens[value_index])) {
        ++value_index;
    }
    return alter_schema_read_only_value_is_supported(scan, value_index);
}

static bool alter_schema_read_only_value_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, index, "DEFAULT") ||
           placeholder_scan_token_text_equals(scan, index, "0") ||
           placeholder_scan_token_text_equals(scan, index, "1");
}

static bool alter_table_engine_first_multi_action_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 2U;

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_starts_alter_table_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        return false;
    }
    ++index;
    if (placeholder_scan_token_text_equals(scan, index, ".") &&
        placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        index += 2U;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "ENGINE")) {
        return false;
    }
    ++index;
    if (index < scan->token_count && mylite_sql_parser_token_is_equal_sign(&scan->tokens[index])) {
        ++index;
    }
    if (index >= scan->token_count ||
        (!placeholder_scan_token_can_name_loose_identifier(scan, index) &&
         scan->tokens[index].kind != MYLITE_SQL_TOKEN_STRING)) {
        return false;
    }
    ++index;
    if (index >= scan->token_count || !mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
        return false;
    }
    return alter_table_engine_first_multi_action_starter_is_supported(scan, index + 1U);
}

static bool alter_table_engine_first_multi_action_starter_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const starters[] = {
        "ADD",
        "ALTER",
        "CHANGE",
        "DROP",
        "MODIFY",
        "RENAME",
    };

    return placeholder_scan_token_text_equals_any(
        scan,
        index,
        starters,
        sizeof(starters) / sizeof(starters[0])
    );
}

static bool create_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { create_view_scan_limit = 16 };

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < create_view_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "VIEW")) {
            return view_placeholder_statement_has_query_body(scan, index);
        }
        if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool alter_view_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    enum { alter_view_scan_limit = 16 };

    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < alter_view_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "VIEW")) {
            return view_placeholder_statement_has_query_body(scan, index);
        }
        if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "USER") ||
            placeholder_scan_token_text_equals(scan, index, "INSTANCE") ||
            placeholder_scan_token_text_equals(scan, index, "TABLESPACE") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool view_placeholder_statement_has_query_body(
    const struct placeholder_statement_scan *scan,
    size_t view_index
) {
    if (scan == NULL || view_index >= scan->token_count) {
        return false;
    }
    for (size_t index = view_index + 1U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "AS") &&
            create_table_select_query_starts_at(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool undo_tablespace_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && scan->token_count > 3U &&
           placeholder_scan_token_text_equals(scan, 1U, "UNDO") &&
           placeholder_scan_token_text_equals(scan, 2U, "TABLESPACE") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool logfile_group_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && scan->token_count > 3U &&
           placeholder_scan_token_text_equals(scan, 1U, "LOGFILE") &&
           placeholder_scan_token_text_equals(scan, 2U, "GROUP") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool ddl_zerofill_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && placeholder_scan_contains_text(scan, "ZEROFILL") &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan));
}

static bool ddl_extended_option_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (!ddl_extended_option_placeholder_statement_is_candidate(scan)) {
        return false;
    }
    return ddl_extended_option_scan_has_marker(scan) ||
           ddl_check_expression_placeholder_scan_has_marker(scan);
}

static bool ddl_extended_option_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan));
}

static bool ddl_extended_option_scan_has_marker(const struct placeholder_statement_scan *scan) {
    static const char *const column_storage_values[] = {"DISK", "MEMORY"};
    static const char *const column_format_values[] = {"FIXED", "DYNAMIC", "DEFAULT"};

    if (placeholder_scan_contains_text(scan, "ENCRYPTION") ||
        placeholder_scan_contains_text(scan, "SECONDARY_ENGINE") ||
        placeholder_scan_contains_text(scan, "ENGINE_ATTRIBUTE") ||
        placeholder_scan_contains_text(scan, "SECONDARY_ENGINE_ATTRIBUTE")) {
        return true;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if ((placeholder_scan_token_text_equals(scan, index, "DATA") ||
             placeholder_scan_token_text_equals(scan, index, "INDEX")) &&
            placeholder_scan_token_text_equals(scan, index + 1U, "DIRECTORY")) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "COLUMN_FORMAT") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index + 1U,
                column_format_values,
                sizeof(column_format_values) / sizeof(column_format_values[0])
            )) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "STORAGE") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index + 1U,
                column_storage_values,
                sizeof(column_storage_values) / sizeof(column_storage_values[0])
            )) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "NOT") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "SECONDARY")) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (!ddl_residual_placeholder_statement_is_candidate(scan)) {
        return false;
    }
    return ddl_residual_scan_has_fulltext_parser_clause(scan) ||
           ddl_residual_scan_has_generated_column_surface(scan) ||
           ddl_residual_scan_has_foreign_key_set_default_action(scan) ||
           ddl_residual_scan_has_alter_table_order_by_action(scan) ||
           ddl_residual_scan_has_create_table_start_transaction_option(scan);
}

static bool ddl_residual_placeholder_statement_is_candidate(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U) &&
           (placeholder_scan_starts_create_table_statement(scan) ||
            placeholder_scan_starts_alter_table_statement(scan) ||
            placeholder_scan_starts_create_index_statement(scan));
}

static bool ddl_residual_scan_has_fulltext_parser_clause(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "FULLTEXT")) {
        return false;
    }
    for (size_t index = 0U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARSER") &&
            placeholder_scan_token_can_name_loose_identifier(scan, index + 2U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_scan_has_generated_column_surface(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_starts_create_table_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "GENERATED") &&
            ddl_generated_column_clause_is_complete(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool ddl_generated_column_clause_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t generated_index
) {
    size_t left_paren_index = generated_index + 3U;
    size_t right_paren_index = 0U;

    if (scan == NULL || generated_index + 4U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, generated_index + 1U, "ALWAYS") ||
        !placeholder_scan_token_text_equals(scan, generated_index + 2U, "AS") ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        placeholder_scan_token_is_incomplete_statement_tail(scan, right_paren_index - 1U)) {
        return false;
    }
    return right_paren_index + 1U >= scan->token_count ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "VIRTUAL") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "STORED") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") ||
           placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NULL") ||
           mylite_sql_parser_token_is_comma(&scan->tokens[right_paren_index + 1U]) ||
           mylite_sql_parser_token_is_right_paren(&scan->tokens[right_paren_index + 1U]);
}

static bool ddl_residual_scan_has_foreign_key_set_default_action(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "FOREIGN")) {
        return false;
    }
    for (size_t index = 0U; index + 3U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "ON") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "DELETE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE")) &&
            placeholder_scan_token_text_equals(scan, index + 2U, "SET") &&
            placeholder_scan_token_text_equals(scan, index + 3U, "DEFAULT")) {
            return true;
        }
    }
    return false;
}

static bool ddl_residual_scan_has_alter_table_order_by_action(
    const struct placeholder_statement_scan *scan
) {
    size_t start_index = 0U;
    int paren_depth = 0;

    if (scan == NULL || !placeholder_scan_starts_alter_table_statement(scan)) {
        return false;
    }

    start_index = alter_table_partition_operation_start_index(scan);
    for (size_t index = start_index; index + 3U < scan->token_count; ++index) {
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
        if (paren_depth != 0 || !mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
            !placeholder_scan_token_text_equals(scan, index + 1U, "ORDER") ||
            !placeholder_scan_token_text_equals(scan, index + 2U, "BY")) {
            continue;
        }
        return placeholder_scan_token_can_name_loose_identifier(scan, index + 3U);
    }
    return false;
}

static bool ddl_residual_scan_has_create_table_start_transaction_option(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_starts_create_table_statement(scan)) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (index > 0U && mylite_sql_parser_token_is_right_paren(&scan->tokens[index - 1U]) &&
            placeholder_scan_token_text_equals(scan, index, "START") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "TRANSACTION")) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_placeholder_scan_has_marker(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || !placeholder_scan_contains_text(scan, "CHECK")) {
        return false;
    }
    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "CHECK") &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            ddl_check_expression_placeholder_clause_has_marker(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_placeholder_clause_has_marker(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth == 0) {
                return false;
            }
            --paren_depth;
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, "INTERVAL") ||
            placeholder_scan_token_text_equals(scan, index, "&&") ||
            placeholder_scan_token_text_equals(scan, index, "||")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "IN") &&
            ddl_check_expression_in_list_marker_is_supported(scan, index + 1U)) {
            return true;
        }
    }
    return false;
}

static bool ddl_check_expression_in_list_marker_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index
) {
    int paren_depth = 0;
    bool saw_item = false;
    bool need_item = true;

    if (scan == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index])) {
        return false;
    }
    for (size_t index = left_paren_index + 1U; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            saw_item = true;
            need_item = false;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth > 0) {
                --paren_depth;
                continue;
            }
            return saw_item && !need_item;
        }
        if (paren_depth == 0 && mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            if (need_item) {
                return false;
            }
            need_item = true;
            continue;
        }
        saw_item = true;
        need_item = false;
    }
    return false;
}

static bool placeholder_scan_starts_create_table_statement(
    const struct placeholder_statement_scan *scan
) {
    enum { create_table_scan_limit = 6 };

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count && index < create_table_scan_limit; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "TABLE")) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, index, "VIEW") ||
            placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
            placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
            placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
            placeholder_scan_token_text_equals(scan, index, "EVENT")) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_starts_alter_table_statement(
    const struct placeholder_statement_scan *scan
) {
    return placeholder_scan_token_text_equals(scan, 0U, "ALTER") &&
           placeholder_scan_token_text_equals(scan, 1U, "TABLE");
}

static bool placeholder_scan_starts_create_index_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 1U;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "UNIQUE") ||
        placeholder_scan_token_text_equals(scan, index, "FULLTEXT") ||
        placeholder_scan_token_text_equals(scan, index, "SPATIAL")) {
        ++index;
    }
    return placeholder_scan_token_text_equals(scan, index, "INDEX");
}

static bool alter_table_partition_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t operation_start_index = 0U;

    if (scan == NULL || scan->token_count < alter_table_partition_min_token_count ||
        !alter_table_partition_scan_has_balanced_parentheses(scan, 2U)) {
        return false;
    }
    operation_start_index = alter_table_partition_operation_start_index(scan);
    if (operation_start_index >= scan->token_count) {
        return false;
    }
    return alter_table_partition_scan_has_operation(scan, operation_start_index);
}

static bool alter_table_tablespace_file_operation_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t operation_start_index = 0U;

    if (scan == NULL || scan->token_count < alter_table_partition_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 1U, "TABLE") ||
        !alter_table_partition_scan_has_balanced_parentheses(scan, 2U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    operation_start_index = alter_table_partition_operation_start_index(scan);
    for (size_t index = operation_start_index; index + 1U < scan->token_count; ++index) {
        if (alter_table_tablespace_file_operation_starts_at(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool alter_table_tablespace_file_operation_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t operation_index
) {
    size_t index = operation_index + 1U;

    if (scan == NULL || operation_index + 1U >= scan->token_count ||
        (!placeholder_scan_token_text_equals(scan, operation_index, "DISCARD") &&
         !placeholder_scan_token_text_equals(scan, operation_index, "IMPORT"))) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "TABLESPACE")) {
        return true;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
        !placeholder_scan_token_text_equals(scan, index, "SUBPARTITION")) {
        return false;
    }
    ++index;
    while (placeholder_scan_token_can_name_loose_identifier(scan, index)) {
        ++index;
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            break;
        }
        ++index;
    }
    return index < scan->token_count &&
           placeholder_scan_token_text_equals(scan, index, "TABLESPACE");
}

static size_t alter_table_partition_operation_start_index(
    const struct placeholder_statement_scan *scan
) {
    size_t table_name_index = 2U;

    if (scan == NULL) {
        return table_name_index;
    }
    if (table_name_index + 2U < scan->token_count &&
        placeholder_scan_token_text_equals(scan, table_name_index + 1U, ".")) {
        return table_name_index + 3U;
    }
    return table_name_index + 1U;
}

static bool alter_table_partition_scan_has_operation(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    static const char *const partition_action_prefixes[] = {
        "ADD",
        "DROP",
        "REORGANIZE",
        "REBUILD",
        "COALESCE",
        "TRUNCATE",
        "EXCHANGE",
        "ANALYZE",
        "CHECK",
        "OPTIMIZE",
        "REPAIR",
    };
    int paren_depth = 0;

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
        if (paren_depth != 0) {
            continue;
        }
        if (index + 2U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index, "REMOVE") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARTITIONING")) {
            return true;
        }
        if (index + 1U < scan->token_count &&
            placeholder_scan_token_text_equals(scan, index + 1U, "PARTITION") &&
            placeholder_scan_token_text_equals_any(
                scan,
                index,
                partition_action_prefixes,
                sizeof(partition_action_prefixes) / sizeof(partition_action_prefixes[0])
            )) {
            if (placeholder_scan_token_text_equals(scan, index, "ADD") ||
                placeholder_scan_token_text_equals(scan, index, "REORGANIZE")) {
                return true;
            }
            return index + 2U < scan->token_count;
        }
    }
    return false;
}

static bool alter_table_partition_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

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

static bool placeholder_scan_token_text_equals_any(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *const *texts,
    size_t text_count
) {
    if (texts == NULL) {
        return false;
    }
    for (size_t text_index = 0U; text_index < text_count; ++text_index) {
        if (placeholder_scan_token_text_equals(scan, index, texts[text_index])) {
            return true;
        }
    }
    return false;
}

static bool load_xml_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "LOAD") ||
        !placeholder_scan_token_text_equals(scan, 1U, "XML") ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 2U, "INFILE") ||
           (placeholder_scan_token_text_equals(scan, 2U, "LOCAL") &&
            placeholder_scan_token_text_equals(scan, 3U, "INFILE"));
}

static bool select_into_file_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    for (size_t index = 1U; index + 2U < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, "INTO") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "OUTFILE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "DUMPFILE")) &&
            scan->tokens[index + 2U].kind == MYLITE_SQL_TOKEN_STRING) {
            return true;
        }
    }
    return false;
}

static bool import_table_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           placeholder_scan_token_text_equals(scan, 0U, "IMPORT") &&
           placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
           placeholder_scan_token_text_equals(scan, 2U, "FROM") && scan->token_count > 3U &&
           scan->tokens[3U].kind == MYLITE_SQL_TOKEN_STRING &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool help_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan) {
    return scan != NULL && !scan->has_non_trailing_semicolon &&
           placeholder_scan_token_text_equals(scan, 0U, "HELP") && scan->token_count > 1U &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
}

static bool lock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count == 4U &&
           placeholder_scan_token_text_equals(scan, 0U, "LOCK") &&
           placeholder_scan_token_text_equals(scan, 1U, "INSTANCE") &&
           placeholder_scan_token_text_equals(scan, 2U, "FOR") &&
           placeholder_scan_token_text_equals(scan, 3U, "BACKUP");
}

static bool unlock_instance_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count == 2U &&
           placeholder_scan_token_text_equals(scan, 0U, "UNLOCK") &&
           placeholder_scan_token_text_equals(scan, 1U, "INSTANCE");
}

static bool change_replication_source_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count > 4U &&
           placeholder_scan_token_text_equals(scan, 0U, "CHANGE") &&
           placeholder_scan_token_text_equals(scan, 1U, "REPLICATION") &&
           placeholder_scan_token_text_equals(scan, 2U, "SOURCE") &&
           placeholder_scan_token_text_equals(scan, 3U, "TO") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U);
}

static bool change_replication_filter_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    return scan != NULL && !scan->has_non_trailing_semicolon && scan->token_count > 3U &&
           placeholder_scan_token_text_equals(scan, 0U, "CHANGE") &&
           placeholder_scan_token_text_equals(scan, 1U, "REPLICATION") &&
           placeholder_scan_token_text_equals(scan, 2U, "FILTER") &&
           !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
           placeholder_scan_parentheses_are_balanced(scan, 0U);
}

static bool replication_control_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->has_non_trailing_semicolon || scan->token_count < 2U ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        (!placeholder_scan_token_text_equals(scan, 0U, "START") &&
         !placeholder_scan_token_text_equals(scan, 0U, "STOP"))) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 1U, "REPLICA") ||
           placeholder_scan_token_text_equals(scan, 1U, "GROUP_REPLICATION");
}

static enum placeholder_statement_kind classify_drop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "USER") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (foreign_server_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (spatial_reference_system_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TABLESPACE") ||
        logfile_group_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (undo_tablespace_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, 1U, "TRIGGER") ||
        placeholder_scan_token_text_equals(scan, 1U, "EVENT")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_set_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind leading_kind = classify_set_leading_placeholder_statement(scan);
    bool statement_is_complete = false;

    if (leading_kind != PLACEHOLDER_STATEMENT_NONE) {
        return leading_kind;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "TRANSACTION")) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    statement_is_complete = !scan->has_non_trailing_semicolon &&
                            placeholder_scan_parentheses_are_balanced(scan, 0U) &&
                            !placeholder_scan_statement_tail_is_obviously_incomplete(scan);
    if (!statement_is_complete) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_contains_expression_operator_surface(scan) ||
        set_placeholder_scan_contains_unsupported_residual(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_set_leading_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 1U, "PASSWORD") ||
        placeholder_scan_token_text_equals(scan, 1U, "ROLE") ||
        placeholder_scan_token_text_equals(scan, 1U, "RESOURCE") ||
        placeholder_scan_token_text_equals(scan, 1U, "PERSIST") ||
        placeholder_scan_token_text_equals(scan, 1U, "PERSIST_ONLY") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@PERSIST.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@PERSIST_ONLY.")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "GLOBAL") ||
        placeholder_scan_token_text_equals(scan, 1U, "SESSION") ||
        placeholder_scan_token_text_equals(scan, 1U, "LOCAL") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@GLOBAL.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@SESSION.") ||
        placeholder_scan_token_text_starts_with(scan, 1U, "@@LOCAL.")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "DEFAULT") &&
        placeholder_scan_token_text_equals(scan, 2U, "ROLE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool set_placeholder_scan_contains_unsupported_residual(
    const struct placeholder_statement_scan *scan
) {
    bool saw_assignment_operator = false;

    for (size_t index = 1U; index < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];
        bool is_assignment_operator = mylite_sql_parser_token_is_equal_sign(token) ||
                                      placeholder_scan_token_is_assignment_operator(scan, index);

        if (set_placeholder_token_requires_unsupported_utility(
                scan,
                index,
                saw_assignment_operator
            )) {
            return true;
        }
        if (is_assignment_operator) {
            saw_assignment_operator = true;
        }
    }
    return false;
}

static bool set_placeholder_token_requires_unsupported_utility(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
) {
    const struct mylite_sql_token *token = &scan->tokens[index];

    if (placeholder_scan_token_text_equals(scan, index, "PERSIST") ||
        placeholder_scan_token_text_equals(scan, index, "PERSIST_ONLY")) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_STRING && index > 1U &&
        scan->tokens[index - 1U].kind == MYLITE_SQL_TOKEN_STRING) {
        return true;
    }
    if (token->kind == MYLITE_SQL_TOKEN_SYSTEM_VARIABLE && index > 1U &&
        (mylite_sql_parser_token_is_equal_sign(&scan->tokens[index - 1U]) ||
         placeholder_scan_token_is_assignment_operator(scan, index - 1U))) {
        return true;
    }
    if (set_placeholder_assignment_value_starts_unsupported_surface(scan, index)) {
        return true;
    }
    return set_placeholder_assigned_token_is_unsupported_surface(
        scan,
        index,
        saw_assignment_operator
    );
}

static bool set_placeholder_assignment_value_starts_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (!mylite_sql_parser_token_is_equal_sign(&scan->tokens[index]) &&
        !placeholder_scan_token_is_assignment_operator(scan, index)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, index + 1U, "EXISTS") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "FROM_UNIXTIME") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "CHAR") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "LEFT") ||
           placeholder_scan_token_text_equals(scan, index + 1U, "DEBUG");
}

static bool set_placeholder_assigned_token_is_unsupported_surface(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool saw_assignment_operator
) {
    const struct mylite_sql_token *token = &scan->tokens[index];

    if (!saw_assignment_operator) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "IN") ||
        placeholder_scan_token_text_equals(scan, index, "EXISTS") ||
        placeholder_scan_token_text_equals(scan, index, "FROM_UNIXTIME")) {
        return true;
    }
    return token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '.';
}

static enum placeholder_statement_kind classify_show_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (show_engine_mutex_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (!scan->has_non_trailing_semicolon && placeholder_scan_parentheses_are_balanced(scan, 0U) &&
        !placeholder_scan_statement_tail_is_obviously_incomplete(scan) &&
        placeholder_scan_contains_text(scan, "WHERE") &&
        (placeholder_scan_token_text_equals(scan, 1U, "TRIGGERS") ||
         (placeholder_scan_token_text_equals(scan, 1U, "OPEN") &&
          placeholder_scan_token_text_equals(scan, 2U, "TABLES")))) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if ((placeholder_scan_token_text_equals(scan, 1U, "PROCEDURE") ||
         placeholder_scan_token_text_equals(scan, 1U, "FUNCTION")) &&
        placeholder_scan_token_text_equals(scan, 2U, "CODE")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    if (placeholder_scan_token_text_equals(scan, 1U, "PROFILE") ||
        placeholder_scan_token_text_equals(scan, 1U, "PROFILES")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_describe_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count < 2U || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_explain_statement_start_is_supported(scan, 1U)) {
        return PLACEHOLDER_STATEMENT_EXPLAIN;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool show_engine_mutex_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count != 4U || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "SHOW") ||
        !placeholder_scan_token_text_equals(scan, 1U, "ENGINE") ||
        !placeholder_scan_token_is_identifier_like(scan, 2U) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 3U, "MUTEX");
}

static enum placeholder_statement_kind classify_explain_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    size_t statement_index = 1U;

    if (scan == NULL || scan->token_count < 2U) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (placeholder_scan_token_text_equals(scan, statement_index, "ANALYZE")) {
        ++statement_index;
    }
    if (placeholder_scan_token_text_equals(scan, statement_index, "FORMAT")) {
        statement_index += 3U;
        if (!placeholder_scan_token_text_equals(scan, statement_index - 2U, "=") ||
            !placeholder_scan_token_is_identifier_like(scan, statement_index - 1U)) {
            return PLACEHOLDER_STATEMENT_NONE;
        }
    }
    if (statement_index >= scan->token_count) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    if (!placeholder_explain_statement_start_is_supported(scan, statement_index)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    return PLACEHOLDER_STATEMENT_EXPLAIN;
}

static bool placeholder_explain_statement_start_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
        placeholder_scan_token_text_equals(scan, index, "TABLE") ||
        placeholder_scan_token_text_equals(scan, index, "WITH") ||
        placeholder_scan_token_text_equals(scan, index, "(")) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, index, "INSERT") ||
           placeholder_scan_token_text_equals(scan, index, "REPLACE") ||
           placeholder_scan_token_text_equals(scan, index, "UPDATE") ||
           placeholder_scan_token_text_equals(scan, index, "DELETE");
}

static bool placeholder_scan_contains_text(
    const struct placeholder_statement_scan *scan,
    const char *text
) {
    if (scan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_token_text_equals(scan, index, text)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_identifier_like(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           placeholder_scan_token_text_equals(scan, index, "CURRENT") ||
           placeholder_scan_token_text_equals(scan, index, "LOCKED") ||
           placeholder_scan_token_text_equals(scan, index, "NOWAIT") ||
           placeholder_scan_token_text_equals(scan, index, "SESSION_USER") ||
           placeholder_scan_token_text_equals(scan, index, "SKIP") ||
           placeholder_scan_token_text_equals(scan, index, "SYSTEM_USER") ||
           placeholder_scan_token_text_equals(scan, index, "TRADITIONAL") ||
           placeholder_scan_token_text_equals(scan, index, "USER") ||
           placeholder_scan_token_text_equals(scan, index, "JSON") ||
           placeholder_scan_token_text_equals(scan, index, "TREE") ||
           placeholder_scan_token_text_equals(scan, index, "CSV");
}

static bool placeholder_scan_token_text_equals(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *text
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    return mylite_sql_parser_token_text_equals(&scan->tokens[index], text);
}

static bool placeholder_scan_token_text_starts_with(
    const struct placeholder_statement_scan *scan,
    size_t index,
    const char *prefix
) {
    const struct mylite_sql_token *token = NULL;
    size_t prefix_size = strlen(prefix);

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    if (token->length < prefix_size) {
        return false;
    }
    for (size_t offset = 0U; offset < prefix_size; ++offset) {
        if (mylite_sql_parser_ascii_upper((unsigned char)token->text[offset]) !=
            mylite_sql_parser_ascii_upper((unsigned char)prefix[offset])) {
            return false;
        }
    }
    return true;
}

static bool placeholder_token_is_semicolon(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ';';
}

static enum mylite_sql_ast_node_kind ast_kind_for_placeholder_statement(
    enum placeholder_statement_kind kind
) {
    switch (kind) {
    case PLACEHOLDER_STATEMENT_ADMIN_NOOP:
        return MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT;
    case PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM:
        return MYLITE_SQL_AST_UNSUPPORTED_STORED_PROGRAM_STATEMENT;
    case PLACEHOLDER_STATEMENT_UTILITY_NOOP:
        return MYLITE_SQL_AST_UTILITY_NOOP_STATEMENT;
    case PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY:
        return MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT;
    case PLACEHOLDER_STATEMENT_EXPLAIN:
        return MYLITE_SQL_AST_EXPLAIN_STATEMENT;
    case PLACEHOLDER_STATEMENT_ALTER_TABLE_MULTI_ACTION_UNSUPPORTED:
        return MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT;
    case PLACEHOLDER_STATEMENT_NONE:
        break;
    }
    return MYLITE_SQL_AST_SCRIPT;
}

static enum mylite_sql_parse_status finish_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum placeholder_statement_kind kind
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *script = NULL;

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);

    state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };
    statement = mylite_sql_parser_make_raw_statement(
        &state,
        ast_kind_for_placeholder_statement(kind),
        (struct mylite_sql_source_span){
            .text = config.input,
            .length = config.length,
            .offset = 0U,
            .source_length = config.length,
        }
    );
    script = mylite_sql_parser_make_script_with_statement(&state, statement);
    if (statement == NULL || script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }

    mylite_sql_parser_state_set_root(&state, script);
    return result->status;
}

static enum mylite_sql_parse_status finish_explain_placeholder_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *format = NULL;
    struct mylite_sql_ast_node *analyze = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *script = NULL;
    size_t token_index = 1U;

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);

    state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };

    if (placeholder_scan_token_text_equals(scan, token_index, "ANALYZE")) {
        analyze = mylite_sql_parser_make_explain_analyze(&state, scan->tokens[token_index]);
        ++token_index;
    }
    if (placeholder_scan_token_text_equals(scan, token_index, "FORMAT")) {
        struct mylite_sql_ast_node *format_name =
            mylite_sql_parser_make_identifier(&state, scan->tokens[token_index + 2U]);
        format =
            mylite_sql_parser_make_explain_format(&state, scan->tokens[token_index], format_name);
    }

    statement =
        mylite_sql_parser_make_explain_statement(&state, scan->tokens[0], format, analyze, NULL);
    script = mylite_sql_parser_make_script_with_statement(&state, statement);
    if (statement == NULL || script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }

    mylite_sql_parser_state_set_root(&state, script);
    return result->status;
}
