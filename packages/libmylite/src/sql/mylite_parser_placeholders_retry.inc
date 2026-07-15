enum mylite_sql_parse_status mylite_sql_parser_try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t duplicate_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_select_result_option_before_duplicate(&scan, &duplicate_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = parse_select_result_option_before_duplicate_tokens(
        config,
        &scan,
        duplicate_index,
        &retry_result
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        result->status = status;
        *out_handled = true;
        return status;
    }
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_select_result_option_before_duplicate(
    const struct placeholder_statement_scan *scan,
    size_t *out_duplicate_index
) {
    size_t index = 1U;

    if (out_duplicate_index != NULL) {
        *out_duplicate_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < 3U || !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_token_is_select_result_option(scan, index)) {
        return false;
    }
    while (index < scan->token_count) {
        if (!placeholder_scan_token_is_select_result_option(scan, index)) {
            break;
        }
        ++index;
    }
    if (index >= scan->token_count ||
        !placeholder_scan_token_is_select_duplicate_modifier(scan, index)) {
        return false;
    }
    if (out_duplicate_index != NULL) {
        *out_duplicate_index = index;
    }
    return true;
}

static bool placeholder_scan_token_is_select_result_option(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "SQL_SMALL_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_BIG_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_BUFFER_RESULT") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_NO_CACHE") ||
           placeholder_scan_token_text_equals(scan, index, "SQL_CALC_FOUND_ROWS");
}

static bool placeholder_scan_token_is_select_duplicate_modifier(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "ALL") ||
           placeholder_scan_token_text_equals(scan, index, "DISTINCT") ||
           placeholder_scan_token_text_equals(scan, index, "DISTINCTROW");
}

static enum mylite_sql_parse_status parse_select_result_option_before_duplicate_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t duplicate_index,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL || scan == NULL || scan->tokens == NULL ||
        duplicate_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    mylite_sql_parser_reset_parse_result(out_result);

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    status = feed_select_modifier_reordered_token(
        config,
        parser,
        &state,
        &token_history,
        &previous_token_was_dot,
        &scan->tokens[0]
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[duplicate_index]
        );
    }
    for (size_t index = 1U; status == MYLITE_SQL_PARSE_OK && index < duplicate_index; ++index) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    for (size_t index = duplicate_index + 1U;
         status == MYLITE_SQL_PARSE_OK && index < scan->token_count;
         ++index) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_select_modifier_reordered_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token
        );
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

static enum mylite_sql_parse_status feed_select_modifier_reordered_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
) {
    return feed_token_with_parser_token_override(
        config,
        parser,
        state,
        history,
        previous_token_was_dot,
        token,
        0
    );
}

static enum mylite_sql_parse_status feed_token_with_parser_token_override(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token,
    int parser_token_override
) {
    struct mylite_sql_parser_token_map token_map;
    struct mylite_sql_lexer token_lexer;

    if (parser == NULL || state == NULL || state->result == NULL || history == NULL ||
        previous_token_was_dot == NULL || token == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    if (parser_token_override > 0) {
        mylite_sql_lemon(parser, parser_token_override, *token, state);
        *previous_token_was_dot = false;
        mylite_sql_parser_update_token_history(history, parser_token_override);
        return state->result->status;
    }

    token_lexer = (struct mylite_sql_lexer){
        .input = config.input,
        .length = config.length,
        .offset = token->offset + token->length,
        .modes = config.modes,
    };
    if (token_lexer.offset > config.length) {
        token_lexer.offset = config.length;
    }
    if (!mylite_sql_parser_map_lexer_token(
            state,
            &token_lexer,
            token,
            *previous_token_was_dot,
            history,
            &token_map
        )) {
        record_parse_error(
            state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                .parser_token = -1,
                .token = *token,
            }
        );
        return state->result->status;
    }

    mylite_sql_lemon(parser, token_map.parser_token, *token, state);
    *previous_token_was_dot = token_map.previous_token_was_dot;
    mylite_sql_parser_update_token_history(history, token_map.parser_token);
    return state->result->status;
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_constructor_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_parenthesized_row_constructors(&scan)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = mylite_sql_parser_parse_with_lemon_options(config, &retry_result, true);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_parenthesized_row_constructors(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count == 0U || !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        if (placeholder_scan_row_tuple_starts_at(scan, index, NULL)) {
            return true;
        }
    }
    return false;
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_row_constructor_predicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct placeholder_row_arithmetic_subject_retries retries = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    bool can_retry = false;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    status = scan_row_constructor_predicate_retries(&scan, &retries, &can_retry);
    if (status != MYLITE_SQL_PARSE_OK) {
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return status;
    }
    if (!can_retry) {
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status =
        parse_parenthesized_row_arithmetic_predicate_tokens(config, &scan, &retries, &retry_result);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    placeholder_row_arithmetic_subject_retries_deinit(&retries);
    free(tokens);
    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        result->status = status;
        *out_handled = true;
        return status;
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_row_constructor_predicate_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry
) {
    static const char placeholder_text[] = "__mylite_row_constructor_predicate";

    if (out_can_retry != NULL) {
        *out_can_retry = false;
    }
    if (out_retries != NULL) {
        *out_retries = (struct placeholder_row_arithmetic_subject_retries){0};
    }
    if (scan == NULL || scan->tokens == NULL || out_retries == NULL || out_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (scan->has_non_trailing_semicolon || scan->token_count == 0U ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return MYLITE_SQL_PARSE_OK;
    }

    for (size_t index = 0U; index < scan->token_count; ++index) {
        struct row_constructor_predicate_scan predicate_scan = {0};
        size_t end_index = index;
        enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

        if (!placeholder_scan_row_constructor_predicate_is_in_where_context(scan, index)) {
            continue;
        }
        if (!placeholder_scan_row_constructor_comparison_starts_at(scan, index, &predicate_scan)) {
            status = scan_row_constructor_in_predicate_retries_at(
                scan,
                index,
                placeholder_text,
                out_retries,
                out_can_retry,
                &end_index
            );
            if (status != MYLITE_SQL_PARSE_OK) {
                return status;
            }
            if (end_index > index) {
                index = end_index;
            }
            continue;
        }

        status = append_row_constructor_predicate_retry(
            scan,
            index,
            predicate_scan.left_right_paren_index,
            placeholder_text,
            out_retries,
            out_can_retry
        );
        if (status == MYLITE_SQL_PARSE_OK) {
            status = append_row_constructor_predicate_retry(
                scan,
                predicate_scan.right_row_index,
                predicate_scan.right_right_paren_index,
                placeholder_text,
                out_retries,
                out_can_retry
            );
        }
        if (status != MYLITE_SQL_PARSE_OK) {
            return status;
        }
        index = predicate_scan.right_right_paren_index;
    }
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_row_constructor_predicate_is_in_where_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t cursor = index;

    if (scan == NULL || scan->tokens == NULL || index == 0U || index >= scan->token_count) {
        return false;
    }
    --cursor;
    while (cursor > 0U && mylite_sql_parser_token_is_left_paren(&scan->tokens[cursor])) {
        --cursor;
    }
    if (placeholder_scan_token_text_equals(scan, cursor, "WHERE")) {
        return true;
    }
    if (!placeholder_scan_token_continues_row_arithmetic_predicate_subject(scan, cursor, false)) {
        return false;
    }
    while (cursor > 0U) {
        --cursor;
        if (placeholder_scan_token_text_equals(scan, cursor, "WHERE")) {
            return true;
        }
        if (placeholder_scan_token_text_equals(scan, cursor, "ON") ||
            placeholder_scan_token_text_equals(scan, cursor, "HAVING") ||
            placeholder_scan_token_stops_predicate_clause_search(scan, cursor)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_row_constructor_comparison_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct row_constructor_predicate_scan *out_predicate_scan
) {
    size_t left_right_paren_index = 0U;
    size_t operator_index = 0U;
    size_t right_row_index = 0U;
    size_t right_right_paren_index = 0U;

    if (!placeholder_scan_predicate_row_value_starts_at(scan, index, &left_right_paren_index)) {
        return false;
    }
    operator_index = left_right_paren_index + 1U;
    right_row_index = operator_index + 1U;
    if (right_row_index >= scan->token_count ||
        !placeholder_scan_token_is_row_constructor_comparison_operator(scan, operator_index) ||
        !placeholder_scan_predicate_row_value_starts_at(
            scan,
            right_row_index,
            &right_right_paren_index
        )) {
        return false;
    }

    if (out_predicate_scan != NULL) {
        *out_predicate_scan = (struct row_constructor_predicate_scan){
            .left_right_paren_index = left_right_paren_index,
            .right_row_index = right_row_index,
            .right_right_paren_index = right_right_paren_index,
        };
    }
    return true;
}

static enum mylite_sql_parse_status scan_row_constructor_in_predicate_retries_at(
    const struct placeholder_statement_scan *scan,
    size_t row_index,
    const char *placeholder_text,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry,
    size_t *out_end_index
) {
    struct row_constructor_in_predicate_scan predicate_scan = {0};
    size_t value_start = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_end_index != NULL) {
        *out_end_index = row_index;
    }
    if (!placeholder_scan_row_constructor_in_predicate_starts_at(
            scan,
            row_index,
            &predicate_scan
        )) {
        return MYLITE_SQL_PARSE_OK;
    }

    status = append_row_constructor_predicate_retry(
        scan,
        row_index,
        predicate_scan.left_right_paren_index,
        placeholder_text,
        out_retries,
        out_can_retry
    );
    value_start = predicate_scan.list_left_paren_index + 1U;
    while (status == MYLITE_SQL_PARSE_OK && value_start < predicate_scan.list_right_paren_index) {
        size_t value_right_paren_index = 0U;

        if (!placeholder_scan_predicate_row_value_starts_at(
                scan,
                value_start,
                &value_right_paren_index
            ) ||
            value_right_paren_index >= predicate_scan.list_right_paren_index) {
            placeholder_row_arithmetic_subject_retries_deinit(out_retries);
            return MYLITE_SQL_PARSE_MISUSE;
        }
        status = append_row_arithmetic_predicate_value_retry(
            scan,
            value_start,
            value_right_paren_index,
            out_retries,
            out_can_retry
        );
        if (value_right_paren_index + 1U >= predicate_scan.list_right_paren_index) {
            break;
        }
        value_start = value_right_paren_index + 2U;
    }
    if (status == MYLITE_SQL_PARSE_OK && out_end_index != NULL) {
        *out_end_index = predicate_scan.list_right_paren_index;
    }
    return status;
}

static bool placeholder_scan_row_constructor_in_predicate_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct row_constructor_in_predicate_scan *out_predicate_scan
) {
    size_t left_right_paren_index = 0U;
    size_t operator_index = 0U;
    size_t list_left_paren_index = 0U;
    size_t list_right_paren_index = 0U;
    size_t value_start = 0U;

    if (!placeholder_scan_predicate_row_value_starts_at(scan, index, &left_right_paren_index)) {
        return false;
    }
    operator_index = left_right_paren_index + 1U;
    if (placeholder_scan_token_text_equals(scan, operator_index, "IN")) {
        list_left_paren_index = operator_index + 1U;
    } else if (placeholder_scan_token_text_equals(scan, operator_index, "NOT") &&
               placeholder_scan_token_text_equals(scan, operator_index + 1U, "IN")) {
        list_left_paren_index = operator_index + 2U;
    } else {
        return false;
    }
    if (list_left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[list_left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(
            scan,
            list_left_paren_index,
            &list_right_paren_index
        ) ||
        list_right_paren_index <= list_left_paren_index + 1U ||
        placeholder_scan_in_list_is_subquery(
            scan,
            list_left_paren_index + 1U,
            list_right_paren_index - 1U
        )) {
        return false;
    }

    value_start = list_left_paren_index + 1U;
    while (value_start < list_right_paren_index) {
        size_t value_right_paren_index = 0U;

        if (!placeholder_scan_predicate_row_value_starts_at(
                scan,
                value_start,
                &value_right_paren_index
            ) ||
            value_right_paren_index >= list_right_paren_index) {
            return false;
        }
        if (value_right_paren_index + 1U == list_right_paren_index) {
            break;
        }
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[value_right_paren_index + 1U])) {
            return false;
        }
        value_start = value_right_paren_index + 2U;
    }

    if (out_predicate_scan != NULL) {
        *out_predicate_scan = (struct row_constructor_in_predicate_scan){
            .left_right_paren_index = left_right_paren_index,
            .list_left_paren_index = list_left_paren_index,
            .list_right_paren_index = list_right_paren_index,
        };
    }
    return true;
}

static bool placeholder_scan_predicate_row_value_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index,
    size_t *out_right_paren_index
) {
    return placeholder_scan_row_constructor_starts_at(scan, index, out_right_paren_index) ||
           placeholder_scan_row_tuple_starts_at(scan, index, out_right_paren_index);
}

static enum mylite_sql_parse_status append_row_constructor_predicate_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    const char *placeholder_text,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
) {
    struct placeholder_row_arithmetic_subject_retry retry;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL || placeholder_text == NULL ||
        start_index > end_index || end_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    retry = (struct placeholder_row_arithmetic_subject_retry){
        .start_index = start_index,
        .end_index = end_index,
        .placeholder_token = {
            .kind = MYLITE_SQL_TOKEN_IDENTIFIER,
            .text = placeholder_text,
            .length = scan->tokens[end_index].offset + scan->tokens[end_index].length -
                      scan->tokens[start_index].offset,
            .offset = scan->tokens[start_index].offset,
        },
    };
    if (!placeholder_row_arithmetic_subject_retries_push(retries, retry)) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    *inout_can_retry = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_token_is_row_constructor_comparison_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "=") ||
           placeholder_scan_token_text_equals(scan, index, "<=>") ||
           placeholder_scan_token_text_equals(scan, index, "<>") ||
           placeholder_scan_token_text_equals(scan, index, "!=") ||
           placeholder_scan_token_text_equals(scan, index, "<") ||
           placeholder_scan_token_text_equals(scan, index, "<=") ||
           placeholder_scan_token_text_equals(scan, index, ">") ||
           placeholder_scan_token_text_equals(scan, index, ">=");
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct placeholder_row_arithmetic_subject_retries retries = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    bool allow_concat_operator = (config.modes & MYLITE_SQL_MODE_PIPES_AS_CONCAT) != 0U;
    bool can_retry = false;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    status = scan_parenthesized_row_arithmetic_predicate_retries(
        &scan,
        &retries,
        &can_retry,
        allow_concat_operator
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return status;
    }
    if (!can_retry) {
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status =
        parse_parenthesized_row_arithmetic_predicate_tokens(config, &scan, &retries, &retry_result);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        placeholder_row_arithmetic_subject_retries_deinit(&retries);
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    placeholder_row_arithmetic_subject_retries_deinit(&retries);
    free(tokens);
    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        result->status = status;
        *out_handled = true;
        return status;
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_parenthesized_row_arithmetic_predicate_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *out_retries,
    bool *out_can_retry,
    bool allow_concat_operator
) {
    static const char placeholder_text[] = "__mylite_row_arithmetic_subject";
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_can_retry != NULL) {
        *out_can_retry = false;
    }
    if (out_retries != NULL) {
        *out_retries = (struct placeholder_row_arithmetic_subject_retries){0};
    }
    if (scan == NULL || scan->tokens == NULL || out_retries == NULL || out_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (scan->has_non_trailing_semicolon || scan->token_count == 0U ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return MYLITE_SQL_PARSE_OK;
    }

    for (size_t index = 0U; index < scan->token_count; ++index) {
        size_t right_paren_index = 0U;
        struct placeholder_row_arithmetic_subject_retry retry;

        if (!placeholder_scan_parenthesized_row_arithmetic_predicate_starts_at(
                scan,
                index,
                &right_paren_index,
                allow_concat_operator
            )) {
            continue;
        }
        retry = (struct placeholder_row_arithmetic_subject_retry){
            .start_index = index,
            .end_index = right_paren_index,
            .placeholder_token = {
                .kind = MYLITE_SQL_TOKEN_IDENTIFIER,
                .text = placeholder_text,
                .length = scan->tokens[right_paren_index].offset +
                          scan->tokens[right_paren_index].length - scan->tokens[index].offset,
                .offset = scan->tokens[index].offset,
            },
        };
        if (!placeholder_row_arithmetic_subject_retries_push(out_retries, retry)) {
            placeholder_row_arithmetic_subject_retries_deinit(out_retries);
            return MYLITE_SQL_PARSE_NOMEM;
        }
        *out_can_retry = true;
        index = right_paren_index;
    }

    if (placeholder_scan_can_retry_row_arithmetic_predicate_values(scan)) {
        for (size_t index = 0U; index < scan->token_count; ++index) {
            size_t end_index = 0U;

            status = scan_row_arithmetic_predicate_retry_at(
                scan,
                index,
                out_retries,
                out_can_retry,
                &end_index,
                allow_concat_operator
            );
            if (status != MYLITE_SQL_PARSE_OK) {
                placeholder_row_arithmetic_subject_retries_deinit(out_retries);
                return status;
            }
            if (end_index > index) {
                index = end_index;
            }
        }
    }

    return scan_row_scalar_non_predicate_context_retries(
        scan,
        out_retries,
        out_can_retry,
        allow_concat_operator
    );
}

