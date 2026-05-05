#include "mylite_dml_insert_set_copy.h"

#include "mylite_dml.h"
#include "mylite_dml_insert_copy_value.h"
#include "mylite_span.h"

#include <stdlib.h>

static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan);
static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment);

int mylite_dml_copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                           struct mylite_insert_set_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_insert_set_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan)
{
    struct mylite_insert_set_assignment insert_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_dml_copy_insert_column_reference(mylite_ast_child_at(assignment, 0U),
                                                     &insert_assignment.target);
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_value(mylite_ast_child_at(assignment, 1U),
                                              &insert_assignment.value);
    }
    if (status == MYLITE_OK) {
        status = add_insert_set_assignment(plan, insert_assignment);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_set_assignment_deinit(&insert_assignment);
    }
    return status;
}

static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment)
{
    struct mylite_insert_set_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}
