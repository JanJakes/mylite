#include "mylite_select_materialize_common.h"

#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_eval.h"
#include "mylite_select_group.h"
#include "mylite_select_rowset.h"
#include "mylite_select_rowset_distinct.h"

static int append_finalized_table_select_group(
    mylite_stmt *stmt,
    struct mylite_table_select_group *group,
    const struct mylite_select_eval_callbacks *callbacks
);

int mylite_select_materialize_append_finalized_groups(
    mylite_stmt *stmt,
    struct mylite_table_select_group *groups,
    size_t group_count,
    const struct mylite_select_eval_callbacks *callbacks
) {
    for (size_t index = 0U; index < group_count; ++index) {
        int status = append_finalized_table_select_group(stmt, &groups[index], callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_materialize_check_distinct_duplicate(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    bool *out_duplicate,
    const struct mylite_select_eval_callbacks *callbacks
) {
    int status = mylite_select_eval_materialize_output_values(stmt, row, callbacks);

    *out_duplicate = false;
    if (status != MYLITE_OK) {
        return status;
    }
    *out_duplicate = mylite_select_result_distinct_row_exists(
        &stmt->select_result,
        &stmt->select_plan,
        &stmt->result_metadata,
        row
    );
    return MYLITE_OK;
}

static int append_finalized_table_select_group(
    mylite_stmt *stmt,
    struct mylite_table_select_group *group,
    const struct mylite_select_eval_callbacks *callbacks
) {
    struct mylite_table_select_row row = {0};
    bool having_matches = true;
    bool duplicate = false;
    int status = mylite_select_group_finalize(stmt, group, &row);

    if (status == MYLITE_OK) {
        status = mylite_select_eval_having(stmt, &row, callbacks, &having_matches);
    }
    if (status == MYLITE_OK && having_matches &&
        mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode)) {
        status =
            mylite_select_materialize_check_distinct_duplicate(stmt, &row, &duplicate, callbacks);
    }
    if (status == MYLITE_OK && duplicate) {
        mylite_select_row_deinit(&row);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && having_matches && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_eval_order_values(stmt, &row, callbacks);
    }
    if (status == MYLITE_OK && having_matches) {
        status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}
