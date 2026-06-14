#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *pairs
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&rename_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (pairs != NULL) {
        span = mylite_sql_parser_span_join(span, pairs->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, pairs);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *pair
) {
    struct mylite_sql_source_span span =
        pair == NULL ? (struct mylite_sql_source_span){0} : pair->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, pair);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *pair
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, pair);
    if (pair != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, pair->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span =
        source_name == NULL ? mylite_sql_parser_span_from_token(&to_token) : source_name->span;
    struct mylite_sql_ast_node *pair = NULL;

    if (target_name != NULL) {
        span = mylite_sql_parser_span_join(span, target_name->span);
    }

    pair = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR, span);
    if (pair == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(pair, source_name);
    mylite_sql_ast_node_append_child(pair, target_name);
    return pair;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target_name != NULL) {
        span = mylite_sql_parser_span_join(span, target_name->span);
    } else if (source_name != NULL) {
        span = mylite_sql_parser_span_join(span, source_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, source_name);
    mylite_sql_ast_node_append_child(statement, target_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = mylite_sql_parser_span_join(span, position->span);
    } else if (column != NULL) {
        span = mylite_sql_parser_span_join(span, column->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token add_token,
    struct mylite_sql_ast_node *column_definitions
) {
    struct mylite_sql_ast_node *list = NULL;
    struct mylite_sql_ast_node *column = NULL;

    if (column_definitions == NULL ||
        column_definitions->kind != MYLITE_SQL_AST_COLUMN_DEFINITION_LIST) {
        return NULL;
    }

    column = mylite_sql_parser_child_at(column_definitions, 0U);
    while (column != NULL) {
        struct mylite_sql_ast_node *next_column = column->next_sibling;
        struct mylite_sql_ast_node *action =
            mylite_sql_parser_make_alter_table_add_column_statement(
                state,
                add_token,
                NULL,
                column,
                NULL,
                mylite_sql_parser_empty_alter_table_options()
            );

        list = mylite_sql_parser_append_alter_table_action(state, list, action);
        column = next_column;
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_alter_table_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (list == NULL) {
        return mylite_sql_parser_make_alter_table_action_list(state, action);
    }
    if (action != NULL) {
        mylite_sql_ast_node_append_child(list, action);
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_multi_action_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *actions,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (actions != NULL) {
        span = mylite_sql_parser_span_join(span, actions->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, actions);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *primary_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (primary_key != NULL) {
        span = mylite_sql_parser_span_join(span, primary_key->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, primary_key);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *secondary_index,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (secondary_index != NULL) {
        span = mylite_sql_parser_span_join(span, secondary_index->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, secondary_index);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key != NULL) {
        span = mylite_sql_parser_span_join(span, foreign_key->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key_name != NULL) {
        span = mylite_sql_parser_span_join(span, foreign_key_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_constraint_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (constraint_name != NULL) {
        span = mylite_sql_parser_span_join(span, constraint_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, constraint_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_index_name,
    struct mylite_sql_ast_node *new_index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_index_name != NULL) {
        span = mylite_sql_parser_span_join(span, new_index_name->span);
    } else if (old_index_name != NULL) {
        span = mylite_sql_parser_span_join(span, old_index_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_index_name);
    mylite_sql_ast_node_append_child(statement, new_index_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_index_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&visibility_token));

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_constraint
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_constraint != NULL) {
        span = mylite_sql_parser_span_join(span, check_constraint->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_constraint);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_name != NULL) {
        span = mylite_sql_parser_span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (enforcement != NULL) {
        span = mylite_sql_parser_span_join(span, enforcement->span);
    } else if (check_name != NULL) {
        span = mylite_sql_parser_span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    mylite_sql_ast_node_append_child(statement, enforcement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token key_token,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (key_token.text != NULL) {
        span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&key_token));
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_auto_increment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *auto_increment_option
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (auto_increment_option != NULL) {
        span = mylite_sql_parser_span_join(span, auto_increment_option->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, auto_increment_option);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column_name != NULL) {
        span = mylite_sql_parser_span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_column_name != NULL) {
        span = mylite_sql_parser_span_join(span, new_column_name->span);
    } else if (old_column_name != NULL) {
        span = mylite_sql_parser_span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, new_column_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = mylite_sql_parser_span_join(span, position->span);
    } else if (column != NULL) {
        span = mylite_sql_parser_span_join(span, column->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = mylite_sql_parser_span_join(span, position->span);
    } else if (column != NULL) {
        span = mylite_sql_parser_span_join(span, column->span);
    } else if (old_column_name != NULL) {
        span = mylite_sql_parser_span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, column);
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_first(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_COLUMN_POSITION_FIRST,
        mylite_sql_parser_span_from_token(&first_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_after(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token after_token,
    struct mylite_sql_ast_node *column_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&after_token);
    struct mylite_sql_ast_node *position = NULL;

    if (column_name != NULL) {
        span = mylite_sql_parser_span_join(span, column_name->span);
    }

    position = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, span);
    if (position == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(position, column_name);
    return position;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_set_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_ast_node *default_node
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (default_node != NULL) {
        span = mylite_sql_parser_span_join(span, default_node->span);
    } else if (column_name != NULL) {
        span = mylite_sql_parser_span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    mylite_sql_ast_node_append_child(statement, default_node);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token default_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&default_token));

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_column_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = mylite_sql_parser_span_join(span, mylite_sql_parser_span_from_token(&visibility_token));

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = mylite_sql_parser_span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_convert_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = mylite_sql_parser_span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_comment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *comment_option,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (comment_option != NULL) {
        span = mylite_sql_parser_span_join(span, comment_option->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, comment_option);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_storage_statistics_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = mylite_sql_parser_span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_order_by_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_items
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (order_items != NULL) {
        span = mylite_sql_parser_span_join(span, order_items->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, order_items);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_force_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_disable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_enable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_parser_apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_alter_table_options mylite_sql_parser_empty_alter_table_options(void) {
    return (struct mylite_sql_alter_table_options){
        .algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED,
        .lock = MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED,
        .span = {0},
        .has_span = 0,
    };
}

struct mylite_sql_alter_algorithm_value mylite_sql_parser_make_alter_algorithm_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_algorithm kind = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;

    if (mylite_sql_parser_token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_DEFAULT;
    } else if (mylite_sql_parser_token_text_equals(&token, "INSTANT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT;
    } else if (mylite_sql_parser_token_text_equals(&token, "INPLACE")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE;
    } else if (mylite_sql_parser_token_text_equals(&token, "COPY")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_COPY;
    }

    return (struct mylite_sql_alter_algorithm_value){
        .kind = kind,
        .span = mylite_sql_parser_span_from_token(&token),
    };
}

struct mylite_sql_alter_lock_value mylite_sql_parser_make_alter_lock_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_lock kind = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;

    if (mylite_sql_parser_token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_DEFAULT;
    } else if (mylite_sql_parser_token_text_equals(&token, "NONE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_NONE;
    } else if (mylite_sql_parser_token_text_equals(&token, "SHARED")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_SHARED;
    } else if (mylite_sql_parser_token_text_equals(&token, "EXCLUSIVE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE;
    }

    return (struct mylite_sql_alter_lock_value){
        .kind = kind,
        .span = mylite_sql_parser_span_from_token(&token),
    };
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_algorithm_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_algorithm_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.algorithm = value.kind;
    options.span =
        mylite_sql_parser_span_join(mylite_sql_parser_span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_lock_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_lock_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.lock = value.kind;
    options.span =
        mylite_sql_parser_span_join(mylite_sql_parser_span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_append_alter_table_option(
    struct mylite_sql_alter_table_options list,
    struct mylite_sql_alter_table_options option
) {
    if (option.algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED) {
        if (list.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN ||
            option.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN) {
            list.algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;
        } else {
            list.algorithm = option.algorithm;
        }
    }
    if (option.lock != MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED) {
        if (list.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN ||
            option.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN) {
            list.lock = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;
        } else {
            list.lock = option.lock;
        }
    }
    if (!list.has_span) {
        list.span = option.span;
        list.has_span = option.has_span;
    } else if (option.has_span) {
        list.span = mylite_sql_parser_span_join(list.span, option.span);
    }
    return list;
}
