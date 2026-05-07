#include "mylite_select_metadata.h"

#include "mylite_diagnostics.h"
#include "mylite_metadata.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdlib.h>

static int copy_select_result_column_metadata(
    mylite_db *database,
    struct mylite_result_column_metadata *metadata,
    const struct mylite_select_plan *plan,
    size_t output_index,
    const struct mylite_select_metadata_callbacks *callbacks
);

static int copy_select_base_column_metadata(
    mylite_db *database,
    struct mylite_result_column_metadata *metadata,
    const struct mylite_select_plan *plan,
    size_t column_index
);

static bool expression_is_unary_positive_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_reference
);

int mylite_select_attach_result_metadata(
    mylite_stmt *stmt,
    const struct mylite_select_plan *plan,
    const struct mylite_select_metadata_callbacks *callbacks
) {
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
        int status = copy_select_result_column_metadata(
            stmt->database,
            &metadata.columns[index],
            plan,
            index,
            callbacks
        );

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
static int copy_select_result_column_metadata(
    mylite_db *database,
    struct mylite_result_column_metadata *metadata,
    const struct mylite_select_plan *plan,
    size_t output_index,
    const struct mylite_select_metadata_callbacks *callbacks
) {
    const struct mylite_select_output_column *output = &plan->outputs[output_index];
    int status = mylite_result_metadata_copy_text(database, &metadata->name, output->label);

    if (status != MYLITE_OK) {
        return status;
    }
    if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
        const struct mylite_sql_ast_node *positive_reference = NULL;

        if (expression_is_unary_positive_column_reference(
                output->expression,
                &positive_reference
            )) {
            size_t column_index = mylite_select_plan_column_count(plan);

            status = mylite_select_resolve_plan_column_reference(
                database,
                plan,
                positive_reference,
                "field list",
                &column_index
            );
            if (status != MYLITE_OK) {
                return status;
            }
            if (column_index < mylite_select_plan_column_count(plan)) {
                return copy_select_base_column_metadata(database, metadata, plan, column_index);
            }
        }
        if (status == MYLITE_OK) {
            status = callbacks->infer_expression_descriptor(
                database,
                plan,
                output->expression,
                &metadata->descriptor
            );
        }
        return status;
    }

    return copy_select_base_column_metadata(database, metadata, plan, output->column_index);
}

static int copy_select_base_column_metadata(
    mylite_db *database,
    struct mylite_result_column_metadata *metadata,
    const struct mylite_select_plan *plan,
    size_t column_index
) {
    const struct mylite_select_table *table = NULL;
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, column_index, &table);
    const char *visible_table_name = NULL;
    int status = MYLITE_OK;

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
        status = mylite_result_metadata_copy_text(
            database,
            &metadata->origin_schema_name,
            table->schema_name
        );
    }
    if (status == MYLITE_OK) {
        status = mylite_result_metadata_copy_text(
            database,
            &metadata->origin_table_name,
            table->table_name
        );
    }
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->origin_column_name, column->name);
    }
    return status;
}

static bool expression_is_unary_positive_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_reference
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (out_reference == NULL) {
        return false;
    }
    *out_reference = NULL;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNARY_EXPRESSION ||
        expression->operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return false;
    }
    child = mylite_ast_child_at(expression, 0U);
    if (child == NULL || (child->kind != MYLITE_SQL_AST_IDENTIFIER &&
                          child->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }

    *out_reference = child;
    return true;
}