static enum mylite_sql_parse_status scan_row_arithmetic_predicate_retry_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    enum mylite_sql_parse_status status = scan_literal_left_string_predicate_subject_retry(
        scan,
        start_index,
        retries,
        inout_can_retry,
        out_end_index,
        allow_concat_operator
    );

    if (status == MYLITE_SQL_PARSE_OK && out_end_index != NULL && *out_end_index == start_index) {
        status = scan_row_scalar_predicate_subject_retry(
            scan,
            start_index,
            retries,
            inout_can_retry,
            out_end_index,
            allow_concat_operator
        );
    }
    if (status == MYLITE_SQL_PARSE_OK && out_end_index != NULL && *out_end_index == start_index) {
        status = scan_row_arithmetic_predicate_value_retries(
            scan,
            start_index,
            retries,
            inout_can_retry,
            out_end_index,
            allow_concat_operator
        );
    }
    return status;
}

static enum mylite_sql_parse_status scan_row_scalar_non_predicate_context_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
) {
    enum mylite_sql_parse_status status = scan_row_scalar_update_assignment_retries(
        scan,
        retries,
        inout_can_retry,
        allow_concat_operator
    );

    if (status != MYLITE_SQL_PARSE_OK) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return status;
    }
    status = scan_row_scalar_duplicate_update_assignment_retries(
        scan,
        retries,
        inout_can_retry,
        allow_concat_operator
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return status;
    }
    status =
        scan_row_bitwise_order_key_retries(scan, retries, inout_can_retry, allow_concat_operator);
    if (status != MYLITE_SQL_PARSE_OK) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
    }
    return status;
}

static enum mylite_sql_parse_status scan_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    bool handled = false;

    if (out_end_index != NULL) {
        *out_end_index = start_index;
    }
    if (scan == NULL || retries == NULL || inout_can_retry == NULL || out_end_index == NULL ||
        start_index + 1U >= scan->token_count ||
        !placeholder_scan_token_is_top_level_where_predicate_context(scan, start_index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    status = scan_direct_row_scalar_predicate_subject_retry(
        scan,
        start_index,
        retries,
        inout_can_retry,
        out_end_index,
        allow_concat_operator,
        &handled
    );
    if (status != MYLITE_SQL_PARSE_OK || handled) {
        return status;
    }

    if (!placeholder_scan_find_row_scalar_predicate_subject_end(
            scan,
            start_index,
            allow_concat_operator,
            out_end_index
        )) {
        return MYLITE_SQL_PARSE_OK;
    }
    return append_row_scalar_predicate_subject_retry(
        scan,
        start_index,
        *out_end_index,
        retries,
        inout_can_retry
    );
}

static enum mylite_sql_parse_status scan_direct_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator,
    bool *out_handled
) {
    size_t right_paren_index = 0U;

    *out_handled = false;
    if (!placeholder_scan_token_is_retryable_row_scalar_context_function(scan, start_index) ||
        !placeholder_scan_token_is_supported_row_scalar_value_function(scan, start_index) ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index + 1U]) ||
        !placeholder_scan_find_matching_right_paren(scan, start_index + 1U, &right_paren_index) ||
        !placeholder_scan_predicate_value_is_direct_row_scalar_expression(
            scan,
            start_index,
            right_paren_index
        ) ||
        !placeholder_scan_row_arithmetic_value_has_row_operand(
            scan,
            start_index,
            right_paren_index
        ) ||
        !placeholder_scan_row_scalar_predicate_subject_has_valid_suffix(
            scan,
            start_index,
            right_paren_index,
            allow_concat_operator
        )) {
        return MYLITE_SQL_PARSE_OK;
    }

    *out_handled = true;
    *out_end_index = right_paren_index;
    if (placeholder_scan_row_scalar_predicate_subject_uses_truth_placeholder(
            scan,
            start_index,
            right_paren_index,
            allow_concat_operator
        )) {
        return append_row_arithmetic_predicate_value_retry(
            scan,
            start_index,
            right_paren_index,
            retries,
            inout_can_retry
        );
    }
    return append_row_scalar_predicate_subject_retry(
        scan,
        start_index,
        right_paren_index,
        retries,
        inout_can_retry
    );
}

static bool placeholder_scan_find_row_scalar_predicate_subject_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    bool allow_concat_operator,
    size_t *out_end_index
) {
    size_t subject_end_index = SIZE_MAX;
    int paren_depth = 0;

    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth == 0) {
                break;
            }
            --paren_depth;
            continue;
        }
        if (paren_depth != 0) {
            continue;
        }
        if (placeholder_scan_token_is_comparison_operator(scan, index) ||
            placeholder_scan_token_text_equals(scan, index, "IS") ||
            placeholder_scan_token_text_equals(scan, index, "BETWEEN") ||
            (placeholder_scan_token_text_equals(scan, index, "NOT") &&
             placeholder_scan_token_text_equals(scan, index + 1U, "BETWEEN"))) {
            if (index == start_index) {
                return false;
            }
            subject_end_index = index - 1U;
            break;
        }
        if (placeholder_scan_token_stops_predicate_value(
                scan,
                index,
                false,
                allow_concat_operator
            )) {
            break;
        }
    }

    if (subject_end_index == SIZE_MAX ||
        !placeholder_scan_row_scalar_predicate_subject_has_valid_suffix(
            scan,
            start_index,
            subject_end_index,
            allow_concat_operator
        ) ||
        !allow_concat_operator ||
        !placeholder_scan_row_concat_range_has_operator(scan, start_index, subject_end_index) ||
        !placeholder_scan_row_arithmetic_value_has_operator(
            scan,
            start_index,
            subject_end_index,
            allow_concat_operator
        )) {
        return false;
    }
    *out_end_index = subject_end_index;
    return true;
}

static enum mylite_sql_parse_status scan_literal_left_string_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t suffix_index = start_index + 1U;
    size_t predicate_end_index = start_index;
    bool has_descriptor_context = false;

    if (out_end_index != NULL) {
        *out_end_index = start_index;
    }
    if (scan == NULL || retries == NULL || inout_can_retry == NULL || out_end_index == NULL ||
        start_index >= scan->token_count ||
        scan->tokens[start_index].kind != MYLITE_SQL_TOKEN_STRING ||
        !placeholder_scan_token_is_in_where_predicate_context(scan, start_index) ||
        suffix_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (placeholder_scan_token_text_equals(scan, suffix_index, "BETWEEN")) {
        has_descriptor_context =
            placeholder_scan_literal_left_string_between_has_descriptor_context(
                scan,
                suffix_index,
                &predicate_end_index,
                allow_concat_operator
            );
    } else if (placeholder_scan_token_text_equals(scan, suffix_index, "IN")) {
        has_descriptor_context = placeholder_scan_literal_left_string_in_has_descriptor_context(
            scan,
            suffix_index,
            &predicate_end_index,
            allow_concat_operator
        );
    } else if (
        placeholder_scan_token_text_equals(scan, suffix_index, "NOT") &&
        suffix_index + 1U < scan->token_count &&
        placeholder_scan_token_text_equals(scan, suffix_index + 1U, "BETWEEN")
    ) {
        has_descriptor_context =
            placeholder_scan_literal_left_string_between_has_descriptor_context(
                scan,
                suffix_index + 1U,
                &predicate_end_index,
                allow_concat_operator
            );
    } else if (
        placeholder_scan_token_text_equals(scan, suffix_index, "NOT") &&
        suffix_index + 1U < scan->token_count &&
        placeholder_scan_token_text_equals(scan, suffix_index + 1U, "IN")
    ) {
        has_descriptor_context = placeholder_scan_literal_left_string_in_has_descriptor_context(
            scan,
            suffix_index + 1U,
            &predicate_end_index,
            allow_concat_operator
        );
    }

    if (!has_descriptor_context) {
        return MYLITE_SQL_PARSE_OK;
    }
    return append_row_arithmetic_predicate_value_retry(
        scan,
        start_index,
        start_index,
        retries,
        inout_can_retry
    );
}

static enum mylite_sql_parse_status append_row_scalar_predicate_subject_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
) {
    static const char placeholder_text[] = "__mylite_row_scalar_subject";
    struct placeholder_row_arithmetic_subject_retry retry;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL || start_index > end_index ||
        end_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    retry = (struct placeholder_row_arithmetic_subject_retry){
        .start_index = start_index,
        .end_index = end_index,
        .placeholder_token = {
            .kind = MYLITE_SQL_TOKEN_IDENTIFIER,
            .text = placeholder_text,
            .length = scan->tokens[end_index].offset + scan->tokens[end_index].length -
                      scan->tokens[start_index].offset,
            .offset = scan->tokens[start_index].offset,
        },
    };
    if (!placeholder_row_arithmetic_subject_retries_push(retries, retry)) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    *inout_can_retry = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_token_is_retryable_row_scalar_context_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const function_names[] = {
        "LEFT",    "RIGHT",          "LOWER",     "LCASE",      "UPPER",
        "UCASE",   "LTRIM",          "RTRIM",     "TRIM",       "LPAD",
        "RPAD",    "REPEAT",         "SPACE",     "QUOTE",      "REVERSE",
        "SOUNDEX", "UNHEX",          "INET_ATON",  "INET_NTOA",  "EXPORT_SET",
        "MAKE_SET", "SUBSTRING_INDEX", "STRCMP",   "ROW_COUNT",  "FOUND_ROWS",
        "LAST_INSERT_ID", "VALIDATE_PASSWORD_STRENGTH",
    };

    for (size_t name_index = 0U; name_index < sizeof(function_names) / sizeof(function_names[0]);
         ++name_index) {
        if (placeholder_scan_token_text_equals(scan, index, function_names[name_index])) {
            return true;
        }
    }
    return placeholder_scan_token_is_sys_helper_function(scan, index);
}

static bool placeholder_scan_token_is_sys_helper_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const function_names[] = {
        "EXTRACT_SCHEMA_FROM_FILE_NAME",
        "EXTRACT_TABLE_FROM_FILE_NAME",
        "FORMAT_BYTES",
        "FORMAT_PATH",
        "FORMAT_STATEMENT",
        "FORMAT_TIME",
        "LIST_ADD",
        "LIST_DROP",
        "QUOTE_IDENTIFIER",
        "SYS_GET_CONFIG",
        "VERSION_MAJOR",
        "VERSION_MINOR",
        "VERSION_PATCH",
    };

    return placeholder_scan_token_text_equals_any(
        scan,
        index,
        function_names,
        sizeof(function_names) / sizeof(function_names[0])
    );
}

static bool placeholder_scan_token_is_top_level_where_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    int paren_depth = 0;
    bool in_where = false;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    for (size_t cursor = 0U; cursor < index; ++cursor) {
        if (placeholder_scan_token_text_equals(scan, cursor, "WHERE")) {
            in_where = true;
            paren_depth = 0;
            continue;
        }
        if (!in_where) {
            continue;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, cursor)) {
            return false;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[cursor])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[cursor])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return in_where && paren_depth == 0;
}

static bool placeholder_scan_row_scalar_predicate_subject_has_valid_suffix(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    size_t suffix_index = end_index + 1U;

    if (scan == NULL || start_index >= scan->token_count || end_index >= scan->token_count) {
        return false;
    }
    if (suffix_index >= scan->token_count) {
        return placeholder_scan_token_text_equals(scan, start_index, "STRCMP");
    }
    if (placeholder_scan_token_is_comparison_operator(scan, suffix_index) ||
        placeholder_scan_token_text_equals(scan, suffix_index, "IS") ||
        placeholder_scan_token_text_equals(scan, suffix_index, "BETWEEN")) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, suffix_index, "NOT")) {
        return placeholder_scan_token_text_equals(scan, suffix_index + 1U, "BETWEEN");
    }
    return placeholder_scan_token_text_equals(scan, start_index, "STRCMP") &&
           placeholder_scan_token_stops_predicate_value(
               scan,
               suffix_index,
               false,
               allow_concat_operator
           );
}

static bool placeholder_scan_row_scalar_predicate_subject_uses_truth_placeholder(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    size_t suffix_index = end_index + 1U;

    if (scan == NULL || start_index >= scan->token_count || end_index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, start_index, "STRCMP")) {
        return false;
    }
    if (suffix_index >= scan->token_count) {
        return true;
    }
    return placeholder_scan_token_stops_predicate_value(
        scan,
        suffix_index,
        false,
        allow_concat_operator
    );
}

static enum mylite_sql_parse_status scan_row_scalar_update_assignment_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
) {
    size_t set_index = 0U;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (!placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
        !placeholder_scan_top_level_update_set_index(scan, &set_index) ||
        !placeholder_scan_update_target_is_single_table(scan, set_index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    return scan_row_scalar_assignment_list_retries(
        scan,
        set_index + 1U,
        retries,
        inout_can_retry,
        false,
        allow_concat_operator
    );
}

static enum mylite_sql_parse_status scan_row_scalar_duplicate_update_assignment_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
) {
    size_t update_index = 0U;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (!placeholder_scan_top_level_duplicate_update_index(scan, &update_index)) {
        return MYLITE_SQL_PARSE_OK;
    }
    return scan_row_scalar_assignment_list_retries(
        scan,
        update_index + 1U,
        retries,
        inout_can_retry,
        true,
        allow_concat_operator
    );
}

static enum mylite_sql_parse_status scan_row_scalar_assignment_list_retries(
    const struct placeholder_statement_scan *scan,
    size_t index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool use_literal_placeholder,
    bool allow_concat_operator
) {
    if (scan == NULL || retries == NULL || inout_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    while (index < scan->token_count &&
           !placeholder_scan_token_stops_update_assignment_list(scan, index)) {
        size_t equal_index = 0U;
        size_t value_start = 0U;
        size_t value_end = 0U;
        size_t next_index = index;

        if (!placeholder_scan_update_assignment_equal_index(scan, index, &equal_index)) {
            break;
        }
        value_start = equal_index + 1U;
        if (value_start >= scan->token_count || !placeholder_scan_update_assignment_value_end(
                                                    scan,
                                                    value_start,
                                                    &value_end,
                                                    &next_index
                                                )) {
            break;
        }
        if (placeholder_scan_update_assignment_value_can_retry_as_followup_row_scalar(
                scan,
                value_start,
                value_end,
                allow_concat_operator
            )) {
            enum mylite_sql_parse_status status =
                use_literal_placeholder
                    ? append_row_arithmetic_predicate_value_retry(
                          scan,
                          value_start,
                          value_end,
                          retries,
                          inout_can_retry
                      )
                    : append_row_scalar_predicate_subject_retry(
                          scan,
                          value_start,
                          value_end,
                          retries,
                          inout_can_retry
                      );

            if (status != MYLITE_SQL_PARSE_OK) {
                return status;
            }
        }
        if (next_index <= index) {
            break;
        }
        index = next_index;
    }
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_update_assignment_value_can_retry_as_followup_row_scalar(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    while (mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index])) {
        if (!placeholder_scan_find_matching_right_paren(scan, start_index, &right_paren_index) ||
            right_paren_index != end_index || start_index + 1U >= end_index) {
            return false;
        }
        ++start_index;
        --end_index;
    }
    if (!placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index)) {
        return false;
    }
    if (placeholder_scan_token_is_retryable_update_assignment_row_scalar_function(
            scan,
            start_index
        )) {
        return placeholder_scan_predicate_value_is_direct_row_scalar_expression(
            scan,
            start_index,
            end_index
        );
    }
    return allow_concat_operator &&
           placeholder_scan_row_concat_range_has_operator(scan, start_index, end_index) &&
           placeholder_scan_row_arithmetic_value_has_operator(
               scan,
               start_index,
               end_index,
               allow_concat_operator
           );
}

static bool placeholder_scan_token_is_retryable_update_assignment_row_scalar_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const function_names[] = {
        "LOWER",         "LCASE",
        "UPPER",         "UCASE",
        "LTRIM",         "RTRIM",
        "TRIM",          "QUOTE",
        "REVERSE",       "SOUNDEX",
        "UNHEX",         "JSON_EXTRACT",
        "JSON_UNQUOTE",  "JSON_LENGTH",
        "JSON_KEYS",     "JSON_TYPE",
        "JSON_QUOTE",    "JSON_ARRAY",
        "JSON_OBJECT",   "JSON_SET",
        "JSON_INSERT",   "JSON_REPLACE",
        "JSON_REMOVE",   "JSON_ARRAY_APPEND",
        "JSON_ARRAY_INSERT", "JSON_MERGE",
        "JSON_MERGE_PATCH", "JSON_MERGE_PRESERVE",
        "JSON_SEARCH",
        "GREATEST",
        "LEAST",         "INTERVAL",
        "ISNULL",        "CRC32",
        "FORMAT",        "TRUNCATE",
        "MOD",           "CONCAT",
        "CONCAT_WS",     "FIELD",
        "HEX",           "TO_BASE64",
        "FROM_BASE64",   "LEFT",
        "RIGHT",         "SUBSTRING",
        "SUBSTR",        "MID",
        "LPAD",          "RPAD",
        "REPEAT",        "SPACE",
        "LOCATE",        "INSTR",
        "POSITION",      "REPLACE",
        "INSERT",        "SUBSTRING_INDEX",
        "FIND_IN_SET",   "STRCMP",
        "REGEXP_LIKE",   "REGEXP_INSTR",
        "REGEXP_SUBSTR", "REGEXP_REPLACE",
        "EXPORT_SET",    "MAKE_SET",
        "MD5",           "SHA",
        "SHA1",          "SHA2",
        "COMPRESS",      "UNCOMPRESS",
        "UNCOMPRESSED_LENGTH",
        "RANDOM_BYTES",
        "DATE",          "TIME",
        "DATE_FORMAT",   "TIME_FORMAT",
        "STR_TO_DATE",   "DATEDIFF",
        "TIMEDIFF",      "TIMESTAMP",
        "TIMESTAMPADD",  "TIMESTAMPDIFF",
        "UNIX_TIMESTAMP",
        "SEC_TO_TIME",   "FROM_UNIXTIME",
        "FROM_DAYS",     "MAKEDATE",
        "MAKETIME",      "TIME_TO_SEC",
        "TO_DAYS",       "TO_SECONDS",
        "DAY",           "DAYOFMONTH",
        "DAYOFWEEK",     "DAYOFYEAR",
        "DAYNAME",       "LAST_DAY",
        "HOUR",          "MINUTE",
        "SECOND",        "MICROSECOND",
        "MONTH",         "MONTHNAME",
        "QUARTER",       "WEEK",
        "WEEKDAY",       "WEEKOFYEAR",
        "YEAR",          "YEARWEEK",
        "EXTRACT",
        "VALIDATE_PASSWORD_STRENGTH",
    };

    for (size_t name_index = 0U; name_index < sizeof(function_names) / sizeof(function_names[0]);
         ++name_index) {
        if (placeholder_scan_token_text_equals(scan, index, function_names[name_index])) {
            return true;
        }
    }
    return placeholder_scan_token_is_sys_helper_function(scan, index);
}

