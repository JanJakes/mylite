#include "mylite_dml.h"

#include "mylite_expression.h"

#include <stdlib.h>

static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference);

static void insert_value_child_deinit(struct mylite_insert_value *value);

static void insert_unique_index_deinit(struct mylite_insert_unique_index *index);

static void update_target_deinit(struct mylite_update_target *target);

static void update_column_reference_deinit(struct mylite_update_column_reference *reference);

static void delete_target_deinit(struct mylite_delete_target *target);

void mylite_dml_insert_values_plan_deinit(struct mylite_insert_values_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        free(plan->columns[index]);
    }
    free((void *)plan->columns);
    free(plan->row_alias);
    for (size_t index = 0U; index < plan->alias_column_count; ++index) {
        free(plan->alias_columns[index]);
    }
    free((void *)plan->alias_columns);
    for (size_t index = 0U; index < plan->row_count; ++index) {
        mylite_dml_insert_row_deinit(&plan->rows[index]);
    }
    free(plan->rows);
    *plan = (struct mylite_insert_values_plan){0};
}

void mylite_dml_insert_set_plan_deinit(struct mylite_insert_set_plan *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        mylite_dml_insert_set_assignment_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_insert_set_plan){0};
}

void mylite_dml_insert_set_assignment_deinit(struct mylite_insert_set_assignment *assignment) {
    if (assignment == NULL) {
        return;
    }

    insert_column_reference_deinit(&assignment->target);
    mylite_dml_insert_value_deinit(&assignment->value);
    *assignment = (struct mylite_insert_set_assignment){0};
}

void mylite_dml_insert_duplicate_update_plan_deinit(
    struct mylite_insert_duplicate_update_plan *plan
) {
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        mylite_dml_insert_update_assignment_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_insert_duplicate_update_plan){0};
}

void mylite_dml_insert_update_assignment_deinit(
    struct mylite_insert_update_assignment *assignment
) {
    if (assignment == NULL) {
        return;
    }

    insert_column_reference_deinit(&assignment->target);
    mylite_dml_insert_value_deinit(&assignment->value);
    *assignment = (struct mylite_insert_update_assignment){0};
}

void mylite_dml_update_plan_deinit(struct mylite_update_plan *plan) {
    if (plan == NULL) {
        return;
    }

    update_target_deinit(&plan->target);
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        mylite_dml_update_assignment_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_update_plan){0};
}

void mylite_dml_update_assignment_deinit(struct mylite_update_assignment *assignment) {
    if (assignment == NULL) {
        return;
    }

    update_column_reference_deinit(&assignment->target);
    *assignment = (struct mylite_update_assignment){0};
}

void mylite_dml_update_order_plan_deinit(struct mylite_update_order_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->order_keys);
    *plan = (struct mylite_update_order_plan){0};
}

void mylite_dml_update_rowset_deinit(struct mylite_update_rowset *rowset) {
    if (rowset == NULL) {
        return;
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        mylite_dml_update_row_deinit(&rowset->rows[index]);
    }
    free(rowset->rows);
    *rowset = (struct mylite_update_rowset){0};
}

void mylite_dml_update_row_deinit(struct mylite_update_row *row) {
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_expression_value_deinit(&row->values[index]);
    }
    for (size_t index = 0U; index < row->order_value_count; ++index) {
        mylite_expression_value_deinit(&row->order_values[index]);
    }
    free(row->values);
    free(row->order_values);
    *row = (struct mylite_update_row){0};
}

void mylite_dml_delete_plan_deinit(struct mylite_delete_plan *plan) {
    if (plan == NULL) {
        return;
    }

    delete_target_deinit(&plan->target);
    for (size_t index = 0U; index < plan->target_count; ++index) {
        delete_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_delete_plan){0};
}

void mylite_dml_insert_row_deinit(struct mylite_insert_row *row) {
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_dml_insert_value_deinit(&row->values[index]);
    }
    free(row->values);
    *row = (struct mylite_insert_row){0};
}

void mylite_dml_insert_value_deinit(struct mylite_insert_value *value) {
    if (value == NULL) {
        return;
    }

    free(value->text);
    insert_column_reference_deinit(&value->column_reference);
    insert_value_child_deinit(value->left);
    insert_value_child_deinit(value->right);
    free(value->left);
    free(value->right);
    *value = (struct mylite_insert_value){0};
}

void mylite_dml_insert_table_deinit(struct mylite_insert_table *table) {
    if (table == NULL) {
        return;
    }

    free(table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        mylite_dml_insert_table_column_deinit(&table->columns[index]);
    }
    free(table->columns);
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        insert_unique_index_deinit(&table->unique_indexes[index]);
    }
    free(table->unique_indexes);
    *table = (struct mylite_insert_table){0};
}

void mylite_dml_insert_table_column_deinit(struct mylite_insert_table_column *column) {
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->default_text);
    free(column->data_type);
    free(column->column_type);
    free(column->character_set_name);
    free(column->extra);
    *column = (struct mylite_insert_table_column){0};
}

void mylite_dml_insert_bound_values_deinit(
    struct mylite_insert_bound_value *values,
    size_t value_count
) {
    if (values == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        mylite_dml_insert_bound_value_deinit(&values[index]);
    }
    free(values);
}

void mylite_dml_insert_bound_value_deinit(struct mylite_insert_bound_value *value) {
    if (value == NULL) {
        return;
    }

    free(value->text_value);
    *value = (struct mylite_insert_bound_value){0};
}

static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference) {
    if (reference == NULL) {
        return;
    }

    free(reference->schema_name);
    free(reference->table_name);
    free(reference->column_name);
    *reference = (struct mylite_insert_column_reference){0};
}

static void insert_value_child_deinit(struct mylite_insert_value *value) {
    if (value == NULL) {
        return;
    }

    free(value->text);
    insert_column_reference_deinit(&value->column_reference);
    free(value->left);
    free(value->right);
    *value = (struct mylite_insert_value){0};
}

static void insert_unique_index_deinit(struct mylite_insert_unique_index *index) {
    if (index == NULL) {
        return;
    }

    free(index->name);
    free(index->column_indexes);
    free(index->prefix_lengths);
    *index = (struct mylite_insert_unique_index){0};
}

static void update_target_deinit(struct mylite_update_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    free(target->alias);
    *target = (struct mylite_update_target){0};
}

static void update_column_reference_deinit(struct mylite_update_column_reference *reference) {
    if (reference == NULL) {
        return;
    }

    free(reference->schema_name);
    free(reference->table_name);
    free(reference->column_name);
    *reference = (struct mylite_update_column_reference){0};
}

static void delete_target_deinit(struct mylite_delete_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    free(target->alias);
    *target = (struct mylite_delete_target){0};
}
