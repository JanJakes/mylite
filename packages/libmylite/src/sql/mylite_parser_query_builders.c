#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token wildcard_token
) {
    return mylite_sql_parser_make_select_list(
        state,
        mylite_sql_parser_make_select_item(
            state,
            mylite_sql_parser_make_wildcard(state, wildcard_token),
            NULL
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SELECT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_ast_node *alias
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    if (alias != NULL) {
        mylite_sql_ast_node_set_span(item, mylite_sql_parser_span_join(span, alias->span));
    }
    mylite_sql_ast_node_append_child(item, expression);
    mylite_sql_ast_node_append_child(item, alias);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_token dual_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_FROM_DUAL,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&from_token),
            mylite_sql_parser_span_from_token(&dual_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&from_token);
    struct mylite_sql_ast_node *from_table = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }
    if (alias != NULL) {
        span = mylite_sql_parser_span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = mylite_sql_parser_span_join(span, index_hints->span);
    }

    from_table = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span =
        table_name != NULL ? table_name->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *from_table = NULL;

    if (alias != NULL) {
        span = mylite_sql_parser_span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = mylite_sql_parser_span_join(span, index_hints->span);
    }

    from_table = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_derived_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_parenthesis,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_parenthesis,
    struct mylite_sql_ast_node *alias
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&left_parenthesis),
        mylite_sql_parser_span_from_token(&right_parenthesis)
    );
    struct mylite_sql_ast_node *derived = NULL;

    if (select_statement != NULL) {
        span = mylite_sql_parser_span_join(span, select_statement->span);
    }
    if (alias != NULL) {
        span = mylite_sql_parser_span_join(span, alias->span);
    }

    derived = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FROM_DERIVED, span);
    if (derived == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(derived, select_statement);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(derived, alias);
    }
    return derived;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_join(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&from_token);
    struct mylite_sql_ast_node *join = NULL;

    if (left != NULL) {
        span = mylite_sql_parser_span_join(span, left->span);
    }
    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }
    if (condition != NULL) {
        span = mylite_sql_parser_span_join(span, condition->span);
    }

    join = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_join_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span =
        left != NULL ? left->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *join = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }
    if (condition != NULL) {
        span = mylite_sql_parser_span_join(span, condition->span);
    }

    join = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_join_using_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_parenthesis
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&using_token),
        mylite_sql_parser_span_from_token(&right_parenthesis)
    );
    struct mylite_sql_ast_node *clause = NULL;

    if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    }

    clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_JOIN_USING_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, columns);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *hint
) {
    struct mylite_sql_source_span span =
        hint != NULL ? hint->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_HINT_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, hint);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_hint(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *hint
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, hint);
    if (hint != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, hint->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *names,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *hint = mylite_sql_parser_make_node(
        state,
        kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&start_token),
            mylite_sql_parser_span_from_token(&right_paren)
        )
    );

    if (hint == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(hint, scope);
    mylite_sql_ast_node_append_child(hint, names);
    return hint;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_scope(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token for_token,
    struct mylite_sql_token last_token
) {
    return mylite_sql_parser_make_node(
        state,
        kind,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&for_token),
            mylite_sql_parser_span_from_token(&last_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_where_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token where_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&where_token);
    struct mylite_sql_ast_node *where_clause = NULL;

    if (predicate != NULL) {
        span = mylite_sql_parser_span_join(span, predicate->span);
    }

    where_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_WHERE_CLAUSE, span);
    if (where_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(where_clause, predicate);
    return where_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_key_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *group_key
) {
    struct mylite_sql_source_span span =
        group_key == NULL ? (struct mylite_sql_source_span){0} : group_key->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_GROUP_BY_ITEM_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_group_by_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *group_key
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    if (group_key != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, group_key->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token group_token,
    struct mylite_sql_ast_node *group_keys
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&group_token);
    struct mylite_sql_ast_node *group_clause = NULL;

    if (group_keys != NULL) {
        span = mylite_sql_parser_span_join(span, group_keys->span);
    }

    group_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_GROUP_BY_CLAUSE, span);
    if (group_clause == NULL) {
        return NULL;
    }

    if (group_keys != NULL && group_keys->kind == MYLITE_SQL_AST_GROUP_BY_ITEM_LIST) {
        struct mylite_sql_ast_node *group_key = group_keys->first_child;

        while (group_key != NULL) {
            struct mylite_sql_ast_node *next = group_key->next_sibling;
            mylite_sql_ast_node_append_child(group_clause, group_key);
            group_key = next;
        }
    } else {
        mylite_sql_ast_node_append_child(group_clause, group_keys);
    }

    return group_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_rollup_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token rollup_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_GROUP_BY_ROLLUP_MODIFIER,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&with_token),
            mylite_sql_parser_span_from_token(&rollup_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_having_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token having_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&having_token);
    struct mylite_sql_ast_node *having_clause = NULL;

    if (predicate != NULL) {
        span = mylite_sql_parser_span_join(span, predicate->span);
    }

    having_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_HAVING_CLAUSE, span);
    if (having_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(having_clause, predicate);
    return having_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_comparison_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COMPARISON_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_like_comparison_predicate(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_parser_like_comparison_predicate_request *request
) {
    struct mylite_sql_ast_node *predicate = NULL;

    if (request == NULL) {
        return NULL;
    }

    predicate = mylite_sql_parser_make_comparison_predicate(
        state,
        request->left,
        request->operator_token,
        request->operator_kind,
        request->right
    );

    if (predicate == NULL || request->escape == NULL) {
        return predicate;
    }

    predicate->span = mylite_sql_parser_span_join(predicate->span, request->escape->span);
    mylite_sql_ast_node_append_child(predicate, request->escape);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_is_null_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&null_token));
    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_IS_NULL_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_is_boolean_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token truth_token
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&truth_token));
    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_apply_comparison_result_is_suffix(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *comparison,
    struct mylite_sql_comparison_operator_tokens suffix
) {
    if (suffix.operator_kind == MYLITE_SQL_AST_OPERATOR_NONE) {
        return comparison;
    }
    if (suffix.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL ||
        suffix.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
        return mylite_sql_parser_make_is_null_predicate(
            state,
            comparison,
            suffix.token,
            suffix.operator_kind,
            suffix.token
        );
    }
    if (suffix.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN ||
        suffix.operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN) {
        return mylite_sql_parser_make_is_boolean_predicate(
            state,
            comparison,
            suffix.token,
            suffix.operator_kind,
            suffix.token
        );
    }

    return comparison;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_between_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token between_token,
    struct mylite_sql_ast_node *lower,
    struct mylite_sql_ast_node *upper
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&between_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (upper != NULL) {
        span = mylite_sql_parser_span_join(span, upper->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_BETWEEN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, lower);
    mylite_sql_ast_node_append_child(predicate, upper);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_in_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token in_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&in_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&right_paren));
    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_IN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, values);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_quantified_subquery_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_comparison_operator_tokens operator_tokens,
    struct mylite_sql_quantified_subquery_tokens quantifier_tokens,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_tokens.token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&right_paren));
    predicate = mylite_sql_parser_make_node(state, quantifier_tokens.predicate_kind, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_tokens.operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, select_statement);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_exists_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token exists_token,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&exists_token);
    struct mylite_sql_ast_node *predicate = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&right_paren));
    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_EXISTS_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, select_statement);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_predicate_value_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_PREDICATE_VALUE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, value);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_predicate_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *value
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, value->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_and_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_AND_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_or_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_OR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_xor_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? mylite_sql_parser_span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = mylite_sql_parser_span_join(span, right->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_XOR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_not_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *child
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&operator_token);
    struct mylite_sql_ast_node *predicate = NULL;

    if (child != NULL) {
        span = mylite_sql_parser_span_join(span, child->span);
    }

    predicate = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_NOT_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT);
    mylite_sql_ast_node_append_child(predicate, child);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&order_token);
    struct mylite_sql_ast_node *order_clause = NULL;

    if (direction != NULL) {
        span = mylite_sql_parser_span_join(span, direction->span);
    } else if (order_key != NULL) {
        span = mylite_sql_parser_span_join(span, order_key->span);
    }

    order_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, order_key);
    mylite_sql_ast_node_append_child(order_clause, direction);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause_from_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *item_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&order_token);
    struct mylite_sql_ast_node *order_clause = NULL;
    struct mylite_sql_ast_node *order_child = item_list;

    if (item_list != NULL) {
        span = mylite_sql_parser_span_join(span, item_list->span);
    }
    if (item_list != NULL && item_list->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST &&
        mylite_sql_ast_node_child_count(item_list) == 1U) {
        order_child = mylite_sql_parser_child_at(item_list, 0U);
    }
    order_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }
    if (order_child != NULL && order_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM) {
        struct mylite_sql_ast_node *key = mylite_sql_parser_child_at(order_child, 0U);
        struct mylite_sql_ast_node *direction = mylite_sql_parser_child_at(order_child, 1U);

        mylite_sql_ast_node_append_child(order_clause, key);
        mylite_sql_ast_node_append_child(order_clause, direction);
    } else {
        mylite_sql_ast_node_append_child(order_clause, order_child);
    }
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_parser_select_order_by_parts parts
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&order_token);
    struct mylite_sql_ast_node *first_item = NULL;
    struct mylite_sql_ast_node *list = NULL;
    struct mylite_sql_ast_node *order_clause = NULL;
    struct mylite_sql_ast_node *item = NULL;

    if (parts.tail_items == NULL) {
        return mylite_sql_parser_make_order_by_clause(
            state,
            order_token,
            parts.first_order_key,
            parts.first_direction
        );
    }

    first_item =
        mylite_sql_parser_make_order_by_item(state, parts.first_order_key, parts.first_direction);
    list = mylite_sql_parser_make_order_by_item_list(state, first_item);
    if (list == NULL) {
        return NULL;
    }

    item = parts.tail_items->first_child;
    while (item != NULL) {
        struct mylite_sql_ast_node *next = item->next_sibling;
        mylite_sql_parser_append_order_by_item(state, list, item);
        item = next;
    }

    span = mylite_sql_parser_span_join(span, list->span);
    order_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, list);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        order_key == NULL ? (struct mylite_sql_source_span){0} : order_key->span;
    struct mylite_sql_ast_node *item = NULL;

    if (direction != NULL) {
        span = mylite_sql_parser_span_join(span, direction->span);
    }

    item = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, order_key);
    mylite_sql_ast_node_append_child(item, direction);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_direction(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token direction_token,
    enum mylite_sql_ast_order_direction direction
) {
    struct mylite_sql_ast_node *direction_node = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ORDER_DIRECTION,
        mylite_sql_parser_span_from_token(&direction_token)
    );
    if (direction_node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_order_direction(direction_node, direction);
    return direction_node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *row_count,
    struct mylite_sql_ast_node *offset
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (offset != NULL) {
        span = mylite_sql_parser_span_join(span, offset->span);
    }
    if (row_count != NULL) {
        span = mylite_sql_parser_span_join(span, row_count->span);
    }

    limit_clause = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(limit_clause, row_count);
    mylite_sql_ast_node_append_child(limit_clause, offset);
    return limit_clause;
}
