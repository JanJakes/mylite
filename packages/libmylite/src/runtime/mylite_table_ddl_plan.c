#include "mylite_table_ddl.h"

#include <stdlib.h>

static void create_table_options_deinit(struct mylite_create_table_options *options);

void mylite_table_ddl_create_table_plan_deinit(struct mylite_create_table_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    free(plan->source_schema_name);
    free(plan->source_table_name);
    create_table_options_deinit(&plan->options);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        mylite_table_ddl_create_table_column_deinit(&plan->columns[index]);
    }
    free(plan->columns);
    for (size_t index = 0U; index < plan->index_count; ++index) {
        mylite_table_ddl_create_table_index_deinit(&plan->indexes[index]);
    }
    free(plan->indexes);
    for (size_t index = 0U; index < plan->check_count; ++index) {
        mylite_table_ddl_create_table_check_deinit(&plan->checks[index]);
    }
    free(plan->checks);
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        mylite_table_ddl_create_table_foreign_key_deinit(&plan->foreign_keys[index]);
    }
    free(plan->foreign_keys);
    *plan = (struct mylite_create_table_plan){0};
}

void mylite_table_ddl_drop_table_plan_deinit(struct mylite_drop_table_plan *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        mylite_table_ddl_drop_table_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_drop_table_plan){0};
}

void mylite_table_ddl_rename_table_plan_deinit(struct mylite_rename_table_plan *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        mylite_table_ddl_rename_table_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_rename_table_plan){0};
}

void mylite_table_ddl_truncate_table_plan_deinit(struct mylite_truncate_table_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    *plan = (struct mylite_truncate_table_plan){0};
}

void mylite_table_ddl_alter_table_plan_deinit(struct mylite_alter_table_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    for (size_t index = 0U; index < plan->action_count; ++index) {
        mylite_table_ddl_alter_table_action_deinit(&plan->actions[index]);
    }
    free(plan->actions);
    free(plan->unsupported_algorithm);
    free(plan->unsupported_lock);
    *plan = (struct mylite_alter_table_plan){0};
}

void mylite_table_ddl_index_ddl_plan_deinit(struct mylite_index_ddl_plan *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    free(plan->index_name);
    mylite_table_ddl_create_table_index_deinit(&plan->index);
    *plan = (struct mylite_index_ddl_plan){0};
}

void mylite_table_ddl_drop_table_target_deinit(struct mylite_drop_table_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_drop_table_target){0};
}

void mylite_table_ddl_rename_table_target_deinit(struct mylite_rename_table_target *target) {
    if (target == NULL) {
        return;
    }

    free(target->source_schema_name);
    free(target->source_table_name);
    free(target->target_schema_name);
    free(target->target_table_name);
    *target = (struct mylite_rename_table_target){0};
}

void mylite_table_ddl_alter_table_model_deinit(struct mylite_alter_table_model *model) {
    if (model == NULL) {
        return;
    }

    free(model->schema_name);
    free(model->table_name);
    free(model->physical_name);
    free(model->table_collation);
    for (size_t index = 0U; index < model->column_count; ++index) {
        mylite_table_ddl_alter_table_column_deinit(&model->columns[index]);
    }
    free(model->columns);
    for (size_t index = 0U; index < model->index_count; ++index) {
        mylite_table_ddl_alter_table_index_deinit(&model->indexes[index]);
    }
    free(model->indexes);
    *model = (struct mylite_alter_table_model){0};
}

void mylite_table_ddl_alter_table_column_deinit(struct mylite_alter_table_column *column) {
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->source_name);
    free(column->column_default);
    free(column->is_nullable);
    free(column->data_type);
    free(column->character_set_name);
    free(column->collation_name);
    free(column->column_type);
    free(column->column_key);
    free(column->extra);
    free(column->column_comment);
    free(column->generation_expression);
    *column = (struct mylite_alter_table_column){0};
}

void mylite_table_ddl_alter_table_index_deinit(struct mylite_alter_table_index *index) {
    if (index == NULL) {
        return;
    }

    free(index->index_schema);
    free(index->name);
    free(index->index_type);
    free(index->comment);
    free(index->index_comment);
    free(index->is_visible);
    for (size_t part = 0U; part < index->part_count; ++part) {
        mylite_table_ddl_alter_table_index_part_deinit(&index->parts[part]);
    }
    free(index->parts);
    *index = (struct mylite_alter_table_index){0};
}

void mylite_table_ddl_alter_table_index_part_deinit(struct mylite_alter_table_index_part *part) {
    if (part == NULL) {
        return;
    }

    free(part->column_name);
    free(part->collation);
    free(part->nullable);
    *part = (struct mylite_alter_table_index_part){0};
}

void mylite_table_ddl_create_table_column_deinit(struct mylite_create_table_column *column) {
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->type.character_set);
    free(column->type.collation);
    free(column->default_text);
    free(column->comment);
    free(column->generation_expression);
    *column = (struct mylite_create_table_column){0};
}

void mylite_table_ddl_create_table_index_deinit(struct mylite_create_table_index *index) {
    if (index == NULL) {
        return;
    }

    free(index->name);
    free(index->comment);
    for (size_t part = 0U; part < index->part_count; ++part) {
        mylite_table_ddl_create_table_key_part_deinit(&index->parts[part]);
    }
    free(index->parts);
    *index = (struct mylite_create_table_index){0};
}

void mylite_table_ddl_create_table_check_deinit(struct mylite_create_table_check *check) {
    if (check == NULL) {
        return;
    }

    free(check->name);
    free(check->clause);
    *check = (struct mylite_create_table_check){0};
}

void mylite_table_ddl_create_table_foreign_key_deinit(
    struct mylite_create_table_foreign_key *foreign_key
) {
    if (foreign_key == NULL) {
        return;
    }

    free(foreign_key->constraint_name);
    free(foreign_key->supporting_index_name);
    for (size_t index = 0U; index < foreign_key->column_count; ++index) {
        free(foreign_key->column_names[index]);
    }
    free(foreign_key->column_names);
    free(foreign_key->referenced_schema_name);
    free(foreign_key->referenced_table_name);
    for (size_t index = 0U; index < foreign_key->referenced_column_count; ++index) {
        free(foreign_key->referenced_column_names[index]);
    }
    free(foreign_key->referenced_column_names);
    free(foreign_key->unique_constraint_name);
    *foreign_key = (struct mylite_create_table_foreign_key){0};
}

void mylite_table_ddl_create_table_key_part_deinit(struct mylite_create_table_key_part *part) {
    if (part == NULL) {
        return;
    }

    free(part->column_name);
    *part = (struct mylite_create_table_key_part){0};
}

void mylite_table_ddl_alter_table_action_deinit(struct mylite_alter_table_action *action) {
    if (action == NULL) {
        return;
    }

    free(action->old_name);
    free(action->new_name);
    free(action->new_schema_name);
    free(action->after_column);
    mylite_table_ddl_create_table_column_deinit(&action->column);
    mylite_table_ddl_create_table_index_deinit(&action->index);
    mylite_table_ddl_create_table_foreign_key_deinit(&action->foreign_key);
    *action = (struct mylite_alter_table_action){0};
}

static void create_table_options_deinit(struct mylite_create_table_options *options) {
    if (options == NULL) {
        return;
    }

    free(options->engine);
    free(options->character_set);
    free(options->collation);
    free(options->comment);
    *options = (struct mylite_create_table_options){0};
}
