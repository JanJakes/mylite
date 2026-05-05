#include "mylite_table_ddl_alter.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter_model.h"
#include "sqlite3.h"

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
static int apply_alter_table_add_index(mylite_db *database,
                                       const struct mylite_alter_table_action *action,
                                       struct mylite_alter_table_model *model,
                                       const struct mylite_table_ddl_alter_callbacks *callbacks,
                                       bool is_primary);
static int apply_alter_table_drop_primary_key(mylite_db *database,
                                              struct mylite_alter_table_model *model);
static int apply_alter_table_drop_index(mylite_db *database,
                                        const struct mylite_alter_table_action *action,
                                        struct mylite_alter_table_model *model);
static int apply_alter_table_rename_index(mylite_db *database,
                                          const struct mylite_alter_table_action *action,
                                          struct mylite_alter_table_model *model);
static int apply_alter_table_alter_index_visibility(mylite_db *database,
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
static int insert_alter_table_index(struct mylite_alter_table_model *model,
                                    struct mylite_alter_table_index table_index, size_t position);
static int remove_alter_table_index(struct mylite_alter_table_model *model, size_t index);
static int init_alter_table_index_from_create_index(mylite_db *database,
                                                    struct mylite_alter_table_model *model,
                                                    const struct mylite_create_table_index *source,
                                                    bool is_primary,
                                                    struct mylite_alter_table_index *out_index);
static int
init_alter_table_index_part_from_key_part(mylite_db *database,
                                          const struct mylite_alter_table_model *model,
                                          const struct mylite_create_table_key_part *source,
                                          struct mylite_alter_table_index_part *out_part);
static int assign_alter_table_generated_index_name(mylite_db *database,
                                                   const struct mylite_alter_table_model *model,
                                                   const struct mylite_create_table_index *source,
                                                   char **out_name);
static bool alter_table_index_name_exists(const struct mylite_alter_table_model *model,
                                          const char *name);
static int validate_alter_table_added_index(mylite_db *database,
                                            const struct mylite_alter_table_model *model,
                                            const struct mylite_create_table_index *index,
                                            const char *index_name, bool is_primary);
static int
validate_alter_table_primary_key_values(const struct mylite_table_ddl_alter_callbacks *callbacks,
                                        const struct mylite_alter_table_model *model,
                                        const struct mylite_create_table_index *index);
static int
apply_alter_table_primary_key_column_nullability(mylite_db *database,
                                                 struct mylite_alter_table_model *model,
                                                 const struct mylite_create_table_index *index);
static int set_alter_table_column_nullable(mylite_db *database,
                                           struct mylite_alter_table_column *column, bool nullable);
static const char *alter_table_column_key(const struct mylite_alter_table_model *model,
                                          const char *column_name);
static int refresh_alter_table_column_keys(struct mylite_alter_table_model *model);
static int refresh_alter_table_index_nullability(struct mylite_alter_table_model *model);
static size_t alter_table_index_index(const struct mylite_alter_table_model *model,
                                      const char *name);
static bool alter_table_column_definition_has_deferred_features(
    const struct mylite_create_table_column *column);
static int set_alter_table_multiple_primary_key_error(mylite_db *database);
static int set_alter_table_duplicate_key_name_error(mylite_db *database, const char *index_name);
static int set_alter_table_missing_key_column_error(mylite_db *database, const char *column_name);
static int set_alter_table_primary_invisible_error(mylite_db *database);

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

int mylite_table_ddl_apply_alter_table_index_action(
    mylite_db *database, const struct mylite_alter_table_action *action,
    struct mylite_alter_table_model *model,
    const struct mylite_table_ddl_alter_callbacks *callbacks)
{
    if (database == NULL || action == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }

    switch (action->kind) {
    case MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY:
        return apply_alter_table_add_index(database, action, model, callbacks, true);
    case MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY:
        return apply_alter_table_drop_primary_key(database, model);
    case MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX:
        return apply_alter_table_add_index(database, action, model, callbacks, false);
    case MYLITE_ALTER_TABLE_ACTION_DROP_INDEX:
        return apply_alter_table_drop_index(database, action, model);
    case MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX:
        return apply_alter_table_rename_index(database, action, model);
    case MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY:
        return apply_alter_table_alter_index_visibility(database, action, model);
    case MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN:
    case MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE:
    case MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK:
    case MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY:
        return MYLITE_MISUSE;
    }
    return MYLITE_MISUSE;
}

int mylite_table_ddl_refresh_alter_table_index_metadata(mylite_db *database,
                                                        struct mylite_alter_table_model *model)
{
    int status = MYLITE_OK;

    if (database == NULL || model == NULL) {
        return MYLITE_MISUSE;
    }
    status = refresh_alter_table_column_keys(model);
    if (status == MYLITE_OK) {
        status = refresh_alter_table_index_nullability(model);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
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

static int apply_alter_table_add_index(mylite_db *database,
                                       const struct mylite_alter_table_action *action,
                                       struct mylite_alter_table_model *model,
                                       const struct mylite_table_ddl_alter_callbacks *callbacks,
                                       bool is_primary)
{
    struct mylite_alter_table_index table_index = {0};
    char *index_name = NULL;
    size_t position = 0U;
    int status = MYLITE_OK;

    if (is_primary) {
        index_name = mylite_copy_nonempty_cstring("PRIMARY");
    } else if (action->index.name == NULL) {
        status =
            assign_alter_table_generated_index_name(database, model, &action->index, &index_name);
    } else {
        index_name = mylite_copy_nonempty_cstring(action->index.name);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    if (index_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status =
        validate_alter_table_added_index(database, model, &action->index, index_name, is_primary);
    if (status == MYLITE_OK && is_primary) {
        status = validate_alter_table_primary_key_values(callbacks, model, &action->index);
    }
    if (status == MYLITE_OK && is_primary) {
        status = apply_alter_table_primary_key_column_nullability(database, model, &action->index);
    }
    if (status == MYLITE_OK) {
        status = init_alter_table_index_from_create_index(database, model, &action->index,
                                                          is_primary, &table_index);
    }
    if (status == MYLITE_OK) {
        free(table_index.name);
        table_index.name = index_name;
        index_name = NULL;
        table_index.hash_fallback_warning =
            action->index.algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH;
        if (is_primary) {
            position = 0U;
        } else {
            position = model->index_count;
        }
        status = insert_alter_table_index(model, table_index, position);
    }
    if (status != MYLITE_OK) {
        mylite_table_ddl_alter_table_index_deinit(&table_index);
    }
    free(index_name);
    return status;
}

static int apply_alter_table_drop_primary_key(mylite_db *database,
                                              struct mylite_alter_table_model *model)
{
    size_t primary_index = alter_table_index_index(model, "PRIMARY");

    if (primary_index == model->index_count) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, "PRIMARY");
    }
    for (size_t part = 0U; part < model->indexes[primary_index].part_count; ++part) {
        const struct mylite_alter_table_column *column = mylite_table_ddl_find_alter_table_column(
            model, model->indexes[primary_index].parts[part].column_name);

        if (column != NULL && column->auto_increment) {
            return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
        }
    }
    return remove_alter_table_index(model, primary_index);
}

static int apply_alter_table_drop_index(mylite_db *database,
                                        const struct mylite_alter_table_action *action,
                                        struct mylite_alter_table_model *model)
{
    size_t index = alter_table_index_index(model, action->old_name);

    if (index == model->index_count || mylite_ascii_case_equal(action->old_name, "PRIMARY")) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    return remove_alter_table_index(model, index);
}

static int apply_alter_table_rename_index(mylite_db *database,
                                          const struct mylite_alter_table_action *action,
                                          struct mylite_alter_table_model *model)
{
    size_t index = alter_table_index_index(model, action->old_name);
    char *new_name = NULL;

    if (index == model->index_count) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    if (mylite_ascii_case_equal(action->old_name, "PRIMARY") ||
        mylite_ascii_case_equal(action->new_name, "PRIMARY")) {
        return set_alter_table_primary_invisible_error(database);
    }
    if (!mylite_ascii_case_equal(action->old_name, action->new_name) &&
        alter_table_index_name_exists(model, action->new_name)) {
        return set_alter_table_duplicate_key_name_error(database, action->new_name);
    }

    new_name = mylite_copy_nonempty_cstring(action->new_name);
    if (new_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(model->indexes[index].name);
    model->indexes[index].name = new_name;
    return MYLITE_OK;
}

static int apply_alter_table_alter_index_visibility(mylite_db *database,
                                                    const struct mylite_alter_table_action *action,
                                                    struct mylite_alter_table_model *model)
{
    size_t index = alter_table_index_index(model, action->old_name);
    char *visibility = NULL;
    bool has_primary = alter_table_index_index(model, "PRIMARY") < model->index_count;
    bool is_implicit_primary = false;

    if (index == model->index_count) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    if (!action->index_visible) {
        if (!has_primary && model->indexes[index].non_unique == 0) {
            is_implicit_primary = true;
        }
        for (size_t part = 0U; is_implicit_primary && part < model->indexes[index].part_count;
             ++part) {
            const struct mylite_alter_table_column *column =
                mylite_table_ddl_find_alter_table_column(
                    model, model->indexes[index].parts[part].column_name);

            if (column == NULL || column->nullable) {
                is_implicit_primary = false;
            }
        }
        if (mylite_ascii_case_equal(model->indexes[index].name, "PRIMARY") || is_implicit_primary) {
            return set_alter_table_primary_invisible_error(database);
        }
    }

    const char *visibility_text = "NO";

    if (action->index_visible) {
        visibility_text = "YES";
    }

    visibility = mylite_copy_span_text(visibility_text, strlen(visibility_text));
    if (visibility == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(model->indexes[index].is_visible);
    model->indexes[index].is_visible = visibility;
    return MYLITE_OK;
}

static int insert_alter_table_index(struct mylite_alter_table_model *model,
                                    struct mylite_alter_table_index table_index, size_t position)
{
    struct mylite_alter_table_index *indexes = NULL;

    if (position > model->index_count) {
        return MYLITE_MISUSE;
    }

    indexes = realloc(model->indexes, (model->index_count + 1U) * sizeof(*model->indexes));
    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }
    model->indexes = indexes;
    for (size_t index = model->index_count; index > position; --index) {
        model->indexes[index] = model->indexes[index - 1U];
    }
    model->indexes[position] = table_index;
    ++model->index_count;
    return MYLITE_OK;
}

static int remove_alter_table_index(struct mylite_alter_table_model *model, size_t index)
{
    if (index >= model->index_count) {
        return MYLITE_MISUSE;
    }

    mylite_table_ddl_alter_table_index_deinit(&model->indexes[index]);
    for (size_t next = index + 1U; next < model->index_count; ++next) {
        model->indexes[next - 1U] = model->indexes[next];
    }
    --model->index_count;
    model->indexes[model->index_count] = (struct mylite_alter_table_index){0};
    return MYLITE_OK;
}

static int init_alter_table_index_from_create_index(mylite_db *database,
                                                    struct mylite_alter_table_model *model,
                                                    const struct mylite_create_table_index *source,
                                                    bool is_primary,
                                                    struct mylite_alter_table_index *out_index)
{
    const char *index_type = "BTREE";
    const char *comment = "";
    const char *index_comment = source->comment == NULL ? "" : source->comment;
    const char *is_visible = "NO";
    int status = MYLITE_OK;

    if (source->is_visible) {
        is_visible = "YES";
    }

    *out_index = (struct mylite_alter_table_index){0};
    out_index->index_schema = mylite_copy_nonempty_cstring(model->schema_name);
    out_index->index_type = mylite_copy_span_text(index_type, strlen(index_type));
    out_index->comment = mylite_copy_span_text(comment, strlen(comment));
    out_index->index_comment = mylite_copy_span_text(index_comment, strlen(index_comment));
    out_index->is_visible = mylite_copy_span_text(is_visible, strlen(is_visible));
    out_index->non_unique = 1;
    if (source->is_unique || is_primary) {
        out_index->non_unique = 0;
    }
    out_index->changed = true;
    if (out_index->index_schema == NULL || out_index->index_type == NULL ||
        out_index->comment == NULL || out_index->index_comment == NULL ||
        out_index->is_visible == NULL) {
        mylite_table_ddl_alter_table_index_deinit(out_index);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t part = 0U; part < source->part_count; ++part) {
        struct mylite_alter_table_index_part table_part = {0};

        status = init_alter_table_index_part_from_key_part(database, model, &source->parts[part],
                                                           &table_part);
        if (status == MYLITE_OK) {
            status = mylite_table_ddl_append_alter_table_index_part(out_index, table_part);
        }
        if (status != MYLITE_OK) {
            mylite_table_ddl_alter_table_index_part_deinit(&table_part);
            mylite_table_ddl_alter_table_index_deinit(out_index);
            return status;
        }
    }
    return MYLITE_OK;
}

static int
init_alter_table_index_part_from_key_part(mylite_db *database,
                                          const struct mylite_alter_table_model *model,
                                          const struct mylite_create_table_key_part *source,
                                          struct mylite_alter_table_index_part *out_part)
{
    const struct mylite_alter_table_column *column =
        mylite_table_ddl_find_alter_table_column(model, source->column_name);
    const char *nullable = "";
    const char *collation = mylite_table_ddl_index_collation_for_order(source->order);

    if (column != NULL && column->nullable) {
        nullable = "YES";
    }

    *out_part = (struct mylite_alter_table_index_part){0};
    out_part->column_name = mylite_copy_nonempty_cstring(source->column_name);
    out_part->collation = mylite_copy_span_text(collation, strlen(collation));
    out_part->nullable = mylite_copy_span_text(nullable, strlen(nullable));
    if (out_part->column_name == NULL || out_part->collation == NULL ||
        out_part->nullable == NULL) {
        mylite_table_ddl_alter_table_index_part_deinit(out_part);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (source->has_prefix_length) {
        out_part->has_sub_part = true;
        out_part->sub_part = (int64_t)source->prefix_length;
    }
    return MYLITE_OK;
}

static int assign_alter_table_generated_index_name(mylite_db *database,
                                                   const struct mylite_alter_table_model *model,
                                                   const struct mylite_create_table_index *source,
                                                   char **out_name)
{
    const char *base = NULL;
    unsigned int suffix = 1U;

    *out_name = NULL;
    if (source->part_count == 0U || source->parts[0].column_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "Index has no key parts");
        return MYLITE_EXEC_ERROR;
    }

    base = source->parts[0].column_name;
    for (;;) {
        char *candidate = mylite_table_ddl_generated_index_name_candidate(base, suffix);

        if (candidate == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (!alter_table_index_name_exists(model, candidate)) {
            *out_name = candidate;
            return MYLITE_OK;
        }
        free(candidate);
        ++suffix;
    }
}

static bool alter_table_index_name_exists(const struct mylite_alter_table_model *model,
                                          const char *name)
{
    return alter_table_index_index(model, name) < model->index_count;
}

static int validate_alter_table_added_index(mylite_db *database,
                                            const struct mylite_alter_table_model *model,
                                            const struct mylite_create_table_index *index,
                                            const char *index_name, bool is_primary)
{
    if (index->has_with_parser) {
        (void)mylite_diagnostics_set_error_message(
            database, "WITH PARSER is only supported for FULLTEXT indexes");
        return MYLITE_EXEC_ERROR;
    }
    if (is_primary && alter_table_index_name_exists(model, "PRIMARY")) {
        return set_alter_table_multiple_primary_key_error(database);
    }
    if (!is_primary && mylite_ascii_case_equal(index_name, "PRIMARY")) {
        return set_alter_table_duplicate_key_name_error(database, index_name);
    }
    if (alter_table_index_name_exists(model, index_name)) {
        return set_alter_table_duplicate_key_name_error(database, index_name);
    }
    if (is_primary && !index->is_visible) {
        return set_alter_table_primary_invisible_error(database);
    }
    for (size_t part = 0U; part < index->part_count; ++part) {
        if (mylite_table_ddl_find_alter_table_column(model, index->parts[part].column_name) ==
            NULL) {
            return set_alter_table_missing_key_column_error(database,
                                                            index->parts[part].column_name);
        }
        for (size_t previous = 0U; previous < part; ++previous) {
            if (mylite_ascii_case_equal(index->parts[previous].column_name,
                                        index->parts[part].column_name)) {
                return mylite_table_ddl_set_alter_table_duplicate_column_error(
                    database, index->parts[part].column_name);
            }
        }
    }
    return MYLITE_OK;
}

static int
validate_alter_table_primary_key_values(const struct mylite_table_ddl_alter_callbacks *callbacks,
                                        const struct mylite_alter_table_model *model,
                                        const struct mylite_create_table_index *index)
{
    if (callbacks == NULL || callbacks->validate_primary_key_part_not_null == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t part = 0U; part < index->part_count; ++part) {
        int status = callbacks->validate_primary_key_part_not_null(callbacks->user_data, model,
                                                                   &index->parts[part]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
apply_alter_table_primary_key_column_nullability(mylite_db *database,
                                                 struct mylite_alter_table_model *model,
                                                 const struct mylite_create_table_index *index)
{
    for (size_t part = 0U; part < index->part_count; ++part) {
        struct mylite_alter_table_column *column = NULL;
        size_t column_index =
            mylite_table_ddl_alter_table_column_index(model, index->parts[part].column_name);
        int status = MYLITE_OK;

        if (column_index == model->column_count) {
            return MYLITE_MISUSE;
        }
        column = &model->columns[column_index];
        status = set_alter_table_column_nullable(database, column, false);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int set_alter_table_column_nullable(mylite_db *database,
                                           struct mylite_alter_table_column *column, bool nullable)
{
    const char *nullable_text = "NO";
    char *copy = NULL;

    if (nullable) {
        nullable_text = "YES";
    }

    copy = mylite_copy_span_text(nullable_text, strlen(nullable_text));
    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    free(column->is_nullable);
    column->is_nullable = copy;
    column->nullable = nullable;
    return MYLITE_OK;
}

static const char *alter_table_column_key(const struct mylite_alter_table_model *model,
                                          const char *column_name)
{
    bool indexed = false;
    bool unique = false;

    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            if (!mylite_ascii_case_equal(model->indexes[index].parts[part].column_name,
                                         column_name)) {
                continue;
            }
            indexed = true;
            if (mylite_ascii_case_equal(model->indexes[index].name, "PRIMARY")) {
                return "PRI";
            }
            if (model->indexes[index].non_unique == 0 && part == 0U) {
                unique = true;
            }
        }
    }
    if (unique) {
        return "UNI";
    }
    if (indexed) {
        return "MUL";
    }
    return "";
}

static int refresh_alter_table_column_keys(struct mylite_alter_table_model *model)
{
    for (size_t column = 0U; column < model->column_count; ++column) {
        const char *key = alter_table_column_key(model, model->columns[column].name);
        char *copy = mylite_copy_span_text(key, strlen(key));

        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        free(model->columns[column].column_key);
        model->columns[column].column_key = copy;
    }
    return MYLITE_OK;
}

static int refresh_alter_table_index_nullability(struct mylite_alter_table_model *model)
{
    for (size_t index = 0U; index < model->index_count; ++index) {
        for (size_t part = 0U; part < model->indexes[index].part_count; ++part) {
            struct mylite_alter_table_index_part *index_part = &model->indexes[index].parts[part];
            const struct mylite_alter_table_column *column =
                mylite_table_ddl_find_alter_table_column(model, index_part->column_name);
            const char *nullable = "";
            char *copy = NULL;

            if (column != NULL && column->nullable) {
                nullable = "YES";
            }

            copy = mylite_copy_span_text(nullable, strlen(nullable));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(index_part->nullable);
            index_part->nullable = copy;
        }
    }
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

static size_t alter_table_index_index(const struct mylite_alter_table_model *model,
                                      const char *name)
{
    for (size_t index = 0U; index < model->index_count; ++index) {
        if (mylite_ascii_case_equal(model->indexes[index].name, name)) {
            return index;
        }
    }
    return model->index_count;
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

int mylite_table_ddl_set_alter_table_duplicate_column_error(mylite_db *database,
                                                            const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Duplicate column name '",
                                                            column_name, "'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_DUP_FIELDNAME,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_unknown_column_error(mylite_db *database,
                                                          const char *table_name,
                                                          const char *column_name)
{
    char *message = sqlite3_mprintf("Unknown column '%q' in '%q'", column_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_cant_drop_column_error(mylite_db *database,
                                                            const char *column_name)
{
    char *message = sqlite3_mprintf("Can't DROP '%q'; check that column/key exists", column_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    sqlite3_free(message);
    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_CANT_DROP_FIELD_OR_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_cant_remove_all_columns_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "You can't delete all columns with ALTER TABLE; use DROP TABLE");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_CANT_REMOVE_ALL_FIELDS,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_table_ddl_set_alter_table_wrong_auto_increment_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database,
        "Incorrect table definition; there can be only one auto column and it must be defined "
        "as a key");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_AUTO_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_multiple_primary_key_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(database, "Multiple primary key defined");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_MULTIPLE_PRI_KEY,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_duplicate_key_name_error(mylite_db *database, const char *index_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Duplicate key name '",
                                                            index_name, "'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_DUP_KEYNAME,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_missing_key_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Key column '", column_name,
                                                            "' doesn't exist in table");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_KEY_COLUMN_DOES_NOT_EXITS,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_alter_table_primary_invisible_error(mylite_db *database)
{
    int status =
        mylite_diagnostics_set_error_message(database, "A primary key index cannot be invisible");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_PK_INDEX_CANT_BE_INVISIBLE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}
