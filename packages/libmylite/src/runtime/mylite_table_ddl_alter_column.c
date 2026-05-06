#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter_column_definition.h"
#include "mylite_table_ddl_alter_model.h"

#include <mylite/mylite.h>

#include <stdlib.h>

static int apply_alter_table_add_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_drop_column(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_rename_column(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_change_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_modify_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
);

static int apply_alter_table_column_position(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model,
    size_t column_index
);

static int remove_alter_table_column(struct mylite_alter_table_model *model, size_t column_index);

int mylite_table_ddl_apply_alter_table_column_action(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    if (database == NULL || schema_default == NULL || action == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }

    switch (action->kind) {
    case MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN:
        return apply_alter_table_add_column(database, schema_default, action, model);
    case MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN:
        return apply_alter_table_drop_column(database, action, model);
    case MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN:
        return apply_alter_table_rename_column(database, action, model);
    case MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN:
        return apply_alter_table_change_column(database, schema_default, action, model);
    case MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN:
        return apply_alter_table_modify_column(database, schema_default, action, model);
    case MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE:
    case MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
    case MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_DROP_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY:
        return MYLITE_MISUSE;
    }
    return MYLITE_MISUSE;
}

static int apply_alter_table_add_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    struct mylite_alter_table_column column = {0};
    int status = MYLITE_OK;

    if (mylite_table_ddl_find_alter_table_column(model, action->column.name) != NULL) {
        return mylite_table_ddl_set_alter_table_duplicate_column_error(
            database,
            action->column.name
        );
    }
    if (mylite_table_ddl_alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = mylite_table_ddl_init_alter_table_column_from_definition(
        database,
        schema_default,
        &action->column,
        NULL,
        true,
        &column
    );
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_add_alter_table_column(model, column);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        return status;
    }
    return apply_alter_table_column_position(database, action, model, model->column_count - 1U);
}

static int apply_alter_table_drop_column(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->old_name);

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    if (model->column_count == 1U) {
        return mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(database);
    }

    for (size_t index = 0U; index < model->index_count;) {
        struct mylite_alter_table_index *table_index = &model->indexes[index];

        for (size_t part = 0U; part < table_index->part_count;) {
            if (mylite_ascii_case_equal(table_index->parts[part].column_name, action->old_name)) {
                mylite_table_ddl_alter_table_index_part_deinit(&table_index->parts[part]);
                for (size_t next = part + 1U; next < table_index->part_count; ++next) {
                    table_index->parts[next - 1U] = table_index->parts[next];
                }
                --table_index->part_count;
                table_index->changed = true;
                continue;
            }
            ++part;
        }
        if (table_index->part_count == 0U) {
            mylite_table_ddl_alter_table_index_deinit(table_index);
            for (size_t next = index + 1U; next < model->index_count; ++next) {
                model->indexes[next - 1U] = model->indexes[next];
            }
            --model->index_count;
            continue;
        }
        ++index;
    }

    if (model->columns[column_index].auto_increment) {
        model->clear_auto_increment = true;
        model->report_copied_rows = true;
    }
    return remove_alter_table_column(model, column_index);
}

static int apply_alter_table_rename_column(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->old_name);
    char *new_name = NULL;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(
            database,
            model->table_name,
            action->old_name
        );
    }
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_ascii_case_equal(action->old_name, action->new_name) &&
        mylite_table_ddl_find_alter_table_column(model, action->new_name) != NULL) {
        return mylite_table_ddl_set_alter_table_duplicate_column_error(database, action->new_name);
    }

    new_name = mylite_copy_nonempty_cstring(action->new_name);
    if (new_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(model->columns[column_index].name);
    model->columns[column_index].name = new_name;

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (mylite_ascii_case_equal(
                    model->indexes[index].parts[part].column_name,
                    action->old_name
                )) {
                char *part_name = mylite_copy_nonempty_cstring(action->new_name);

                if (part_name == NULL) {
                    (void)mylite_diagnostics_set_error_message(database, "out of memory");
                    return MYLITE_NOMEM;
                }
                free(model->indexes[index].parts[part].column_name);
                model->indexes[index].parts[part].column_name = part_name;
                model->indexes[index].changed = true;
            }
        }
    }
    return MYLITE_OK;
}

