static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (count_function_from_expression(expression) != PLANNED_COUNT_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int execute_grouped_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_grouped_aggregate_result_columns(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = read_grouped_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return grouped_aggregate_execution_error(database, rc);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_grouped_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_grouped_aggregate *plan
) {
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        const char *projection_column_name = plan->projections[projection_index].column.name;
        char *projection_alias_text = NULL;

        if (plan->projections[projection_index].alias != NULL) {
            rc = copy_select_item_alias_text(
                database,
                plan->projections[projection_index].alias,
                &projection_alias_text
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            projection_column_name = projection_alias_text;
        }
        rc = mylite_result_append_column(result, projection_column_name);
        free(projection_alias_text);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        char *aggregate_column_name = NULL;
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->alias != NULL) {
            rc = copy_select_item_alias_text(database, item->alias, &aggregate_column_name);
        } else {
            rc = copy_aggregate_result_column_name(
                database,
                &item->expression->span,
                &aggregate_column_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, aggregate_column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(aggregate_column_name);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return rc;
}

static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *count_expression = NULL;
    struct planned_count_source_nodes source_nodes = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count){false};
    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    count_expression = unwrap_parenthesized_expression(expression);
    out_plan->function = count_function_from_expression(count_expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_NONE) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_LITERAL) {
        rc = plan_count_literal(database, count_expression, &out_plan->count_literal);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
            set_unsupported_error(database, count_supported_clauses_message(out_plan->function));
            return MYLITE_ERROR;
        }
        where_clause = optional_clause;
        optional_clause = optional_clause->next_sibling;
    }

    source_nodes.from_clause = from_clause;
    source_nodes.where_clause = where_clause;
    source_nodes.count_expression = count_expression;
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return plan_count_without_source(database, &source_nodes, out_plan);
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, count_descriptor_table_message(out_plan->function));
        return MYLITE_ERROR;
    }

    return plan_count_table_source(database, &source_nodes, out_plan);
}

static void planned_count_deinit(struct planned_count *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_predicate_deinit(&plan->predicate);
    planned_value_deinit(&plan->count_literal);
    *plan = (struct planned_count){false};
}

static int plan_count_without_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc = MYLITE_OK;

    if (out_plan->function == PLANNED_COUNT_COLUMN ||
        out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        const struct mylite_sql_ast_node *column_node = child_at(nodes->count_expression, 0U);
        char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t part_count = 0U;

        rc = collect_column_reference_parts(database, column_node, parts, &part_count);
        if (rc == MYLITE_OK) {
            rc = format_column_reference_name(
                database,
                parts,
                part_count,
                column_name,
                sizeof(column_name)
            );
        }
        if (rc == MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }
    if (nodes->where_clause != NULL) {
        if (out_plan->function == PLANNED_COUNT_LITERAL) {
            set_unsupported_error(
                database,
                "COUNT(literal) WHERE requires a descriptor-backed table"
            );
        } else {
            set_unsupported_error(database, "COUNT(*) WHERE requires a descriptor-backed table");
        }
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    out_plan->has_source = true;
    rc = resolve_visible_table_reference(
        database,
        child_at(nodes->from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, nodes->from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            nodes->from_clause,
            &out_plan->source,
            &source_context
        );
    }
    if (rc == MYLITE_OK &&
        (nodes->where_clause != NULL || out_plan->function == PLANNED_COUNT_COLUMN ||
         out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK && (out_plan->function == PLANNED_COUNT_COLUMN ||
                            out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = plan_count_column(
            database,
            nodes->count_expression,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->count_column
        );
    }
    if (rc == MYLITE_OK && out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &out_plan->count_column,
            "COUNT(DISTINCT column) supports only integer descriptor columns",
            &range
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            nodes->where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COUNT_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        return PLANNED_COUNT_STAR;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_COLUMN;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION) {
        return PLANNED_COUNT_LITERAL;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_DISTINCT_COLUMN;
    }

    return PLANNED_COUNT_NONE;
}

static const char *count_exactly_one_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_STAR) {
        return "COUNT(*) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports exactly one aggregate select item";
    }

    return "COUNT aggregate supports exactly one aggregate select item";
}

static const char *count_supported_clauses_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only WHERE";
    }

    return "COUNT(*) supports only WHERE";
}

