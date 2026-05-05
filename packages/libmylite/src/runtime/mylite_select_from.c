#include "mylite_select_from.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_field_descriptor.h"
#include "mylite_select.h"
#include "mylite_select_catalog.h"
#include "mylite_select_from_using.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int copy_select_from_clause(mylite_db *database,
                                   const struct mylite_sql_ast_node *from_clause,
                                   struct mylite_select_plan *plan);
static int copy_select_table_reference(const struct mylite_sql_ast_node *from_clause,
                                       struct mylite_select_table *table);
static int copy_select_table_reference_node(mylite_db *database,
                                            const struct mylite_sql_ast_node *reference,
                                            struct mylite_select_plan *plan,
                                            struct mylite_select_table_range *out_range);
static int copy_select_base_table_reference_node(mylite_db *database,
                                                 const struct mylite_sql_ast_node *reference,
                                                 struct mylite_select_plan *plan,
                                                 struct mylite_select_table_range *out_range);
static int copy_select_table_reference_list(mylite_db *database,
                                            const struct mylite_sql_ast_node *list,
                                            struct mylite_select_plan *plan);
static int add_select_from_range(mylite_db *database, struct mylite_select_plan *plan,
                                 struct mylite_select_table_range range);
static int copy_select_join_expression(mylite_db *database, const struct mylite_sql_ast_node *join,
                                       struct mylite_select_plan *plan,
                                       struct mylite_select_table_range *out_range);
static int add_select_join_stack_entry(mylite_db *database,
                                       struct mylite_select_join_stack_entry **entries,
                                       size_t *entry_count, const struct mylite_sql_ast_node *right,
                                       const struct mylite_sql_ast_node *condition,
                                       enum mylite_sql_ast_join_type join_type);
static int copy_select_join_right_operand(mylite_db *database,
                                          const struct mylite_select_join_stack_entry *entry,
                                          struct mylite_select_plan *plan,
                                          struct mylite_select_table_range *left_range);
static int apply_select_join_condition(mylite_db *database,
                                       const struct mylite_select_join_stack_entry *entry,
                                       struct mylite_select_plan *plan,
                                       struct mylite_select_table_range left_range,
                                       struct mylite_select_table_range right_range);
static int add_select_join_step(mylite_db *database, struct mylite_select_plan *plan,
                                enum mylite_sql_ast_join_type join_type,
                                struct mylite_select_table_range left_range,
                                struct mylite_select_table_range right_range,
                                struct mylite_select_table_range joined_range);
static void apply_select_outer_join_nullability(struct mylite_select_plan *plan);
static void mark_select_range_nullable(struct mylite_select_plan *plan,
                                       struct mylite_select_table_range range);
static int add_select_plan_table(mylite_db *database, struct mylite_select_plan *plan,
                                 struct mylite_select_table *table, size_t *out_table_index);
static int add_select_join_predicate(mylite_db *database, struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *expression,
                                     size_t first_table, size_t table_count);
static int resolve_select_plan_tables(mylite_db *database, struct mylite_select_plan *plan);
static int validate_select_table_aliases(mylite_db *database,
                                         const struct mylite_select_plan *plan);
static int load_select_plan_columns(mylite_db *database, struct mylite_select_plan *plan);

int mylite_select_bind_from_clause(mylite_db *database,
                                   const struct mylite_sql_ast_node *from_clause,
                                   struct mylite_select_plan *plan)
{
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL) {
        return MYLITE_MISUSE;
    }

    status = copy_select_from_clause(database, from_clause, plan);
    if (status == MYLITE_OK) {
        status = resolve_select_plan_tables(database, plan);
    }
    if (status == MYLITE_OK) {
        status = load_select_plan_columns(database, plan);
    }
    if (status == MYLITE_OK) {
        apply_select_outer_join_nullability(plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_from_resolve_using_requests(database, plan);
    }
    return status;
}

