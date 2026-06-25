#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

static bool generic_function_ip_address_kinds(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind,
    enum mylite_sql_ast_node_kind *out_error_kind
);
static bool generic_function_uuid_short_kinds(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind,
    enum mylite_sql_ast_node_kind *out_error_kind
);
static bool generic_function_statistical_aggregate_kind(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind
);

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_IDENTIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_ignore_space_sensitive_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    if (mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, token);
        return NULL;
    }
    return mylite_sql_parser_make_identifier(state, token);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? (struct mylite_sql_source_span){0} : left->span;
    struct mylite_sql_ast_node *identifier = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    identifier = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, span);
    if (identifier == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(identifier, left);
    mylite_sql_ast_node_append_child(identifier, right);
    return identifier;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *qualifier,
    struct mylite_sql_token token
) {
    struct mylite_sql_ast_node *wildcard = NULL;
    struct mylite_sql_source_span span =
        qualifier == NULL ? mylite_sql_parser_span_from_token(&token) : qualifier->span;
    struct mylite_sql_ast_node *qualified = NULL;

    wildcard = mylite_sql_parser_make_wildcard(state, token);
    if (wildcard != NULL) {
        span = mylite_sql_parser_span_join(span, wildcard->span);
    }

    qualified = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_QUALIFIED_WILDCARD, span);
    if (qualified == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(qualified, qualifier);
    mylite_sql_ast_node_append_child(qualified, wildcard);
    return qualified;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_WILDCARD,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_literal(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_literal_kind literal_kind
) {
    struct mylite_sql_ast_node *literal = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_LITERAL,
        mylite_sql_parser_span_from_token(&token)
    );
    if (literal == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_literal_kind(literal, literal_kind);
    return literal;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_string_literal_segment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token segment
) {
    struct mylite_sql_ast_node *right =
        mylite_sql_parser_make_literal(state, segment, MYLITE_SQL_AST_LITERAL_STRING);
    struct mylite_sql_ast_node *literal = left;

    if (right == NULL) {
        return NULL;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_STRING, segment);
        return NULL;
    }
    if (literal->first_child == NULL) {
        struct mylite_sql_ast_node *first = literal;

        literal = mylite_sql_parser_make_node(
            state,
            MYLITE_SQL_AST_LITERAL,
            mylite_sql_parser_span_join(first->span, right->span)
        );
        if (literal == NULL) {
            return NULL;
        }
        mylite_sql_ast_node_set_literal_kind(literal, MYLITE_SQL_AST_LITERAL_STRING);
        mylite_sql_ast_node_append_child(literal, first);
    } else {
        mylite_sql_ast_node_set_span(
            literal,
            mylite_sql_parser_span_join(literal->span, right->span)
        );
    }
    mylite_sql_ast_node_append_child(literal, right);
    return literal;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_dml_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_DML_DEFAULT_VALUE,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_system_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SYSTEM_VARIABLE,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *operand
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&operator_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (operand != NULL) {
        span = mylite_sql_parser_span_join(span, operand->span);
    }

    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UNARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, operand);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_BINARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, left);
    mylite_sql_ast_node_append_child(expression, right);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_member_of_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token member_token,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&member_token) : left->span;
    struct mylite_sql_ast_node *expression = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&right_paren));
    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_JSON_MEMBER_OF_FUNCTION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, left);
    mylite_sql_ast_node_append_child(expression, right);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_cast_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token cast_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&cast_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *expression =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CAST_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token binary_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&binary_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CAST_BINARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&convert_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *expression =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_binary_type_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&convert_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *expression =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_charset_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *charset,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&convert_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *expression =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    mylite_sql_ast_node_append_child(expression, charset);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_collate_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&collate_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(value->span, span);
    }
    if (collation != NULL) {
        span = mylite_sql_parser_span_join(span, collation->span);
    }

    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLLATE_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    mylite_sql_ast_node_append_child(expression, collation);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *parenthesized = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&left_paren),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (parenthesized == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(parenthesized, expression);
    return parenthesized;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_scalar_subquery_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *subquery = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&left_paren),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (subquery == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(subquery, select_statement);
    return subquery;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&case_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *case_value,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&case_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, case_value);
    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *when_clause
) {
    struct mylite_sql_source_span span =
        when_clause == NULL ? (struct mylite_sql_source_span){0} : when_clause->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CASE_WHEN_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_case_when(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *when_clause
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    if (when_clause != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, when_clause->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token when_token,
    struct mylite_sql_ast_node *condition,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&when_token);
    struct mylite_sql_ast_node *when_clause = NULL;

    if (result != NULL) {
        span = mylite_sql_parser_span_join(span, result->span);
    } else if (condition != NULL) {
        span = mylite_sql_parser_span_join(span, condition->span);
    }

    when_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CASE_WHEN_CLAUSE, span);
    if (when_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(when_clause, condition);
    mylite_sql_ast_node_append_child(when_clause, result);
    return when_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_else_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token else_token,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&else_token);
    struct mylite_sql_ast_node *else_clause = NULL;

    if (result != NULL) {
        span = mylite_sql_parser_span_join(span, result->span);
    }

    else_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CASE_ELSE_CLAUSE, span);
    if (else_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(else_clause, result);
    return else_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    return mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_zero_argument_function(
        state,
        function_token,
        function_kind,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_one_argument_function(
        state,
        function_token,
        function_kind,
        argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_concat_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *separator,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    function = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, order_clause);
    mylite_sql_ast_node_append_child(function, separator);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_concat_distinct_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    const struct mylite_sql_token *distinct_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *separator,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = mylite_sql_parser_make_group_concat_function(
        state,
        function_token,
        left_paren,
        value,
        order_clause,
        separator,
        right_paren
    );
    struct mylite_sql_ast_node *distinct = NULL;

    if (function == NULL) {
        return NULL;
    }
    distinct = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_GROUP_CONCAT_DISTINCT_MODIFIER,
        mylite_sql_parser_span_from_token(distinct_token)
    );
    if (distinct == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, distinct);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_aggregate_distinct_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *function,
    const struct mylite_sql_token *distinct_token
) {
    struct mylite_sql_ast_node *distinct = NULL;

    if (function == NULL) {
        return NULL;
    }
    distinct = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_AGGREGATE_DISTINCT_MODIFIER,
        mylite_sql_parser_span_from_token(distinct_token)
    );
    if (distinct == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, distinct);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_function_window_clause(
    struct mylite_sql_ast_node *function,
    struct mylite_sql_ast_node *window_clause
) {
    if (function == NULL || window_clause == NULL) {
        return function;
    }
    mylite_sql_ast_node_append_child(function, window_clause);
    mylite_sql_ast_node_set_span(
        function,
        mylite_sql_parser_span_join(function->span, window_clause->span)
    );
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_row_number_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *window_clause
) {
    return mylite_sql_parser_make_window_function_with_clause(
        state,
        function_token,
        MYLITE_SQL_AST_ROW_NUMBER_FUNCTION,
        (struct mylite_sql_window_function_arguments){0},
        window_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *window_clause
) {
    return mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
        state,
        function_token,
        function_kind,
        arguments,
        NULL,
        window_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_function_with_clause_and_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_window_function_arguments arguments,
    struct mylite_sql_ast_node *null_treatment,
    struct mylite_sql_ast_node *window_clause
) {
    struct mylite_sql_ast_node *argument_list = NULL;
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&function_token);
    struct mylite_sql_ast_node *function = NULL;

    if (arguments.count > sizeof(arguments.items) / sizeof(arguments.items[0])) {
        return NULL;
    }
    if (arguments.count != 0U) {
        argument_list = mylite_sql_parser_make_function_argument_list(state, arguments.items[0]);
    }
    for (size_t argument_index = 1U; argument_list != NULL && argument_index < arguments.count;
         ++argument_index) {
        argument_list = mylite_sql_parser_append_function_argument(
            state,
            argument_list,
            arguments.items[argument_index]
        );
    }
    if (arguments.count != 0U && argument_list == NULL) {
        return NULL;
    }
    if (null_treatment != NULL) {
        span = mylite_sql_parser_span_join(span, null_treatment->span);
    }
    if (window_clause != NULL) {
        span = mylite_sql_parser_span_join(span, window_clause->span);
    }

    function = mylite_sql_parser_make_node(state, function_kind, span);
    if (function == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(function, argument_list);
    mylite_sql_ast_node_append_child(function, window_clause);
    if (null_treatment != NULL) {
        mylite_sql_ast_node_append_child(function, null_treatment);
    }
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_null_treatment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token treatment_token,
    enum mylite_sql_ast_node_kind treatment_kind,
    struct mylite_sql_token nulls_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&treatment_token),
        mylite_sql_parser_span_from_token(&nulls_token)
    );

    return mylite_sql_parser_make_node(state, treatment_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_empty_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token over_token,
    struct mylite_sql_token right_paren
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_WINDOW_SPEC,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&over_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_spec(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *window_reference,
    struct mylite_sql_ast_node *partition_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *frame_clause
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *spec = NULL;

    if (window_reference != NULL) {
        span = window_reference->span;
    }
    if (partition_clause != NULL) {
        span = window_reference == NULL ? partition_clause->span
                                        : mylite_sql_parser_span_join(span, partition_clause->span);
    }
    if (order_clause != NULL) {
        span = window_reference == NULL && partition_clause == NULL
                   ? order_clause->span
                   : mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (frame_clause != NULL) {
        span = window_reference == NULL && partition_clause == NULL && order_clause == NULL
                   ? frame_clause->span
                   : mylite_sql_parser_span_join(span, frame_clause->span);
    }

    spec = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_SPEC, span);
    if (spec == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(spec, window_reference);
    mylite_sql_ast_node_append_child(spec, partition_clause);
    mylite_sql_ast_node_append_child(spec, order_clause);
    mylite_sql_ast_node_append_child(spec, frame_clause);
    return spec;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_partition_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token partition_token,
    struct mylite_sql_ast_node *key_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&partition_token);
    struct mylite_sql_ast_node *clause = NULL;
    struct mylite_sql_ast_node *key = key_list;

    if (key_list != NULL) {
        span = mylite_sql_parser_span_join(span, key_list->span);
    }
    if (key_list != NULL && key_list->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST &&
        mylite_sql_ast_node_child_count(key_list) == 1U) {
        key = mylite_sql_parser_child_at(key_list, 0U);
    }
    clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(clause, key);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_order_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&order_token);
    struct mylite_sql_ast_node *clause = NULL;
    struct mylite_sql_ast_node *order_child = order_list;

    if (order_list != NULL) {
        span = mylite_sql_parser_span_join(span, order_list->span);
    }
    if (order_list != NULL && order_list->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST &&
        mylite_sql_ast_node_child_count(order_list) == 1U) {
        order_child = mylite_sql_parser_child_at(order_list, 0U);
    }
    clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    if (order_child != NULL && order_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM) {
        struct mylite_sql_ast_node *key = mylite_sql_parser_child_at(order_child, 0U);
        struct mylite_sql_ast_node *direction = mylite_sql_parser_child_at(order_child, 1U);

        mylite_sql_ast_node_append_child(clause, key);
        mylite_sql_ast_node_append_child(clause, direction);
    } else {
        mylite_sql_ast_node_append_child(clause, order_child);
    }
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_ast_node *reference = NULL;

    if (name == NULL) {
        return NULL;
    }
    reference = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_REFERENCE, name->span);
    if (reference == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(reference, name);
    return reference;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *definition
) {
    struct mylite_sql_ast_node *list = NULL;

    if (definition == NULL) {
        return NULL;
    }
    list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_DEFINITION_LIST, definition->span);
    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, definition);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *definition
) {
    (void)state;

    if (list == NULL || definition == NULL) {
        return list;
    }
    mylite_sql_ast_node_append_child(list, definition);
    mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, definition->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *spec
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *definition = NULL;

    if (name != NULL) {
        span = name->span;
    }
    if (spec != NULL) {
        span = name == NULL ? spec->span : mylite_sql_parser_span_join(span, spec->span);
    }
    definition = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(definition, name);
    mylite_sql_ast_node_append_child(definition, spec);
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token frame_token,
    struct mylite_sql_ast_node *first_bound,
    struct mylite_sql_ast_node *second_bound
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&frame_token);
    struct mylite_sql_ast_node *clause = NULL;

    if (first_bound != NULL) {
        span = mylite_sql_parser_span_join(span, first_bound->span);
    }
    if (second_bound != NULL) {
        span = mylite_sql_parser_span_join(span, second_bound->span);
    }
    clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_set_window_frame_unit(
        clause,
        frame_token.length == 4U ? MYLITE_SQL_AST_WINDOW_FRAME_UNIT_ROWS
                                 : MYLITE_SQL_AST_WINDOW_FRAME_UNIT_RANGE
    );
    mylite_sql_ast_node_append_child(clause, first_bound);
    mylite_sql_ast_node_append_child(clause, second_bound);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_window_frame_bound(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token bound_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&bound_token);
    struct mylite_sql_ast_node *bound = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(value->span, span);
    }
    bound = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WINDOW_FRAME_BOUND, span);
    if (bound == NULL) {
        return NULL;
    }
    if (bound_token.text != NULL && (bound_token.text[0] == 'C' || bound_token.text[0] == 'c')) {
        mylite_sql_ast_node_set_window_frame_bound_kind(
            bound,
            MYLITE_SQL_AST_WINDOW_FRAME_BOUND_CURRENT_ROW
        );
    } else if (bound_token.text != NULL &&
               (bound_token.text[0] == 'P' || bound_token.text[0] == 'p')) {
        mylite_sql_ast_node_set_window_frame_bound_kind(
            bound,
            value == NULL ? MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_PRECEDING
                          : MYLITE_SQL_AST_WINDOW_FRAME_BOUND_VALUE_PRECEDING
        );
    } else if (bound_token.text != NULL &&
               (bound_token.text[0] == 'F' || bound_token.text[0] == 'f')) {
        mylite_sql_ast_node_set_window_frame_bound_kind(
            bound,
            value == NULL ? MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_FOLLOWING
                          : MYLITE_SQL_AST_WINDOW_FRAME_BOUND_VALUE_FOLLOWING
        );
    }
    mylite_sql_ast_node_append_child(bound, value);
    return bound;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_two_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_three_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        third_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_trim_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *remove_string,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, remove_string);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *error = NULL;

    error = mylite_sql_parser_make_node(
        state,
        error_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (error == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(error, arguments);
    return error;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_function_argument_count_error(
        state,
        function_token,
        error_kind,
        arguments,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_four_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_ast_node *fourth_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    mylite_sql_ast_node_append_child(function, fourth_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_list_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, arguments);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_char_using_charset_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_ast_node *charset,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CHAR_FUNCTION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, arguments);
    mylite_sql_ast_node_append_child(function, charset);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;
    struct mylite_sql_ast_node *name = NULL;
    enum mylite_sql_ast_node_kind specialized_function_kind = MYLITE_SQL_AST_SCRIPT;
    enum mylite_sql_ast_node_kind specialized_error_kind = MYLITE_SQL_AST_SCRIPT;

    if (generic_function_statistical_aggregate_kind(&function_token, &specialized_function_kind) &&
        arguments != NULL && mylite_sql_ast_node_child_count(arguments) == 1U) {
        return mylite_sql_parser_make_one_argument_function(
            state,
            function_token,
            specialized_function_kind,
            arguments->first_child,
            right_paren
        );
    }

    if (generic_function_ip_address_kinds(
            &function_token,
            &specialized_function_kind,
            &specialized_error_kind
        )) {
        if (arguments == NULL || mylite_sql_ast_node_child_count(arguments) != 1U) {
            return mylite_sql_parser_make_function_argument_count_error(
                state,
                function_token,
                specialized_error_kind,
                arguments,
                right_paren
            );
        }
        return mylite_sql_parser_make_one_argument_function(
            state,
            function_token,
            specialized_function_kind,
            arguments->first_child,
            right_paren
        );
    }
    if (generic_function_uuid_short_kinds(
            &function_token,
            &specialized_function_kind,
            &specialized_error_kind
        )) {
        if (arguments != NULL && mylite_sql_ast_node_child_count(arguments) != 0U) {
            return mylite_sql_parser_make_function_argument_count_error(
                state,
                function_token,
                specialized_error_kind,
                arguments,
                right_paren
            );
        }
        return mylite_sql_parser_make_zero_argument_function(
            state,
            function_token,
            specialized_function_kind,
            right_paren
        );
    }

    name = mylite_sql_parser_make_identifier(state, function_token);
    if (name == NULL) {
        return NULL;
    }

    function = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_GENERIC_FUNCTION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, name);
    mylite_sql_ast_node_append_child(function, arguments);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_statistical_aggregate_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    enum mylite_sql_ast_node_kind function_kind = MYLITE_SQL_AST_SCRIPT;

    if (!generic_function_statistical_aggregate_kind(&function_token, &function_kind)) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }
    return mylite_sql_parser_make_one_argument_function(
        state,
        function_token,
        function_kind,
        argument,
        right_paren
    );
}

