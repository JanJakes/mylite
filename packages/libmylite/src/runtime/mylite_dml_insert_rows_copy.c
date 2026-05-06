#include "mylite_dml_insert_rows_copy.h"

#include "mylite_dml.h"
#include "mylite_dml_insert_copy_value.h"

#include <stdlib.h>

static int copy_insert_row(
    const struct mylite_sql_ast_node *row,
    struct mylite_insert_values_plan *plan
);

static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row);

int mylite_dml_copy_insert_rows(
    const struct mylite_sql_ast_node *rows,
    struct mylite_insert_values_plan *plan
) {
    if (rows == NULL || rows->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *row = rows->first_child; row != NULL;
         row = row->next_sibling) {
        int status = copy_insert_row(row, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->row_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_row(
    const struct mylite_sql_ast_node *row,
    struct mylite_insert_values_plan *plan
) {
    struct mylite_insert_row insert_row = {0};
    int status = MYLITE_OK;

    if (row == NULL || row->kind != MYLITE_SQL_AST_INSERT_ROW) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *value = row->first_child; value != NULL;
         value = value->next_sibling) {
        struct mylite_insert_value *values = NULL;
        struct mylite_insert_value insert_value = {0};

        status = mylite_dml_copy_insert_value(value, &insert_value);
        if (status != MYLITE_OK) {
            mylite_dml_insert_value_deinit(&insert_value);
            mylite_dml_insert_row_deinit(&insert_row);
            return status;
        }

        values =
            realloc(insert_row.values, (insert_row.value_count + 1U) * sizeof(*insert_row.values));
        if (values == NULL) {
            mylite_dml_insert_value_deinit(&insert_value);
            mylite_dml_insert_row_deinit(&insert_row);
            return MYLITE_NOMEM;
        }
        insert_row.values = values;
        insert_row.values[insert_row.value_count++] = insert_value;
    }

    status = add_insert_row(plan, insert_row);
    if (status != MYLITE_OK) {
        mylite_dml_insert_row_deinit(&insert_row);
    }
    return status;
}

static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row) {
    struct mylite_insert_row *rows =
        realloc(plan->rows, (plan->row_count + 1U) * sizeof(*plan->rows));

    if (rows == NULL) {
        return MYLITE_NOMEM;
    }

    plan->rows = rows;
    plan->rows[plan->row_count++] = row;
    return MYLITE_OK;
}