static bool placeholder_scan_top_level_update_set_index(
    const struct placeholder_statement_scan *scan,
    size_t *out_set_index
) {
    int paren_depth = 0;

    if (out_set_index != NULL) {
        *out_set_index = 0U;
    }
    if (scan == NULL || out_set_index == NULL || scan->token_count < 4U) {
        return false;
    }
    for (size_t index = 1U; index < scan->token_count; ++index) {
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
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "SET")) {
            *out_set_index = index;
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_top_level_duplicate_update_index(
    const struct placeholder_statement_scan *scan,
    size_t *out_update_index
) {
    enum {
        duplicate_update_min_token_count = 6,
        duplicate_update_tail_token_count = 3,
    };
    int paren_depth = 0;

    if (out_update_index != NULL) {
        *out_update_index = 0U;
    }
    if (scan == NULL || out_update_index == NULL ||
        !placeholder_scan_token_text_equals(scan, 0U, "INSERT") ||
        scan->token_count < duplicate_update_min_token_count) {
        return false;
    }
    for (size_t index = 1U; index + duplicate_update_tail_token_count < scan->token_count;
         ++index) {
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
        if (paren_depth != 0 || !placeholder_scan_token_text_equals(scan, index, "ON")) {
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index + 1U, "DUPLICATE") &&
            placeholder_scan_token_text_equals(scan, index + 2U, "KEY") &&
            placeholder_scan_token_text_equals(scan, index + 3U, "UPDATE")) {
            *out_update_index = index + 3U;
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_update_target_is_single_table(
    const struct placeholder_statement_scan *scan,
    size_t set_index
) {
    int paren_depth = 0;

    if (scan == NULL || set_index <= 1U || set_index > scan->token_count) {
        return false;
    }
    for (size_t index = 1U; index < set_index; ++index) {
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
        if (mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
            placeholder_scan_token_text_equals(scan, index, "JOIN") ||
            placeholder_scan_token_text_equals(scan, index, "STRAIGHT_JOIN")) {
            return false;
        }
    }
    return paren_depth == 0;
}

static bool placeholder_scan_update_assignment_equal_index(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_equal_index
) {
    int paren_depth = 0;

    if (out_equal_index != NULL) {
        *out_equal_index = 0U;
    }
    if (scan == NULL || out_equal_index == NULL || start_index >= scan->token_count) {
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
        if (paren_depth != 0) {
            continue;
        }
        if (mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
            placeholder_scan_token_stops_update_assignment_list(scan, index)) {
            return false;
        }
        if (placeholder_scan_token_is_update_assignment_equal(scan, index)) {
            *out_equal_index = index;
            return index > start_index;
        }
    }
    return false;
}

static bool placeholder_scan_update_assignment_value_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index,
    size_t *out_next_index
) {
    int paren_depth = 0;
    size_t end_index = SIZE_MAX;

    if (out_end_index != NULL) {
        *out_end_index = 0U;
    }
    if (out_next_index != NULL) {
        *out_next_index = start_index;
    }
    if (scan == NULL || out_end_index == NULL || out_next_index == NULL ||
        start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            end_index = index;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth == 0) {
                break;
            }
            --paren_depth;
            end_index = index;
            continue;
        }
        if (paren_depth == 0 &&
            (mylite_sql_parser_token_is_comma(&scan->tokens[index]) ||
             placeholder_scan_token_stops_update_assignment_list(scan, index))) {
            break;
        }
        end_index = index;
    }

    if (end_index == SIZE_MAX || paren_depth != 0) {
        return false;
    }
    *out_end_index = end_index;
    *out_next_index = end_index + 1U;
    if (*out_next_index < scan->token_count &&
        mylite_sql_parser_token_is_comma(&scan->tokens[*out_next_index])) {
        ++*out_next_index;
    }
    return true;
}

static bool placeholder_scan_token_is_update_assignment_equal(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return scan != NULL && index < scan->token_count &&
           mylite_sql_parser_token_is_equal_sign(&scan->tokens[index]);
}

static bool placeholder_scan_token_stops_update_assignment_list(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "WHERE") ||
           placeholder_scan_token_text_equals(scan, index, "ORDER") ||
           placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_token_is_semicolon(&scan->tokens[index]);
}

static enum mylite_sql_parse_status scan_row_bitwise_order_key_retries(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    bool allow_concat_operator
) {
    int paren_depth = 0;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (scan->token_count < row_bitwise_order_by_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "SELECT")) {
        return MYLITE_SQL_PARSE_OK;
    }

    for (size_t index = 0U; index + 1U < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
            mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            paren_depth += mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ? 1 : -1;
            if (paren_depth < 0) {
                return MYLITE_SQL_PARSE_OK;
            }
            continue;
        }
        if (!placeholder_scan_token_starts_top_level_order_by(scan, index, paren_depth)) {
            continue;
        }

        enum mylite_sql_parse_status status =
            scan_row_bitwise_order_items(
                scan,
                index + 2U,
                retries,
                inout_can_retry,
                &index,
                allow_concat_operator
            );

        if (status != MYLITE_SQL_PARSE_OK) {
            return status;
        }
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_row_bitwise_order_items(
    const struct placeholder_statement_scan *scan,
    size_t item_start,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_next_index,
    bool allow_concat_operator
) {
    if (out_next_index != NULL) {
        *out_next_index = item_start;
    }
    while (item_start < scan->token_count) {
        size_t item_end = 0U;
        size_t next_index = item_start;

        if (!placeholder_scan_row_bitwise_order_key_end(scan, item_start, &item_end, &next_index)) {
            break;
        }
        if (placeholder_scan_row_bitwise_order_key_can_retry(
                scan,
                item_start,
                item_end,
                allow_concat_operator
            )) {
            enum mylite_sql_parse_status status = append_row_bitwise_order_key_retry(
                scan,
                item_start,
                item_end,
                retries,
                inout_can_retry
            );

            if (status != MYLITE_SQL_PARSE_OK) {
                return status;
            }
        }
        if (next_index <= item_start) {
            break;
        }
        item_start = next_index;
        if (out_next_index != NULL) {
            *out_next_index = item_start;
        }
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status append_row_bitwise_order_key_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
) {
    static const char placeholder_text[] = "__mylite_row_bitwise_order_key";
    struct placeholder_row_arithmetic_subject_retry retry;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL || start_index > end_index ||
        end_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    retry = (struct placeholder_row_arithmetic_subject_retry){
        .start_index = start_index,
        .end_index = end_index,
        .placeholder_token = {
            .kind = MYLITE_SQL_TOKEN_IDENTIFIER,
            .text = placeholder_text,
            .length = scan->tokens[end_index].offset + scan->tokens[end_index].length -
                      scan->tokens[start_index].offset,
            .offset = scan->tokens[start_index].offset,
        },
    };
    if (!placeholder_row_arithmetic_subject_retries_push(retries, retry)) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    *inout_can_retry = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_row_bitwise_order_key_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index,
    size_t *out_next_index
) {
    int paren_depth = 0;
    size_t end_index = SIZE_MAX;

    if (out_end_index != NULL) {
        *out_end_index = 0U;
    }
    if (out_next_index != NULL) {
        *out_next_index = start_index;
    }
    if (scan == NULL || out_end_index == NULL || out_next_index == NULL ||
        start_index >= scan->token_count) {
        return false;
    }

    for (size_t index = start_index; index < scan->token_count; ++index) {
        bool stop = false;

        if (placeholder_scan_token_updates_row_bitwise_order_depth(
                scan,
                index,
                &paren_depth,
                &end_index,
                &stop
            )) {
            if (stop) {
                break;
            }
            continue;
        }
        if (paren_depth != 0) {
            end_index = index;
            continue;
        }
        if (mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
            return placeholder_scan_finish_row_bitwise_order_key(
                scan,
                (struct placeholder_row_bitwise_order_key_end){
                    .end_index = end_index,
                    .next_index = index + 1U,
                    .mode = PLACEHOLDER_ROW_BITWISE_ORDER_KEY_END_NORMAL,
                    .out_end_index = out_end_index,
                    .out_next_index = out_next_index,
                }
            );
        }
        if (placeholder_scan_token_text_equals(scan, index, "ASC") ||
            placeholder_scan_token_text_equals(scan, index, "DESC")) {
            return placeholder_scan_finish_row_bitwise_order_key(
                scan,
                (struct placeholder_row_bitwise_order_key_end){
                    .end_index = end_index,
                    .next_index = index + 1U,
                    .mode = PLACEHOLDER_ROW_BITWISE_ORDER_KEY_END_SKIP_COMMA,
                    .out_end_index = out_end_index,
                    .out_next_index = out_next_index,
                }
            );
        }
        if (placeholder_scan_token_stops_row_bitwise_order_key(scan, index)) {
            break;
        }
        end_index = index;
    }

    if (end_index == SIZE_MAX || paren_depth != 0) {
        return false;
    }
    *out_end_index = end_index;
    *out_next_index = end_index + 1U;
    return true;
}

static bool placeholder_scan_token_updates_row_bitwise_order_depth(
    const struct placeholder_statement_scan *scan,
    size_t index,
    int *inout_paren_depth,
    size_t *inout_end_index,
    bool *out_stop
) {
    if (out_stop != NULL) {
        *out_stop = false;
    }
    if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
        ++*inout_paren_depth;
        *inout_end_index = index;
        return true;
    }
    if (!mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
        return false;
    }
    if (*inout_paren_depth == 0) {
        if (out_stop != NULL) {
            *out_stop = true;
        }
        return true;
    }
    --*inout_paren_depth;
    *inout_end_index = index;
    return true;
}

static bool placeholder_scan_finish_row_bitwise_order_key(
    const struct placeholder_statement_scan *scan,
    struct placeholder_row_bitwise_order_key_end key_end
) {
    if (key_end.end_index == SIZE_MAX) {
        return false;
    }
    *key_end.out_end_index = key_end.end_index;
    *key_end.out_next_index = key_end.next_index;
    if (key_end.mode == PLACEHOLDER_ROW_BITWISE_ORDER_KEY_END_SKIP_COMMA &&
        *key_end.out_next_index < scan->token_count &&
        mylite_sql_parser_token_is_comma(&scan->tokens[*key_end.out_next_index])) {
        ++*key_end.out_next_index;
    }
    return true;
}

static bool placeholder_scan_token_starts_top_level_order_by(
    const struct placeholder_statement_scan *scan,
    size_t index,
    int paren_depth
) {
    return paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "ORDER") &&
           placeholder_scan_token_text_equals(scan, index + 1U, "BY");
}

static bool placeholder_scan_token_stops_row_bitwise_order_key(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "LIMIT") ||
           placeholder_scan_token_text_equals(scan, index, "OFFSET") ||
           placeholder_scan_token_text_equals(scan, index, "FETCH") ||
           placeholder_scan_token_text_equals(scan, index, "FOR") ||
           placeholder_scan_token_text_equals(scan, index, "LOCK") ||
           placeholder_scan_token_text_equals(scan, index, "UNION") ||
           placeholder_scan_token_text_equals(scan, index, "EXCEPT") ||
           placeholder_scan_token_text_equals(scan, index, "INTERSECT") ||
           placeholder_scan_token_text_equals(scan, index, "PROCEDURE") ||
           placeholder_token_is_semicolon(&scan->tokens[index]);
}

static bool placeholder_scan_row_bitwise_order_key_can_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_is_retryable_row_scalar_context_function(scan, start_index) &&
        placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index) &&
        placeholder_scan_predicate_value_is_direct_row_scalar_expression(
            scan,
            start_index,
            end_index
        )) {
        return true;
    }
    if (placeholder_scan_row_arithmetic_value_has_unsupported_function_call(
            scan,
            start_index,
            end_index
        )) {
        return false;
    }
    if (!placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index)) {
        return false;
    }
    return placeholder_scan_row_bitwise_range_has_operator(
        scan,
        start_index,
        end_index,
        allow_concat_operator
    );
}

static bool placeholder_scan_row_bitwise_range_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    while (start_index < end_index &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index]) &&
           placeholder_scan_find_matching_right_paren(scan, start_index, &right_paren_index) &&
           right_paren_index == end_index) {
        ++start_index;
        --end_index;
    }
    return placeholder_scan_range_has_top_level_bitwise_operator(
        scan,
        start_index,
        end_index,
        allow_concat_operator
    );
}

static bool placeholder_scan_row_concat_range_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    while (start_index < end_index &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index]) &&
           placeholder_scan_find_matching_right_paren(scan, start_index, &right_paren_index) &&
           right_paren_index == end_index) {
        ++start_index;
        --end_index;
    }
    return placeholder_scan_range_has_top_level_concat_operator(scan, start_index, end_index);
}

static bool placeholder_scan_range_has_top_level_bitwise_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index <= end_index; ++index) {
        const struct mylite_sql_token *token = NULL;

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

        token = &scan->tokens[index];
        if (allow_concat_operator && placeholder_scan_token_is_row_concat_operator(scan, index)) {
            return index > start_index && index < end_index;
        }
        if (!placeholder_scan_token_is_row_bitwise_operator(scan, index)) {
            continue;
        }
        if (token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT) {
            return index < end_index;
        }
        if (index > start_index && index < end_index) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_range_has_top_level_concat_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index <= end_index; ++index) {
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
        if (paren_depth == 0 && placeholder_scan_token_is_row_concat_operator(scan, index)) {
            return index > start_index && index < end_index;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_row_arithmetic_predicate_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t left_paren_index,
    size_t *out_right_paren_index,
    bool allow_concat_operator
) {
    size_t right_paren_index = 0U;

    if (out_right_paren_index != NULL) {
        *out_right_paren_index = 0U;
    }
    if (scan == NULL || left_paren_index == 0U || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_token_starts_row_arithmetic_predicate_subject(
            scan,
            left_paren_index - 1U,
            allow_concat_operator
        ) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        !placeholder_scan_parenthesized_row_arithmetic_has_suffix(scan, right_paren_index) ||
        !placeholder_scan_parenthesized_row_arithmetic_has_operator(
            scan,
            left_paren_index + 1U,
            right_paren_index,
            allow_concat_operator
        )) {
        return false;
    }
    if (out_right_paren_index != NULL) {
        *out_right_paren_index = right_paren_index;
    }
    return true;
}

static bool placeholder_scan_can_retry_row_arithmetic_predicate_values(
    const struct placeholder_statement_scan *scan
) {
    if (scan == NULL || scan->token_count == 0U) {
        return false;
    }
    return placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
           placeholder_scan_token_text_equals(scan, 0U, "UPDATE") ||
           placeholder_scan_token_text_equals(scan, 0U, "DELETE");
}

static enum mylite_sql_parse_status scan_row_arithmetic_predicate_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    if (out_end_index != NULL) {
        *out_end_index = start_index;
    }
    if (scan == NULL || retries == NULL || inout_can_retry == NULL || out_end_index == NULL ||
        start_index >= scan->token_count ||
        !placeholder_scan_token_is_in_where_predicate_context(scan, start_index)) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_scan_token_is_comparison_operator(scan, start_index)) {
        return scan_row_arithmetic_comparison_value_retry(
            scan,
            start_index,
            retries,
            inout_can_retry,
            out_end_index,
            allow_concat_operator
        );
    }
    if (placeholder_scan_token_text_equals(scan, start_index, "BETWEEN")) {
        return scan_row_arithmetic_between_value_retries(
            scan,
            start_index,
            retries,
            inout_can_retry,
            out_end_index,
            allow_concat_operator
        );
    }
    if (placeholder_scan_token_text_equals(scan, start_index, "IN")) {
        return scan_row_arithmetic_in_value_retries(
            scan,
            start_index,
            retries,
            inout_can_retry,
            out_end_index,
            allow_concat_operator
        );
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_row_arithmetic_comparison_value_retry(
    const struct placeholder_statement_scan *scan,
    size_t comparison_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t value_start = comparison_index + 1U;
    size_t value_end = 0U;

    if (value_start >= scan->token_count ||
        !placeholder_scan_predicate_value_end(
            scan,
            value_start,
            false,
            &value_end,
            allow_concat_operator
        )) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_scan_predicate_value_can_retry_as_row_scalar(
            scan,
            value_start,
            value_end,
            allow_concat_operator
        )) {
        enum mylite_sql_parse_status status = append_row_arithmetic_predicate_value_retry(
            scan,
            value_start,
            value_end,
            retries,
            inout_can_retry
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            return status;
        }
        *out_end_index = value_end;
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_row_arithmetic_between_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t between_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t lower_start = between_index + 1U;
    size_t and_index = 0U;
    size_t upper_start = 0U;
    size_t upper_end = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (lower_start >= scan->token_count ||
        !placeholder_scan_between_lower_end(scan, lower_start, &and_index) ||
        and_index <= lower_start || and_index + 1U >= scan->token_count ||
        !placeholder_scan_predicate_value_end(
            scan,
            and_index + 1U,
            false,
            &upper_end,
            allow_concat_operator
        )) {
        return MYLITE_SQL_PARSE_OK;
    }
    upper_start = and_index + 1U;

    if (placeholder_scan_predicate_value_can_retry_as_row_scalar(
            scan,
            lower_start,
            and_index - 1U,
            allow_concat_operator
        )) {
        status = append_row_arithmetic_predicate_value_retry(
            scan,
            lower_start,
            and_index - 1U,
            retries,
            inout_can_retry
        );
    }
    if (status == MYLITE_SQL_PARSE_OK &&
        placeholder_scan_predicate_value_can_retry_as_row_scalar(
            scan,
            upper_start,
            upper_end,
            allow_concat_operator
        )) {
        status = append_row_arithmetic_predicate_value_retry(
            scan,
            upper_start,
            upper_end,
            retries,
            inout_can_retry
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        *out_end_index = upper_end;
    }
    return status;
}

static enum mylite_sql_parse_status scan_row_arithmetic_in_value_retries(
    const struct placeholder_statement_scan *scan,
    size_t in_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t left_paren_index = in_index + 1U;
    size_t right_paren_index = 0U;
    size_t value_start = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        placeholder_scan_in_list_is_subquery(scan, left_paren_index + 1U, right_paren_index - 1U)) {
        return MYLITE_SQL_PARSE_OK;
    }

    value_start = left_paren_index + 1U;
    while (status == MYLITE_SQL_PARSE_OK && value_start < right_paren_index) {
        size_t value_end = 0U;

        if (!placeholder_scan_predicate_value_end(
                scan,
                value_start,
                true,
                &value_end,
                allow_concat_operator
            ) ||
            value_end >= right_paren_index) {
            value_end = right_paren_index - 1U;
        }
        if (value_end >= value_start) {
            bool can_retry_value = placeholder_scan_predicate_value_can_retry_as_row_scalar(
                scan,
                value_start,
                value_end,
                allow_concat_operator
            );
            if (can_retry_value) {
                status = append_row_arithmetic_predicate_value_retry(
                    scan,
                    value_start,
                    value_end,
                    retries,
                    inout_can_retry
                );
            }
        }
        if (value_end + 1U >= right_paren_index ||
            !mylite_sql_parser_token_is_comma(&scan->tokens[value_end + 1U])) {
            break;
        }
        value_start = value_end + 2U;
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        *out_end_index = right_paren_index;
    }
    return status;
}

static bool placeholder_scan_literal_left_string_between_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t between_index,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t lower_start = between_index + 1U;
    size_t and_index = 0U;
    size_t upper_start = 0U;
    size_t upper_end = 0U;

    if (out_end_index != NULL) {
        *out_end_index = between_index;
    }
    if (scan == NULL || out_end_index == NULL || lower_start >= scan->token_count ||
        !placeholder_scan_between_lower_end(scan, lower_start, &and_index) ||
        and_index <= lower_start || and_index + 1U >= scan->token_count ||
        !placeholder_scan_predicate_value_end(
            scan,
            and_index + 1U,
            false,
            &upper_end,
            allow_concat_operator
        )) {
        return false;
    }
    upper_start = and_index + 1U;
    *out_end_index = upper_end;
    return placeholder_scan_predicate_value_range_has_descriptor_context(
               scan,
               lower_start,
               and_index - 1U
           ) ||
           placeholder_scan_predicate_value_range_has_descriptor_context(
               scan,
               upper_start,
               upper_end
           );
}

