#include "mylite_table_ddl_alter_column_definition.h"

#include "mylite_diagnostics.h"
#include "mylite_schema_types.h"
#include "mylite_span.h"
#include "mylite_table_ddl.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int alter_table_column_descriptor(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition,
    struct mylite_column_type_descriptor *out_descriptor
);

int mylite_table_ddl_replace_alter_table_column_from_definition(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition,
    const char *source_name,
    bool added,
    struct mylite_alter_table_column *target
) {
    char *source_copy = source_name == NULL ? NULL : mylite_copy_nonempty_cstring(source_name);
    struct mylite_alter_table_column replacement = {0};
    int status = MYLITE_OK;

    if (source_name != NULL && source_copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_table_ddl_init_alter_table_column_from_definition(
        database,
        schema_default,
        definition,
        source_copy,
        added,
        &replacement
    );
    free(source_copy);
    if (status != MYLITE_OK) {
        return status;
    }

    mylite_table_ddl_alter_table_column_deinit(target);
    *target = replacement;
    return MYLITE_OK;
}

int mylite_table_ddl_init_alter_table_column_from_definition(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition,
    const char *source_name,
    bool added,
    struct mylite_alter_table_column *out_column
) {
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
    out_column->column_comment = mylite_copy_span_text(
        definition->comment == NULL ? "" : definition->comment,
        definition->comment == NULL ? 0U : strlen(definition->comment)
    );
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

bool mylite_table_ddl_alter_table_column_definition_has_deferred_features(
    const struct mylite_create_table_column *column
) {
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

static int alter_table_column_descriptor(
    mylite_db *database,
    const struct mylite_schema_default *schema_default,
    const struct mylite_create_table_column *definition,
    struct mylite_column_type_descriptor *out_descriptor
) {
    struct mylite_create_table_options options = {0};

    if (definition == NULL || schema_default == NULL) {
        return MYLITE_MISUSE;
    }
    if (schema_default->character_set == NULL || schema_default->collation == NULL) {
        (void)mylite_diagnostics_set_error_message(
            database,
            "Unsupported charset/collation registry entry"
        );
        return MYLITE_EXEC_ERROR;
    }
    options.character_set = (char *)schema_default->character_set;
    options.collation = (char *)schema_default->collation;
    return mylite_table_ddl_describe_create_table_column(
        definition,
        schema_default,
        &options,
        out_descriptor
    );
}