static int copy_select_from_clause(mylite_db *database,
                                   const struct mylite_sql_ast_node *from_clause,
                                   struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *references = NULL;

    if (from_clause == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    references = mylite_ast_child_at(from_clause, 0U);
    if (from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) {
        int status = copy_select_table_reference(from_clause, &plan->table);

        if (status == MYLITE_OK) {
            status = add_select_from_range(database, plan,
                                           (struct mylite_select_table_range){
                                               .first_table = 0U,
                                               .table_count = 1U,
                                           });
        }
        return status;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE_REFERENCES || references == NULL ||
        references->kind != MYLITE_SQL_AST_TABLE_REFERENCE_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    return copy_select_table_reference_list(database, references, plan);
}

static int copy_select_table_reference(const struct mylite_sql_ast_node *from_clause,
                                       struct mylite_select_table *table)
{
    const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(from_clause, 0U);
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(from_clause, 1U);

    if (table_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        table->table_name = mylite_copy_identifier_span(table_name);
        if (table->table_name == NULL) {
            return MYLITE_NOMEM;
        }
    } else if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
               mylite_ast_child_at(table_name, 0U) != NULL &&
               mylite_ast_child_at(table_name, 1U) != NULL &&
               mylite_ast_child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
               mylite_ast_child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        table->schema_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 0U));
        table->table_name = mylite_copy_identifier_span(mylite_ast_child_at(table_name, 1U));
        if (table->schema_name == NULL || table->table_name == NULL) {
            return MYLITE_NOMEM;
        }
    } else {
        return MYLITE_UNSUPPORTED;
    }

    if (alias != NULL) {
        if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        table->alias = mylite_copy_identifier_span(alias);
        if (table->alias == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_select_table_reference_node(mylite_db *database,
                                            const struct mylite_sql_ast_node *reference,
                                            struct mylite_select_plan *plan,
                                            struct mylite_select_table_range *out_range)
{
    if (reference == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (reference->kind == MYLITE_SQL_AST_FROM_TABLE) {
        return copy_select_base_table_reference_node(database, reference, plan, out_range);
    }
    if (reference->kind == MYLITE_SQL_AST_JOIN_EXPRESSION) {
        return copy_select_join_expression(database, reference, plan, out_range);
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_select_base_table_reference_node(mylite_db *database,
                                                 const struct mylite_sql_ast_node *reference,
                                                 struct mylite_select_plan *plan,
                                                 struct mylite_select_table_range *out_range)
{
    struct mylite_select_table table = {0};
    int status = MYLITE_OK;

    *out_range = (struct mylite_select_table_range){
        .first_table = mylite_select_plan_table_count(plan),
    };
    if (reference == NULL || reference->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_select_table_reference(reference, &table);
    if (status == MYLITE_OK) {
        status = add_select_plan_table(database, plan, &table, &out_range->first_table);
    }
    if (status != MYLITE_OK) {
        mylite_select_table_deinit(&table);
        return status;
    }
    out_range->table_count = 1U;
    return MYLITE_OK;
}

static int copy_select_table_reference_list(mylite_db *database,
                                            const struct mylite_sql_ast_node *list,
                                            struct mylite_select_plan *plan)
{
    for (const struct mylite_sql_ast_node *reference = list == NULL ? NULL : list->first_child;
         reference != NULL; reference = reference->next_sibling) {
        struct mylite_select_table_range range = {0};
        int status = copy_select_table_reference_node(database, reference, plan, &range);

        if (status == MYLITE_OK) {
            status = add_select_from_range(database, plan, range);
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_table_count(plan) == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int add_select_from_range(mylite_db *database, struct mylite_select_plan *plan,
                                 struct mylite_select_table_range range)
{
    struct mylite_select_table_range *ranges =
        realloc(plan->from_ranges, (plan->from_range_count + 1U) * sizeof(*plan->from_ranges));

    if (ranges == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->from_ranges = ranges;
    plan->from_ranges[plan->from_range_count++] = range;
    return MYLITE_OK;
}

static int copy_select_join_expression(mylite_db *database, const struct mylite_sql_ast_node *join,
                                       struct mylite_select_plan *plan,
                                       struct mylite_select_table_range *out_range)
{
    struct mylite_select_join_stack_entry *entries = NULL;
    size_t entry_count = 0U;
    const struct mylite_sql_ast_node *leftmost = join;
    int status = MYLITE_OK;

    while (leftmost != NULL && leftmost->kind == MYLITE_SQL_AST_JOIN_EXPRESSION) {
        status = add_select_join_stack_entry(
            database, &entries, &entry_count, mylite_ast_child_at(leftmost, 1U),
            mylite_ast_child_at(leftmost, 2U), leftmost->join_type);
        if (status != MYLITE_OK) {
            free(entries);
            return status;
        }
        leftmost = mylite_ast_child_at(leftmost, 0U);
    }

    status = copy_select_base_table_reference_node(database, leftmost, plan, out_range);
    for (size_t index = entry_count; status == MYLITE_OK && index > 0U; --index) {
        status = copy_select_join_right_operand(database, &entries[index - 1U], plan, out_range);
    }

    free(entries);
    return status;
}

static int add_select_join_stack_entry(mylite_db *database,
                                       struct mylite_select_join_stack_entry **entries,
                                       size_t *entry_count, const struct mylite_sql_ast_node *right,
                                       const struct mylite_sql_ast_node *condition,
                                       enum mylite_sql_ast_join_type join_type)
{
    struct mylite_select_join_stack_entry *new_entries = NULL;

    if (right == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    new_entries = realloc(*entries, (*entry_count + 1U) * sizeof(**entries));
    if (new_entries == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *entries = new_entries;
    (*entries)[(*entry_count)++] = (struct mylite_select_join_stack_entry){
        .right = right,
        .condition = condition,
        .join_type = join_type,
    };
    return MYLITE_OK;
}

static int copy_select_join_right_operand(mylite_db *database,
                                          const struct mylite_select_join_stack_entry *entry,
                                          struct mylite_select_plan *plan,
                                          struct mylite_select_table_range *left_range)
{
    struct mylite_select_table_range right_range = {0};
    struct mylite_select_table_range joined_range = {0};
    int status = copy_select_base_table_reference_node(database, entry->right, plan, &right_range);

    if (status != MYLITE_OK) {
        return status;
    }
    status = apply_select_join_condition(database, entry, plan, *left_range, right_range);
    if (status == MYLITE_OK) {
        joined_range = (struct mylite_select_table_range){
            .first_table = left_range->first_table,
            .table_count = left_range->table_count + right_range.table_count,
        };
        status = add_select_join_step(database, plan, entry->join_type, *left_range, right_range,
                                      joined_range);
    }
    if (status == MYLITE_OK) {
        *left_range = joined_range;
    }
    return status;
}

static int apply_select_join_condition(mylite_db *database,
                                       const struct mylite_select_join_stack_entry *entry,
                                       struct mylite_select_plan *plan,
                                       struct mylite_select_table_range left_range,
                                       struct mylite_select_table_range right_range)
{
    const struct mylite_sql_ast_node *condition = entry->condition;

    if (condition == NULL) {
        return MYLITE_OK;
    }
    if (condition->join_condition_type == MYLITE_SQL_AST_JOIN_CONDITION_ON) {
        return add_select_join_predicate(database, plan, mylite_ast_child_at(condition, 0U),
                                         left_range.first_table,
                                         left_range.table_count + right_range.table_count);
    }
    if (condition->join_condition_type == MYLITE_SQL_AST_JOIN_CONDITION_USING) {
        return mylite_select_from_add_using_request(
            database, plan, condition, left_range.first_table, left_range.table_count,
            right_range.first_table, right_range.table_count, entry->join_type);
    }
    return MYLITE_UNSUPPORTED;
}

static int add_select_join_step(mylite_db *database, struct mylite_select_plan *plan,
                                enum mylite_sql_ast_join_type join_type,
                                struct mylite_select_table_range left_range,
                                struct mylite_select_table_range right_range,
                                struct mylite_select_table_range joined_range)
{
    struct mylite_select_join_step *steps =
        realloc(plan->join_steps, (plan->join_step_count + 1U) * sizeof(*plan->join_steps));

    if (steps == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->join_steps = steps;
    plan->join_steps[plan->join_step_count++] = (struct mylite_select_join_step){
        .join_type = join_type,
        .left_range = left_range,
        .right_range = right_range,
        .joined_range = joined_range,
    };
    return MYLITE_OK;
}

static void apply_select_outer_join_nullability(struct mylite_select_plan *plan)
{
    for (size_t index = 0U; index < plan->join_step_count; ++index) {
        const struct mylite_select_join_step *step = &plan->join_steps[index];

        if (step->join_type == MYLITE_SQL_AST_JOIN_LEFT) {
            mark_select_range_nullable(plan, step->right_range);
        } else if (step->join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
            mark_select_range_nullable(plan, step->left_range);
        }
    }
}

static void mark_select_range_nullable(struct mylite_select_plan *plan,
                                       struct mylite_select_table_range range)
{
    size_t last_table = range.first_table + range.table_count;

    for (size_t table_index = range.first_table; table_index < last_table; ++table_index) {
        struct mylite_select_table *table = mylite_select_plan_table(plan, table_index);

        if (table == NULL) {
            continue;
        }
        for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
            mylite_field_descriptor_set_nullable(&table->columns[column_index].descriptor, true);
        }
    }
}

static int add_select_plan_table(mylite_db *database, struct mylite_select_plan *plan,
                                 struct mylite_select_table *table, size_t *out_table_index)
{
    struct mylite_select_table *tables =
        realloc(plan->tables, (plan->table_count + 1U) * sizeof(*plan->tables));

    if (tables == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->tables = tables;
    plan->tables[plan->table_count] = *table;
    *table = (struct mylite_select_table){0};
    *out_table_index = plan->table_count++;
    return MYLITE_OK;
}

static int add_select_join_predicate(mylite_db *database, struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *expression,
                                     size_t first_table, size_t table_count)
{
    struct mylite_select_join_predicate *predicates = NULL;

    if (expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    predicates = realloc(plan->join_predicates,
                         (plan->join_predicate_count + 1U) * sizeof(*plan->join_predicates));
    if (predicates == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->join_predicates = predicates;
    plan->join_predicates[plan->join_predicate_count++] = (struct mylite_select_join_predicate){
        .expression = expression,
        .first_table = first_table,
        .table_count = table_count,
    };
    return MYLITE_OK;
}

static int resolve_select_plan_tables(mylite_db *database, struct mylite_select_plan *plan)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t index = 0U; index < table_count; ++index) {
        struct mylite_select_table *table = mylite_select_plan_table(plan, index);
        int status = mylite_select_resolve_table_target(database, table);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return validate_select_table_aliases(database, plan);
}

static int validate_select_table_aliases(mylite_db *database, const struct mylite_select_plan *plan)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t left = 0U; left < table_count; ++left) {
        const struct mylite_select_table *left_table = mylite_select_plan_table_const(plan, left);
        const char *left_name =
            left_table->alias == NULL ? left_table->table_name : left_table->alias;

        for (size_t right = left + 1U; right < table_count; ++right) {
            const struct mylite_select_table *right_table =
                mylite_select_plan_table_const(plan, right);
            const char *right_name =
                right_table->alias == NULL ? right_table->table_name : right_table->alias;

            if (strcmp(left_name, right_name) == 0) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database, "Not unique table/alias: '", left_name, "'");

                if (status == MYLITE_OK) {
                    status = mylite_diagnostics_append_error(
                        database, MYLITE_MYSQL_ER_NONUNIQ_TABLE, mylite_error_message(database));
                }
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }
    return MYLITE_OK;
}

static int load_select_plan_columns(mylite_db *database, struct mylite_select_plan *plan)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    plan->column_count = 0U;
    for (size_t index = 0U; index < table_count; ++index) {
        struct mylite_select_table *table = mylite_select_plan_table(plan, index);
        int status = MYLITE_OK;

        table->first_column_index = plan->column_count;
        status = mylite_select_load_table_columns(database, table);
        if (status != MYLITE_OK) {
            return status;
        }
        plan->column_count += table->column_count;
    }
    return MYLITE_OK;
}
