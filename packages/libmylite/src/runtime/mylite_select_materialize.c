#include "mylite_select_materialize.h"

#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_materialize.h"
#include "mylite_select_join_materialize.h"
#include "mylite_select_table_materialize.h"

int mylite_select_materialize_table_result(mylite_stmt *stmt,
                                           const struct mylite_select_eval_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (mylite_select_plan_table_count(&stmt->select_plan) > 1U) {
        status = mylite_select_materialize_joined_table_result(stmt, callbacks);
    } else if (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
               stmt->select_plan.has_having) {
        status = mylite_select_materialize_aggregate_table_result(stmt, callbacks);
    } else {
        status = mylite_select_materialize_single_table_result(stmt, callbacks);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}