static bool generic_function_statistical_aggregate_kind(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind
) {
    if (function_token == NULL || out_function_kind == NULL) {
        return false;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "STD")) {
        *out_function_kind = MYLITE_SQL_AST_STD_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "STDDEV")) {
        *out_function_kind = MYLITE_SQL_AST_STDDEV_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "STDDEV_POP")) {
        *out_function_kind = MYLITE_SQL_AST_STDDEV_POP_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "STDDEV_SAMP")) {
        *out_function_kind = MYLITE_SQL_AST_STDDEV_SAMP_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "VAR_POP")) {
        *out_function_kind = MYLITE_SQL_AST_VAR_POP_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "VAR_SAMP")) {
        *out_function_kind = MYLITE_SQL_AST_VAR_SAMP_AGGREGATE_FUNCTION;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "VARIANCE")) {
        *out_function_kind = MYLITE_SQL_AST_VARIANCE_AGGREGATE_FUNCTION;
        return true;
    }
    return false;
}

static bool generic_function_ip_address_kinds(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind,
    enum mylite_sql_ast_node_kind *out_error_kind
) {
    if (function_token == NULL || out_function_kind == NULL || out_error_kind == NULL) {
        return false;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "INET_ATON")) {
        *out_function_kind = MYLITE_SQL_AST_INET_ATON_FUNCTION;
        *out_error_kind = MYLITE_SQL_AST_INET_ATON_ARGUMENT_COUNT_ERROR;
        return true;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "INET_NTOA")) {
        *out_function_kind = MYLITE_SQL_AST_INET_NTOA_FUNCTION;
        *out_error_kind = MYLITE_SQL_AST_INET_NTOA_ARGUMENT_COUNT_ERROR;
        return true;
    }
    return false;
}

