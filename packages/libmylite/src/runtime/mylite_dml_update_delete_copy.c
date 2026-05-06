#include "mylite_dml.h"

#include "mylite_span.h"

#include <stdlib.h>

struct dml_table_name_output {
    char **schema_name;
    char **table_name;
};

static int copy_update_target(const struct mylite_sql_ast_node *target,
                              struct mylite_update_target *out_target);
static int copy_update_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_update_target *target);
static int copy_update_assignments(const struct mylite_sql_ast_node *assignments,
                                   struct mylite_update_plan *plan);
static int copy_update_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_update_plan *plan);
static int add_update_assignment(struct mylite_update_plan *plan,
                                 struct mylite_update_assignment assignment);
static int copy_update_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_update_column_reference *out_reference);
static int copy_delete_target(const struct mylite_sql_ast_node *target,
                              struct mylite_delete_target *out_target);
static int copy_multi_delete_targets(const struct mylite_sql_ast_node *targets,
                                     struct mylite_delete_plan *plan);
static int copy_multi_delete_target(const struct mylite_sql_ast_node *target,
                                    struct mylite_delete_plan *plan);
static int add_delete_target(struct mylite_delete_plan *plan, struct mylite_delete_target target);
static void delete_target_parts_deinit(struct mylite_delete_target *target);
static int copy_delete_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_delete_target *target);
static int copy_update_identifier_table_name(const struct mylite_sql_ast_node *table_name,
                                             struct dml_table_name_output output);
static void free_identifier_parts(char **parts, size_t part_count);

int mylite_dml_copy_update_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_update_plan *plan)
{
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *assignments = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    target = mylite_ast_child_at(statement, 0U);
    assignments = mylite_ast_child_at(statement, 1U);
    if (target != NULL && target->kind == MYLITE_SQL_AST_UPDATE_TARGET) {
        plan->form = MYLITE_UPDATE_SINGLE_TABLE;
        status = copy_update_target(target, &plan->target);
    } else if (target != NULL && (target->kind == MYLITE_SQL_AST_FROM_TABLE ||
                                  target->kind == MYLITE_SQL_AST_FROM_TABLE_REFERENCES)) {
        plan->form = MYLITE_UPDATE_JOINED_TABLES;
        status = MYLITE_OK;
    } else {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = copy_update_assignments(assignments, plan);
    }
    return status;
}

int mylite_dml_copy_delete_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_delete_plan *plan)
{
    const struct mylite_sql_ast_node *target = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    plan->form = statement->delete_form;
    target = mylite_ast_child_at(statement, 0U);
    if (statement->delete_form == MYLITE_SQL_AST_DELETE_SINGLE_TABLE) {
        return copy_delete_target(target, &plan->target);
    }

    if (statement->delete_form != MYLITE_SQL_AST_DELETE_TARGETS_FROM &&
        statement->delete_form != MYLITE_SQL_AST_DELETE_FROM_TARGETS_USING) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_multi_delete_targets(target, plan);
    if (status == MYLITE_OK) {
        plan->from_clause = mylite_ast_child_at(statement, 1U);
    }
    return status;
}

