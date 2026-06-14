static enum placeholder_statement_kind classify_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind kind;

    if (scan == NULL || scan->token_count == 0U) {
        return PLACEHOLDER_STATEMENT_NONE;
    }

    kind = classify_schema_security_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }

    kind = classify_stored_program_script_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }

    return classify_utility_admin_placeholder_statement(scan);
}

static enum placeholder_statement_kind classify_schema_security_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "CALL")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return classify_create_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "ALTER")) {
        return classify_alter_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DROP")) {
        return classify_drop_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "RENAME") &&
        placeholder_scan_token_text_equals(scan, 1U, "USER")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "GRANT") ||
        placeholder_scan_token_text_equals(scan, 0U, "REVOKE")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }

    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_stored_program_script_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count == 0U ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan)) {
        return PLACEHOLDER_STATEMENT_NONE;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_stored_program_statement_starts_at(scan, index)) {
            return PLACEHOLDER_STATEMENT_UNSUPPORTED_STORED_PROGRAM;
        }
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool placeholder_scan_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (placeholder_scan_token_text_equals(scan, index, "SIGNAL") ||
        placeholder_scan_token_text_equals(scan, index, "RESIGNAL")) {
        return true;
    }
    return placeholder_scan_create_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_alter_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_drop_stored_program_statement_starts_at(scan, index) ||
           placeholder_scan_show_stored_program_statement_starts_at(scan, index);
}

static bool placeholder_scan_create_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t limit = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "CREATE")) {
        return false;
    }
    limit = index + placeholder_create_scan_token_limit;
    if (limit > scan->token_count) {
        limit = scan->token_count;
    }
    for (size_t scan_index = index + 1U; scan_index < limit; ++scan_index) {
        if (placeholder_scan_token_is_stored_program_object_keyword(scan, scan_index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_alter_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t limit = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "ALTER")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index + 1U, "PROCEDURE") ||
        placeholder_scan_token_text_equals(scan, index + 1U, "FUNCTION") ||
        placeholder_scan_token_text_equals(scan, index + 1U, "EVENT")) {
        return true;
    }
    if (!placeholder_scan_token_text_equals(scan, index + 1U, "DEFINER")) {
        return false;
    }
    limit = index + placeholder_create_scan_token_limit;
    if (limit > scan->token_count) {
        limit = scan->token_count;
    }
    for (size_t scan_index = index + 2U; scan_index < limit; ++scan_index) {
        if (placeholder_scan_token_text_equals(scan, scan_index, "EVENT")) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_drop_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "DROP") &&
           placeholder_scan_token_is_stored_program_object_keyword(scan, index + 1U);
}

static bool placeholder_scan_show_stored_program_statement_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SHOW") &&
           placeholder_scan_token_text_equals(scan, index + 1U, "CREATE") &&
           placeholder_scan_token_is_stored_program_object_keyword(scan, index + 2U);
}

static bool placeholder_scan_token_is_stored_program_object_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
           placeholder_scan_token_text_equals(scan, index, "FUNCTION") ||
           placeholder_scan_token_text_equals(scan, index, "TRIGGER") ||
           placeholder_scan_token_text_equals(scan, index, "EVENT");
}

static enum placeholder_statement_kind classify_utility_admin_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    enum placeholder_statement_kind kind;

    kind = classify_unsupported_utility_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    kind = classify_utility_noop_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    kind = classify_admin_noop_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SET")) {
        return classify_set_placeholder_statement(scan);
    }
    kind = classify_query_surface_placeholder_statement(scan);
    if (kind != PLACEHOLDER_STATEMENT_NONE) {
        return kind;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "SHOW")) {
        return classify_show_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "DESCRIBE") ||
        placeholder_scan_token_text_equals(scan, 0U, "DESC")) {
        return classify_describe_placeholder_statement(scan);
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "EXPLAIN")) {
        return classify_explain_placeholder_statement(scan);
    }

    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_unsupported_utility_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "XA") ||
        placeholder_scan_token_text_equals(scan, 0U, "HANDLER")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "GET") &&
        placeholder_scan_token_text_equals(scan, 1U, "DIAGNOSTICS")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (help_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "LOAD") &&
        placeholder_scan_token_text_equals(scan, 1U, "DATA")) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    if (load_xml_placeholder_statement_is_supported(scan) ||
        select_into_file_placeholder_statement_is_supported(scan) ||
        import_table_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_UNSUPPORTED_UTILITY;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_utility_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (placeholder_scan_token_text_equals(scan, 0U, "ANALYZE") &&
        placeholder_scan_token_text_equals(scan, 1U, "TABLE") &&
        placeholder_scan_contains_text(scan, "HISTOGRAM")) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "INSTALL") &&
        (placeholder_scan_token_text_equals(scan, 1U, "COMPONENT") ||
         placeholder_scan_token_text_equals(scan, 1U, "PLUGIN"))) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "UNINSTALL") &&
        (placeholder_scan_token_text_equals(scan, 1U, "COMPONENT") ||
         placeholder_scan_token_text_equals(scan, 1U, "PLUGIN"))) {
        return PLACEHOLDER_STATEMENT_UTILITY_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static enum placeholder_statement_kind classify_admin_noop_placeholder_statement(
    const struct placeholder_statement_scan *scan
) {
    if (lock_instance_placeholder_statement_is_supported(scan) ||
        unlock_instance_placeholder_statement_is_supported(scan) ||
        change_replication_source_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (flush_placeholder_statement_is_supported(scan)) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "RESET") ||
        placeholder_scan_token_text_equals(scan, 0U, "PURGE") ||
        placeholder_scan_token_text_equals(scan, 0U, "KILL") ||
        placeholder_scan_token_text_equals(scan, 0U, "CACHE") ||
        placeholder_scan_token_text_equals(scan, 0U, "RESTART") ||
        placeholder_scan_token_text_equals(scan, 0U, "SHUTDOWN") ||
        placeholder_scan_token_text_equals(scan, 0U, "CLONE") ||
        placeholder_scan_token_text_equals(scan, 0U, "BINLOG")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    if (placeholder_scan_token_text_equals(scan, 0U, "LOAD") &&
        placeholder_scan_token_text_equals(scan, 1U, "INDEX")) {
        return PLACEHOLDER_STATEMENT_ADMIN_NOOP;
    }
    return PLACEHOLDER_STATEMENT_NONE;
}

static bool flush_placeholder_statement_is_supported(const struct placeholder_statement_scan *scan
) {
    size_t index = 1U;

    if (scan == NULL || scan->token_count < 2U ||
        !placeholder_scan_token_text_equals(scan, 0U, "FLUSH")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NO_WRITE_TO_BINLOG") ||
        placeholder_scan_token_text_equals(scan, index, "LOCAL")) {
        ++index;
    }
    if (index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "TABLE") ||
        placeholder_scan_token_text_equals(scan, index, "TABLES")) {
        return flush_placeholder_table_option_is_supported(scan, index);
    }
    for (;;) {
        if (!flush_placeholder_option_is_supported(scan, &index)) {
            return false;
        }
        if (index == scan->token_count) {
            return true;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return false;
        }
        ++index;
        if (index == scan->token_count) {
            return false;
        }
    }
}