static const char *count_descriptor_table_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only descriptor-backed table reads";
    }

    return "COUNT(*) supports only descriptor-backed table reads";
}

static int plan_count_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    struct planned_value *out_literal
) {
    const struct mylite_sql_ast_node *argument = child_at(function, 0U);
    const struct mylite_sql_ast_node *literal = argument;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    *out_literal = (struct planned_value){.is_null = false, .integer = 1};
    if (argument == NULL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(
                database,
                "COUNT(literal) supports only integer, boolean, and NULL literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(argument, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_literal->is_null = true;
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_literal->integer = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "COUNT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column
    );
}

static int execute_count_from_plan(
    struct mylite_db *database,
    const struct planned_count *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    char count_text[integer_text_capacity];
    int64_t count_value = 1;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (plan->function == PLANNED_COUNT_LITERAL && plan->count_literal.is_null) {
        count_value = 0;
    }

    rc = append_count_result_column(database, result, plan);
    if (rc == MYLITE_OK && plan->has_source) {
        rc = read_count_from_source(database, plan, &count_value);
    }
    if (rc == MYLITE_OK) {
        rc = format_count_value(database, count_value, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = append_count_value_row(database, result, count_text);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return count_execution_error(database, rc);
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
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

static int copy_aggregate_result_column_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    char *text = NULL;
    size_t extra_spaces = 0U;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    extra_spaces = aggregate_label_extra_spaces_after_block_comments(span);
    if (extra_spaces > SIZE_MAX - span->length - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    text = (char *)malloc(span->length + extra_spaces + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy_aggregate_label_with_spacing(span, text);
    *out_text = text;

    return MYLITE_OK;
}

static size_t aggregate_label_extra_spaces_after_block_comments(
    const struct mylite_sql_source_span *span
) {
    size_t extra_spaces = 0U;
    size_t source_index = 0U;

    while (source_index + 1U < span->length) {
        if (span->text[source_index] == '/' && span->text[source_index + 1U] == '*') {
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    ++extra_spaces;
                }
            }
        } else {
            ++source_index;
        }
    }

    return extra_spaces;
}

static void copy_aggregate_label_with_spacing(
    const struct mylite_sql_source_span *span,
    char *destination
) {
    size_t destination_index = 0U;
    size_t source_index = 0U;

    while (source_index < span->length) {
        if (source_index + 1U < span->length && span->text[source_index] == '/' &&
            span->text[source_index + 1U] == '*') {
            destination[destination_index] = span->text[source_index];
            destination[destination_index + 1U] = span->text[source_index + 1U];
            destination_index += 2U;
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                destination[destination_index] = span->text[source_index];
                ++destination_index;
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                destination[destination_index] = span->text[source_index];
                destination[destination_index + 1U] = span->text[source_index + 1U];
                destination_index += 2U;
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    destination[destination_index] = ' ';
                    ++destination_index;
                }
            }
        } else {
            destination[destination_index] = span->text[source_index];
            ++destination_index;
            ++source_index;
        }
    }

    destination[destination_index] = '\0';
}

static bool aggregate_label_needs_space_after_block_comment(char next_byte) {
    switch (next_byte) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case ')':
    case ',':
        return false;
    default:
        return true;
    }
}

