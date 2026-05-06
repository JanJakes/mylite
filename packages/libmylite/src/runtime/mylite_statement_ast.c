#include "mylite_statement_ast.h"

#include <mylite/mylite.h>

#include <stdint.h>

static struct mylite_sql_source_span statement_remap_source_span(
    struct mylite_sql_source_span span,
    const char *source_sql,
    const char *sql_copy,
    size_t sql_length
);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_statement_ast_clone_subtree(
    struct mylite_sql_ast *ast,
    const struct mylite_sql_ast_node *node,
    const char *source_sql,
    const char *sql_copy,
    size_t sql_length,
    struct mylite_sql_ast_node **out_node
) {
    struct mylite_sql_ast_node *clone = NULL;

    *out_node = NULL;
    if (node == NULL) {
        return MYLITE_OK;
    }

    clone = mylite_sql_ast_new_node(
        ast,
        node->kind,
        statement_remap_source_span(node->span, source_sql, sql_copy, sql_length)
    );
    if (clone == NULL) {
        return MYLITE_NOMEM;
    }

    {
        struct mylite_sql_ast_node *next_allocated = clone->next_allocated;

        *clone = *node;
        clone->first_child = NULL;
        clone->last_child = NULL;
        clone->next_sibling = NULL;
        clone->next_allocated = next_allocated;
        clone->span = statement_remap_source_span(node->span, source_sql, sql_copy, sql_length);
        clone->column_character_set = statement_remap_source_span(
            node->column_character_set,
            source_sql,
            sql_copy,
            sql_length
        );
        clone->column_collation =
            statement_remap_source_span(node->column_collation, source_sql, sql_copy, sql_length);
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_sql_ast_node *child_clone = NULL;
        int status = mylite_statement_ast_clone_subtree(
            ast,
            child,
            source_sql,
            sql_copy,
            sql_length,
            &child_clone
        );

        if (status != MYLITE_OK) {
            return status;
        }
        mylite_sql_ast_node_append_child(clone, child_clone);
    }

    *out_node = clone;
    return MYLITE_OK;
}

static struct mylite_sql_source_span statement_remap_source_span(
    struct mylite_sql_source_span span,
    const char *source_sql,
    const char *sql_copy,
    size_t sql_length
) {
    uintptr_t base = (uintptr_t)source_sql;
    uintptr_t end = base + sql_length;
    uintptr_t text = (uintptr_t)span.text;

    if (span.text == NULL || source_sql == NULL || sql_copy == NULL) {
        return span;
    }
    if (text < base || text > end || span.length > (size_t)(end - text)) {
        return span;
    }

    span.text = sql_copy + (text - base);
    return span;
}