static bool placeholder_scan_literal_left_string_in_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t in_index,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    size_t left_paren_index = in_index + 1U;
    size_t right_paren_index = 0U;
    size_t value_start = 0U;
    bool found = false;

    if (out_end_index != NULL) {
        *out_end_index = in_index;
    }
    if (scan == NULL || out_end_index == NULL || left_paren_index >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[left_paren_index]) ||
        !placeholder_scan_find_matching_right_paren(scan, left_paren_index, &right_paren_index) ||
        right_paren_index <= left_paren_index + 1U ||
        placeholder_scan_in_list_is_subquery(scan, left_paren_index + 1U, right_paren_index - 1U)) {
        return false;
    }

    value_start = left_paren_index + 1U;
    while (value_start < right_paren_index) {
        size_t value_end = 0U;

        if (!placeholder_scan_predicate_value_end(
                scan,
                value_start,
                true,
                &value_end,
                allow_concat_operator
            ) ||
            value_end >= right_paren_index) {
            value_end = right_paren_index - 1U;
        }
        if (value_end >= value_start &&
            placeholder_scan_predicate_value_range_has_descriptor_context(
                scan,
                value_start,
                value_end
            )) {
            found = true;
        }
        if (value_end + 1U >= right_paren_index ||
            !mylite_sql_parser_token_is_comma(&scan->tokens[value_end + 1U])) {
            break;
        }
        value_start = value_end + 2U;
    }
    *out_end_index = right_paren_index;
    return found;
}

static bool placeholder_scan_predicate_value_range_has_descriptor_context(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    return placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index);
}

static bool placeholder_scan_token_is_in_where_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    while (index > 0U) {
        --index;
        if (placeholder_scan_token_text_equals(scan, index, "WHERE")) {
            return true;
        }
        if (placeholder_scan_token_starts_predicate_clause(scan, index) ||
            placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_predicate_value_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    bool stop_at_comma,
    size_t *out_end_index,
    bool allow_concat_operator
) {
    int paren_depth = 0;
    size_t end_index = SIZE_MAX;

    if (out_end_index != NULL) {
        *out_end_index = 0U;
    }
    if (scan == NULL || out_end_index == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            end_index = index;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            if (paren_depth == 0) {
                break;
            }
            --paren_depth;
            end_index = index;
            continue;
        }
        if (paren_depth != 0) {
            end_index = index;
            continue;
        }
        if (placeholder_scan_token_stops_predicate_value(
                scan,
                index,
                stop_at_comma,
                allow_concat_operator
            )) {
            break;
        }
        end_index = index;
    }

    if (end_index == SIZE_MAX || paren_depth != 0) {
        return false;
    }
    *out_end_index = end_index;
    return true;
}

static bool placeholder_scan_between_lower_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_and_index
) {
    int paren_depth = 0;

    if (out_and_index != NULL) {
        *out_and_index = 0U;
    }
    if (scan == NULL || out_and_index == NULL || start_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < scan->token_count; ++index) {
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
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "AND")) {
            *out_and_index = index;
            return index > start_index;
        }
    }
    return false;
}

static bool placeholder_scan_row_arithmetic_value_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_row_arithmetic_value_has_unsupported_function_call(
            scan,
            start_index,
            end_index
        )) {
        return false;
    }
    if (!placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index)) {
        return false;
    }
    if (placeholder_scan_row_arithmetic_value_has_operator_in_range(
            scan,
            start_index,
            end_index,
            allow_concat_operator
        )) {
        return true;
    }
    return mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index]) &&
           placeholder_scan_find_matching_right_paren(scan, start_index, &right_paren_index) &&
           right_paren_index == end_index &&
           placeholder_scan_row_arithmetic_value_has_operator_in_range(
               scan,
               start_index + 1U,
               end_index - 1U,
               allow_concat_operator
           );
}

static bool placeholder_scan_predicate_value_can_retry_as_row_scalar(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    if (placeholder_scan_row_arithmetic_value_has_operator(
            scan,
            start_index,
            end_index,
            allow_concat_operator
        )) {
        return true;
    }
    if (!placeholder_scan_row_arithmetic_value_has_row_operand(scan, start_index, end_index)) {
        return false;
    }
    return placeholder_scan_predicate_value_is_direct_row_scalar_expression(
        scan,
        start_index,
        end_index
    );
}

static bool placeholder_scan_predicate_value_is_direct_row_scalar_expression(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    size_t right_paren_index = 0U;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    while (mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index])) {
        if (!placeholder_scan_find_matching_right_paren(scan, start_index, &right_paren_index) ||
            right_paren_index != end_index || start_index + 1U >= end_index) {
            return false;
        }
        ++start_index;
        --end_index;
    }
    if (placeholder_scan_predicate_value_is_bare_row_operand(scan, start_index, end_index)) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, start_index, "CASE")) {
        return placeholder_scan_token_text_equals(scan, end_index, "END");
    }
    return placeholder_scan_token_is_supported_row_scalar_value_function(scan, start_index) &&
           start_index + 1U < scan->token_count &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[start_index + 1U]) &&
           placeholder_scan_find_matching_right_paren(scan, start_index + 1U, &right_paren_index) &&
           right_paren_index == end_index;
}

static bool placeholder_scan_predicate_value_is_bare_row_operand(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    return start_index == end_index &&
           placeholder_scan_token_is_row_arithmetic_value_identifier_operand(scan, start_index);
}

static bool placeholder_scan_row_arithmetic_value_has_row_operand(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index <= end_index; ++index) {
        if (placeholder_scan_token_is_row_arithmetic_value_identifier_operand(scan, index)) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_token_is_row_arithmetic_value_identifier_operand(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_EOF;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }

    kind = scan->tokens[index].kind;
    if (kind != MYLITE_SQL_TOKEN_IDENTIFIER && kind != MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return false;
    }
    return index + 1U >= scan->token_count ||
           !mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]);
}

static bool placeholder_scan_row_arithmetic_value_has_operator_in_range(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index <= end_index; ++index) {
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
        if (placeholder_scan_row_arithmetic_value_has_operator_at(
                scan,
                start_index,
                end_index,
                index,
                allow_concat_operator
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_row_arithmetic_value_has_operator_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    size_t index,
    bool allow_concat_operator
) {
    if (placeholder_scan_token_starts_mod_function(scan, index)) {
        return true;
    }
    if (placeholder_scan_token_is_row_bitwise_operator(scan, index)) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT) {
            return index < end_index;
        }
        return index > start_index && index < end_index;
    }
    if (allow_concat_operator && placeholder_scan_token_is_row_concat_operator(scan, index)) {
        return index > start_index && index < end_index;
    }
    return index > start_index && index < end_index &&
           placeholder_scan_token_is_row_arithmetic_operator(scan, index);
}

static bool placeholder_scan_row_arithmetic_value_has_unsupported_function_call(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    int paren_depth = 0;

    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return true;
    }
    for (size_t index = start_index; index <= end_index; ++index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return true;
            }
            continue;
        }
        if (placeholder_scan_token_is_predicate_value_function_name(scan, index) &&
            index + 1U <= end_index &&
            mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
            !placeholder_scan_token_is_supported_row_arithmetic_value_function(scan, index)) {
            return true;
        }
    }
    return paren_depth != 0;
}

static bool placeholder_scan_token_is_predicate_value_function_name(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_EOF;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }

    kind = scan->tokens[index].kind;
    return kind == MYLITE_SQL_TOKEN_IDENTIFIER || kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER ||
           kind == MYLITE_SQL_TOKEN_KEYWORD;
}

static bool placeholder_scan_token_is_supported_row_arithmetic_value_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "ABS") ||
           placeholder_scan_token_text_equals(scan, index, "SIGN") ||
           placeholder_scan_token_text_equals(scan, index, "CEIL") ||
           placeholder_scan_token_text_equals(scan, index, "CEILING") ||
           placeholder_scan_token_text_equals(scan, index, "FLOOR") ||
           placeholder_scan_token_text_equals(scan, index, "ROUND") ||
           placeholder_scan_token_text_equals(scan, index, "SQRT") ||
           placeholder_scan_token_text_equals(scan, index, "DEGREES") ||
           placeholder_scan_token_text_equals(scan, index, "RADIANS") ||
           placeholder_scan_token_text_equals(scan, index, "ACOS") ||
           placeholder_scan_token_text_equals(scan, index, "ASIN") ||
           placeholder_scan_token_text_equals(scan, index, "ATAN") ||
           placeholder_scan_token_text_equals(scan, index, "ATAN2") ||
           placeholder_scan_token_text_equals(scan, index, "SIN") ||
           placeholder_scan_token_text_equals(scan, index, "COS") ||
           placeholder_scan_token_text_equals(scan, index, "TAN") ||
           placeholder_scan_token_text_equals(scan, index, "COT") ||
           placeholder_scan_token_text_equals(scan, index, "EXP") ||
           placeholder_scan_token_text_equals(scan, index, "LN") ||
           placeholder_scan_token_text_equals(scan, index, "LOG") ||
           placeholder_scan_token_text_equals(scan, index, "LOG10") ||
           placeholder_scan_token_text_equals(scan, index, "LOG2") ||
           placeholder_scan_token_text_equals(scan, index, "POW") ||
           placeholder_scan_token_text_equals(scan, index, "POWER") ||
           placeholder_scan_token_text_equals(scan, index, "BIT_COUNT") ||
           placeholder_scan_token_text_equals(scan, index, "LENGTH") ||
           placeholder_scan_token_text_equals(scan, index, "CHAR_LENGTH") ||
           placeholder_scan_token_text_equals(scan, index, "CHARACTER_LENGTH") ||
           placeholder_scan_token_text_equals(scan, index, "BIT_LENGTH") ||
           placeholder_scan_token_text_equals(scan, index, "OCTET_LENGTH") ||
           placeholder_scan_token_text_equals(scan, index, "MOD");
}

static bool placeholder_scan_token_is_supported_row_scalar_value_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    static const char *const value_functions[] = {
        "CAST",
        "CONVERT",
        "IF",
        "IFNULL",
        "NULLIF",
        "ISNULL",
        "COALESCE",
        "CONCAT",
        "CONCAT_WS",
        "FIELD",
        "GREATEST",
        "LEAST",
        "INTERVAL",
        "CRC32",
        "FORMAT",
        "TRUNCATE",
        "PI",
        "HEX",
        "UNHEX",
        "TO_BASE64",
        "FROM_BASE64",
        "MD5",
        "SHA",
        "SHA1",
        "SHA2",
        "COMPRESS",
        "UNCOMPRESS",
        "UNCOMPRESSED_LENGTH",
        "RANDOM_BYTES",
        "INET_ATON",
        "INET_NTOA",
        "BIN",
        "OCT",
        "CONV",
        "LOWER",
        "LCASE",
        "UPPER",
        "UCASE",
        "LTRIM",
        "RTRIM",
        "TRIM",
        "LEFT",
        "RIGHT",
        "SUBSTRING",
        "SUBSTR",
        "MID",
        "LPAD",
        "RPAD",
        "REPEAT",
        "SPACE",
        "LOCATE",
        "INSTR",
        "POSITION",
        "REPLACE",
        "INSERT",
        "REVERSE",
        "SOUNDEX",
        "QUOTE",
        "SUBSTRING_INDEX",
        "FIND_IN_SET",
        "STRCMP",
        "REGEXP_LIKE",
        "REGEXP_INSTR",
        "REGEXP_SUBSTR",
        "REGEXP_REPLACE",
        "JSON_VALID",
        "JSON_CONTAINS",
        "JSON_CONTAINS_PATH",
        "JSON_EXTRACT",
        "JSON_UNQUOTE",
        "JSON_LENGTH",
        "JSON_KEYS",
        "JSON_TYPE",
        "JSON_QUOTE",
        "JSON_ARRAY",
        "JSON_OBJECT",
        "JSON_SET",
        "JSON_INSERT",
        "JSON_REPLACE",
        "JSON_REMOVE",
        "JSON_ARRAY_APPEND",
        "JSON_ARRAY_INSERT",
        "JSON_MERGE",
        "JSON_MERGE_PATCH",
        "JSON_MERGE_PRESERVE",
        "JSON_SEARCH",
        "DATE",
        "TIME",
        "EXTRACT",
        "DATEDIFF",
        "TIMEDIFF",
        "TIMESTAMPDIFF",
        "TIMESTAMPADD",
        "TIMESTAMP",
        "UNIX_TIMESTAMP",
        "SEC_TO_TIME",
        "FROM_UNIXTIME",
        "FROM_DAYS",
        "MAKEDATE",
        "MAKETIME",
        "TIME_TO_SEC",
        "TO_DAYS",
        "TO_SECONDS",
        "DAY",
        "DAYOFMONTH",
        "DAYOFWEEK",
        "DAYOFYEAR",
        "DAYNAME",
        "LAST_DAY",
        "HOUR",
        "MINUTE",
        "SECOND",
        "MICROSECOND",
        "WEEK",
        "WEEKDAY",
        "WEEKOFYEAR",
        "MONTH",
        "MONTHNAME",
        "QUARTER",
        "YEAR",
        "YEARWEEK",
        "DATE_FORMAT",
        "TIME_FORMAT",
        "STR_TO_DATE",
        "GET_FORMAT",
        "PERIOD_ADD",
        "PERIOD_DIFF",
        "CONVERT_TZ",
        "CHARSET",
        "COLLATION",
        "COERCIBILITY",
        "ROW_COUNT",
        "FOUND_ROWS",
        "LAST_INSERT_ID",
        "UUID",
        "IS_UUID",
        "UUID_TO_BIN",
        "BIN_TO_UUID",
        "RAND",
        "EXPORT_SET",
        "MAKE_SET",
        "VALIDATE_PASSWORD_STRENGTH",
    };

    if (placeholder_scan_token_is_supported_row_arithmetic_value_function(scan, index)) {
        return true;
    }
    if (placeholder_scan_token_is_sys_helper_function(scan, index)) {
        return true;
    }
    return placeholder_scan_token_text_equals_any(
        scan,
        index,
        value_functions,
        sizeof(value_functions) / sizeof(value_functions[0])
    );
}

