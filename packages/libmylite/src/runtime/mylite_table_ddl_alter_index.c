#include "mylite_table_ddl_alter_index.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"
#include "mylite_table_ddl_alter_index_model.h"
#include "mylite_table_ddl_alter_model.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

    status = mylite_table_ddl_validate_alter_table_added_index(database, model, &action->index,
                                                               index_name, is_primary);
    if (status == MYLITE_OK && is_primary) {
        status = mylite_table_ddl_validate_alter_table_primary_key_values(callbacks, model,
                                                                          &action->index);
    }
    if (status == MYLITE_OK && is_primary) {
        status = mylite_table_ddl_apply_alter_table_primary_key_column_nullability(database, model,
                                                                                   &action->index);
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
        status = mylite_table_ddl_insert_alter_table_index(model, table_index, position);
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
    size_t primary_index = mylite_table_ddl_alter_table_index_index(model, "PRIMARY");

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
    return mylite_table_ddl_remove_alter_table_index(model, primary_index);
}

static int apply_alter_table_drop_index(mylite_db *database,
                                        const struct mylite_alter_table_action *action,
                                        struct mylite_alter_table_model *model)
{
    size_t index = mylite_table_ddl_alter_table_index_index(model, action->old_name);

    if (index == model->index_count || mylite_ascii_case_equal(action->old_name, "PRIMARY")) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    return mylite_table_ddl_remove_alter_table_index(model, index);
}

static int apply_alter_table_rename_index(mylite_db *database,
                                          const struct mylite_alter_table_action *action,
                                          struct mylite_alter_table_model *model)
{
    size_t index = mylite_table_ddl_alter_table_index_index(model, action->old_name);
    char *new_name = NULL;

    if (index == model->index_count) {
        return mylite_table_ddl_set_alter_table_cant_drop_column_error(database, action->old_name);
    }
    if (mylite_ascii_case_equal(action->old_name, "PRIMARY") ||
        mylite_ascii_case_equal(action->new_name, "PRIMARY")) {
        return mylite_table_ddl_set_alter_table_primary_invisible_error(database);
    }
    if (!mylite_ascii_case_equal(action->old_name, action->new_name) &&
        mylite_table_ddl_alter_table_index_name_exists(model, action->new_name)) {
        return mylite_table_ddl_set_alter_table_duplicate_key_name_error(database,
                                                                         action->new_name);
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
    size_t index = mylite_table_ddl_alter_table_index_index(model, action->old_name);
    char *visibility = NULL;
    bool has_primary =
        mylite_table_ddl_alter_table_index_index(model, "PRIMARY") < model->index_count;
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
            return mylite_table_ddl_set_alter_table_primary_invisible_error(database);
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
        if (!mylite_table_ddl_alter_table_index_name_exists(model, candidate)) {
            *out_name = candidate;
            return MYLITE_OK;
        }
        free(candidate);
        ++suffix;
    }
}
