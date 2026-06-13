#include "mylite_parse.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
);
static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
);
static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text);
static bool span_text_matches_ignore_space_function_name(const struct mylite_sql_source_span *span);

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&create_token),
        mylite_sql_parser_span_from_token(&right_paren)
    );
    struct mylite_sql_ast_node *statement = NULL;

    if (create_table_name_is_no_space_function_identifier(state, table_name, &left_paren)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (table_options != NULL) {
        mylite_sql_ast_node_append_child(statement, table_options);
        mylite_sql_ast_node_set_span(
            statement,
            mylite_sql_parser_span_join(statement->span, table_options->span)
        );
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        left_paren,
        columns,
        right_paren,
        table_options
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source_table != NULL) {
        span = mylite_sql_parser_span_join(span, source_table->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, source_table);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_like_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        source_table
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = mylite_sql_parser_span_join(span, select_statement->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (table_options != NULL) {
        mylite_sql_ast_node_append_child(statement, table_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_select_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        table_options,
        select_statement
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *or_replace_clause,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = mylite_sql_parser_span_join(span, select_statement->span);
    } else if (view_name != NULL) {
        span = mylite_sql_parser_span_join(span, view_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_VIEW_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, view_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (or_replace_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, or_replace_clause);
    }
    if (view_options != NULL) {
        mylite_sql_ast_node_append_child(statement, view_options);
    }
    if (column_names != NULL) {
        mylite_sql_ast_node_append_child(statement, column_names);
    }
    if (check_option != NULL) {
        mylite_sql_ast_node_append_child(statement, check_option);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *view_options,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *column_names,
    struct mylite_sql_ast_node *check_option,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = mylite_sql_parser_span_join(span, select_statement->span);
    } else if (view_name != NULL) {
        span = mylite_sql_parser_span_join(span, view_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_ALTER_VIEW_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, view_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (view_options != NULL) {
        mylite_sql_ast_node_append_child(statement, view_options);
    }
    if (column_names != NULL) {
        mylite_sql_ast_node_append_child(statement, column_names);
    }
    if (check_option != NULL) {
        mylite_sql_ast_node_append_child(statement, check_option);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_or_replace_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token or_token,
    struct mylite_sql_token replace_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_CREATE_OR_REPLACE_CLAUSE,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&or_token),
            mylite_sql_parser_span_from_token(&replace_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VIEW_OPTION_LIST, span);

    if (list != NULL) {
        mylite_sql_ast_node_append_child(list, option);
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_view_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (list == NULL) {
        return mylite_sql_parser_make_view_option_list(state, option);
    }
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_algorithm_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token algorithm_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&algorithm_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VIEW_ALGORITHM_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, value);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token definer_token,
    struct mylite_sql_ast_node *account
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&definer_token);
    struct mylite_sql_ast_node *option = NULL;

    if (account != NULL) {
        span = mylite_sql_parser_span_join(span, account->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VIEW_DEFINER_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, account);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_security_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token sql_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&sql_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VIEW_SECURITY_OPTION, span);
    if (option != NULL) {
        mylite_sql_ast_node_append_child(option, value);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_definer_account(
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
    account = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT, span);
    if (account != NULL) {
        mylite_sql_ast_node_append_child(account, user);
        mylite_sql_ast_node_append_child(account, host);
    }
    return account;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_view_definer_account(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token,
    struct mylite_sql_token end_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_VIEW_DEFINER_ACCOUNT,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&current_user_token),
            mylite_sql_parser_span_from_token(&end_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_view_check_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token with_token,
    struct mylite_sql_token option_token
) {
    return mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_VIEW_CHECK_OPTION,
        mylite_sql_parser_span_join(
            mylite_sql_parser_span_from_token(&with_token),
            mylite_sql_parser_span_from_token(&option_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_procedure_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *procedure_name,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&create_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_PROCEDURE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, procedure_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_type != NULL) {
        span = mylite_sql_parser_span_join(span, index_type->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    if (is_unique) {
        statement_kind = MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT;
    }
    statement = mylite_sql_parser_make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(statement, index_type);
    }
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_fulltext_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_spatial_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = mylite_sql_parser_span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = mylite_sql_parser_span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = mylite_sql_parser_span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = mylite_sql_parser_span_join(span, index_name->span);
    }

    statement = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&if_token),
        mylite_sql_parser_span_from_token(&exists_token)
    );

    return mylite_sql_parser_make_node(state, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (!mylite_sql_parser_is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_engine_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token engine_token,
    struct mylite_sql_ast_node *engine_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&engine_token);
    struct mylite_sql_ast_node *option = NULL;

    if (engine_name != NULL) {
        span = mylite_sql_parser_span_join(span, engine_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_ENGINE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, engine_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_charset_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&charset_token);
    struct mylite_sql_ast_node *option = NULL;

    if (charset_name != NULL) {
        span = mylite_sql_parser_span_join(span, charset_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_CHARSET_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, charset_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_collation_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&collate_token);
    struct mylite_sql_ast_node *option = NULL;

    if (collation_name != NULL) {
        span = mylite_sql_parser_span_join(span, collation_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_COLLATION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, collation_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&auto_increment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_row_format_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_format_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&row_format_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token key_block_size_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&key_block_size_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_pack_keys_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token pack_keys_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&pack_keys_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_checksum_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token checksum_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&checksum_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_persistent_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_persistent_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&stats_persistent_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_auto_recalc_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_auto_recalc_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        mylite_sql_parser_span_from_token(&stats_auto_recalc_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_sample_pages_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_sample_pages_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        mylite_sql_parser_span_from_token(&stats_sample_pages_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_min_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token min_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&min_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_MIN_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_max_rows_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token max_rows_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&max_rows_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_MAX_ROWS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_avg_row_length_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token avg_row_length_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&avg_row_length_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_AVG_ROW_LENGTH_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_delay_key_write_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delay_key_write_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&delay_key_write_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_DELAY_KEY_WRITE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_tablespace_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token tablespace_token,
    struct mylite_sql_ast_node *tablespace_name,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&tablespace_token);
    struct mylite_sql_ast_node *option = NULL;

    if (storage != NULL) {
        span = mylite_sql_parser_span_join(span, storage->span);
    } else if (tablespace_name != NULL) {
        span = mylite_sql_parser_span_join(span, tablespace_name->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_TABLESPACE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, tablespace_name);
    if (storage != NULL) {
        mylite_sql_ast_node_append_child(option, storage);
    }
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_union_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    struct mylite_sql_ast_node *table_names,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_join(
        mylite_sql_parser_span_from_token(&union_token),
        mylite_sql_parser_span_from_token(&end_token)
    );
    struct mylite_sql_ast_node *option =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_UNION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, table_names);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_insert_method_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_method_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&insert_method_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_INSERT_METHOD_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_storage_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token storage_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&storage_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }

    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_TABLE_STORAGE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list =
        mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    (void)state;
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, mylite_sql_parser_span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_type_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *type_name
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&using_token);
    struct mylite_sql_ast_node *option = NULL;

    if (type_name != NULL) {
        span = mylite_sql_parser_span_join(span, type_name->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_TYPE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, type_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = mylite_sql_parser_span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = mylite_sql_parser_span_join(span, value->span);
    }
    option = mylite_sql_parser_make_node(state, MYLITE_SQL_AST_INDEX_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_visibility_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *option = mylite_sql_parser_make_node(
        state,
        MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION,
        mylite_sql_parser_span_from_token(&visibility_token)
    );
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(option, visibility);
    return option;
}

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

static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
) {
    const struct mylite_sql_ast_node *last_identifier = NULL;

    if (mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) || table_name == NULL ||
        left_paren == NULL) {
        return false;
    }
    last_identifier = last_identifier_component(table_name);
    if (last_identifier == NULL) {
        return false;
    }
    if (left_paren->offset != last_identifier->span.offset + last_identifier->span.length) {
        return false;
    }
    return span_text_matches_ignore_space_function_name(&last_identifier->span);
}

static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
) {
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = current->last_child;
    }
    if (current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return current;
    }
    return NULL;
}

static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text) {
    size_t length = strlen(text);

    if (span == NULL || span->text == NULL || span->length != length) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (mylite_sql_parser_ascii_upper((unsigned char)span->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

static bool span_text_matches_ignore_space_function_name(const struct mylite_sql_source_span *span
) {
    static const char *const function_names[] = {
        "BIT_AND",
        "BIT_OR",
        "BIT_XOR",
        "CAST",
        "CONVERT",
        "COUNT",
        "CURDATE",
        "CURTIME",
        "DATE_ADD",
        "DATE_SUB",
        "GROUP_CONCAT",
        "MAX",
        "MIN",
        "SUM",
        "SYSDATE",
    };

    for (size_t index = 0U; index < sizeof(function_names) / sizeof(function_names[0]); ++index) {
        if (span_text_equals(span, function_names[index])) {
            return true;
        }
    }
    return false;
}
