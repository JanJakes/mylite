#include "mylite_select_from_nullability.h"

#include "mylite_field_descriptor.h"
#include "mylite_select.h"

static void mark_select_range_nullable(
    struct mylite_select_plan *plan,
    struct mylite_select_table_range range
);

void mylite_select_from_apply_outer_join_nullability(struct mylite_select_plan *plan) {
    for (size_t index = 0U; index < plan->join_step_count; ++index) {
        const struct mylite_select_join_step *step = &plan->join_steps[index];

        if (step->join_type == MYLITE_SQL_AST_JOIN_LEFT) {
            mark_select_range_nullable(plan, step->right_range);
        } else if (step->join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
            mark_select_range_nullable(plan, step->left_range);
        }
    }
}

static void mark_select_range_nullable(
    struct mylite_select_plan *plan,
    struct mylite_select_table_range range
) {
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
