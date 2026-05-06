#include "mylite_select_aggregate_count_distinct_bind.h"

#include "mylite_diagnostics.h"
#include "mylite_select_predicate_bind.h"
#include "sql/mylite_ast.h"

#include <stdlib.h>

int mylite_select_bind_count_distinct_arguments(
    mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_select_plan *plan,
    const struct mylite_select_aggregate_bind_callbacks *callbacks
) {
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_EXPRESSION_LIST ||
        arguments->first_child == NULL) {
        return callbacks->set_invalid_group_function_error(database);
    }

    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int status = mylite_select_bind_predicate_expression(
            database,
            argument,
            plan,
            callbacks->predicate_callbacks
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_infer_count_distinct_argument_descriptors(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_select_aggregate_binding *binding,
    const struct mylite_select_aggregate_bind_callbacks *callbacks
) {
    size_t argument_count = mylite_sql_ast_node_child_count(arguments);

    if (argument_count == 0U) {
        return callbacks->set_invalid_group_function_error(database);
    }

    binding->argument_descriptors = calloc(argument_count, sizeof(*binding->argument_descriptors));
    if (binding->argument_descriptors == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    binding->argument_descriptor_count = argument_count;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int status = callbacks->infer_expression_descriptor(
            database,
            plan,
            argument,
            NULL,
            &binding->argument_descriptors[index]
        );

        if (status != MYLITE_OK) {
            return status;
        }
        ++index;
    }
    return MYLITE_OK;
}
