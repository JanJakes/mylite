#include "mylite_select.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sqlite3.h"

size_t mylite_select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_ascii_case_equal(plan->outputs[index].label, label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

size_t mylite_select_output_label_span_count(const struct mylite_select_plan *plan,
                                             struct mylite_sql_source_span label, size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_span_equal_ci(label, plan->outputs[index].label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

const struct mylite_select_column *
mylite_select_plan_column_const(const struct mylite_select_plan *plan, size_t column_index,
                                const struct mylite_select_table **out_table)
{
    size_t table_count = mylite_select_plan_table_count(plan);

    if (out_table != NULL) {
        *out_table = NULL;
    }
    for (size_t table_index = 0U; table_index < table_count; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table != NULL && column_index >= table->first_column_index &&
            column_index < table->first_column_index + table->column_count) {
            if (out_table != NULL) {
                *out_table = table;
            }
            return &table->columns[column_index - table->first_column_index];
        }
    }
    return NULL;
}

int mylite_select_resolve_column_in_table(const struct mylite_select_plan *plan,
                                          const struct mylite_select_table *table,
                                          const char *column_name, size_t *out_index)
{
    (void)plan;
    if (table == NULL || column_name == NULL || out_index == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            *out_index = table->first_column_index + index;
            return MYLITE_OK;
        }
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_set_ambiguous_column_error(mylite_db *database, const char *column_name,
                                             const char *clause_context)
{
    char *message = sqlite3_mprintf("Column '%q' in %s is ambiguous", column_name,
                                    clause_context == NULL ? "field list" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

bool mylite_select_plan_has_column_span(const struct mylite_select_plan *plan,
                                        struct mylite_sql_source_span name)
{
    for (size_t index = 0U; index < mylite_select_plan_column_count(plan); ++index) {
        const struct mylite_select_column *column =
            mylite_select_plan_column_const(plan, index, NULL);

        if (column != NULL && column->name != NULL && mylite_span_equal_ci(name, column->name)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_plan_has_visible_table_span(const struct mylite_select_plan *plan,
                                               struct mylite_sql_source_span name)
{
    for (size_t index = 0U; index < mylite_select_plan_table_count(plan); ++index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, index);
        const char *visible_name = table == NULL || table->alias == NULL ? NULL : table->alias;

        if (table != NULL && visible_name == NULL) {
            visible_name = table->table_name;
        }
        if (visible_name != NULL && mylite_span_equal_ci(name, visible_name)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_plan_has_outer_join(const struct mylite_select_plan *plan)
{
    for (size_t index = 0U; plan != NULL && index < plan->join_step_count; ++index) {
        enum mylite_sql_ast_join_type join_type = plan->join_steps[index].join_type;

        if (join_type == MYLITE_SQL_AST_JOIN_LEFT || join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
            return true;
        }
    }
    return false;
}

bool mylite_select_duplicate_mode_is_distinct(enum mylite_sql_ast_select_duplicate_mode mode)
{
    return mode == MYLITE_SQL_AST_SELECT_DUPLICATES_DISTINCT;
}

bool mylite_select_plan_requires_custom_runtime(const struct mylite_select_plan *plan,
                                                const struct mylite_select_clause_nodes *clauses)
{
    if (clauses != NULL &&
        (clauses->where != NULL || clauses->group_by != NULL || clauses->having != NULL ||
         clauses->order_by != NULL || clauses->limit != NULL)) {
        return true;
    }
    if (plan == NULL) {
        return false;
    }
    if (plan->calc_found_rows) {
        return true;
    }
    if (mylite_select_duplicate_mode_is_distinct(plan->duplicate_mode)) {
        return true;
    }
    if (mylite_select_plan_table_count(plan) > 1U) {
        return true;
    }
    if (plan->has_aggregate) {
        return true;
    }
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
            return true;
        }
    }
    return false;
}

size_t mylite_select_plan_table_count(const struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return 0U;
    }
    if (plan->table_count != 0U) {
        return plan->table_count;
    }
    return plan->table.table_name == NULL ? 0U : 1U;
}

struct mylite_select_table *mylite_select_plan_table(struct mylite_select_plan *plan,
                                                     size_t table_index)
{
    if (plan == NULL) {
        return NULL;
    }
    if (plan->table_count != 0U) {
        return table_index < plan->table_count ? &plan->tables[table_index] : NULL;
    }
    return table_index == 0U && plan->table.table_name != NULL ? &plan->table : NULL;
}

const struct mylite_select_table *
mylite_select_plan_table_const(const struct mylite_select_plan *plan, size_t table_index)
{
    if (plan == NULL) {
        return NULL;
    }
    if (plan->table_count != 0U) {
        return table_index < plan->table_count ? &plan->tables[table_index] : NULL;
    }
    return table_index == 0U && plan->table.table_name != NULL ? &plan->table : NULL;
}

size_t mylite_select_plan_column_count(const struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return 0U;
    }
    if (plan->column_count != 0U || plan->table_count != 0U) {
        return plan->column_count;
    }
    return plan->table.column_count;
}
