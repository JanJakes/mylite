#include "mylite_select_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_materialize.h"
#include "mylite_select_join_materialize.h"
#include "mylite_select_table_materialize.h"

#include <stddef.h>
#include <stdint.h>

static int record_table_select_found_rows(mylite_stmt *stmt);
static uint64_t found_rows_count_after_limit(uint64_t pre_limit_count,
                                             const struct mylite_select_limit *limit,
                                             size_t returned_count);
static int append_table_select_calc_found_rows_warning(mylite_stmt *stmt);

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
        status = record_table_select_found_rows(stmt);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}

static int record_table_select_found_rows(mylite_stmt *stmt)
{
    uint64_t found_rows;

    if (stmt->select_plan.calc_found_rows) {
        found_rows = stmt->found_rows;
    } else {
        found_rows = found_rows_count_after_limit(stmt->found_rows, &stmt->select_plan.limit,
                                                  stmt->select_result.row_count);
    }

    stmt->database->previous_found_rows = found_rows;
    stmt->previous_found_rows_recorded = true;
    if (stmt->select_plan.calc_found_rows) {
        return append_table_select_calc_found_rows_warning(stmt);
    }
    return MYLITE_OK;
}

static uint64_t found_rows_count_after_limit(uint64_t pre_limit_count,
                                             const struct mylite_select_limit *limit,
                                             size_t returned_count)
{
    uint64_t returned = (uint64_t)returned_count;
    uint64_t limited_count;

    if (limit == NULL || !limit->has_limit) {
        return pre_limit_count;
    }
    if (limit->offset > UINT64_MAX - returned) {
        limited_count = UINT64_MAX;
    } else {
        limited_count = limit->offset + returned;
    }
    return limited_count < pre_limit_count ? limited_count : pre_limit_count;
}

static int append_table_select_calc_found_rows_warning(mylite_stmt *stmt)
{
    return mylite_diagnostics_append_warning(
        stmt->database, MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
        "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. "
        "Consider using two separate queries instead.");
}
