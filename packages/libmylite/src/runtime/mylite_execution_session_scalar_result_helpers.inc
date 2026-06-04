
static struct mylite_result_cell session_scalar_cell_result_cell(
    const struct session_scalar_cell *cell
) {
    size_t value_size = 0U;

    if (cell == NULL || cell->value == NULL) {
        return (struct mylite_result_cell){.is_null = true};
    }
    value_size = strlen(cell->value);
    if (cell->has_value_size) {
        value_size = cell->value_size;
    }

    return (struct mylite_result_cell){
        .bytes = cell->value,
        .size = value_size,
        .is_null = false,
    };
}

static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    unwrapped = unwrap_parenthesized_expression(expression);
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(unwrapped, 0U));

        while (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
               mylite_sql_ast_node_operator(literal) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        }
        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
            enum mylite_sql_ast_literal_kind literal_kind =
                mylite_sql_ast_node_literal_kind(literal);

            if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
                return copy_source_span_text(database, &literal->span, out_text);
            }
        }
    }
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(unwrapped);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return copy_source_span_text(database, &unwrapped->span, out_text);
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            size_t text_length = 0U;

            return decode_sql_string_literal_with_policy(
                database,
                unwrapped,
                "SELECT string literal labels support only ordinary string literals",
                "SELECT string literal labels do not support NUL bytes",
                true,
                out_text,
                &text_length
            );
        }
    }

    return copy_source_span_text(database, &expression->span, out_text);
}
