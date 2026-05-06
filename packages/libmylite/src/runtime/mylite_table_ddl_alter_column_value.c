#include "mylite_table_ddl_alter_column_value.h"

#include "mylite_dml.h"
#include "mylite_dml_insert_default.h"
#include "mylite_span.h"
#include "mylite_table_ddl_alter.h"

int mylite_table_ddl_resolve_alter_table_added_column_value(
    mylite_db *database,
    const struct mylite_alter_table_column *column,
    struct mylite_insert_bound_value *out_value
) {
    struct mylite_insert_table_column insert_column = {0};

    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    insert_column = (struct mylite_insert_table_column){
        .name = column->name,
        .default_text = column->column_default,
        .data_type = column->data_type,
        .extra = column->extra,
        .nullable = column->nullable,
        .auto_increment = column->auto_increment,
        .generated_default = mylite_text_contains_word(column->extra, "DEFAULT_GENERATED"),
    };

    if (column->auto_increment) {
        return mylite_table_ddl_set_alter_table_wrong_auto_increment_error(database);
    }
    if (column->column_default != NULL) {
        return mylite_dml_resolve_insert_default_bound_value(
            database,
            &insert_column,
            0U,
            NULL,
            out_value
        );
    }
    if (column->nullable) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    return mylite_dml_resolve_insert_implicit_expression_default(
        database,
        &insert_column,
        out_value
    );
}