static int read_count_from_source(
    struct mylite_db *database,
    const struct planned_count *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_count_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_count = sqlite3_column_int64(statement, 0);
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int format_count_value(
    struct mylite_db *database,
    int64_t count_value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, count_value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format COUNT(*) value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_count_value_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *count_text
) {
    const char *values[] = {count_text};
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int count_execution_error(struct mylite_db *database, int rc) {
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static bool select_statement_has_column_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (column_aggregate_function_from_expression(expression) !=
            PLANNED_COLUMN_AGGREGATE_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int plan_column_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_column_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_column_aggregate){0};

    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    expression = unwrap_parenthesized_expression(expression);
    out_plan->function = column_aggregate_function_from_expression(expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        if (out_plan->function == PLANNED_COLUMN_AGGREGATE_NONE) {
            out_plan->function = select_list_column_aggregate_function(select_list);
        }
        set_unsupported_error(database, column_aggregate_single_item_error(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COLUMN_AGGREGATE_NONE) {
        set_unsupported_error(database, column_aggregate_single_item_error(out_plan->function));
        return MYLITE_ERROR;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                column_aggregate_optional_clause_error(out_plan->function)
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        set_unsupported_error(database, column_aggregate_source_error(out_plan->function));
        return MYLITE_ERROR;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, column_aggregate_source_error(out_plan->function));
        return MYLITE_ERROR;
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        if (out_plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
            rc = plan_column_group_concat_options(
                database,
                expression,
                &source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else {
            rc = plan_column_aggregate_column(
                database,
                expression,
                out_plan->function,
                &source_context,
                table_columns,
                table_column_count,
                &out_plan->aggregate_column
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->aggregate_order);
    planned_value_deinit(&plan->separator);
    *plan = (struct planned_column_aggregate){0};
}

static enum planned_column_aggregate_function column_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_MIN;
    }
    if (expression->kind == MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_MAX;
    }
    if (expression->kind == MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_SUM;
    }
    if (expression->kind == MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_AVG;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_AND;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_OR;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_XOR;
    }
    if (expression->kind == MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static enum planned_column_aggregate_function select_list_column_aggregate_function(
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);

    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        enum planned_column_aggregate_function function =
            column_aggregate_function_from_expression(unwrap_parenthesized_expression(expression));

        if (function != PLANNED_COLUMN_AGGREGATE_NONE) {
            return function;
        }
        select_item = select_item->next_sibling;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_BIT_AND ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_OR ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_XOR) != 0;
}

static const char *column_aggregate_single_item_error(
    enum planned_column_aggregate_function function
) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports exactly one aggregate select item";
    }

    return "MIN/MAX supports exactly one aggregate select item";
}

static const char *column_aggregate_optional_clause_error(
    enum planned_column_aggregate_function function
) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only WHERE";
    }

    return "MIN/MAX supports only WHERE";
}

static const char *column_aggregate_source_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor-backed table reads";
    }

    return "MIN/MAX supports only descriptor-backed table reads";
}

static const char *column_aggregate_column_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor columns";
    }

    return "MIN/MAX supports only descriptor columns";
}

static const char *column_aggregate_integer_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only integer descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only integer and nonbinary string descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only integer descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only integer descriptor columns";
    }

    return "MIN/MAX supports only integer descriptor columns";
}

static int plan_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);
    struct integer_column_range range;
    int rc = MYLITE_OK;

    rc = resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        column_aggregate_column_error(aggregate_function),
        table_columns,
        table_column_count,
        out_column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = integer_range_for_column(
        database,
        out_column,
        column_aggregate_integer_error(aggregate_function),
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_column_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
) {
    size_t value_source_index = 0U;
    int rc = plan_group_concat_value_column(
        database,
        function,
        source_context,
        table_columns,
        table_column_count,
        &out_plan->aggregate_column,
        &value_source_index
    );

    (void)value_source_index;
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_order_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            false,
            &out_plan->aggregate_order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_separator(
            database,
            function,
            &out_plan->has_separator,
            &out_plan->separator
        );
    }

    return rc;
}

