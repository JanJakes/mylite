#include "mylite_sqlite_translator.h"

#include <stdlib.h>
#include <string.h>

static enum mylite_sqlite_translate_status translate_script(
    const struct mylite_sql_ast_node *root,
    struct mylite_sqlite_translate_result *out_result
);

static enum mylite_sqlite_translate_status translate_select_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_sqlite_translate_result *out_result
);

static enum mylite_sqlite_translate_status translate_integer_literal_select(
    const struct mylite_sql_ast_node *literal,
    struct mylite_sqlite_translate_result *out_result
);

static const struct mylite_sql_ast_node *only_child(const struct mylite_sql_ast_node *node);

enum mylite_sqlite_translate_status mylite_sqlite_translate(
    const struct mylite_sql_ast_node *root,
    struct mylite_sqlite_translate_result *out_result
) {
    if (out_result == NULL) {
        return MYLITE_SQLITE_TRANSLATE_UNSUPPORTED;
    }

    *out_result = (struct mylite_sqlite_translate_result){0};

    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT) {
        return MYLITE_SQLITE_TRANSLATE_UNSUPPORTED;
    }

    return translate_script(root, out_result);
}

void mylite_sqlite_translate_result_deinit(struct mylite_sqlite_translate_result *result) {
    if (result == NULL) {
        return;
    }

    free(result->sql);
    *result = (struct mylite_sqlite_translate_result){0};
}

static enum mylite_sqlite_translate_status translate_script(
    const struct mylite_sql_ast_node *root,
    struct mylite_sqlite_translate_result *out_result
) {
    const struct mylite_sql_ast_node *statement = only_child(root);

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_SQLITE_TRANSLATE_UNSUPPORTED;
    }

    return translate_select_statement(statement, out_result);
}

static enum mylite_sqlite_translate_status translate_select_statement(
    const struct mylite_sql_ast_node *statement,
    struct mylite_sqlite_translate_result *out_result
) {
    const struct mylite_sql_ast_node *select_list = only_child(statement);
    const struct mylite_sql_ast_node *select_item = only_child(select_list);
    const struct mylite_sql_ast_node *expression = only_child(select_item);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        expression->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_SQLITE_TRANSLATE_UNSUPPORTED;
    }

    return translate_integer_literal_select(expression, out_result);
}

static enum mylite_sqlite_translate_status translate_integer_literal_select(
    const struct mylite_sql_ast_node *literal,
    struct mylite_sqlite_translate_result *out_result
) {
    static const char prefix[] = "SELECT ";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t sql_length = prefix_length + literal->span.length;
    char *sql = malloc(sql_length + 1U);

    if (sql == NULL) {
        return MYLITE_SQLITE_TRANSLATE_NOMEM;
    }

    memcpy(sql, prefix, prefix_length);
    memcpy(sql + prefix_length, literal->span.text, literal->span.length);
    sql[sql_length] = '\0';

    out_result->sql = sql;
    return MYLITE_SQLITE_TRANSLATE_OK;
}

static const struct mylite_sql_ast_node *only_child(const struct mylite_sql_ast_node *node) {
    if (node == NULL || node->first_child == NULL || node->first_child->next_sibling != NULL) {
        return NULL;
    }

    return node->first_child;
}