static int apply_alter_table_change_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->old_name);
    const char *new_name = action->column.name;
    int status = MYLITE_OK;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(
            database,
            model->table_name,
            action->old_name
        );
    }
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_ascii_case_equal(action->old_name, new_name) &&
        mylite_table_ddl_find_alter_table_column(model, new_name) != NULL) {
        return mylite_table_ddl_set_alter_table_duplicate_column_error(database, new_name);
    }
    if (mylite_table_ddl_alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = mylite_table_ddl_replace_alter_table_column_from_definition(
        database,
        schema_default,
        &action->column,
        model->columns[column_index].source_name,
        false,
        &model->columns[column_index]
    );
    if (status != MYLITE_OK) {
        return status;
    }
    model->report_copied_rows = true;

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (mylite_ascii_case_equal(
                    model->indexes[index].parts[part].column_name,
                    action->old_name
                )) {
                char *part_name = mylite_copy_nonempty_cstring(new_name);

                if (part_name == NULL) {
                    (void)mylite_diagnostics_set_error_message(database, "out of memory");
                    return MYLITE_NOMEM;
                }
                free(model->indexes[index].parts[part].column_name);
                model->indexes[index].parts[part].column_name = part_name;
                model->indexes[index].changed = true;
            }
        }
    }
    return apply_alter_table_column_position(database, action, model, column_index);
}

static int apply_alter_table_modify_column(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model
) {
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->column.name);
    int status = MYLITE_OK;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(
            database,
            model->table_name,
            action->column.name
        );
    }
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }
    if (mylite_table_ddl_alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = mylite_table_ddl_replace_alter_table_column_from_definition(
        database,
        schema_default,
        &action->column,
        model->columns[column_index].source_name,
        false,
        &model->columns[column_index]
    );
    if (status != MYLITE_OK) {
        return status;
    }
    return apply_alter_table_column_position(database, action, model, column_index);
}

static int apply_alter_table_column_position(
    mylite_db *database,
    const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model,
    size_t column_index
) {
    struct mylite_alter_table_column moved = {0};
    size_t target_index = 0U;

    if (action->position == MYLITE_ALTER_TABLE_COLUMN_POSITION_NONE) {
        return MYLITE_OK;
    }
    if (column_index >= model->column_count) {
        return MYLITE_MISUSE;
    }
    if (action->position == MYLITE_ALTER_TABLE_COLUMN_POSITION_AFTER) {
        if (mylite_ascii_case_equal(model->columns[column_index].name, action->after_column)) {
            return MYLITE_OK;
        }
        target_index = mylite_table_ddl_alter_table_column_index(model, action->after_column);
        if (target_index == model->column_count) {
            return mylite_table_ddl_set_alter_table_unknown_column_error(
                database,
                model->table_name,
                action->after_column
            );
        }
    }

    moved = model->columns[column_index];
    for (size_t next = column_index + 1U; next < model->column_count; ++next) {
        model->columns[next - 1U] = model->columns[next];
    }
    --model->column_count;

    if (action->position == MYLITE_ALTER_TABLE_COLUMN_POSITION_FIRST) {
        target_index = 0U;
    } else {
        target_index = mylite_table_ddl_alter_table_column_index(model, action->after_column);
        if (target_index == model->column_count) {
            model->columns[model->column_count++] = moved;
            return mylite_table_ddl_set_alter_table_unknown_column_error(
                database,
                model->table_name,
                action->after_column
            );
        }
        ++target_index;
    }
    for (size_t index = model->column_count; index > target_index; --index) {
        model->columns[index] = model->columns[index - 1U];
    }
    model->columns[target_index] = moved;
    ++model->column_count;
    return MYLITE_OK;
}

static int remove_alter_table_column(struct mylite_alter_table_model *model, size_t column_index) {
    if (column_index >= model->column_count) {
        return MYLITE_MISUSE;
    }

    mylite_table_ddl_alter_table_column_deinit(&model->columns[column_index]);
    for (size_t index = column_index + 1U; index < model->column_count; ++index) {
        model->columns[index - 1U] = model->columns[index];
    }
    --model->column_count;
    model->columns[model->column_count] = (struct mylite_alter_table_column){0};
    return MYLITE_OK;
}