static int execute_column_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_column_aggregate_result_column(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = read_column_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return column_aggregate_execution_error(database, rc);
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_column_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_column_aggregate *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
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

static int read_column_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_column_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_column_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        database->session.group_concat_value_ordinal = 0U;
    }
    if (rc == MYLITE_OK) {
        rc = step_column_aggregate_statement(database, statement, plan, result);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int read_grouped_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_grouped_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_grouped_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        for (size_t index = 0U; index < plan->aggregate_count; ++index) {
            if (plan->aggregates[index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
                database->session.group_concat_value_ordinal = 0U;
                break;
            }
        }
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = grouped_aggregate_step_error(database, statement, plan, sqlite_rc);
            break;
        }
        rc = append_grouped_aggregate_sqlite_row(database, statement, plan, result);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_column_aggregate_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    mylite_result *result
) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc != SQLITE_ROW) {
        return column_aggregate_step_error(database, statement, plan, sqlite_rc);
    }

    if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
        rc = append_avg_sqlite_row(database, statement, result);
    } else if (column_aggregate_function_is_bitwise(plan->function)) {
        rc = append_bitwise_aggregate_sqlite_row(database, statement, result);
    } else {
        rc = append_selected_sqlite_row(database, statement, result, NULL, 0U);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return column_aggregate_step_error(database, statement, plan, sqlite_rc);
    }

    return MYLITE_OK;
}

static int append_grouped_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    struct mylite_result_cell *cells = NULL;
    char (*projection_buffers)[approximate_numeric_text_capacity] = NULL;
    char (*aggregate_buffers)[approximate_numeric_text_capacity] = NULL;
    int sqlite_column_index = 0;
    size_t result_column_count = plan->projection_count + plan->aggregate_count;
    int rc = MYLITE_OK;

    if (plan->projection_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    if (result_column_count == 0U) {
        return MYLITE_ERROR;
    }
    if (result_column_count > SIZE_MAX / sizeof(*cells)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    sqlite_column_index = (int)plan->projection_count;

    cells = calloc(result_column_count, sizeof(*cells));
    if (plan->projection_count > 0U) {
        projection_buffers = calloc(plan->projection_count, sizeof(*projection_buffers));
    }
    if (plan->aggregate_count > 0U) {
        aggregate_buffers = calloc(plan->aggregate_count, sizeof(*aggregate_buffers));
    }
    if (cells == NULL || (plan->projection_count > 0U && projection_buffers == NULL) ||
        (plan->aggregate_count > 0U && aggregate_buffers == NULL)) {
        free(cells);
        free(projection_buffers);
        free(aggregate_buffers);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        rc = append_grouped_aggregate_projection_cell(
            database,
            statement,
            plan,
            projection_index,
            &cells[projection_index],
            projection_buffers[projection_index],
            sizeof(projection_buffers[projection_index])
        );
    }
    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        rc = append_grouped_aggregate_sqlite_cell(
            database,
            statement,
            &plan->aggregates[aggregate_index],
            &sqlite_column_index,
            &cells[plan->projection_count + aggregate_index],
            aggregate_buffers[aggregate_index],
            sizeof(aggregate_buffers[aggregate_index])
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, cells);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    free(cells);
    free(projection_buffers);
    free(aggregate_buffers);

    return rc;
}

static int append_grouped_aggregate_projection_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    size_t projection_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    if (projection_index >= plan->projection_count) {
        return MYLITE_ERROR;
    }
    return append_selected_sqlite_row_value_with_descriptor(
        database,
        statement,
        projection_index,
        &plan->projections[projection_index].column,
        buffer,
        buffer_size,
        out_cell
    );
}

