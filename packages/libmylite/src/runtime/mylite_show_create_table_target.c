#include "mylite_show_create_table_target.h"

#include "mylite_diagnostics.h"
#include "mylite_show.h"
#include "mylite_show_types.h"
#include "mylite_span.h"

#include <stdlib.h>

int mylite_show_create_table_copy_target(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         struct mylite_show_create_table_target *out_target)
{
    struct mylite_show_columns_target target = {0};
    int status = MYLITE_OK;

    *out_target = (struct mylite_show_create_table_target){0};
    status = mylite_show_copy_columns_table_target(
        &(const struct mylite_show_columns_source_nodes){
            .table_name = mylite_ast_child_at(statement, 0U),
            .explicit_schema = NULL,
        },
        &target);
    if (status != MYLITE_OK) {
        if (status == MYLITE_UNSUPPORTED) {
            (void)mylite_diagnostics_set_error_message(
                database, "SHOW CREATE TABLE names with more than two parts are not supported");
        }
        return status;
    }
    if (target.schema_name == NULL) {
        status = mylite_show_copy_columns_selected_schema(database, &target);
    }
    if (status == MYLITE_OK) {
        out_target->schema_name = target.schema_name;
        out_target->table_name = target.table_name;
        target = (struct mylite_show_columns_target){0};
    }

    mylite_show_columns_target_deinit(&target);
    return status;
}

int mylite_show_create_table_validate_target(mylite_db *database,
                                             const struct mylite_show_create_table_target *target)
{
    struct mylite_show_columns_target columns_target = {
        .schema_name = target->schema_name,
        .table_name = target->table_name,
    };

    return mylite_show_validate_columns_target(
        database, &columns_target,
        "SHOW CREATE TABLE for information_schema tables is not supported");
}

void mylite_show_create_table_target_deinit(struct mylite_show_create_table_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_show_create_table_target){0};
}