static bool placeholder_scan_token_stops_predicate_value(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool stop_at_comma,
    bool allow_concat_operator
) {
    if (stop_at_comma && index < scan->token_count &&
        mylite_sql_parser_token_is_comma(&scan->tokens[index])) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "&&") ||
        placeholder_scan_token_text_equals(scan, index, "OR") ||
        (!allow_concat_operator && placeholder_scan_token_text_equals(scan, index, "||")) ||
        placeholder_scan_token_text_equals(scan, index, "XOR")) {
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "AND")) {
        return true;
    }
    return placeholder_scan_token_stops_predicate_clause_search(scan, index);
}

static bool placeholder_scan_in_list_is_subquery(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index
) {
    if (scan == NULL || start_index > end_index || end_index >= scan->token_count) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, start_index, "SELECT") ||
           placeholder_scan_token_text_equals(scan, start_index, "VALUES") ||
           placeholder_scan_token_text_equals(scan, start_index, "TABLE");
}

static enum mylite_sql_parse_status append_row_arithmetic_predicate_value_retry(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct placeholder_row_arithmetic_subject_retries *retries,
    bool *inout_can_retry
) {
    static const char placeholder_text[] = "0";
    struct placeholder_row_arithmetic_subject_retry retry;

    if (scan == NULL || retries == NULL || inout_can_retry == NULL || start_index > end_index ||
        end_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    retry = (struct placeholder_row_arithmetic_subject_retry){
        .start_index = start_index,
        .end_index = end_index,
        .placeholder_token = {
            .kind = MYLITE_SQL_TOKEN_INTEGER,
            .text = placeholder_text,
            .length = scan->tokens[end_index].offset + scan->tokens[end_index].length -
                      scan->tokens[start_index].offset,
            .offset = scan->tokens[start_index].offset,
        },
    };
    if (!placeholder_row_arithmetic_subject_retries_push(retries, retry)) {
        placeholder_row_arithmetic_subject_retries_deinit(retries);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    *inout_can_retry = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool placeholder_scan_token_starts_row_arithmetic_predicate_subject(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
) {
    size_t cursor = index;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    while (cursor > 0U && mylite_sql_parser_token_is_left_paren(&scan->tokens[cursor])) {
        --cursor;
    }
    if (cursor < index) {
        return placeholder_scan_token_starts_predicate_clause(scan, cursor) ||
               placeholder_scan_boolean_connective_is_in_predicate_context(
                   scan,
                   cursor,
                   allow_concat_operator
               );
    }
    return placeholder_scan_token_starts_predicate_clause(scan, index) ||
           placeholder_scan_boolean_connective_is_in_predicate_context(
               scan,
               index,
               allow_concat_operator
           );
}

static bool placeholder_scan_token_continues_row_arithmetic_predicate_subject(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
) {
    return placeholder_scan_token_text_equals(scan, index, "AND") ||
           placeholder_scan_token_text_equals(scan, index, "&&") ||
           placeholder_scan_token_text_equals(scan, index, "OR") ||
           (!allow_concat_operator && placeholder_scan_token_text_equals(scan, index, "||")) ||
           placeholder_scan_token_text_equals(scan, index, "XOR") ||
           placeholder_scan_token_text_equals(scan, index, "NOT");
}

static bool placeholder_scan_boolean_connective_is_in_predicate_context(
    const struct placeholder_statement_scan *scan,
    size_t index,
    bool allow_concat_operator
) {
    if (!placeholder_scan_token_continues_row_arithmetic_predicate_subject(
            scan,
            index,
            allow_concat_operator
        )) {
        return false;
    }
    while (index > 0U) {
        --index;
        if (placeholder_scan_token_starts_predicate_clause(scan, index)) {
            return true;
        }
        if (placeholder_scan_token_stops_predicate_clause_search(scan, index)) {
            return false;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_row_arithmetic_has_suffix(
    const struct placeholder_statement_scan *scan,
    size_t right_paren_index
) {
    if (scan == NULL || right_paren_index + 1U >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_is_comparison_operator(scan, right_paren_index + 1U) ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IS") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "BETWEEN") ||
        placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "IN")) {
        return true;
    }
    return placeholder_scan_token_text_equals(scan, right_paren_index + 1U, "NOT") &&
           (placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "BETWEEN") ||
            placeholder_scan_token_text_equals(scan, right_paren_index + 2U, "IN"));
}

static bool placeholder_scan_parenthesized_row_arithmetic_has_operator(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    bool allow_concat_operator
) {
    int paren_depth = 0;

    if (scan == NULL || start_index >= end_index || end_index > scan->token_count) {
        return false;
    }
    for (size_t index = start_index; index < end_index; ++index) {
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
        if (placeholder_scan_parenthesized_row_arithmetic_has_operator_at(
                scan,
                start_index,
                end_index,
                index,
                allow_concat_operator
            )) {
            return true;
        }
    }
    return false;
}

static bool placeholder_scan_parenthesized_row_arithmetic_has_operator_at(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    size_t index,
    bool allow_concat_operator
) {
    if (placeholder_scan_token_starts_mod_function(scan, index)) {
        return true;
    }
    if (placeholder_scan_token_is_row_bitwise_operator(scan, index)) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT) {
            return index + 1U < end_index;
        }
        return index > start_index && index + 1U < end_index;
    }
    if (allow_concat_operator && placeholder_scan_token_is_row_concat_operator(scan, index)) {
        return index > start_index && index + 1U < end_index;
    }
    return index > start_index && index + 1U < end_index &&
           placeholder_scan_token_is_row_arithmetic_operator(scan, index);
}

static bool placeholder_scan_token_is_row_bitwise_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           (token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_NOT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_XOR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_LEFT_SHIFT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_RIGHT_SHIFT ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_AND ||
            token->operator_kind == MYLITE_SQL_OPERATOR_BITWISE_OR);
}

static bool placeholder_scan_token_is_row_concat_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           token->operator_kind == MYLITE_SQL_OPERATOR_LOGICAL_OR;
}

static bool placeholder_scan_token_is_row_arithmetic_operator(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    if (placeholder_scan_token_text_equals(scan, index, "DIV") ||
        placeholder_scan_token_text_equals(scan, index, "MOD")) {
        return true;
    }
    return token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
           (token->operator_kind == MYLITE_SQL_OPERATOR_PLUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_MINUS ||
            token->operator_kind == MYLITE_SQL_OPERATOR_STAR ||
            token->operator_kind == MYLITE_SQL_OPERATOR_SLASH ||
            token->operator_kind == MYLITE_SQL_OPERATOR_PERCENT);
}

static bool placeholder_scan_token_starts_mod_function(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t right_paren_index = 0U;

    return scan != NULL && index + 1U < scan->token_count &&
           placeholder_scan_token_text_equals(scan, index, "MOD") &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[index + 1U]) &&
           placeholder_scan_find_matching_right_paren(scan, index + 1U, &right_paren_index) &&
           right_paren_index > index + 2U;
}

static bool placeholder_row_arithmetic_subject_retries_push(
    struct placeholder_row_arithmetic_subject_retries *retries,
    struct placeholder_row_arithmetic_subject_retry retry
) {
    struct placeholder_row_arithmetic_subject_retry *items = NULL;
    size_t capacity = 0U;
    size_t insert_index = 0U;

    if (retries == NULL) {
        return false;
    }
    if (retries->count == retries->capacity) {
        capacity = retries->capacity == 0U ? placeholder_row_arithmetic_retry_initial_capacity
                                           : retries->capacity * 2U;
        if (capacity < retries->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = (struct placeholder_row_arithmetic_subject_retry *)
            realloc(retries->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        retries->items = items;
        retries->capacity = capacity;
    }

    while (insert_index < retries->count &&
           retries->items[insert_index].start_index < retry.start_index) {
        ++insert_index;
    }
    if (insert_index < retries->count) {
        memmove(
            &retries->items[insert_index + 1U],
            &retries->items[insert_index],
            (retries->count - insert_index) * sizeof(*retries->items)
        );
    }
    retries->items[insert_index] = retry;
    ++retries->count;
    return true;
}

static void placeholder_row_arithmetic_subject_retries_deinit(
    struct placeholder_row_arithmetic_subject_retries *retries
) {
    if (retries == NULL) {
        return;
    }
    free(retries->items);
    *retries = (struct placeholder_row_arithmetic_subject_retries){0};
}

static enum mylite_sql_parse_status parse_parenthesized_row_arithmetic_predicate_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    status = validate_parenthesized_row_arithmetic_retries(scan, retries);
    if (status != MYLITE_SQL_PARSE_OK || out_result == NULL) {
        return status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_MISUSE : status;
    }

    mylite_sql_parser_reset_parse_result(out_result);

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    status = feed_parenthesized_row_arithmetic_retry_statement_tokens(
        config,
        parser,
        &state,
        &token_history,
        &previous_token_was_dot,
        scan,
        retries
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token,
            0
        );
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (out_result->status != MYLITE_SQL_PARSE_OK) {
        return out_result->status;
    }

    status = replace_parenthesized_row_arithmetic_retry_subjects(config, scan, retries, out_result);
    if (status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }

    return out_result->status;
}

static enum mylite_sql_parse_status validate_parenthesized_row_arithmetic_retries(
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries
) {
    if (scan == NULL || scan->tokens == NULL || retries == NULL || retries->items == NULL ||
        retries->count == 0U) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    for (size_t index = 0U; index < retries->count; ++index) {
        const struct placeholder_row_arithmetic_subject_retry *retry = &retries->items[index];

        if (retry->start_index >= scan->token_count || retry->end_index >= scan->token_count ||
            retry->start_index > retry->end_index ||
            (index != 0U && retry->start_index <= retries->items[index - 1U].end_index)) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
    }
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status feed_parenthesized_row_arithmetic_retry_statement_tokens(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries
) {
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t retry_index = 0U;

    for (size_t index = 0U; status == MYLITE_SQL_PARSE_OK && index < scan->token_count; ++index) {
        const struct placeholder_row_arithmetic_subject_retry *retry =
            retry_index < retries->count ? &retries->items[retry_index] : NULL;

        if (retry != NULL && index == retry->start_index) {
            int parser_token = retry->placeholder_token.kind == MYLITE_SQL_TOKEN_INTEGER
                                   ? MYLITE_SQL_PARSE_INTEGER
                                   : MYLITE_SQL_PARSE_IDENTIFIER;

            status = feed_token_with_parser_token_override(
                config,
                parser,
                state,
                history,
                previous_token_was_dot,
                &retry->placeholder_token,
                parser_token
            );
            index = retry->end_index;
            ++retry_index;
            continue;
        }
        status = feed_parenthesized_row_arithmetic_retry_token(
            config,
            parser,
            state,
            history,
            previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    return status;
}

static enum mylite_sql_parse_status feed_parenthesized_row_arithmetic_retry_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
) {
    return feed_token_with_parser_token_override(
        config,
        parser,
        state,
        history,
        previous_token_was_dot,
        token,
        0
    );
}

static enum mylite_sql_parse_status replace_parenthesized_row_arithmetic_retry_subjects(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retries *retries,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    for (size_t index = 0U; status == MYLITE_SQL_PARSE_OK && index < retries->count; ++index) {
        status = replace_parenthesized_row_arithmetic_retry_subject(
            config,
            scan,
            &retries->items[index],
            out_result
        );
    }
    return status;
}

static enum mylite_sql_parse_status replace_parenthesized_row_arithmetic_retry_subject(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    const struct placeholder_row_arithmetic_subject_retry *retry,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_ast_node *placeholder = NULL;
    struct mylite_sql_ast_node *expression = NULL;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    status = find_row_arithmetic_placeholder_node(
        out_result->root,
        &retry->placeholder_token,
        &placeholder
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        return status;
    }
    if (placeholder == NULL) {
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    status = parse_row_arithmetic_subject_expression_tokens(
        config,
        scan,
        retry->start_index,
        retry->end_index,
        out_result,
        &expression
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        return status;
    }
    replace_row_arithmetic_placeholder_node(placeholder, expression);
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status parse_row_arithmetic_subject_expression_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t end_index,
    struct mylite_sql_parse_result *out_result,
    struct mylite_sql_ast_node **out_expression
) {
    static const char do_text[] = "DO";
    static const char row_text[] = "ROW";
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    bool inject_parenthesized_row_constructor = false;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_ast_node *expression_list = NULL;
    struct mylite_sql_ast_node *expression = NULL;
    struct mylite_sql_parse_result expression_result = {0};
    struct mylite_sql_token do_token = {
        .kind = MYLITE_SQL_TOKEN_KEYWORD,
        .text = do_text,
        .length = sizeof(do_text) - 1U,
        .offset = scan == NULL || start_index >= scan->token_count
                      ? 0U
                      : scan->tokens[start_index].offset,
    };
    struct mylite_sql_token row_token = {
        .kind = MYLITE_SQL_TOKEN_KEYWORD,
        .text = row_text,
        .length = sizeof(row_text) - 1U,
        .offset = scan == NULL || start_index >= scan->token_count
                      ? 0U
                      : scan->tokens[start_index].offset,
    };
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (scan == NULL || scan->tokens == NULL || out_result == NULL || out_expression == NULL ||
        start_index > end_index || end_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_expression = NULL;

    state = (struct mylite_sql_parser_state){
        .result = &expression_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        return MYLITE_SQL_PARSE_NOMEM;
    }
    inject_parenthesized_row_constructor =
        placeholder_scan_row_tuple_starts_at(scan, start_index, NULL);

    status = feed_token_with_parser_token_override(
        config,
        parser,
        &state,
        &token_history,
        &previous_token_was_dot,
        &do_token,
        MYLITE_SQL_PARSE_DO
    );
    if (status == MYLITE_SQL_PARSE_OK && inject_parenthesized_row_constructor) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &row_token,
            MYLITE_SQL_PARSE_ROW
        );
    }
    for (size_t index = start_index; status == MYLITE_SQL_PARSE_OK && index <= end_index; ++index) {
        status = feed_row_arithmetic_subject_expression_token(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index]
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token,
            0
        );
    }
    mylite_sql_lemonFree(parser, free);

    if (expression_result.status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        expression_result.status = status;
    }
    if (expression_result.status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        expression_result.status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (expression_result.status == MYLITE_SQL_PARSE_OK) {
        statement = mylite_sql_parser_child_at(expression_result.root, 0U);
        expression_list = mylite_sql_parser_child_at(statement, 0U);
        expression = mylite_sql_parser_child_at(expression_list, 0U);
        if (expression != NULL) {
            *out_expression =
                clone_row_arithmetic_retry_expression_tree(&out_result->ast, expression);
            if (*out_expression == NULL) {
                expression_result.status = MYLITE_SQL_PARSE_NOMEM;
            }
        }
    }

    status = expression_result.status;
    mylite_sql_parse_result_deinit(&expression_result);
    if (status != MYLITE_SQL_PARSE_OK) {
        return status;
    }
    return *out_expression == NULL ? MYLITE_SQL_PARSE_SYNTAX_ERROR : MYLITE_SQL_PARSE_OK;
}

static struct mylite_sql_ast_node *clone_row_arithmetic_retry_expression_tree(
    struct mylite_sql_ast *ast,
    const struct mylite_sql_ast_node *node
) {
    struct row_arithmetic_retry_clone_stack stack = {0};
    struct mylite_sql_ast_node *clone = NULL;

    if (ast == NULL || node == NULL) {
        return NULL;
    }

    clone = clone_row_arithmetic_retry_expression_node(ast, node);
    if (clone == NULL) {
        return NULL;
    }

    if (node->first_child != NULL && !row_arithmetic_retry_clone_stack_push(&stack, node, clone)) {
        return NULL;
    }

    while (stack.count != 0U) {
        struct row_arithmetic_retry_clone_work_item item = stack.items[--stack.count];

        if (!clone_row_arithmetic_retry_expression_children(ast, &stack, &item)) {
            row_arithmetic_retry_clone_stack_deinit(&stack);
            return NULL;
        }
    }

    row_arithmetic_retry_clone_stack_deinit(&stack);
    return clone;
}

static struct mylite_sql_ast_node *clone_row_arithmetic_retry_expression_node(
    struct mylite_sql_ast *ast,
    const struct mylite_sql_ast_node *node
) {
    struct mylite_sql_ast_node *clone = NULL;

    if (ast == NULL || node == NULL) {
        return NULL;
    }

    clone = mylite_sql_ast_new_node(ast, node->kind, node->span);
    if (clone != NULL) {
        clone->payload = node->payload;
    }
    return clone;
}

static bool clone_row_arithmetic_retry_expression_children(
    struct mylite_sql_ast *ast,
    struct row_arithmetic_retry_clone_stack *stack,
    const struct row_arithmetic_retry_clone_work_item *item
) {
    for (const struct mylite_sql_ast_node *child = item->source->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_sql_ast_node *child_clone =
            clone_row_arithmetic_retry_expression_node(ast, child);

        if (child_clone == NULL) {
            return false;
        }
        mylite_sql_ast_node_append_child(item->clone, child_clone);

        if (child->first_child != NULL &&
            !row_arithmetic_retry_clone_stack_push(stack, child, child_clone)) {
            return false;
        }
    }

    return true;
}

static bool row_arithmetic_retry_clone_stack_push(
    struct row_arithmetic_retry_clone_stack *stack,
    const struct mylite_sql_ast_node *source,
    struct mylite_sql_ast_node *clone
) {
    if (stack == NULL || source == NULL || clone == NULL) {
        return false;
    }
    if (stack->count == stack->capacity && !row_arithmetic_retry_clone_stack_reserve(stack)) {
        return false;
    }
    stack->items[stack->count] =
        (struct row_arithmetic_retry_clone_work_item){.source = source, .clone = clone};
    ++stack->count;
    return true;
}

static bool row_arithmetic_retry_clone_stack_reserve(
    struct row_arithmetic_retry_clone_stack *stack
) {
    struct row_arithmetic_retry_clone_work_item *items = NULL;
    size_t capacity = row_arithmetic_retry_clone_stack_initial_capacity;

    if (stack == NULL) {
        return false;
    }
    if (stack->capacity != 0U) {
        capacity = stack->capacity * 2U;
    }
    if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
        return false;
    }

    items = (struct row_arithmetic_retry_clone_work_item *)
        realloc(stack->items, capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }
    stack->items = items;
    stack->capacity = capacity;
    return true;
}

static void row_arithmetic_retry_clone_stack_deinit(
    struct row_arithmetic_retry_clone_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0U;
    stack->capacity = 0U;
}

static enum mylite_sql_parse_status feed_row_arithmetic_subject_expression_token(
    struct mylite_sql_parse_config config,
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct mylite_sql_token *token
) {
    return feed_token_with_parser_token_override(
        config,
        parser,
        state,
        history,
        previous_token_was_dot,
        token,
        0
    );
}

static enum mylite_sql_parse_status find_row_arithmetic_placeholder_node(
    struct mylite_sql_ast_node *node,
    const struct mylite_sql_token *placeholder_token,
    struct mylite_sql_ast_node **out_node
) {
    struct placeholder_ast_node_stack stack = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_node == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_node = NULL;
    if (node == NULL) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (!placeholder_ast_node_stack_push(&stack, node)) {
        return MYLITE_SQL_PARSE_NOMEM;
    }

    while (stack.count != 0U && *out_node == NULL) {
        struct mylite_sql_ast_node *current = stack.items[--stack.count];

        if (row_arithmetic_placeholder_node_matches(current, placeholder_token)) {
            *out_node = current;
            break;
        }
        for (struct mylite_sql_ast_node *child = current->first_child;
             status == MYLITE_SQL_PARSE_OK && child != NULL;
             child = child->next_sibling) {
            if (!placeholder_ast_node_stack_push(&stack, child)) {
                status = MYLITE_SQL_PARSE_NOMEM;
            }
        }
    }

    placeholder_ast_node_stack_deinit(&stack);
    return status;
}

static bool row_arithmetic_placeholder_node_matches(
    const struct mylite_sql_ast_node *node,
    const struct mylite_sql_token *placeholder_token
) {
    if (node == NULL || placeholder_token == NULL || node->span.text == NULL ||
        placeholder_token->text == NULL || node->span.length != placeholder_token->length ||
        node->span.offset != placeholder_token->offset) {
        return false;
    }
    if (placeholder_token->kind == MYLITE_SQL_TOKEN_IDENTIFIER &&
        node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }
    if (placeholder_token->kind == MYLITE_SQL_TOKEN_INTEGER &&
        (node->kind != MYLITE_SQL_AST_LITERAL ||
         mylite_sql_ast_node_literal_kind(node) != MYLITE_SQL_AST_LITERAL_INTEGER)) {
        return false;
    }
    return node->span.text == placeholder_token->text;
}

static bool placeholder_ast_node_stack_push(
    struct placeholder_ast_node_stack *stack,
    struct mylite_sql_ast_node *node
) {
    struct mylite_sql_ast_node **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return false;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? placeholder_ast_node_stack_initial_capacity
                                         : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items =
            (struct mylite_sql_ast_node **)realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = node;
    ++stack->count;
    return true;
}

static void placeholder_ast_node_stack_deinit(struct placeholder_ast_node_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free((void *)stack->items);
    *stack = (struct placeholder_ast_node_stack){0};
}

static void replace_row_arithmetic_placeholder_node(
    struct mylite_sql_ast_node *placeholder,
    const struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_ast_node *next_sibling = NULL;

    if (placeholder == NULL || expression == NULL) {
        return;
    }
    next_sibling = placeholder->next_sibling;
    *placeholder = *expression;
    placeholder->next_sibling = next_sibling;
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_tableless_select_limit_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *limit_clause = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    struct mylite_sql_parser_state state;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t limit_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_tableless_select_limit(&scan, &limit_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = mylite_sql_parser_parse_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = scan.tokens[limit_index].offset,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (!parsed_select_accepts_tableless_limit(statement)) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    state = (struct mylite_sql_parser_state){
        .result = &prefix_result,
        .modes = config.modes,
        .accepted = true,
    };
    limit_clause = make_select_limit_clause_from_tail(&state, &scan, limit_index);
    if (limit_clause == NULL) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_NOMEM;
    }
    mylite_sql_ast_node_append_child(statement, limit_clause);
    mylite_sql_ast_node_set_span(
        statement,
        mylite_sql_parser_span_join(statement->span, limit_clause->span)
    );
    mylite_sql_ast_node_set_span(prefix_result.root, statement->span);

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_tableless_select_limit(
    const struct placeholder_statement_scan *scan,
    size_t *out_limit_index
) {
    size_t limit_index = 0U;

    if (out_limit_index != NULL) {
        *out_limit_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_token_text_equals(scan, 0U, "SELECT") ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_find_top_level_keyword(scan, "LIMIT", &limit_index) ||
        !tableless_select_limit_tail_is_supported(scan, limit_index)) {
        return false;
    }
    if (out_limit_index != NULL) {
        *out_limit_index = limit_index;
    }
    return true;
}

static bool tableless_select_limit_tail_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t limit_index
) {
    size_t tail_count = 0U;

    if (scan == NULL || limit_index >= scan->token_count) {
        return false;
    }
    tail_count = scan->token_count - limit_index;
    if (tail_count == 2U) {
        return select_limit_tail_token_is_integer(scan, limit_index + 1U);
    }
    if (tail_count != 4U || !select_limit_tail_token_is_integer(scan, limit_index + 1U)) {
        return false;
    }
    return (placeholder_scan_token_text_equals(scan, limit_index + 2U, "OFFSET") &&
            select_limit_tail_token_is_integer(scan, limit_index + 3U)) ||
           (mylite_sql_parser_token_is_comma(&scan->tokens[limit_index + 2U]) &&
            select_limit_tail_token_is_integer(scan, limit_index + 3U));
}

static bool select_limit_tail_token_is_integer(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return scan != NULL && index < scan->token_count &&
           scan->tokens[index].kind == MYLITE_SQL_TOKEN_INTEGER;
}

static bool parsed_select_accepts_tableless_limit(struct mylite_sql_ast_node *statement) {
    struct mylite_sql_ast_node *source = NULL;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }
    source = mylite_sql_parser_child_at(statement, 1U);
    return source == NULL || source->kind == MYLITE_SQL_AST_FROM_DUAL;
}

static struct mylite_sql_ast_node *make_select_limit_clause_from_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t limit_index
) {
    struct mylite_sql_ast_node *first = NULL;
    struct mylite_sql_ast_node *second = NULL;

    if (state == NULL || scan == NULL ||
        !tableless_select_limit_tail_is_supported(scan, limit_index)) {
        return NULL;
    }
    first = mylite_sql_parser_make_literal(
        state,
        scan->tokens[limit_index + 1U],
        MYLITE_SQL_AST_LITERAL_INTEGER
    );
    if (first == NULL) {
        return NULL;
    }
    if (scan->token_count - limit_index == 2U) {
        return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], first, NULL);
    }
    second = mylite_sql_parser_make_literal(
        state,
        scan->tokens[limit_index + 3U],
        MYLITE_SQL_AST_LITERAL_INTEGER
    );
    if (second == NULL) {
        return NULL;
    }
    if (placeholder_scan_token_text_equals(scan, limit_index + 2U, "OFFSET")) {
        return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], first, second);
    }
    return mylite_sql_parser_make_limit_clause(state, scan->tokens[limit_index], second, first);
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_repeated_select_locking_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t prefix_length = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_repeated_select_locking(&scan, &prefix_length)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = mylite_sql_parser_parse_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        mylite_sql_ast_node_select_locking_clause(statement) ==
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        mylite_sql_parse_result_deinit(&prefix_result);
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_ast_node_set_span(
        statement,
        (struct mylite_sql_source_span){
            .text = config.input,
            .length = config.length,
            .offset = 0U,
        }
    );
    mylite_sql_ast_node_set_span(prefix_result.root, statement->span);

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_repeated_select_locking(
    const struct placeholder_statement_scan *scan,
    size_t *out_prefix_length
) {
    int paren_depth = 0;

    if (out_prefix_length != NULL) {
        *out_prefix_length = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_starts_query_statement(scan) ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U)) {
        return false;
    }
    for (size_t index = 0U; index < scan->token_count; ++index) {
        size_t second_index = 0U;

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
        if (paren_depth != 0 || !placeholder_scan_token_starts_select_locking_clause(scan, index)) {
            continue;
        }
        if (scan_select_locking_clause_sequence(scan, index, &second_index)) {
            if (out_prefix_length != NULL) {
                *out_prefix_length = scan->tokens[second_index].offset;
            }
            return true;
        }
    }
    return false;
}

