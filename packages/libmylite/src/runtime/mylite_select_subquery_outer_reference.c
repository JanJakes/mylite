#include "mylite_select_subquery_outer_reference.h"

#include "mylite_select.h"
#include "mylite_span.h"

static bool select_statement_has_visible_table_span(const struct mylite_sql_ast_node *node,
                                                    struct mylite_sql_source_span name);
static const struct mylite_sql_ast_node *
qualified_identifier_first_part(const struct mylite_sql_ast_node *identifier);
static const struct mylite_sql_ast_node *
qualified_identifier_last_part(const struct mylite_sql_ast_node *identifier);

// NOLINTNEXTLINE(misc-no-recursion)
bool mylite_select_subquery_references_outer_plan(
    const struct mylite_sql_ast_node *node, const struct mylite_select_plan *outer_plan,
    const struct mylite_sql_ast_node *select_statement)
{
    const struct mylite_sql_ast_node *first = NULL;

    if (node == NULL || outer_plan == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        first = qualified_identifier_first_part(node);
        if (first != NULL && mylite_select_plan_has_visible_table_span(outer_plan, first->span) &&
            !select_statement_has_visible_table_span(select_statement, first->span)) {
            return true;
        }
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (mylite_select_subquery_references_outer_plan(child, outer_plan, select_statement)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_subquery_has_unqualified_outer_column_reference( // NOLINT(misc-no-recursion)
    const struct mylite_sql_ast_node *node, const struct mylite_select_plan *outer_plan)
{
    if (node == NULL || outer_plan == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_FROM_TABLE) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_SELECT_ITEM) {
        return mylite_select_subquery_has_unqualified_outer_column_reference(
            mylite_ast_child_at(node, 0U), outer_plan);
    }
    if (node->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_select_plan_has_column_span(outer_plan, node->span)) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (mylite_select_subquery_has_unqualified_outer_column_reference(child, outer_plan)) {
            return true;
        }
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_statement_has_visible_table_span(const struct mylite_sql_ast_node *node,
                                                    struct mylite_sql_source_span name)
{
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_FROM_TABLE) {
        const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(node, 0U);
        const struct mylite_sql_ast_node *alias = mylite_ast_child_at(node, 1U);
        const struct mylite_sql_ast_node *visible_name =
            alias == NULL ? qualified_identifier_last_part(table_name) : alias;

        if (visible_name == NULL) {
            return false;
        }
        return mylite_source_span_equal_ci(visible_name->span, name);
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (select_statement_has_visible_table_span(child, name)) {
            return true;
        }
    }
    return false;
}

static const struct mylite_sql_ast_node *
qualified_identifier_first_part(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 0U);
    }
    return current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER ? current : NULL;
}

static const struct mylite_sql_ast_node *
qualified_identifier_last_part(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 1U);
    }
    return current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER ? current : NULL;
}
