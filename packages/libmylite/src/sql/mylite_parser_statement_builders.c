#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SCRIPT,
        (struct mylite_sql_source_span){0}
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script_with_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_ast_node *script = mylite_sql_parser_make_script(state);
    if (script == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(script, statement->span);
    }
    return script;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *script,
    struct mylite_sql_ast_node *statement
) {
    if (!mylite_sql_parser_is_parse_ok(state) || script == NULL) {
        return script;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(
            script,
            mylite_sql_parser_span_join(script->span, statement->span)
        );
    }
    return script;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token table_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    return mylite_sql_parser_make_select_statement(
        state,
        table_token,
        mylite_sql_parser_make_wildcard_select_list(state, table_token),
        mylite_sql_parser_make_from_table(state, table_token, table_name, NULL, NULL),
        NULL,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token explain_token,
    struct mylite_sql_ast_node *format,
    struct mylite_sql_ast_node *analyze,
    struct mylite_sql_ast_node *statement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&explain_token);
    struct mylite_sql_ast_node *explain = NULL;

    if (format != NULL) {
        span = mylite_sql_parser_span_join(span, format->span);
    }
    if (analyze != NULL) {
        span = mylite_sql_parser_span_join(span, analyze->span);
    }
    if (statement != NULL) {
        span = mylite_sql_parser_span_join(span, statement->span);
    }

    explain = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_EXPLAIN_STATEMENT, span);
    if (explain == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(explain, format);
    mylite_sql_ast_node_append_child(explain, analyze);
    mylite_sql_ast_node_append_child(explain, statement);
    return explain;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_format(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token format_token,
    struct mylite_sql_ast_node *format_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&format_token);
    struct mylite_sql_ast_node *format = NULL;

    if (format_name != NULL) {
        span = mylite_sql_parser_span_join(span, format_name->span);
    }
    format = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_EXPLAIN_FORMAT, span);
    if (format == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(format, format_name);
    return format;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_explain_analyze(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token analyze_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_EXPLAIN_ANALYZE,
        mylite_sql_parser_span_from_token(&analyze_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement_with_modifiers(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_select_modifiers modifiers,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause,
    struct mylite_sql_select_locking_clause locking_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, modifiers.duplicate_modifier);
    mylite_sql_ast_node_set_select_options(statement, modifiers.options);
    mylite_sql_ast_node_set_select_calc_found_rows(statement, modifiers.calc_found_rows);
    mylite_sql_ast_node_set_select_locking_clause(statement, locking_clause.kind);
    if (statement != NULL && locking_clause.kind != MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        mylite_sql_ast_node_set_span(
            statement,
            mylite_sql_parser_span_join(statement->span, locking_clause.span)
        );
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_select_window_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *window_clause
) {
    (void)state;

    if (statement == NULL || window_clause == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, window_clause);
    mylite_sql_ast_node_set_span(
        statement,
        mylite_sql_parser_span_join(statement->span, window_clause->span)
    );
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_select_into_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *into_clause
) {
    (void)state;

    if (statement == NULL || into_clause == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, into_clause);
    mylite_sql_ast_node_set_span(
        statement,
        mylite_sql_parser_span_join(statement->span, into_clause->span)
    );
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_distinct_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_calc_found_rows_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_calc_found_rows(statement, 1);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&select_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_list != NULL) {
        span = mylite_sql_parser_span_join(span, select_list->span);
    }
    if (from_clause != NULL) {
        span = mylite_sql_parser_span_join(span, from_clause->span);
    }
    if (where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, where_clause->span);
    }
    if (group_clause != NULL) {
        span = mylite_sql_parser_span_join(span, group_clause->span);
    }
    if (having_clause != NULL) {
        span = mylite_sql_parser_span_join(span, having_clause->span);
    }
    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, select_list);
    mylite_sql_ast_node_append_child(statement, from_clause);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, group_clause);
    mylite_sql_ast_node_append_child(statement, having_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_with_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_ast_node *with_clause,
    struct mylite_sql_ast_node *query,
    struct mylite_sql_ast_node *order_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&with_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    } else if (query != NULL) {
        span = mylite_sql_parser_span_join(span, query->span);
    } else if (with_clause != NULL) {
        span = mylite_sql_parser_span_join(span, with_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, with_clause);
    mylite_sql_ast_node_append_child(statement, query);
    mylite_sql_ast_node_append_child(statement, order_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_with_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_ast_node *with_clause = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_WITH_CLAUSE,
        expression != NULL ? expression->span : (struct mylite_sql_source_span){0}
    );

    if (with_clause != NULL) {
        mylite_sql_ast_node_append_child(with_clause, expression);
    }
    return with_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_common_table_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *with_clause,
    struct mylite_sql_ast_node *expression
) {
    (void)state;
    if (with_clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(with_clause, expression);
    if (expression != NULL) {
        with_clause->span = mylite_sql_parser_span_join(with_clause->span, expression->span);
    }
    return with_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_common_table_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *query,
    struct mylite_sql_token right_parenthesis
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&right_parenthesis);
    struct mylite_sql_ast_node *expression = NULL;

    if (name != NULL) {
        span = mylite_sql_parser_span_join(name->span, span);
    } else if (query != NULL) {
        span = mylite_sql_parser_span_join(query->span, span);
    }
    expression = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COMMON_TABLE_EXPRESSION, span);
    if (expression != NULL) {
        mylite_sql_ast_node_append_child(expression, name);
        mylite_sql_ast_node_append_child(expression, query);
    }
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_compound_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *first_select,
    struct mylite_sql_ast_node *terms
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *statement = NULL;

    if (first_select != NULL) {
        span = first_select->span;
    }
    if (terms != NULL) {
        span = first_select == NULL ? terms->span : mylite_sql_parser_span_join(span, terms->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, first_select);
    mylite_sql_ast_node_append_child(statement, terms);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_query_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *inner_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&left_parenthesis),
        mylite_sql_parser_span_from_token(&right_parenthesis)
    );
    struct mylite_sql_ast_node *expression = NULL;

    if (inner_statement != NULL) {
        span = mylite_sql_parser_span_join(span, inner_statement->span);
    }
    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    expression =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_PARENTHESIZED_QUERY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, inner_statement);
    if (order_clause != NULL) {
        mylite_sql_ast_node_append_child(expression, order_clause);
    }
    if (limit_clause != NULL) {
        mylite_sql_ast_node_append_child(expression, limit_clause);
    }
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&values_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = mylite_sql_parser_span_join(span, rows->span);
    }
    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *term
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *list = NULL;

    if (term != NULL) {
        span = term->span;
    }
    list = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UNION_TERM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, term);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *terms,
    struct mylite_sql_ast_node *term
) {
    (void)state;

    if (terms == NULL || term == NULL) {
        return terms;
    }

    mylite_sql_ast_node_append_child(terms, term);
    mylite_sql_ast_node_set_span(terms, mylite_sql_parser_span_join(terms->span, term->span));
    return terms;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
) {
    return mylite_sql_parser_make_set_operation_term(
        state,
        union_token,
        MYLITE_SQL_AST_SET_OPERATOR_UNION,
        modifier,
        select_statement
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_operation_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_set_operator operator_kind,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&operator_token);
    struct mylite_sql_ast_node *term = NULL;

    if (select_statement != NULL) {
        span = mylite_sql_parser_span_join(span, select_statement->span);
    }

    term = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UNION_TERM, span);
    if (term == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_set_operator_kind(term, operator_kind);
    mylite_sql_ast_node_set_union_modifier(term, modifier);
    mylite_sql_ast_node_append_child(term, select_statement);
    return term;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token do_token,
    struct mylite_sql_ast_node *expression_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&do_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (expression_list != NULL) {
        span = mylite_sql_parser_span_join(span, expression_list->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DO_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, expression_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_expression_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DO_EXPRESSION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, expression);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_do_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *expression
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, expression);
    if (expression != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, expression->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_use_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token use_token,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&use_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_USE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_begin_immediate_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token begin_token,
    struct mylite_sql_token immediate_token
) {
    if (!mylite_sql_parser_token_text_equals(&immediate_token, "IMMEDIATE")) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, immediate_token);
        return NULL;
    }

    return mylite_sql_parser_make_transaction_control_statement(
        state,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        begin_token,
        immediate_token,
        NULL
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_control_statement_with_completion(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_transaction_control_tokens tokens,
    struct mylite_sql_transaction_completion completion
) {
    struct mylite_sql_token last_token =
        completion.has_completion ? completion.last_token : tokens.statement_token;
    struct mylite_sql_ast_node *chain_completion = NULL;

    if (!completion.has_completion && tokens.work_token.text != NULL) {
        last_token = tokens.work_token;
    }
    if (completion.chain) {
        struct mylite_sql_source_span span = mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&completion.chain_start_token),
            mylite_sql_parser_span_from_token(&completion.chain_end_token)
        );

        chain_completion =
            mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TRANSACTION_CHAIN_COMPLETION, span);
        if (chain_completion == NULL) {
            return NULL;
        }
    }
    return mylite_sql_parser_make_transaction_control_statement(
        state,
        statement_kind,
        tokens.statement_token,
        last_token,
        chain_completion
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&first_token),
        mylite_sql_parser_span_from_token(&last_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = mylite_sql_parser_span_join(span, characteristics->span);
    }

    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_transaction_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = mylite_sql_parser_span_join(span, characteristics->span);
    } else if (scope != NULL) {
        span = mylite_sql_parser_span_join(span, scope->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *characteristic
) {
    struct mylite_sql_source_span span =
        characteristic == NULL ? (struct mylite_sql_source_span){0} : characteristic->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *characteristic
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    if (characteristic != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, characteristic->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&first_token),
        mylite_sql_parser_span_from_token(&last_token)
    );

    return mylite_sql_parser_make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_savepoint_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *savepoint_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (savepoint_name != NULL) {
        span = mylite_sql_parser_span_join(span, savepoint_name->span);
    }

    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, savepoint_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_maintenance_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *table_names,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = mylite_sql_parser_span_join(span, table_names->span);
    }
    if (option != NULL) {
        span = mylite_sql_parser_span_join(span, option->span);
    }

    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(statement, option);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_maintenance_option(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind option_kind,
    struct mylite_sql_token option_token
) {
    return mylite_sql_parser_make_node(
        state,
        option_kind,
        mylite_sql_parser_span_from_token(&option_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token lock_token,
    struct mylite_sql_ast_node *targets
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&lock_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (targets != NULL) {
        span = mylite_sql_parser_span_join(span, targets->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, targets);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unlock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unlock_token,
    struct mylite_sql_token table_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&unlock_token),
        mylite_sql_parser_span_from_token(&table_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target
) {
    struct mylite_sql_source_span span =
        target == NULL ? (struct mylite_sql_source_span){0} : target->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, target);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *target
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, target);
    if (target != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, target->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *lock_type
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *target = NULL;

    if (lock_type != NULL) {
        span = mylite_sql_parser_span_join(span, lock_type->span);
    } else if (alias != NULL) {
        span = mylite_sql_parser_span_join(span, alias->span);
    }

    target = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, table_name);
    mylite_sql_ast_node_append_child(target, alias);
    mylite_sql_ast_node_append_child(target, lock_type);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_type(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    const struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&first_token),
        mylite_sql_parser_span_from_token(&last_token)
    );

    return mylite_sql_parser_make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (collation_name != NULL) {
        span = mylite_sql_parser_span_join(span, collation_name->span);
    } else if (charset_name != NULL) {
        span = mylite_sql_parser_span_join(span, charset_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_NAMES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    mylite_sql_ast_node_append_child(statement, collation_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_attach_set_tail_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_ast_node *assignment_list
) {
    (void)state;

    if (statement == NULL || assignment_list == NULL) {
        return statement;
    }
    mylite_sql_ast_node_append_child(statement, assignment_list);
    mylite_sql_ast_node_set_span(
        statement,
        mylite_sql_parser_span_join(statement->span, assignment_list->span)
    );
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (charset_name != NULL) {
        span = mylite_sql_parser_span_join(span, charset_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_default_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        mylite_sql_parser_span_from_token(&default_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = mylite_sql_parser_span_join(span, assignments->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, assignments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, assignment->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? mylite_sql_parser_span_from_token(&operator_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    assignment = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_system_variable_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span =
        scope == NULL ? (struct mylite_sql_source_span){0} : scope->span;
    struct mylite_sql_ast_node *target = NULL;

    if (name != NULL) {
        span = scope == NULL ? name->span : mylite_sql_parser_span_join(span, name->span);
    }

    target = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, scope);
    mylite_sql_ast_node_append_child(target, name);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SET_DEFAULT_VALUE,
        mylite_sql_parser_span_from_token(&default_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_USER_VARIABLE,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable_assignment_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? mylite_sql_parser_span_from_token(&operator_token) : target->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    expression = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_EXPRESSION,
        span
    );
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, target);
    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_into_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
) {
    struct mylite_sql_source_span span =
        variable == NULL ? (struct mylite_sql_source_span){0} : variable->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SELECT_INTO_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, variable);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_select_into_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, variable);
    if (variable != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, variable->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token prepare_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *source
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&prepare_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source != NULL) {
        span = mylite_sql_parser_span_join(span, source->span);
    } else if (name != NULL) {
        span = mylite_sql_parser_span_join(span, name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, source);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token execute_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *using_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&execute_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (using_list != NULL) {
        span = mylite_sql_parser_span_join(span, using_list->span);
    } else if (name != NULL) {
        span = mylite_sql_parser_span_join(span, name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_EXECUTE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, using_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_using_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
) {
    struct mylite_sql_source_span span =
        variable == NULL ? (struct mylite_sql_source_span){0} : variable->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_EXECUTE_USING_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, variable);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_execute_using_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, variable);
    if (variable != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, variable->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_deallocate_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = mylite_sql_parser_span_join(span, name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}
