#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = mylite_sql_parser_span_join(span, duplicate_update->span);
    } else if (rows != NULL) {
        span = mylite_sql_parser_span_join(span, rows->span);
    } else if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = mylite_sql_parser_span_join(span, duplicate_update->span);
    } else if (select != NULL) {
        span = mylite_sql_parser_span_join(span, select->span);
    } else if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_infile_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token load_token,
    struct mylite_sql_ast_node *file_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *ignore_lines,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *local_modifier
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&load_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    } else if (ignore_lines != NULL) {
        span = mylite_sql_parser_span_join(span, ignore_lines->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (file_name != NULL) {
        span = mylite_sql_parser_span_join(span, file_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, file_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, ignore_lines);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, local_modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_local_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_high_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select != NULL) {
        span = mylite_sql_parser_span_join(span, select->span);
    } else if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = mylite_sql_parser_span_join(span, rows->span);
    } else if (columns != NULL) {
        span = mylite_sql_parser_span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER,
        mylite_sql_parser_span_from_token(&token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = mylite_sql_parser_span_join(span, duplicate_update->span);
    } else if (assignments != NULL) {
        span = mylite_sql_parser_span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = mylite_sql_parser_span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_assignment(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? mylite_sql_parser_span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    assignment = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_update_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&on_token);
    struct mylite_sql_ast_node *clause = NULL;

    if (assignments != NULL) {
        span = mylite_sql_parser_span_join(span, assignments->span);
    }

    clause =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, assignments);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_duplicate_assignment(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? mylite_sql_parser_span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    assignment =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_token close_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&values_token);
    struct mylite_sql_ast_node *reference = NULL;

    if (column != NULL) {
        span = mylite_sql_parser_span_join(span, column->span);
    }
    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&close_token));

    reference = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, span);
    if (reference == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(reference, column);
    return reference;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }
    if (where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL) {
        span = mylite_sql_parser_span_join(span, target->span);
    }
    if (from_join != NULL) {
        span = mylite_sql_parser_span_join(span, from_join->span);
    }
    if (where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, where_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_JOINED_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, from_join);
    mylite_sql_ast_node_append_child(statement, where_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_update_statement_parts parts
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&update_token);
    struct mylite_sql_ast_node *target = parts.target_table;
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL && target->kind == MYLITE_SQL_AST_FROM_TABLE &&
        target->first_child != NULL && target->first_child->next_sibling == NULL) {
        target = target->first_child;
    }

    if (target != NULL) {
        span = mylite_sql_parser_span_join(span, target->span);
    }
    if (parts.assignment_list != NULL) {
        span = mylite_sql_parser_span_join(span, parts.assignment_list->span);
    }
    if (parts.where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, parts.where_clause->span);
    }
    if (parts.order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, parts.order_clause->span);
    }
    if (parts.limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, parts.limit_clause->span);
    }
    if (parts.low_priority_modifier != NULL) {
        span = mylite_sql_parser_span_join(span, parts.low_priority_modifier->span);
    }
    if (parts.ignore_modifier != NULL) {
        span = mylite_sql_parser_span_join(span, parts.ignore_modifier->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, parts.assignment_list);
    mylite_sql_ast_node_append_child(statement, parts.where_clause);
    mylite_sql_ast_node_append_child(statement, parts.order_clause);
    mylite_sql_ast_node_append_child(statement, parts.limit_clause);
    mylite_sql_ast_node_append_child(statement, parts.low_priority_modifier);
    mylite_sql_ast_node_append_child(statement, parts.ignore_modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&update_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (from_join != NULL) {
        span = mylite_sql_parser_span_join(span, from_join->span);
    }
    if (assignments != NULL) {
        span = mylite_sql_parser_span_join(span, assignments->span);
    }
    if (where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = mylite_sql_parser_span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, from_join);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_update_assignment(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? mylite_sql_parser_span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    assignment = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}