static int append_grouped_aggregate_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    const char *value = NULL;
    int64_t count = 0;
    int rc = MYLITE_OK;

    *out_cell = (struct mylite_result_cell){.is_null = true};
    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        rc = append_grouped_group_concat_sqlite_cell(statement, *sqlite_column_index, out_cell);
        ++(*sqlite_column_index);
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        rc = append_selected_sqlite_row_value_with_descriptor(
            database,
            statement,
            (size_t)*sqlite_column_index,
            &item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        ++(*sqlite_column_index);
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        if (sqlite3_column_type(statement, *sqlite_column_index + 1) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        count = (int64_t)sqlite3_column_int64(statement, *sqlite_column_index + 1);
        if (count < 0) {
            return MYLITE_ERROR;
        }
        if (count == 0) {
            *sqlite_column_index += 2;
            return MYLITE_OK;
        }
        if (sqlite3_column_type(statement, *sqlite_column_index) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        rc = format_avg_value(
            database,
            (struct avg_accumulator){
                .sum = (int64_t)sqlite3_column_int64(statement, *sqlite_column_index),
                .count = count,
            },
            buffer,
            buffer_size
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_cell = (struct mylite_result_cell){
            .bytes = buffer,
            .size = strlen(buffer),
            .is_null = false,
        };
        *sqlite_column_index += 2;
        return MYLITE_OK;
    }
    if (grouped_aggregate_function_is_bitwise(item->function)) {
        if (sqlite3_column_type(statement, *sqlite_column_index) != SQLITE_TEXT) {
            return MYLITE_ERROR;
        }
        value = (const char *)sqlite3_column_text(statement, *sqlite_column_index);
        if (value == NULL) {
            return MYLITE_ERROR;
        }
        *out_cell = (struct mylite_result_cell){
            .bytes = value,
            .size = strlen(value),
            .is_null = false,
        };
        ++(*sqlite_column_index);
        return MYLITE_OK;
    }

    rc = sqlite_integer_result_text(statement, *sqlite_column_index, buffer, buffer_size, &value);
    if (rc == MYLITE_OK && value != NULL) {
        *out_cell = (struct mylite_result_cell){
            .bytes = value,
            .size = strlen(value),
            .is_null = false,
        };
    }
    ++(*sqlite_column_index);
    return rc;
}

static int append_grouped_group_concat_sqlite_cell(
    sqlite3_stmt *statement,
    int sqlite_column_index,
    struct mylite_result_cell *out_cell
) {
    int byte_count = sqlite3_column_bytes(statement, sqlite_column_index);
    const unsigned char *text = NULL;
    size_t character_count = 0U;

    *out_cell = (struct mylite_result_cell){.is_null = true};
    if (sqlite3_column_type(statement, sqlite_column_index) == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite3_column_type(statement, sqlite_column_index) != SQLITE_TEXT || byte_count < 0) {
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, sqlite_column_index);
    if (text == NULL || memchr(text, '\0', (size_t)byte_count) != NULL ||
        validate_utf8_text((const char *)text, (size_t)byte_count, &character_count) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    *out_cell = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    (void)character_count;
    return MYLITE_OK;
}

static int sqlite_integer_result_text(
    sqlite3_stmt *statement,
    int column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    int sqlite_type = sqlite3_column_type(statement, column_index);
    int written = 0;

    *out_value = NULL;
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%" PRId64,
        (int64_t)sqlite3_column_int64(statement, column_index)
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        return MYLITE_ERROR;
    }

    *out_value = buffer;
    return MYLITE_OK;
}

static int append_bitwise_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
) {
    const char *values[] = {NULL};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, 0) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }

    values[0] = (const char *)sqlite3_column_text(statement, 0);
    if (values[0] == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int append_avg_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
) {
    const char *values[] = {NULL};
    char average_text[integer_text_capacity + sizeof(".0000")];
    int64_t count = 0;
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, 1) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    count = (int64_t)sqlite3_column_int64(statement, 1);
    if (count == 0) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (count < 0 || sqlite3_column_type(statement, 0) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    rc = format_avg_value(
        database,
        (struct avg_accumulator){
            .sum = (int64_t)sqlite3_column_int64(statement, 0),
            .count = count,
        },
        average_text,
        sizeof(average_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = average_text;
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int format_avg_value(
    struct mylite_db *database,
    struct avg_accumulator accumulator,
    char *buffer,
    size_t buffer_size
) {
    uint64_t magnitude = absolute_int64_magnitude(accumulator.sum);
    uint64_t denominator = (uint64_t)accumulator.count;
    uint64_t integer_part = magnitude / denominator;
    uint64_t remainder = magnitude % denominator;
    unsigned int fraction = 0U;
    int round_digit = 0;
    bool is_negative = accumulator.sum < 0;
    int written = 0;

    for (size_t digit_index = 0U; digit_index < avg_fraction_digits; ++digit_index) {
        int digit = next_decimal_digit(&remainder, denominator);

        if (digit < 0) {
            set_runtime_error(database, "failed to format AVG(column) value");
            return MYLITE_ERROR;
        }
        fraction = (fraction * decimal_base) + (unsigned int)digit;
    }

    round_digit = next_decimal_digit(&remainder, denominator);
    if (round_digit < 0) {
        set_runtime_error(database, "failed to format AVG(column) value");
        return MYLITE_ERROR;
    }
    if (round_digit >= avg_round_half_digit) {
        ++fraction;
        if (fraction == avg_fraction_scale) {
            fraction = 0U;
            ++integer_part;
        }
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s%" PRIu64 ".%04u",
        is_negative && (integer_part != 0U || fraction != 0U) ? "-" : "",
        integer_part,
        fraction
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format AVG(column) value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static uint64_t absolute_int64_magnitude(int64_t value) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (value >= 0) {
        return (uint64_t)value;
    }
    if (value == INT64_MIN) {
        return int64_negative_abs_max;
    }

    return (uint64_t)-value;
}

static int next_decimal_digit(uint64_t *remainder, uint64_t denominator) {
    struct uint128_parts product = {0};
    int digit = 0;

    if (remainder == NULL || denominator == 0U || *remainder >= denominator) {
        return -1;
    }

    product = multiply_u64_by_decimal_radix(*remainder);
    while (uint128_ge_u64(&product, denominator)) {
        uint128_subtract_u64(&product, denominator);
        ++digit;
    }

    *remainder = product.low;
    return digit;
}

static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value) {
    struct uint128_parts product = {0};

    for (unsigned int index = 0U; index < decimal_base; ++index) {
        uint64_t previous_low = product.low;

        product.low += value;
        if (product.low < previous_low) {
            ++product.high;
        }
    }

    return product;
}

static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right) {
    if (left->high != 0U) {
        return true;
    }

    return left->low >= right;
}

static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right) {
    uint64_t previous_low = left->low;

    left->low -= right;
    if (previous_low < right) {
        --left->high;
    }
}

static int column_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    int sqlite_rc
) {
    const char *message = NULL;

    if (sqlite_rc == SQLITE_ERROR && (plan->function == PLANNED_COLUMN_AGGREGATE_SUM ||
                                      plan->function == PLANNED_COLUMN_AGGREGATE_AVG)) {
        message = sqlite3_errmsg(sqlite3_db_handle(statement));
        if (message != NULL && strcmp(message, "integer overflow") == 0) {
            if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
                set_unsupported_error(
                    database,
                    "AVG(column) intermediate sum exceeds MyLite signed 64-bit range"
                );
            } else {
                set_unsupported_error(
                    database,
                    "SUM(column) result exceeds MyLite signed 64-bit range"
                );
            }
            return MYLITE_ERROR;
        }
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int grouped_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int sqlite_rc
) {
    const char *message = NULL;
    bool has_sum = false;
    bool has_avg = false;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_SUM) {
            has_sum = true;
        }
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_AVG) {
            has_avg = true;
        }
    }
    if (sqlite_rc == SQLITE_ERROR && (has_sum || has_avg)) {
        message = sqlite3_errmsg(sqlite3_db_handle(statement));
        if (message != NULL && strcmp(message, "integer overflow") == 0) {
            if (has_avg) {
                set_unsupported_error(
                    database,
                    "AVG(column) intermediate sum exceeds MyLite signed 64-bit range"
                );
            } else {
                set_unsupported_error(
                    database,
                    "SUM(column) result exceeds MyLite signed 64-bit range"
                );
            }
            return MYLITE_ERROR;
        }
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int column_aggregate_execution_error(struct mylite_db *database, int rc) {
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static int grouped_aggregate_execution_error(struct mylite_db *database, int rc) {
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}