static bool flush_placeholder_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "BINARY") ||
        placeholder_scan_token_text_equals(scan, *index, "ENGINE") ||
        placeholder_scan_token_text_equals(scan, *index, "ERROR") ||
        placeholder_scan_token_text_equals(scan, *index, "GENERAL") ||
        placeholder_scan_token_text_equals(scan, *index, "SLOW")) {
        if (!placeholder_scan_token_text_equals(scan, *index + 1U, "LOGS")) {
            return false;
        }
        *index += 2U;
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "RELAY")) {
        if (!placeholder_scan_token_text_equals(scan, *index + 1U, "LOGS")) {
            return false;
        }
        *index += 2U;
        if (placeholder_scan_token_text_equals(scan, *index, "FOR") &&
            placeholder_scan_token_text_equals(scan, *index + 1U, "CHANNEL")) {
            size_t channel_index = *index + 2U;

            if (channel_index >= scan->token_count ||
                (!placeholder_scan_token_is_identifier_like(scan, channel_index) &&
                 !mylite_sql_parser_token_is_string_literal(&scan->tokens[channel_index]))) {
                return false;
            }
            *index += 3U;
        }
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, *index, "LOGS") ||
        placeholder_scan_token_text_equals(scan, *index, "PRIVILEGES") ||
        placeholder_scan_token_text_equals(scan, *index, "OPTIMIZER_COSTS") ||
        placeholder_scan_token_text_equals(scan, *index, "STATUS") ||
        placeholder_scan_token_text_equals(scan, *index, "USER_RESOURCES")) {
        ++*index;
        return true;
    }
    return false;
}

static bool flush_placeholder_table_option_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t table_index
) {
    size_t index = table_index + 1U;

    if (scan == NULL || table_index >= scan->token_count) {
        return false;
    }
    if (index == scan->token_count) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "READ") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "LOCK")) {
        return index + 3U == scan->token_count;
    }
    if (!flush_placeholder_table_name_list_is_supported(scan, &index)) {
        return false;
    }
    if (index == scan->token_count) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "WITH") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "READ") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "LOCK")) {
        return index + 3U == scan->token_count;
    }
    if (placeholder_scan_token_text_equals(scan, index, "FOR") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "EXPORT")) {
        return index + 2U == scan->token_count;
    }
    return false;
}

static bool flush_placeholder_table_name_list_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (!flush_placeholder_table_name_is_supported(scan, index)) {
        return false;
    }
    while (*index < scan->token_count && mylite_sql_parser_token_is_comma(&scan->tokens[*index])) {
        ++*index;
        if (!flush_placeholder_table_name_is_supported(scan, index)) {
            return false;
        }
    }
    return true;
}

static bool flush_placeholder_table_name_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL ||
        !flush_placeholder_table_name_part_is_supported(scan, *index)) {
        return false;
    }
    ++*index;
    while (placeholder_scan_token_text_equals(scan, *index, ".") &&
           flush_placeholder_table_name_part_is_supported(scan, *index + 1U)) {
        *index += 2U;
    }
    return true;
}

static bool flush_placeholder_table_name_part_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count ||
        placeholder_scan_token_text_equals(scan, index, ".") ||
        placeholder_scan_token_text_equals(scan, index, "WITH") ||
        placeholder_scan_token_text_equals(scan, index, "FOR")) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           token->kind == MYLITE_SQL_TOKEN_KEYWORD;
}
