#include "mylite_table_ddl_alter.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter_model.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int apply_alter_table_add_column(mylite_db *database,
                                        const struct mylite_schema_default *schema_default,
                                        const struct mylite_alter_table_action *action,
                                        struct mylite_alter_table_model *model);
static int apply_alter_table_drop_column(mylite_db *database,
                                         const struct mylite_alter_table_action *action,
                                         struct mylite_alter_table_model *model);
static int apply_alter_table_rename_column(mylite_db *database,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model);
static int apply_alter_table_change_column(mylite_db *database,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model);
static int apply_alter_table_modify_column(mylite_db *database,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model);
static int apply_alter_table_column_position(mylite_db *database,
                                             const struct mylite_alter_table_action *action,
                                             struct mylite_alter_table_model *model,
                                             size_t column_index);
static int replace_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *target);
static int init_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *out_column);
static int alter_table_column_descriptor(mylite_db *database,
                                         const struct mylite_schema_default *schema_default,
                                         const struct mylite_create_table_column *definition,
                                         struct mylite_column_type_descriptor *out_descriptor);
static int remove_alter_table_column(struct mylite_alter_table_model *model, size_t column_index);
static bool alter_table_column_definition_has_deferred_features(
    const struct mylite_create_table_column *column);

int mylite_table_ddl_apply_alter_table_column_action(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_alter_table_action *action, struct mylite_alter_table_model *model)
{
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

static int apply_alter_table_add_column(mylite_db *database,
                                        const struct mylite_schema_default *schema_default,
                                        const struct mylite_alter_table_action *action,
                                        struct mylite_alter_table_model *model)
{
    struct mylite_alter_table_column column = {0};
    int status = MYLITE_OK;

    if (mylite_table_ddl_find_alter_table_column(model, action->column.name) != NULL) {
        return mylite_table_ddl_set_alter_table_duplicate_column_error(database,
                                                                       action->column.name);
    }
    if (alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = init_alter_table_column_from_definition(database, schema_default, &action->column,
                                                     NULL, true, &column);
    if (status == MYLITE_OK) {
        status = mylite_table_ddl_add_alter_table_column(model, column);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_column_deinit(&column);
        return status;
    }
    return apply_alter_table_column_position(database, action, model, model->column_count - 1U);
}

static int apply_alter_table_drop_column(mylite_db *database,
                                         const struct mylite_alter_table_action *action,
                                         struct mylite_alter_table_model *model)
{
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

static int apply_alter_table_rename_column(mylite_db *database,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model)
{
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->old_name);
    char *new_name = NULL;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(database, model->table_name,
                                                                     action->old_name);
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
            if (mylite_ascii_case_equal(model->indexes[index].parts[part].column_name,
                                        action->old_name)) {
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

static int apply_alter_table_change_column(mylite_db *database,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model)
{
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->old_name);
    const char *new_name = action->column.name;
    int status = MYLITE_OK;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(database, model->table_name,
                                                                     action->old_name);
    }
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_ascii_case_equal(action->old_name, new_name) &&
        mylite_table_ddl_find_alter_table_column(model, new_name) != NULL) {
        return mylite_table_ddl_set_alter_table_duplicate_column_error(database, new_name);
    }
    if (alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = replace_alter_table_column_from_definition(database, schema_default, &action->column,
                                                        model->columns[column_index].source_name,
                                                        false, &model->columns[column_index]);
    if (status != MYLITE_OK) {
        return status;
    }
    model->report_copied_rows = true;

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (mylite_ascii_case_equal(model->indexes[index].parts[part].column_name,
                                        action->old_name)) {
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

static int apply_alter_table_modify_column(mylite_db *database,
                                           const struct mylite_schema_default *schema_default,
                                           const struct mylite_alter_table_action *action,
                                           struct mylite_alter_table_model *model)
{
    size_t column_index = mylite_table_ddl_alter_table_column_index(model, action->column.name);
    int status = MYLITE_OK;

    if (column_index == model->column_count) {
        return mylite_table_ddl_set_alter_table_unknown_column_error(database, model->table_name,
                                                                     action->column.name);
    }
    if (model->columns == NULL) {
        return MYLITE_MISUSE;
    }
    if (alter_table_column_definition_has_deferred_features(&action->column)) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }

    status = replace_alter_table_column_from_definition(database, schema_default, &action->column,
                                                        model->columns[column_index].source_name,
                                                        false, &model->columns[column_index]);
    if (status != MYLITE_OK) {
        return status;
    }
    return apply_alter_table_column_position(database, action, model, column_index);
}

static int apply_alter_table_column_position(mylite_db *database,
                                             const struct mylite_alter_table_action *action,
                                             struct mylite_alter_table_model *model,
                                             size_t column_index)
{
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
                database, model->table_name, action->after_column);
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
                database, model->table_name, action->after_column);
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

static int replace_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *target)
{
    char *source_copy = source_name == NULL ? NULL : mylite_copy_nonempty_cstring(source_name);
    struct mylite_alter_table_column replacement = {0};
    int status = MYLITE_OK;

    if (source_name != NULL && source_copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = init_alter_table_column_from_definition(database, schema_default, definition,
                                                     source_copy, added, &replacement);
    free(source_copy);
    if (status != MYLITE_OK) {
        return status;
    }

    mylite_table_ddl_alter_table_column_deinit(target);
    *target = replacement;
    return MYLITE_OK;
}

static int init_alter_table_column_from_definition(
    mylite_db *database, const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition, const char *source_name, bool added,
    struct mylite_alter_table_column *out_column)
{
    struct mylite_column_type_descriptor descriptor;
    const char *extra = mylite_table_ddl_create_table_column_extra(definition);
    const char *nullable_text = "NO";
    int status = alter_table_column_descriptor(database, schema_default, definition, &descriptor);

    *out_column = (struct mylite_alter_table_column){0};
    if (status != MYLITE_OK) {
        return status;
    }
    if (definition->nullable) {
        nullable_text = "YES";
    }

    out_column->name = mylite_copy_nonempty_cstring(definition->name);
    if (source_name != NULL) {
        out_column->source_name = mylite_copy_nonempty_cstring(source_name);
    }
    if (definition->default_text != NULL) {
        out_column->column_default =
            mylite_copy_span_text(definition->default_text, strlen(definition->default_text));
    }
    out_column->is_nullable = mylite_copy_span_text(nullable_text, strlen(nullable_text));
    out_column->data_type = mylite_copy_nonempty_cstring(descriptor.data_type);
    out_column->column_type = mylite_copy_nonempty_cstring(descriptor.column_type);
    out_column->column_key = mylite_copy_span_text("", 0U);
    out_column->extra =
        mylite_copy_span_text(extra == NULL ? "" : extra, extra == NULL ? 0U : strlen(extra));
    out_column->column_comment =
        mylite_copy_span_text(definition->comment == NULL ? "" : definition->comment,
                              definition->comment == NULL ? 0U : strlen(definition->comment));
    out_column->generation_expression = mylite_copy_span_text("", 0U);
    if (out_column->name == NULL || (source_name != NULL && out_column->source_name == NULL) ||
        (definition->default_text != NULL && out_column->column_default == NULL) ||
        out_column->is_nullable == NULL || out_column->data_type == NULL ||
        out_column->column_type == NULL || out_column->column_key == NULL ||
        out_column->extra == NULL || out_column->column_comment == NULL ||
        out_column->generation_expression == NULL) {
        mylite_table_ddl_alter_table_column_deinit(out_column);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (descriptor.is_character_string || descriptor.is_binary_string) {
        out_column->character_maximum_length = (int64_t)descriptor.character_maximum_length;
        out_column->character_octet_length = (int64_t)descriptor.character_octet_length;
        out_column->has_character_maximum_length = true;
        out_column->has_character_octet_length = true;
    }
    if (descriptor.numeric_precision != 0U) {
        out_column->numeric_precision = (int64_t)descriptor.numeric_precision;
        out_column->has_numeric_precision = true;
    }
    if (descriptor.has_numeric_scale) {
        out_column->numeric_scale = (int64_t)descriptor.numeric_scale;
        out_column->has_numeric_scale = true;
    }
    if (descriptor.has_datetime_precision) {
        out_column->datetime_precision = (int64_t)descriptor.datetime_precision;
        out_column->has_datetime_precision = true;
    }
    if (descriptor.character_set_name != NULL) {
        out_column->character_set_name =
            mylite_copy_nonempty_cstring(descriptor.character_set_name);
        if (out_column->character_set_name == NULL) {
            mylite_table_ddl_alter_table_column_deinit(out_column);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (descriptor.collation_name != NULL) {
        out_column->collation_name = mylite_copy_nonempty_cstring(descriptor.collation_name);
        if (out_column->collation_name == NULL) {
            mylite_table_ddl_alter_table_column_deinit(out_column);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    out_column->nullable = definition->nullable;
    out_column->auto_increment = definition->auto_increment;
    out_column->visible = definition->visible;
    out_column->added = added;
    return MYLITE_OK;
}

static int alter_table_column_descriptor(mylite_db *database,
                                         const struct mylite_schema_default *schema_default,
                                         const struct mylite_create_table_column *definition,
                                         struct mylite_column_type_descriptor *out_descriptor)
{
    struct mylite_create_table_options options = {0};

    if (definition == NULL || schema_default == NULL) {
        return MYLITE_MISUSE;
    }
    if (schema_default->character_set == NULL || schema_default->collation == NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }
    options.character_set = (char *)schema_default->character_set;
    options.collation = (char *)schema_default->collation;
    return mylite_table_ddl_describe_create_table_column(definition, schema_default, &options,
                                                         out_descriptor);
}

static int remove_alter_table_column(struct mylite_alter_table_model *model, size_t column_index)
{
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

const struct mylite_alter_table_column *
mylite_table_ddl_find_alter_table_column(const struct mylite_alter_table_model *model,
                                         const char *name)
{
    size_t index = mylite_table_ddl_alter_table_column_index(model, name);

    return index == model->column_count ? NULL : &model->columns[index];
}

size_t mylite_table_ddl_alter_table_column_index(const struct mylite_alter_table_model *model,
                                                 const char *name)
{
    for (size_t index = 0U; index < model->column_count; ++index) {
        if (mylite_ascii_case_equal(model->columns[index].name, name)) {
            return index;
        }
    }
    return model->column_count;
}

static bool
alter_table_column_definition_has_deferred_features(const struct mylite_create_table_column *column)
{
    if (column == NULL) {
        return true;
    }
    if (column->primary_key || column->unique_key || column->auto_increment) {
        return true;
    }
    if (column->has_generated_default &&
        !mylite_column_default_is_current_timestamp(column->default_text)) {
        return true;
    }
    return false;
}