static bool scan_select_locking_clause_sequence(
    const struct placeholder_statement_scan *scan,
    size_t first_index,
    size_t *out_second_index
) {
    size_t index = first_index;
    size_t clause_count = 0U;

    if (out_second_index != NULL) {
        *out_second_index = 0U;
    }
    while (index < scan->token_count) {
        size_t end_index = 0U;

        if (!placeholder_scan_token_starts_select_locking_clause(scan, index) ||
            !scan_select_locking_clause_end(scan, index, &end_index) || end_index <= index) {
            return false;
        }
        ++clause_count;
        if (clause_count == 2U && out_second_index != NULL) {
            *out_second_index = index;
        }
        index = end_index;
    }
    return clause_count > 1U;
}

static bool scan_select_locking_clause_end(
    const struct placeholder_statement_scan *scan,
    size_t start_index,
    size_t *out_end_index
) {
    size_t index = start_index;

    if (scan == NULL || out_end_index == NULL || start_index >= scan->token_count) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "FOR") &&
        (placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE") ||
         placeholder_scan_token_text_equals(scan, index + 1U, "SHARE"))) {
        index += 2U;
        if (placeholder_scan_token_text_equals(scan, index, "OF") &&
            !placeholder_scan_lock_target_list_end(scan, index + 1U, &index)) {
            return false;
        }
        if (placeholder_scan_token_text_equals(scan, index, "NOWAIT")) {
            ++index;
        } else if (
            placeholder_scan_token_text_equals(scan, index, "SKIP") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LOCKED")
        ) {
            index += 2U;
        }
        *out_end_index = index;
        return true;
    }
    if (placeholder_scan_token_text_equals(scan, index, "LOCK") &&
        placeholder_scan_token_text_equals(scan, index + 1U, "IN") &&
        placeholder_scan_token_text_equals(scan, index + 2U, "SHARE") &&
        placeholder_scan_token_text_equals(scan, index + 3U, "MODE")) {
        *out_end_index = index + 4U;
        return true;
    }
    return false;
}

static bool placeholder_scan_token_starts_select_locking_clause(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return (placeholder_scan_token_text_equals(scan, index, "FOR") &&
            (placeholder_scan_token_text_equals(scan, index + 1U, "UPDATE") ||
             placeholder_scan_token_text_equals(scan, index + 1U, "SHARE"))) ||
           (placeholder_scan_token_text_equals(scan, index, "LOCK") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "IN") &&
            placeholder_scan_token_text_equals(scan, index + 2U, "SHARE") &&
            placeholder_scan_token_text_equals(scan, index + 3U, "MODE"));
}

static bool placeholder_scan_lock_target_list_end(
    const struct placeholder_statement_scan *scan,
    size_t target_index,
    size_t *out_end_index
) {
    bool expecting_identifier = true;
    size_t index = target_index;

    if (scan == NULL || out_end_index == NULL || target_index >= scan->token_count) {
        return false;
    }
    for (; index < scan->token_count; ++index) {
        const struct mylite_sql_token *token = &scan->tokens[index];

        if (!expecting_identifier &&
            (placeholder_scan_token_starts_select_locking_clause(scan, index) ||
             placeholder_scan_token_starts_select_lock_wait(scan, index))) {
            *out_end_index = index;
            return true;
        }
        if (expecting_identifier) {
            if (!mylite_sql_parser_token_can_be_select_lock_target_identifier(token)) {
                return false;
            }
            expecting_identifier = false;
            continue;
        }
        if (mylite_sql_parser_token_is_comma(token)) {
            expecting_identifier = true;
            continue;
        }
        if (placeholder_scan_token_text_equals(scan, index, ".")) {
            expecting_identifier = true;
            continue;
        }
        *out_end_index = index;
        return true;
    }
    if (expecting_identifier) {
        return false;
    }
    *out_end_index = index;
    return true;
}

static bool placeholder_scan_token_starts_select_lock_wait(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "NOWAIT") ||
           (placeholder_scan_token_text_equals(scan, index, "SKIP") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "LOCKED"));
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_legacy_create_index_type_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    struct mylite_sql_parse_result retry_result;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;
    size_t type_index = 0U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    if (!scan_can_retry_legacy_create_index_type(&scan, &type_index)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    memset(&retry_result, 0, sizeof(retry_result));
    status = parse_legacy_create_index_type_tokens(config, &scan, type_index, &retry_result);
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_ast_deinit(&result->ast);
        *result = retry_result;
        *out_handled = true;
        free(tokens);
        return status;
    }

    mylite_sql_parse_result_deinit(&retry_result);
    free(tokens);
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_can_retry_legacy_create_index_type(
    const struct placeholder_statement_scan *scan,
    size_t *out_type_index
) {
    enum { legacy_create_index_type_min_token_count = 7U };

    size_t index = 1U;
    size_t after_index_name = 0U;

    if (out_type_index != NULL) {
        *out_type_index = 0U;
    }
    if (scan == NULL || scan->tokens == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < legacy_create_index_type_min_token_count ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, index, "UNIQUE")) {
        ++index;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "INDEX") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        return false;
    }

    after_index_name = index + 2U;
    return scan_legacy_create_index_type_prefix(scan, after_index_name, out_type_index) ||
           scan_legacy_create_index_type_suffix(scan, after_index_name, out_type_index);
}

static bool scan_legacy_create_index_type_prefix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
) {
    if (!placeholder_scan_token_text_equals(scan, after_index_name, "TYPE") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, after_index_name + 1U) ||
        !placeholder_scan_token_text_equals(scan, after_index_name + 2U, "ON")) {
        return false;
    }
    if (out_type_index != NULL) {
        *out_type_index = after_index_name;
    }
    return true;
}

static bool scan_legacy_create_index_type_suffix(
    const struct placeholder_statement_scan *scan,
    size_t after_index_name,
    size_t *out_type_index
) {
    size_t index = after_index_name;
    size_t right_paren_index = 0U;

    if (!placeholder_scan_token_text_equals(scan, index, "ON")) {
        return false;
    }
    ++index;
    if (!placeholder_scan_match_column_name_starts_at(scan, index, &index) ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[index]) ||
        !placeholder_scan_find_matching_right_paren(scan, index, &right_paren_index)) {
        return false;
    }
    index = right_paren_index + 1U;
    if (!placeholder_scan_token_text_equals(scan, index, "TYPE") ||
        !placeholder_scan_token_can_name_loose_identifier(scan, index + 1U)) {
        return false;
    }
    if (out_type_index != NULL) {
        *out_type_index = index;
    }
    return true;
}

static enum mylite_sql_parse_status parse_legacy_create_index_type_tokens(
    struct mylite_sql_parse_config config,
    const struct placeholder_statement_scan *scan,
    size_t type_index,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_parser_token_history token_history = {0};
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_token eof_token = {
        .kind = MYLITE_SQL_TOKEN_EOF,
        .text = config.input == NULL ? NULL : config.input + config.length,
        .offset = config.length,
    };
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL || scan == NULL || scan->tokens == NULL ||
        type_index >= scan->token_count) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    mylite_sql_parser_reset_parse_result(out_result);

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .modes = config.modes,
        .accepted = false,
    };
    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    for (size_t index = 0U; status == MYLITE_SQL_PARSE_OK && index < scan->token_count; ++index) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &scan->tokens[index],
            index == type_index ? MYLITE_SQL_PARSE_USING : 0
        );
    }
    if (status == MYLITE_SQL_PARSE_OK) {
        status = feed_token_with_parser_token_override(
            config,
            parser,
            &state,
            &token_history,
            &previous_token_was_dot,
            &eof_token,
            0
        );
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

enum mylite_sql_parse_status mylite_sql_parser_try_parse_placeholder_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
) {
    struct mylite_sql_token *tokens = NULL;
    struct placeholder_statement_scan scan = {0};
    enum placeholder_statement_kind kind = PLACEHOLDER_STATEMENT_NONE;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;

    status = scan_placeholder_statement_tokens(
        config,
        &tokens,
        &scan.token_count,
        &scan.has_non_trailing_semicolon
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        free(tokens);
        return status;
    }
    scan.tokens = tokens;
    status =
        try_parse_alter_table_algorithm_lock_tail_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }
    status = try_parse_create_table_partition_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }
    status = try_parse_admin_set_residual_statement(config, result, &scan, out_handled);
    if (*out_handled) {
        free(tokens);
        return status;
    }

    kind = classify_placeholder_statement(&scan);
    if (kind == PLACEHOLDER_STATEMENT_NONE ||
        ((kind == PLACEHOLDER_STATEMENT_ADMIN_NOOP || kind == PLACEHOLDER_STATEMENT_UTILITY_NOOP) &&
         scan.has_non_trailing_semicolon)) {
        free(tokens);
        return MYLITE_SQL_PARSE_OK;
    }

    if (kind == PLACEHOLDER_STATEMENT_EXPLAIN) {
        status = finish_explain_placeholder_statement_parse(config, result, &scan);
    } else {
        status = finish_placeholder_statement_parse(config, result, kind);
    }
    *out_handled = true;
    free(tokens);
    return status;
}

