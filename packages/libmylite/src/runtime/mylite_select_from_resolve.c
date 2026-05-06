#include "mylite_select_from_resolve.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_select_catalog.h"

#include <string.h>

static int resolve_select_table_targets(mylite_db *database, struct mylite_select_plan *plan);

static int validate_select_table_aliases(
    mylite_db *database,
    const struct mylite_select_plan *plan
);

static int load_select_plan_columns(mylite_db *database, struct mylite_select_plan *plan);

int mylite_select_from_resolve_tables(mylite_db *database, struct mylite_select_plan *plan) {
    int status = resolve_select_table_targets(database, plan);

    if (status == MYLITE_OK) {
        status = load_select_plan_columns(database, plan);
    }
    return status;
}

static int resolve_select_table_targets(mylite_db *database, struct mylite_select_plan *plan) {
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t index = 0U; index < table_count; ++index) {
        struct mylite_select_table *table = mylite_select_plan_table(plan, index);
        int status = mylite_select_resolve_query_table_target(database, table);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return validate_select_table_aliases(database, plan);
}

static int validate_select_table_aliases(
    mylite_db *database,
    const struct mylite_select_plan *plan
) {
    size_t table_count = mylite_select_plan_table_count(plan);

    for (size_t left = 0U; left < table_count; ++left) {
        const struct mylite_select_table *left_table = mylite_select_plan_table_const(plan, left);
        const char *left_name =
            left_table->alias == NULL ? left_table->table_name : left_table->alias;

        for (size_t right = left + 1U; right < table_count; ++right) {
            const struct mylite_select_table *right_table =
                mylite_select_plan_table_const(plan, right);
            const char *right_name =
                right_table->alias == NULL ? right_table->table_name : right_table->alias;

            if (strcmp(left_name, right_name) == 0) {
                int status = mylite_diagnostics_set_error_message_parts(
                    database,
                    "Not unique table/alias: '",
                    left_name,
                    "'"
                );

                if (status == MYLITE_OK) {
                    status = mylite_diagnostics_append_error(
                        database,
                        MYLITE_MYSQL_ER_NONUNIQ_TABLE,
                        mylite_error_message(database)
                    );
                }
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }
    return MYLITE_OK;
}

static int load_select_plan_columns(mylite_db *database, struct mylite_select_plan *plan) {
    size_t table_count = mylite_select_plan_table_count(plan);

    plan->column_count = 0U;
    for (size_t index = 0U; index < table_count; ++index) {
        struct mylite_select_table *table = mylite_select_plan_table(plan, index);
        int status = MYLITE_OK;

        table->first_column_index = plan->column_count;
        status = mylite_select_load_table_columns(database, table);
        if (status != MYLITE_OK) {
            return status;
        }
        plan->column_count += table->column_count;
    }
    return MYLITE_OK;
}