static bool generic_function_uuid_short_kinds(
    const struct mylite_sql_token *function_token,
    enum mylite_sql_ast_node_kind *out_function_kind,
    enum mylite_sql_ast_node_kind *out_error_kind
) {
    if (function_token == NULL || out_function_kind == NULL || out_error_kind == NULL) {
        return false;
    }
    if (mylite_sql_parser_token_text_equals(function_token, "UUID_SHORT")) {
        *out_function_kind = MYLITE_SQL_AST_UUID_SHORT_FUNCTION;
        *out_error_kind = MYLITE_SQL_AST_UUID_SHORT_ARGUMENT_COUNT_ERROR;
        return true;
    }
    return false;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generic_function_with_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *window_clause
) {
    struct mylite_sql_ast_node *function = NULL;

    if (window_clause != NULL &&
        !mylite_sql_parser_token_text_is_generic_aggregate_window_function_name(&function_token)) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }

    if (mylite_sql_parser_token_text_equals(&function_token, "ROW") ||
        (function_token.flags & MYLITE_SQL_TOKEN_SYNTHETIC_ROW_CONSTRUCTOR) != 0U) {
        return mylite_sql_parser_make_row_constructor(
            state,
            function_token,
            arguments,
            right_paren
        );
    }

    function =
        mylite_sql_parser_make_generic_function(state, function_token, arguments, right_paren);
    return mylite_sql_parser_attach_function_window_clause(function, window_clause);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_row_constructor(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *constructor = NULL;
    struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;

    for (argument = arguments == NULL ? NULL : arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        ++argument_count;
    }
    if (argument_count < 2U) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }

    constructor = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ROW_CONSTRUCTOR,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&start_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );
    if (constructor == NULL) {
        return NULL;
    }

    argument = arguments == NULL ? NULL : arguments->first_child;
    while (argument != NULL) {
        struct mylite_sql_ast_node *next = argument->next_sibling;

        mylite_sql_ast_node_append_child(constructor, argument);
        argument = next;
    }
    return constructor;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument
) {
    struct mylite_sql_source_span span =
        argument == NULL ? (struct mylite_sql_source_span){0} : argument->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, argument);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_prepend_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_ast_node *list
) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return list;
    }
    if (list == NULL) {
        return mylite_sql_parser_make_function_argument_list(state, argument);
    }
    if (argument == NULL) {
        return list;
    }

    argument->next_sibling = list->first_child;
    list->first_child = argument;
    if (list->last_child == NULL) {
        list->last_child = argument;
    }
    mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(argument->span, list->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *argument
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, argument);
    if (argument != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, argument->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        mylite_sql_parser_span_from_token(&current_user_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_timestamp_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        mylite_sql_parser_span_from_token(&current_timestamp_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_date_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        mylite_sql_parser_span_from_token(&current_date_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_time_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        mylite_sql_parser_span_from_token(&current_time_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
) {
    struct mylite_sql_ast_node *function = NULL;

    function = mylite_sql_parser_make_node(
        state,
        function_kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&function_token),
            mylite_sql_parser_span_from_token(&precision.end_token)
        )
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_temporal_fractional_precision(
        function,
        (struct mylite_sql_ast_temporal_fractional_precision_payload){
            .has_precision = precision.has_precision,
            .precision_span = mylite_sql_parser_span_from_token(&precision.precision_token),
        }
    );
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_temporal_value_with_precision(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_temporal_fractional_precision_tokens precision
) {
    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_temporal_value_with_precision(
        state,
        function_token,
        function_kind,
        precision
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_date_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_UTC_DATE_VALUE,
        mylite_sql_parser_span_from_token(&utc_date_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_time_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_UTC_TIME_VALUE,
        mylite_sql_parser_span_from_token(&utc_time_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_timestamp_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        mylite_sql_parser_span_from_token(&utc_timestamp_token)
    );
}
