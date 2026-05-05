#include "mylite_select_metadata.h"

#include "mylite_diagnostics.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"

#include <stdlib.h>

static int
copy_select_result_column_metadata(mylite_db *database,
                                   struct mylite_result_column_metadata *metadata,
                                   const struct mylite_select_plan *plan, size_t output_index,
                                   const struct mylite_select_metadata_callbacks *callbacks);

int mylite_select_attach_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan,
                                         const struct mylite_select_metadata_callbacks *callbacks)
{
    struct mylite_result_metadata metadata = {0};

    if (stmt == NULL || stmt->database == NULL || plan == NULL || callbacks == NULL ||
        callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    metadata.columns = calloc(plan->output_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = plan->output_count;
    for (size_t index = 0U; index < metadata.column_count; ++index) {
        int status = copy_select_result_column_metadata(stmt->database, &metadata.columns[index],
                                                        plan, index, callbacks);

        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            }
            return status;
        }
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int
copy_select_result_column_metadata(mylite_db *database,
                                   struct mylite_result_column_metadata *metadata,
                                   const struct mylite_select_plan *plan, size_t output_index,
                                   const struct mylite_select_metadata_callbacks *callbacks)
{
    const struct mylite_select_output_column *output = &plan->outputs[output_index];
    int status = mylite_result_metadata_copy_text(database, &metadata->name, output->label);

    if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
        if (status == MYLITE_OK) {
            status = callbacks->infer_expression_descriptor(database, plan, output->expression,
                                                            &metadata->descriptor);
        }
        return status;
    }

    const struct mylite_select_table *table = NULL;
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, output->column_index, &table);
    const char *visible_table_name = NULL;

    if (column == NULL || table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    visible_table_name = table->alias == NULL ? table->table_name : table->alias;
    metadata->descriptor = column->descriptor;
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->schema_name, table->schema_name);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->table_name, visible_table_name);
    }
    if (status == MYLITE_OK) {
        status = mylite_result_metadata_copy_text(database, &metadata->origin_schema_name,
                                                  table->schema_name);
    }
    if (status == MYLITE_OK) {
        status = mylite_result_metadata_copy_text(database, &metadata->origin_table_name,
                                                  table->table_name);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->origin_column_name, column->name);
    }
    return status;
}