static int copy_multi_delete_targets(const struct mylite_sql_ast_node *targets,
                                     struct mylite_delete_plan *plan)
{
    if (targets == NULL || targets->kind != MYLITE_SQL_AST_DELETE_TARGET_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *target = targets->first_child; target != NULL;
         target = target->next_sibling) {
        int status = copy_multi_delete_target(target, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->target_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_multi_delete_target(const struct mylite_sql_ast_node *target,
                                    struct mylite_delete_plan *plan)
{
    struct mylite_delete_target copied = {0};
    int status = MYLITE_OK;

    if (target == NULL || target->kind != MYLITE_SQL_AST_DELETE_TARGET_NAME) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_delete_table_name(mylite_ast_child_at(target, 0U), &copied);
    if (status == MYLITE_OK) {
        status = add_delete_target(plan, copied);
    }
    if (status != MYLITE_OK) {
        delete_target_parts_deinit(&copied);
    }
    return status;
}

static int add_delete_target(struct mylite_delete_plan *plan, struct mylite_delete_target target)
{
    struct mylite_delete_target *targets =
        realloc(plan->targets, (plan->target_count + 1U) * sizeof(*plan->targets));

    if (targets == NULL) {
        return MYLITE_NOMEM;
    }

    plan->targets = targets;
    plan->targets[plan->target_count++] = target;
    return MYLITE_OK;
}

static void delete_target_parts_deinit(struct mylite_delete_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    free(target->alias);
    *target = (struct mylite_delete_target){0};
}

static int copy_update_target(const struct mylite_sql_ast_node *target,
                              struct mylite_update_target *out_target)
{
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    int status = MYLITE_OK;

    if (target == NULL || target->kind != MYLITE_SQL_AST_UPDATE_TARGET) {
        return MYLITE_UNSUPPORTED;
    }

    table_name = mylite_ast_child_at(target, 0U);
    alias = mylite_ast_child_at(target, 1U);
    status = copy_update_table_name(table_name, out_target);
    if (status != MYLITE_OK) {
        return status;
    }
    if (alias != NULL) {
        out_target->alias = mylite_copy_identifier_span(alias);
        if (out_target->alias == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_update_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_update_target *target)
{
    return copy_update_identifier_table_name(table_name, (struct dml_table_name_output){
                                                             .schema_name = &target->schema_name,
                                                             .table_name = &target->table_name,
                                                         });
}

static int copy_update_assignments(const struct mylite_sql_ast_node *assignments,
                                   struct mylite_update_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_update_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_update_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_update_plan *plan)
{
    struct mylite_update_assignment update_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_update_column_reference(mylite_ast_child_at(assignment, 0U),
                                          &update_assignment.target);
    if (status == MYLITE_OK) {
        status = add_update_assignment(plan, update_assignment);
    }
    if (status != MYLITE_OK) {
        mylite_dml_update_assignment_deinit(&update_assignment);
    }
    return status;
}

static int add_update_assignment(struct mylite_update_plan *plan,
                                 struct mylite_update_assignment assignment)
{
    struct mylite_update_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}

static int copy_update_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_update_column_reference *out_reference)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        free_identifier_parts(parts, part_count);
        return status;
    }

    if (part_count == 1U) {
        out_reference->column_name = parts[0];
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        out_reference->table_name = parts[0];
        out_reference->column_name = parts[1];
        return MYLITE_OK;
    }
    if (part_count == 3U) {
        out_reference->schema_name = parts[0];
        out_reference->table_name = parts[1];
        out_reference->column_name = parts[2];
        return MYLITE_OK;
    }

    free_identifier_parts(parts, part_count);
    return MYLITE_UNSUPPORTED;
}

static int copy_delete_target(const struct mylite_sql_ast_node *target,
                              struct mylite_delete_target *out_target)
{
    const struct mylite_sql_ast_node *table_name = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    int status = MYLITE_OK;

    if (target == NULL || target->kind != MYLITE_SQL_AST_DELETE_TARGET) {
        return MYLITE_UNSUPPORTED;
    }

    table_name = mylite_ast_child_at(target, 0U);
    alias = mylite_ast_child_at(target, 1U);
    status = copy_delete_table_name(table_name, out_target);
    if (status != MYLITE_OK) {
        return status;
    }
    if (alias != NULL) {
        out_target->alias = mylite_copy_identifier_span(alias);
        if (out_target->alias == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_delete_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_delete_target *target)
{
    return copy_update_identifier_table_name(table_name, (struct dml_table_name_output){
                                                             .schema_name = &target->schema_name,
                                                             .table_name = &target->table_name,
                                                         });
}

static int copy_update_identifier_table_name(const struct mylite_sql_ast_node *table_name,
                                             struct dml_table_name_output output)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = MYLITE_OK;

    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }

    status = mylite_copy_identifier_parts(table_name, parts, &part_count);
    if (status != MYLITE_OK) {
        free_identifier_parts(parts, part_count);
        return status;
    }
    if (part_count == 1U) {
        *output.table_name = parts[0];
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        *output.schema_name = parts[0];
        *output.table_name = parts[1];
        return MYLITE_OK;
    }

    free_identifier_parts(parts, part_count);
    return MYLITE_UNSUPPORTED;
}

static void free_identifier_parts(char **parts, size_t part_count)
{
    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
    }
}