static enum mylite_sql_parse_status try_parse_admin_set_residual_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    status = try_parse_plural_table_maintenance_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    status = try_parse_describe_explain_table_filter_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    status = try_parse_show_extended_metadata_statement(config, result, scan, out_handled);
    if (*out_handled) {
        return status;
    }
    return try_parse_set_system_variable_assign_statement(config, result, scan, out_handled);
}

static enum mylite_sql_parse_status try_parse_plural_table_maintenance_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_names = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind;
    size_t index = 1U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        (!placeholder_scan_token_text_equals(scan, 0U, "ANALYZE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "OPTIMIZE"))) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_scan_token_text_equals(scan, index, "NO_WRITE_TO_BINLOG") ||
        placeholder_scan_token_text_equals(scan, index, "LOCAL")) {
        ++index;
    }
    if (!placeholder_scan_token_text_equals(scan, index, "TABLES")) {
        return MYLITE_SQL_PARSE_OK;
    }
    ++index;

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_names = placeholder_parse_table_name_list(&state, scan, &index);
    if (table_names == NULL || index != scan->token_count) {
        result->status = table_names == NULL && result->status != MYLITE_SQL_PARSE_OK
                             ? result->status
                             : MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }

    statement_kind = placeholder_scan_token_text_equals(scan, 0U, "ANALYZE")
                         ? MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT
                         : MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT;
    statement = mylite_sql_parser_make_table_maintenance_statement(
        &state,
        statement_kind,
        scan->tokens[0],
        table_names,
        NULL
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status try_parse_describe_explain_table_filter_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *filter = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    size_t index = 1U;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        (!placeholder_scan_token_text_equals(scan, 0U, "DESCRIBE") &&
         !placeholder_scan_token_text_equals(scan, 0U, "DESC") &&
         !placeholder_scan_token_text_equals(scan, 0U, "EXPLAIN"))) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (!placeholder_scan_token_is_identifier_like(scan, index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_name = placeholder_parse_table_name(&state, scan, &index);
    if (table_name == NULL || index >= scan->token_count) {
        result->status = table_name == NULL && result->status != MYLITE_SQL_PARSE_OK
                             ? result->status
                             : MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }
    if (scan->tokens[index].kind == MYLITE_SQL_TOKEN_STRING) {
        filter = mylite_sql_parser_make_literal(
            &state,
            scan->tokens[index],
            MYLITE_SQL_AST_LITERAL_STRING
        );
    } else if (placeholder_scan_token_is_identifier_like(scan, index)) {
        filter = mylite_sql_parser_make_identifier(&state, scan->tokens[index]);
    } else {
        result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }
    ++index;
    if (filter == NULL || index != scan->token_count) {
        if (filter == NULL) {
            result->status =
                result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        } else {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        }
        *out_handled = true;
        return result->status;
    }

    statement = mylite_sql_parser_make_show_columns_statement(
        &state,
        scan->tokens[0],
        table_name,
        NULL,
        filter
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status try_parse_show_extended_metadata_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *schema_name = NULL;
    struct mylite_sql_ast_node *filter = NULL;
    struct mylite_sql_ast_node *statement = NULL;
    bool full_columns = false;
    bool index_statement = false;
    bool columns_statement = false;
    size_t index = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (!scan_show_extended_metadata_statement(
            scan,
            &full_columns,
            &columns_statement,
            &index_statement,
            &index
        )) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    table_name = placeholder_parse_table_name(&state, scan, &index);
    if (table_name == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_SYNTAX_ERROR : result->status;
        *out_handled = true;
        return result->status;
    }
    status = parse_show_extended_metadata_tail(
        &state,
        scan,
        columns_statement,
        &index,
        &schema_name,
        &filter
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return status;
    }
    if (index != scan->token_count) {
        result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        *out_handled = true;
        return result->status;
    }

    statement = make_show_extended_metadata_statement(
        &state,
        scan,
        full_columns,
        index_statement,
        table_name,
        schema_name,
        filter
    );
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static bool scan_show_extended_metadata_statement(
    const struct placeholder_statement_scan *scan,
    bool *out_full_columns,
    bool *out_columns_statement,
    bool *out_index_statement,
    size_t *out_index
) {
    enum {
        show_extended_min_token_count = 5,
    };

    bool full_columns = false;
    bool columns_statement = false;
    bool index_statement = false;
    size_t keyword_index = 2U;

    if (out_full_columns == NULL || out_columns_statement == NULL || out_index_statement == NULL ||
        out_index == NULL) {
        return false;
    }
    *out_full_columns = false;
    *out_columns_statement = false;
    *out_index_statement = false;
    *out_index = 0U;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        scan->token_count < show_extended_min_token_count ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SHOW") ||
        !placeholder_scan_token_text_equals(scan, 1U, "EXTENDED")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, keyword_index, "FULL")) {
        full_columns = true;
        ++keyword_index;
    }
    columns_statement = placeholder_scan_token_is_show_columns_keyword(scan, keyword_index);
    index_statement =
        !full_columns && placeholder_scan_token_is_show_index_keyword(scan, keyword_index);
    if ((!columns_statement && !index_statement) ||
        !placeholder_scan_token_is_schema_keyword(scan, keyword_index + 1U)) {
        return false;
    }

    *out_full_columns = full_columns;
    *out_columns_statement = columns_statement;
    *out_index_statement = index_statement;
    *out_index = keyword_index + 2U;
    return true;
}

static enum mylite_sql_parse_status parse_show_extended_metadata_tail(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool columns_statement,
    size_t *index,
    struct mylite_sql_ast_node **out_schema_name,
    struct mylite_sql_ast_node **out_filter
) {
    struct mylite_sql_parse_result *result = state == NULL ? NULL : state->result;

    if (state == NULL || result == NULL || scan == NULL || index == NULL ||
        out_schema_name == NULL || out_filter == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_schema_name = NULL;
    *out_filter = NULL;
    if (placeholder_scan_token_is_schema_keyword(scan, *index)) {
        ++*index;
        if (!placeholder_scan_token_is_identifier_like(scan, *index)) {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
            return result->status;
        }
        *out_schema_name = mylite_sql_parser_make_identifier(state, scan->tokens[*index]);
        if (*out_schema_name == NULL) {
            result->status =
                result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
            return result->status;
        }
        ++*index;
    }
    if (columns_statement) {
        size_t filter_index = *index;
        *out_filter = placeholder_parse_show_like_filter(state, scan, index);
        if (result->status != MYLITE_SQL_PARSE_OK) {
            return result->status;
        }
        if (*out_filter == NULL && placeholder_scan_token_text_equals(scan, filter_index, "LIKE")) {
            result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
            return result->status;
        }
    }
    return result->status;
}

static struct mylite_sql_ast_node *make_show_extended_metadata_statement(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    bool full_columns,
    bool index_statement,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
) {
    if (index_statement) {
        return mylite_sql_parser_make_show_index_statement(
            state,
            scan->tokens[0],
            table_name,
            schema_name,
            NULL
        );
    }
    if (full_columns) {
        return mylite_sql_parser_make_show_full_columns_statement(
            state,
            scan->tokens[0],
            table_name,
            schema_name,
            filter
        );
    }
    return mylite_sql_parser_make_show_columns_statement(
        state,
        scan->tokens[0],
        table_name,
        schema_name,
        filter
    );
}

static enum mylite_sql_parse_status try_parse_set_system_variable_assign_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_ast_node *target = NULL;
    struct mylite_sql_ast_node *value = NULL;
    struct mylite_sql_ast_node *assignment = NULL;
    struct mylite_sql_ast_node *assignment_list = NULL;
    struct mylite_sql_ast_node *statement = NULL;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon || scan->token_count != 4U ||
        !placeholder_scan_parentheses_are_balanced(scan, 0U) ||
        placeholder_scan_statement_tail_is_obviously_incomplete(scan) ||
        !placeholder_scan_token_text_equals(scan, 0U, "SET") ||
        scan->tokens[1].kind != MYLITE_SQL_TOKEN_SYSTEM_VARIABLE ||
        !placeholder_scan_token_is_assignment_operator(scan, 2U) ||
        !placeholder_scan_token_is_set_system_variable_scalar_value(scan, 3U)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (init_scanned_statement_parse(config, result, &state) != MYLITE_SQL_PARSE_OK) {
        *out_handled = true;
        return result->status;
    }
    target = mylite_sql_parser_make_set_system_variable_target(
        &state,
        NULL,
        mylite_sql_parser_make_system_variable(&state, scan->tokens[1])
    );
    if (target == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    value = placeholder_parse_set_system_variable_value(&state, scan, 3U);
    if (value == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    assignment = mylite_sql_parser_make_set_assignment(&state, target, scan->tokens[2], value);
    if (assignment == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    assignment_list = mylite_sql_parser_make_set_assignment_list(&state, assignment);
    if (assignment_list == NULL) {
        result->status =
            result->status == MYLITE_SQL_PARSE_OK ? MYLITE_SQL_PARSE_NOMEM : result->status;
        *out_handled = true;
        return result->status;
    }
    statement = mylite_sql_parser_make_set_statement(&state, scan->tokens[0], assignment_list);
    *out_handled = true;
    return finish_scanned_statement_parse(result, &state, statement);
}

static enum mylite_sql_parse_status init_scanned_statement_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *out_state
) {
    if (result == NULL || out_state == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
    result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&result->ast);
    *out_state = (struct mylite_sql_parser_state){
        .result = result,
        .modes = config.modes,
        .accepted = true,
    };
    return result->status;
}

static enum mylite_sql_parse_status finish_scanned_statement_parse(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_ast_node *script = NULL;

    if (result == NULL || state == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (statement == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }
    script = mylite_sql_parser_make_script_with_statement(state, statement);
    if (script == NULL) {
        result->status = MYLITE_SQL_PARSE_NOMEM;
        return result->status;
    }
    mylite_sql_parser_state_set_root(state, script);
    return result->status;
}

static struct mylite_sql_ast_node *placeholder_parse_table_name(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    struct mylite_sql_ast_node *table_name = NULL;
    struct mylite_sql_ast_node *schema_name = NULL;

    if (state == NULL || scan == NULL || index == NULL ||
        !placeholder_scan_token_is_identifier_like(scan, *index)) {
        return NULL;
    }
    table_name = mylite_sql_parser_make_identifier(state, scan->tokens[*index]);
    ++*index;
    if (placeholder_scan_token_is_dot(scan, *index)) {
        ++*index;
        if (!placeholder_scan_token_is_identifier_like(scan, *index)) {
            return NULL;
        }
        schema_name = table_name;
        table_name = mylite_sql_parser_make_qualified_identifier(
            state,
            schema_name,
            mylite_sql_parser_make_identifier(state, scan->tokens[*index])
        );
        ++*index;
    }
    return table_name;
}

static struct mylite_sql_ast_node *placeholder_parse_table_name_list(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    struct mylite_sql_ast_node *table_name = placeholder_parse_table_name(state, scan, index);
    struct mylite_sql_ast_node *table_names = NULL;

    if (table_name == NULL) {
        return NULL;
    }
    table_names = mylite_sql_parser_make_table_name_list(state, table_name);
    while (index != NULL && *index < scan->token_count) {
        if (!mylite_sql_parser_token_is_comma(&scan->tokens[*index])) {
            return NULL;
        }
        ++*index;
        table_name = placeholder_parse_table_name(state, scan, index);
        if (table_name == NULL) {
            return NULL;
        }
        table_names = mylite_sql_parser_append_table_name(state, table_names, table_name);
    }
    return table_names;
}

static struct mylite_sql_ast_node *placeholder_parse_show_like_filter(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t *index
) {
    if (scan == NULL || index == NULL || *index >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, *index, "LIKE")) {
        return NULL;
    }
    ++*index;
    if (*index >= scan->token_count || scan->tokens[*index].kind != MYLITE_SQL_TOKEN_STRING) {
        return NULL;
    }
    ++*index;
    return mylite_sql_parser_make_literal(
        state,
        scan->tokens[*index - 1U],
        MYLITE_SQL_AST_LITERAL_STRING
    );
}

static struct mylite_sql_ast_node *placeholder_parse_set_system_variable_value(
    struct mylite_sql_parser_state *state,
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (state == NULL || scan == NULL || index >= scan->token_count) {
        return NULL;
    }
    token = &scan->tokens[index];
    if (placeholder_scan_token_text_equals(scan, index, "DEFAULT")) {
        return mylite_sql_parser_make_set_default_value(state, *token);
    }
    if (placeholder_scan_token_text_equals(scan, index, "TRUE") ||
        placeholder_scan_token_text_equals(scan, index, "ON")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_TRUE);
    }
    if (placeholder_scan_token_text_equals(scan, index, "FALSE") ||
        placeholder_scan_token_text_equals(scan, index, "OFF")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_FALSE);
    }
    if (placeholder_scan_token_text_equals(scan, index, "NULL")) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_NULL);
    }
    if (token->kind == MYLITE_SQL_TOKEN_INTEGER) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_INTEGER);
    }
    if (token->kind == MYLITE_SQL_TOKEN_DECIMAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_DECIMAL);
    }
    if (token->kind == MYLITE_SQL_TOKEN_FLOAT) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_FLOAT);
    }
    if (token->kind == MYLITE_SQL_TOKEN_STRING) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_STRING);
    }
    if (token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_HEX);
    }
    if (token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL) {
        return mylite_sql_parser_make_literal(state, *token, MYLITE_SQL_AST_LITERAL_BIT);
    }
    if (placeholder_scan_token_is_identifier_like(scan, index) ||
        placeholder_scan_token_text_equals(scan, index, "UTC") ||
        placeholder_scan_token_text_equals(scan, index, "SYSTEM") ||
        placeholder_scan_token_text_equals(scan, index, "SERIALIZABLE") ||
        placeholder_scan_token_text_equals(scan, index, "BINARY")) {
        return mylite_sql_parser_make_identifier(state, *token);
    }
    return NULL;
}

static bool placeholder_scan_token_is_set_system_variable_scalar_value(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return placeholder_scan_token_text_equals(scan, index, "DEFAULT") ||
           placeholder_scan_token_text_equals(scan, index, "TRUE") ||
           placeholder_scan_token_text_equals(scan, index, "ON") ||
           placeholder_scan_token_text_equals(scan, index, "FALSE") ||
           placeholder_scan_token_text_equals(scan, index, "OFF") ||
           placeholder_scan_token_text_equals(scan, index, "NULL") ||
           token->kind == MYLITE_SQL_TOKEN_INTEGER || token->kind == MYLITE_SQL_TOKEN_DECIMAL ||
           token->kind == MYLITE_SQL_TOKEN_FLOAT || token->kind == MYLITE_SQL_TOKEN_STRING ||
           token->kind == MYLITE_SQL_TOKEN_HEX_LITERAL ||
           token->kind == MYLITE_SQL_TOKEN_BIT_LITERAL ||
           placeholder_scan_token_is_identifier_like(scan, index) ||
           placeholder_scan_token_text_equals(scan, index, "UTC") ||
           placeholder_scan_token_text_equals(scan, index, "SYSTEM") ||
           placeholder_scan_token_text_equals(scan, index, "SERIALIZABLE") ||
           placeholder_scan_token_text_equals(scan, index, "BINARY");
}

static bool placeholder_scan_token_is_dot(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    const struct mylite_sql_token *token = NULL;

    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    token = &scan->tokens[index];
    return token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '.';
}

static bool placeholder_scan_token_is_show_columns_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "COLUMNS") ||
           placeholder_scan_token_text_equals(scan, index, "FIELDS");
}

static bool placeholder_scan_token_is_show_index_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "INDEX") ||
           placeholder_scan_token_text_equals(scan, index, "INDEXES") ||
           placeholder_scan_token_text_equals(scan, index, "KEYS");
}

static bool placeholder_scan_token_is_schema_keyword(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return placeholder_scan_token_text_equals(scan, index, "FROM") ||
           placeholder_scan_token_text_equals(scan, index, "IN");
}

static enum mylite_sql_parse_status scan_placeholder_statement_tokens(
    struct mylite_sql_parse_config config,
    struct mylite_sql_token **out_tokens,
    size_t *out_token_count,
    bool *out_has_non_trailing_semicolon
) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token *tokens = NULL;
    size_t token_count = 0U;
    size_t token_capacity = 0U;
    bool saw_semicolon = false;

    if (out_tokens == NULL || out_token_count == NULL || out_has_non_trailing_semicolon == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_tokens = NULL;
    *out_token_count = 0U;
    *out_has_non_trailing_semicolon = false;

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = config.input,
            .length = config.length,
            .modes = config.modes,
        }
    );
    {
        enum mylite_sql_parse_status status = scan_placeholder_lexer_tokens(
            &lexer,
            config.input,
            &tokens,
            &token_count,
            &token_capacity,
            &saw_semicolon,
            out_has_non_trailing_semicolon
        );

        if (status != MYLITE_SQL_PARSE_OK) {
            free(tokens);
            return status;
        }
    }

    *out_tokens = tokens;
    *out_token_count = token_count;
    return MYLITE_SQL_PARSE_OK;
}

