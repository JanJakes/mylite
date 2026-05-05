#include "mylite_select_materialize.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_join_materialize.h"
#include "mylite_select_materialize_common.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_row_match.h"
#include "mylite_select_rowset.h"
#include "mylite_select_table_materialize.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int
materialize_aggregate_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks);
static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count,
                                              const struct mylite_select_eval_callbacks *callbacks);
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
        status = materialize_aggregate_table_select_result(stmt, callbacks);
    } else {
        status = mylite_select_materialize_single_table_result(stmt, callbacks);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}

static int
materialize_aggregate_table_select_result(mylite_stmt *stmt,
                                          const struct mylite_select_eval_callbacks *callbacks)
{
    struct mylite_table_select_group *groups = NULL;
    size_t group_count = 0U;
    int status = scan_aggregate_table_select_groups(stmt, &groups, &group_count, callbacks);

    if (status == MYLITE_OK && group_count == 0U && !stmt->select_plan.has_group_by) {
        status = mylite_select_group_append_empty_implicit(stmt, &groups, &group_count);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_select_materialize_append_finalized_groups(stmt, groups, group_count, callbacks);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    mylite_select_groups_deinit(groups, group_count);
    return status;
}

static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count,
                                              const struct mylite_select_eval_callbacks *callbacks)
{
    int status = mylite_select_eval_constant_predicate(stmt, callbacks);
    int rc = SQLITE_OK;

    *groups = NULL;
    *group_count = 0U;
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_group *group = NULL;
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = mylite_select_row_matches(stmt, &row, &matches, callbacks);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }

        status = mylite_select_group_find(stmt, *groups, *group_count, &row, callbacks, &group);
        if (status == MYLITE_OK && group == NULL) {
            status = mylite_select_group_append(stmt, groups, group_count, &row, callbacks, &group);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_group_update(stmt, group, &row, callbacks);
        }
        mylite_select_row_deinit(&row);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    return status;
}
