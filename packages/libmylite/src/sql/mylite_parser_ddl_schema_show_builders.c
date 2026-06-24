#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_not_exists_clause != NULL) {
        span = mylite_sql_parser_span_join(span, if_not_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }
    if (schema_options != NULL) {
        span = mylite_sql_parser_span_join(span, schema_options->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (schema_options != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_options != NULL) {
        span = mylite_sql_parser_span_join(span, schema_options->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT,
        span
    );
    if (statement == NULL) {
        return NULL;
    }

    if (schema_name != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_name);
    }
    mylite_sql_ast_node_append_child(statement, schema_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&if_token),
        mylite_sql_parser_span_from_token(&exists_token)
    );

    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE,
        span
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = mylite_sql_parser_span_join(span, table_names->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        table_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *view_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        view_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_VIEW_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *procedure_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (procedure_name != NULL) {
        span = mylite_sql_parser_span_join(span, procedure_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_PROCEDURE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_name_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_NAME_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *table_name
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    if (table_name != NULL) {
        mylite_sql_ast_node_set_span(
            list,
            mylite_sql_parser_span_join(list->span, table_name->span)
        );
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&if_token),
        mylite_sql_parser_span_from_token(&exists_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_exists_clause != NULL) {
        span = mylite_sql_parser_span_join(span, if_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&if_token),
        mylite_sql_parser_span_from_token(&exists_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_truncate_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token truncate_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&truncate_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    int is_full,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&tables_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_show_tables_full(statement, is_full);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&variables_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = mylite_sql_parser_span_join(span, filter->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&status_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = mylite_sql_parser_span_join(span, filter->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&status_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = mylite_sql_parser_span_join(span, filter->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token collation_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&collation_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_triggers_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token triggers_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&triggers_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token events_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&events_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_open_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&tables_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_routine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&status_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    }

    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_processlist_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token processlist_token,
    enum mylite_sql_ast_node_kind statement_kind
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&processlist_token)
    );

    return mylite_sql_parser_make_node(state, statement_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_for_target_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *role_list
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (role_list != NULL) {
        span = mylite_sql_parser_span_join(span, role_list->span);
    } else if (target != NULL) {
        span = mylite_sql_parser_span_join(span, target->span);
    }
    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, role_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *user,
    struct mylite_sql_ast_node *host
) {
    struct mylite_sql_source_span span =
        user == NULL ? (struct mylite_sql_source_span){0} : user->span;
    struct mylite_sql_ast_node *account = NULL;

    if (host != NULL) {
        span = mylite_sql_parser_span_join(span, host->span);
    }
    account = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_ACCOUNT, span);
    if (account != NULL) {
        mylite_sql_ast_node_append_child(account, user);
        mylite_sql_ast_node_append_child(account, host);
    }
    return account;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_show_grants_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&current_user_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_role_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *role
) {
    struct mylite_sql_source_span span =
        role == NULL ? (struct mylite_sql_source_span){0} : role->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_GRANTS_ROLE_LIST, span);

    if (list != NULL) {
        mylite_sql_ast_node_append_child(list, role);
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_show_grants_role(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *role
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, role);
    if (role != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, role->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token warnings_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&warnings_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_warnings_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.show),
        mylite_sql_parser_span_from_token(&tokens.warnings)
    );

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token errors_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&errors_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = mylite_sql_parser_span_join(span, limit_clause->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_errors_tokens tokens
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&tokens.show),
        mylite_sql_parser_span_from_token(&tokens.errors)
    );

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_full_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = mylite_sql_parser_span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (where_clause != NULL) {
        span = mylite_sql_parser_span_join(span, where_clause->span);
    } else if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, where_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_databases_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token databases_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&databases_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = mylite_sql_parser_span_join(span, filter->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *view_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, view_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *procedure_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, procedure_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_PROCEDURE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_function_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *function_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, function_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_FUNCTION_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_trigger_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *trigger_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, trigger_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_TRIGGER_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_event_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *event_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, event_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_EVENT_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_user_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *user_target
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, user_target);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_USER_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_database_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = mylite_sql_parser_span_join(span, schema_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_call_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token call_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *arguments
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&call_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (procedure_name != NULL) {
        span = mylite_sql_parser_span_join(span, procedure_name->span);
    }
    if (arguments != NULL) {
        span = mylite_sql_parser_span_join(span, arguments->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CALL_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    mylite_sql_ast_node_append_child(statement, arguments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_raw_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_source_span span
) {
    return mylite_sql_parser_make_node(state, statement_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_admin_noop_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    return mylite_sql_parser_make_raw_statement(
        state,
        MYLITE_SQL_AST_ADMIN_NOOP_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&first_token),
            mylite_sql_parser_span_from_token(&last_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engines_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token engines_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&engines_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *engine_name,
    struct mylite_sql_token status_token
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&status_token)
        )
    );

    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, engine_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_plugins_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token plugins_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&plugins_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_privileges_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token privileges_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&privileges_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_log_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&status_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_logs_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token logs_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&logs_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binlog_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token events_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINLOG_EVENTS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&events_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_relaylog_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token,
    struct mylite_sql_ast_node *channel
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&show_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (channel != NULL) {
        span = mylite_sql_parser_span_join(span, channel->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_SHOW_RELAYLOG_EVENTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    if (channel != NULL) {
        mylite_sql_ast_node_append_child(statement, channel);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replica_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&status_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replicas_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token replicas_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&show_token),
            mylite_sql_parser_span_from_token(&replicas_token)
        )
    );
}