static enum mylite_sql_parse_status scan_placeholder_lexer_tokens(
    struct mylite_sql_lexer *lexer,
    const char *root_input,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon
) {
    struct mylite_sql_lexer lexer_stack[mylite_sql_parser_version_comment_lexer_stack_limit];
    size_t lexer_count = 1U;

    if (lexer == NULL || tokens == NULL || token_count == NULL || token_capacity == NULL ||
        saw_semicolon == NULL || out_has_non_trailing_semicolon == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    lexer_stack[0] = *lexer;
    for (;;) {
        struct mylite_sql_token token;
        struct mylite_sql_lexer *current_lexer = &lexer_stack[lexer_count - 1U];
        bool done = false;
        enum mylite_sql_parse_status status;

        if (mylite_sql_lexer_next(current_lexer, &token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        status = scan_placeholder_lexer_token(
            &token,
            current_lexer,
            root_input,
            lexer_stack,
            &lexer_count,
            tokens,
            token_count,
            token_capacity,
            saw_semicolon,
            out_has_non_trailing_semicolon,
            &done
        );

        if (status != MYLITE_SQL_PARSE_OK || done) {
            return status;
        }
    }
}

static enum mylite_sql_parse_status scan_placeholder_lexer_token(
    struct mylite_sql_token *token,
    struct mylite_sql_lexer *current_lexer,
    const char *root_input,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity,
    bool *saw_semicolon,
    bool *out_has_non_trailing_semicolon,
    bool *out_done
) {
    if (token == NULL || current_lexer == NULL || lexer_stack == NULL || lexer_count == NULL ||
        tokens == NULL || token_count == NULL || token_capacity == NULL || saw_semicolon == NULL ||
        out_has_non_trailing_semicolon == NULL || out_done == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_done = false;

    if (root_input != NULL && token->text != NULL && token->text >= root_input) {
        token->offset = (size_t)(token->text - root_input);
    }
    if (token->kind == MYLITE_SQL_TOKEN_EOF && *lexer_count > 1U) {
        --*lexer_count;
        return MYLITE_SQL_PARSE_OK;
    }
    if (token->kind == MYLITE_SQL_TOKEN_VERSION_COMMENT) {
        return mylite_sql_parser_push_version_comment_payload_lexer(
            token,
            current_lexer->modes,
            lexer_stack,
            lexer_count
        );
    }
    if (mylite_sql_parser_token_is_comment(token->kind)) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (token->kind == MYLITE_SQL_TOKEN_ERROR) {
        return MYLITE_SQL_PARSE_LEXER_ERROR;
    }
    if (token->kind == MYLITE_SQL_TOKEN_EOF) {
        *out_done = true;
        return MYLITE_SQL_PARSE_OK;
    }
    if (placeholder_token_is_semicolon(token)) {
        *saw_semicolon = true;
        return MYLITE_SQL_PARSE_OK;
    }
    if (*saw_semicolon) {
        *out_has_non_trailing_semicolon = true;
    }
    if (!append_placeholder_statement_token(token, tokens, token_count, token_capacity)) {
        return MYLITE_SQL_PARSE_NOMEM;
    }

    return MYLITE_SQL_PARSE_OK;
}

static bool append_placeholder_statement_token(
    const struct mylite_sql_token *token,
    struct mylite_sql_token **tokens,
    size_t *token_count,
    size_t *token_capacity
) {
    struct mylite_sql_token *grown = NULL;
    size_t new_capacity = 0U;

    if (*token_count == *token_capacity) {
        new_capacity =
            *token_capacity == 0U ? placeholder_initial_token_capacity : *token_capacity * 2U;
        if (new_capacity < *token_capacity) {
            return false;
        }
        grown = (struct mylite_sql_token *)realloc(*tokens, new_capacity * sizeof(**tokens));
        if (grown == NULL) {
            return false;
        }
        *tokens = grown;
        *token_capacity = new_capacity;
    }

    (*tokens)[*token_count] = *token;
    ++*token_count;
    return true;
}

static enum mylite_sql_parse_status try_parse_alter_table_algorithm_lock_tail_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();
    struct mylite_sql_parse_result prefix_result;
    struct mylite_sql_ast_node *statement = NULL;
    size_t option_index = 0U;
    size_t prefix_length = 0U;
    bool actionless = false;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->has_non_trailing_semicolon ||
        !scan_alter_table_algorithm_lock_tail(scan, &option_index, &prefix_length, &actionless) ||
        !parse_alter_table_algorithm_lock_options(scan, option_index, &options)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (actionless) {
        if (!alter_table_algorithm_lock_options_are_known(options)) {
            return MYLITE_SQL_PARSE_OK;
        }
        *out_handled = true;
        return finish_placeholder_statement_parse(
            config,
            result,
            PLACEHOLDER_STATEMENT_UTILITY_NOOP
        );
    }

    memset(&prefix_result, 0, sizeof(prefix_result));
    status = mylite_sql_parser_parse_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    statement = mylite_sql_parser_child_at(prefix_result.root, 0U);
    if (!alter_table_statement_accepts_prefix_option_tail(statement)) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_parser_apply_alter_table_options(statement, options);
    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool scan_alter_table_algorithm_lock_tail(
    const struct placeholder_statement_scan *scan,
    size_t *out_option_index,
    size_t *out_prefix_length,
    bool *out_actionless
) {
    size_t table_name_end = 0U;
    int paren_depth = 0;

    if (scan == NULL || out_option_index == NULL || out_prefix_length == NULL ||
        out_actionless == NULL || scan->token_count < 4U ||
        !placeholder_scan_token_text_equals(scan, 0U, "ALTER") ||
        !placeholder_scan_token_text_equals(scan, 1U, "TABLE")) {
        return false;
    }

    table_name_end = alter_table_placeholder_name_end_index(scan);
    if (table_name_end >= scan->token_count) {
        return false;
    }

    for (size_t index = table_name_end; index < scan->token_count; ++index) {
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
        if (paren_depth != 0 || (!placeholder_scan_token_text_equals(scan, index, "ALGORITHM") &&
                                 !placeholder_scan_token_text_equals(scan, index, "LOCK"))) {
            continue;
        }
        if (index == table_name_end) {
            *out_option_index = index;
            *out_prefix_length = 0U;
            *out_actionless = true;
            return true;
        }
        if (index > 0U && mylite_sql_parser_token_is_comma(&scan->tokens[index - 1U])) {
            *out_option_index = index;
            *out_prefix_length = scan->tokens[index - 1U].offset;
            *out_actionless = false;
            return true;
        }
    }
    return false;
}

static size_t alter_table_placeholder_name_end_index(
    const struct placeholder_statement_scan *scan
) {
    size_t index = 2U;

    if (scan == NULL) {
        return index;
    }
    if (index < scan->token_count) {
        ++index;
    }
    while (index + 1U < scan->token_count && placeholder_scan_token_text_equals(scan, index, ".")) {
        index += 2U;
    }
    return index;
}

static bool parse_alter_table_algorithm_lock_options(
    const struct placeholder_statement_scan *scan,
    size_t option_index,
    struct mylite_sql_alter_table_options *out_options
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();
    size_t index = option_index;

    if (scan == NULL || out_options == NULL || index >= scan->token_count) {
        return false;
    }

    for (;;) {
        if (!parse_alter_table_algorithm_lock_option(scan, &index, &options)) {
            return false;
        }
        if (index == scan->token_count) {
            *out_options = options;
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

static bool parse_alter_table_algorithm_lock_option(
    const struct placeholder_statement_scan *scan,
    size_t *index,
    struct mylite_sql_alter_table_options *options
) {
    struct mylite_sql_alter_table_options option;
    const struct mylite_sql_token *option_token = NULL;
    const struct mylite_sql_token *value_token = NULL;
    bool option_is_algorithm = false;

    if (scan == NULL || index == NULL || options == NULL || *index >= scan->token_count) {
        return false;
    }
    option_token = &scan->tokens[*index];
    option_is_algorithm = placeholder_scan_token_text_equals(scan, *index, "ALGORITHM");
    if (!option_is_algorithm && !placeholder_scan_token_text_equals(scan, *index, "LOCK")) {
        return false;
    }
    *index += 1U;
    if (*index < scan->token_count &&
        mylite_sql_parser_token_is_equal_sign(&scan->tokens[*index])) {
        ++*index;
    }
    if (*index >= scan->token_count) {
        return false;
    }
    value_token = &scan->tokens[*index];
    if (!placeholder_scan_token_is_identifier_like(scan, *index) &&
        !placeholder_scan_token_text_equals(scan, *index, "DEFAULT") &&
        !placeholder_scan_token_text_equals(scan, *index, "NONE")) {
        return false;
    }

    if (option_is_algorithm) {
        option = mylite_sql_parser_make_alter_table_algorithm_option(
            *option_token,
            mylite_sql_parser_make_alter_algorithm_value(*value_token)
        );
    } else {
        option = mylite_sql_parser_make_alter_table_lock_option(
            *option_token,
            mylite_sql_parser_make_alter_lock_value(*value_token)
        );
    }
    *options = mylite_sql_parser_append_alter_table_option(*options, option);
    ++*index;
    return true;
}

static bool alter_table_statement_accepts_prefix_option_tail(
    const struct mylite_sql_ast_node *statement
) {
    if (statement == NULL) {
        return false;
    }
    switch (statement->kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
        return true;
    default:
        return false;
    }
}

static bool alter_table_algorithm_lock_options_are_known(
    struct mylite_sql_alter_table_options options
) {
    return options.algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN &&
           options.lock != MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;
}

static enum mylite_sql_parse_status try_parse_create_table_partition_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct placeholder_statement_scan *scan,
    bool *out_handled
) {
    struct mylite_sql_parse_result prefix_result;
    size_t partition_index = 0U;
    size_t prefix_length = 0U;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_handled == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    *out_handled = false;
    if (scan == NULL || scan->tokens == NULL ||
        !scan_is_create_table_partition_statement(scan, &partition_index)) {
        return MYLITE_SQL_PARSE_OK;
    }

    prefix_length = scan->tokens[partition_index].offset;
    status = mylite_sql_parser_parse_with_lemon(
        (struct mylite_sql_parse_config){
            .input = config.input,
            .length = prefix_length,
            .modes = config.modes,
        },
        &prefix_result
    );
    if (status != MYLITE_SQL_PARSE_OK && find_open_version_comment_start_before_token(
                                             config,
                                             &scan->tokens[partition_index],
                                             &prefix_length
                                         )) {
        mylite_sql_parse_result_deinit(&prefix_result);
        status = mylite_sql_parser_parse_with_lemon(
            (struct mylite_sql_parse_config){
                .input = config.input,
                .length = prefix_length,
                .modes = config.modes,
            },
            &prefix_result
        );
    }
    if (status != MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&prefix_result);
        return MYLITE_SQL_PARSE_OK;
    }

    mylite_sql_ast_deinit(&result->ast);
    *result = prefix_result;
    *out_handled = true;
    return MYLITE_SQL_PARSE_OK;
}

static bool find_open_version_comment_start_before_token(
    struct mylite_sql_parse_config config,
    const struct mylite_sql_token *token,
    size_t *out_offset
) {
    const char *end = NULL;
    const char *found = NULL;

    if (config.input == NULL || token == NULL || token->text == NULL || out_offset == NULL ||
        token->text < config.input || token->text > config.input + config.length) {
        return false;
    }

    end = token->text;
    for (const char *cursor = config.input; cursor + 2 < end; ++cursor) {
        if (cursor[0] == '/' && cursor[1] == '*' && cursor[2] == '!') {
            found = cursor;
            cursor += 2;
            continue;
        }
        if (cursor[0] == '*' && cursor[1] == '/') {
            found = NULL;
            ++cursor;
        }
    }

    if (found == NULL) {
        return false;
    }
    *out_offset = (size_t)(found - config.input);
    return true;
}

static bool scan_is_create_table_partition_statement(
    const struct placeholder_statement_scan *scan,
    size_t *out_partition_index
) {
    size_t table_index = 1U;
    int paren_depth = 0;

    if (scan == NULL || scan->tokens == NULL || out_partition_index == NULL ||
        scan->token_count < create_table_partition_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, table_index, "TEMPORARY")) {
        ++table_index;
    }
    if (!placeholder_scan_token_text_equals(scan, table_index, "TABLE")) {
        return false;
    }

    for (size_t index = table_index + 1U; index + 1U < scan->token_count; ++index) {
        if (paren_depth == 0 && placeholder_scan_token_text_equals(scan, index, "PARTITION") &&
            placeholder_scan_token_text_equals(scan, index + 1U, "BY") &&
            create_table_partition_suffix_is_supported(scan, index)) {
            *out_partition_index = index;
            return true;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return false;
}

static bool create_table_partition_suffix_is_supported(
    const struct placeholder_statement_scan *scan,
    size_t partition_index
) {
    int paren_depth = 0;
    bool saw_method = false;
    bool saw_method_paren = false;

    if (partition_index + 2U >= scan->token_count ||
        !placeholder_scan_token_text_equals(scan, partition_index, "PARTITION") ||
        !placeholder_scan_token_text_equals(scan, partition_index + 1U, "BY")) {
        return false;
    }
    for (size_t index = partition_index + 2U; index < scan->token_count; ++index) {
        if (paren_depth == 0 && (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
                                 placeholder_scan_token_text_equals(scan, index, "AS") ||
                                 placeholder_scan_token_text_equals(scan, index, "IGNORE") ||
                                 placeholder_scan_token_text_equals(scan, index, "REPLACE"))) {
            return false;
        }
        if (!saw_method && (placeholder_scan_token_text_equals(scan, index, "HASH") ||
                            placeholder_scan_token_text_equals(scan, index, "KEY") ||
                            placeholder_scan_token_text_equals(scan, index, "RANGE") ||
                            placeholder_scan_token_text_equals(scan, index, "LIST"))) {
            saw_method = true;
        }
        if (saw_method && mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            saw_method_paren = true;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
            if (paren_depth < 0) {
                return false;
            }
        }
    }
    return saw_method && saw_method_paren && paren_depth == 0;
}

static bool create_table_select_placeholder_statement_is_supported(
    const struct placeholder_statement_scan *scan
) {
    size_t table_index = 1U;

    if (scan == NULL || scan->tokens == NULL ||
        scan->token_count < create_table_select_min_token_count ||
        !placeholder_scan_token_text_equals(scan, 0U, "CREATE")) {
        return false;
    }
    if (placeholder_scan_token_text_equals(scan, table_index, "TEMPORARY")) {
        ++table_index;
    }
    if (!placeholder_scan_token_text_equals(scan, table_index, "TABLE") ||
        !create_table_select_scan_has_balanced_parentheses(scan, table_index + 1U)) {
        return false;
    }
    return create_table_select_scan_has_query_source(scan, table_index + 1U);
}

static bool create_table_select_scan_has_query_source(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    for (size_t index = start_index; index < scan->token_count; ++index) {
        if (paren_depth == 0) {
            if (create_table_select_query_keyword_is_complete(scan, index)) {
                return true;
            }
            if (placeholder_scan_token_text_equals(scan, index, "AS") &&
                create_table_select_query_starts_at(scan, index + 1U)) {
                return true;
            }
            if (create_table_select_parenthesized_query_starts_at(scan, index)) {
                return true;
            }
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[index])) {
            --paren_depth;
        }
    }
    return false;
}

static bool create_table_select_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return create_table_select_query_keyword_is_complete(scan, index) ||
           create_table_select_parenthesized_query_starts_at(scan, index);
}

static bool create_table_select_parenthesized_query_starts_at(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    size_t query_start_index = index;
    int paren_depth = 0;

    if (scan == NULL || index + 1U >= scan->token_count ||
        !mylite_sql_parser_token_is_left_paren(&scan->tokens[index])) {
        return false;
    }
    while (query_start_index + 1U < scan->token_count &&
           mylite_sql_parser_token_is_left_paren(&scan->tokens[query_start_index + 1U])) {
        ++query_start_index;
    }
    if (query_start_index + 1U >= scan->token_count ||
        !create_table_select_query_keyword_is_complete(scan, query_start_index + 1U)) {
        return false;
    }
    for (size_t scan_index = index; scan_index < scan->token_count; ++scan_index) {
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
            if (paren_depth == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool create_table_select_with_query_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    int paren_depth = 0;

    if (scan == NULL || !placeholder_scan_token_text_equals(scan, index, "WITH")) {
        return false;
    }
    for (size_t scan_index = index + 1U; scan_index < scan->token_count; ++scan_index) {
        if (paren_depth == 0 &&
            create_table_select_plain_query_keyword_is_complete(scan, scan_index)) {
            return scan_index + 1U < scan->token_count;
        }
        if (mylite_sql_parser_token_is_left_paren(&scan->tokens[scan_index])) {
            ++paren_depth;
        } else if (mylite_sql_parser_token_is_right_paren(&scan->tokens[scan_index])) {
            --paren_depth;
        }
    }
    return false;
}

static bool create_table_select_plain_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    return (placeholder_scan_token_text_equals(scan, index, "SELECT") ||
            placeholder_scan_token_text_equals(scan, index, "TABLE") ||
            placeholder_scan_token_text_equals(scan, index, "VALUES")) &&
           index + 1U < scan->token_count;
}

static bool create_table_select_query_keyword_is_complete(
    const struct placeholder_statement_scan *scan,
    size_t index
) {
    if (scan == NULL || index >= scan->token_count) {
        return false;
    }
    if (create_table_select_plain_query_keyword_is_complete(scan, index)) {
        return true;
    }
    return create_table_select_with_query_is_complete(scan, index);
}

static bool create_table_select_scan_has_balanced_parentheses(
    const struct placeholder_statement_scan *scan,
    size_t start_index
) {
    int paren_depth = 0;

    if (scan == NULL) {
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
