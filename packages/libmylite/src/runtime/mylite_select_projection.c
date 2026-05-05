#include "mylite_select_projection.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"

#include <stdlib.h>

static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan,
                                      const struct mylite_select_projection_callbacks *callbacks);
static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan);
static int
append_select_expression_output(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                const struct mylite_sql_ast_node *alias,
                                struct mylite_select_plan *plan,
                                const struct mylite_select_projection_callbacks *callbacks);
static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier);

int mylite_select_build_outputs(mylite_db *database, const struct mylite_sql_ast_node *select_list,
                                bool allow_expression_outputs, struct mylite_select_plan *plan,
                                const struct mylite_select_projection_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->bind_expression == NULL ||
        callbacks->set_unsupported_projection_error == NULL) {
        return MYLITE_MISUSE;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        int status =
            append_select_item_outputs(database, item, allow_expression_outputs, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->output_count == 0U) {
        return callbacks->set_unsupported_projection_error(database);
    }
    return MYLITE_OK;
}

static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan,
                                      const struct mylite_select_projection_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(select_item, 0U);
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(select_item, 1U);

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
        expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression->kind == MYLITE_SQL_AST_WILDCARD) {
        if (alias != NULL) {
            return callbacks->set_unsupported_projection_error(database);
        }
        return mylite_select_append_wildcard_outputs(database, expression, plan);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return append_select_column_output(database, expression, alias, plan);
    }
    if (allow_expression_outputs) {
        return append_select_expression_output(database, expression, alias, plan, callbacks);
    }
    return callbacks->set_unsupported_projection_error(database);
}

static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan)
{
    size_t column_index = mylite_select_plan_column_count(plan);
    char *label = NULL;
    int status = mylite_select_resolve_plan_column_reference(database, plan, expression,
                                                             "field list", &column_index);

    if (status != MYLITE_OK) {
        return status;
    }
    if (column_index == mylite_select_plan_column_count(plan)) {
        return MYLITE_UNSUPPORTED;
    }

    label = alias == NULL ? copy_select_final_identifier_label(expression)
                          : mylite_select_copy_alias(alias);
    if (label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_plan_add_output_column(plan, &(const struct mylite_select_output_column){
                                                            .kind = MYLITE_SELECT_OUTPUT_COLUMN,
                                                            .column_index = column_index,
                                                            .label = label,
                                                        });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int
append_select_expression_output(mylite_db *database, const struct mylite_sql_ast_node *expression,
                                const struct mylite_sql_ast_node *alias,
                                struct mylite_select_plan *plan,
                                const struct mylite_select_projection_callbacks *callbacks)
{
    char *label = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return callbacks->set_unsupported_projection_error(database);
    }
    status = callbacks->bind_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    label = alias == NULL ? mylite_copy_span_text(expression->span.text, expression->span.length)
                          : mylite_select_copy_alias(alias);
    if (label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_plan_add_output_column(plan, &(const struct mylite_select_output_column){
                                                            .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
                                                            .expression = expression,
                                                            .label = label,
                                                        });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 1U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return NULL;
    }
    return mylite_copy_identifier_span(current);
}
