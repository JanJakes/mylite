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
