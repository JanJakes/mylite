#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_select_types.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdlib.h>

static int evaluate_update_row_matches(mylite_db *database, const struct mylite_update_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_row *row,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       bool *out_matches);
static int evaluate_update_order_values(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        const struct mylite_dml_expression_callbacks *callbacks,
                                        struct mylite_update_row *row);
static int evaluate_update_order_key(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     const struct mylite_dml_expression_callbacks *callbacks,
                                     struct mylite_expression_value *out_value);
static int evaluate_delete_row_matches(mylite_db *database, const struct mylite_delete_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_row *row,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       bool *out_matches);
static int evaluate_delete_order_values(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        const struct mylite_dml_expression_callbacks *callbacks,
                                        struct mylite_update_row *row);
static int evaluate_delete_order_key(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     const struct mylite_dml_expression_callbacks *callbacks,
                                     struct mylite_expression_value *out_value);
static int set_where_predicate_eval_error(const struct mylite_dml_expression_callbacks *callbacks);

int mylite_dml_materialize_update_rows(mylite_db *database, const struct mylite_update_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_order_plan *order_plan,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       struct mylite_update_rowset *rowset)
{
    sqlite3_stmt *scan = NULL;
    char *scan_sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL || order_plan == NULL ||
        callbacks == NULL || rowset == NULL) {
        return MYLITE_MISUSE;
    }

    scan_sql = mylite_dml_build_update_scan_sql(database, table);
    if (scan_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT, &scan, NULL);
    sqlite3_free(scan_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    while ((rc = sqlite3_step(scan)) == SQLITE_ROW) {
        struct mylite_update_row row = {0};
        bool matches = false;

        status = mylite_dml_copy_update_sqlite_row(database, table, scan, &row);
        if (status == MYLITE_OK) {
            status = evaluate_update_row_matches(database, plan, table, &row, callbacks, &matches);
        }
        if (status == MYLITE_OK && matches) {
            status = evaluate_update_order_values(database, table, order_plan, callbacks, &row);
        }
        if (status == MYLITE_OK && matches) {
            status = mylite_dml_append_update_row(database, rowset, &row);
        }
        mylite_dml_update_row_deinit(&row);
        if (status != MYLITE_OK) {
            sqlite3_finalize(scan);
            return status;
        }
    }
    sqlite3_finalize(scan);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int evaluate_update_row_matches(mylite_db *database, const struct mylite_update_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_row *row,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       bool *out_matches)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = mylite_dml_resolve_update_expression_identifier,
        .eval_session_function = mylite_dml_evaluate_session_function,
    };
    struct mylite_expression_value value = {0};
    size_t warning_start = database->warnings.count;
    int truth = -1;
    int status = 0;

    *out_matches = true;
    if (plan->where_clause == NULL) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval_with_context(mylite_ast_child_at(plan->where_clause, 0U),
                                                 &context, &database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        int condition_status = mylite_dml_set_expression_condition_error(database, warning_start);

        return condition_status == MYLITE_OK ? set_where_predicate_eval_error(callbacks)
                                             : condition_status;
    }
    status = mylite_dml_promote_expression_warnings(database, warning_start);
    if (status != MYLITE_OK) {
        return status;
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

static int evaluate_update_order_values(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        const struct mylite_dml_expression_callbacks *callbacks,
                                        struct mylite_update_row *row)
{
    if (order_plan->order_key_count == 0U) {
        return MYLITE_OK;
    }

    row->order_values = calloc(order_plan->order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_plan->order_key_count;

    for (size_t index = 0U; index < order_plan->order_key_count; ++index) {
        int status = evaluate_update_order_key(database, table, row, &order_plan->order_keys[index],
                                               callbacks, &row->order_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_update_order_key(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     const struct mylite_dml_expression_callbacks *callbacks,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = mylite_dml_resolve_update_expression_identifier,
        .eval_session_function = mylite_dml_evaluate_session_function,
    };
    size_t warning_start = database->warnings.count;
    int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                     &database->warnings, out_value);

    if (status != 0) {
        int condition_status = mylite_dml_set_expression_condition_error(database, warning_start);

        return condition_status == MYLITE_OK
                   ? mylite_dml_set_update_unsupported_clause_error(database)
                   : condition_status;
    }
    return mylite_dml_promote_expression_warnings(database, warning_start);
}

int mylite_dml_materialize_delete_rows(mylite_db *database, const struct mylite_delete_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_order_plan *order_plan,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       struct mylite_update_rowset *rowset)
{
    sqlite3_stmt *scan = NULL;
    char *scan_sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || table == NULL || order_plan == NULL ||
        callbacks == NULL || rowset == NULL) {
        return MYLITE_MISUSE;
    }

    scan_sql = mylite_dml_build_update_scan_sql(database, table);
    if (scan_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT, &scan, NULL);
    sqlite3_free(scan_sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    while ((rc = sqlite3_step(scan)) == SQLITE_ROW) {
        struct mylite_update_row row = {0};
        bool matches = false;

        status = mylite_dml_copy_update_sqlite_row(database, table, scan, &row);
        if (status == MYLITE_OK) {
            status = evaluate_delete_row_matches(database, plan, table, &row, callbacks, &matches);
        }
        if (status == MYLITE_OK && matches) {
            status = evaluate_delete_order_values(database, table, order_plan, callbacks, &row);
        }
        if (status == MYLITE_OK && matches) {
            status = mylite_dml_append_update_row(database, rowset, &row);
        }
        mylite_dml_update_row_deinit(&row);
        if (status != MYLITE_OK) {
            sqlite3_finalize(scan);
            return status;
        }
    }
    sqlite3_finalize(scan);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int evaluate_delete_row_matches(mylite_db *database, const struct mylite_delete_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_row *row,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       bool *out_matches)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = mylite_dml_resolve_update_expression_identifier,
        .eval_session_function = mylite_dml_evaluate_session_function,
    };
    struct mylite_expression_value value = {0};
    size_t warning_start = database->warnings.count;
    int truth = -1;
    int status = 0;

    *out_matches = true;
    if (plan->where_clause == NULL) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval_with_context(mylite_ast_child_at(plan->where_clause, 0U),
                                                 &context, &database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        int condition_status = mylite_dml_set_expression_condition_error(database, warning_start);

        return condition_status == MYLITE_OK ? set_where_predicate_eval_error(callbacks)
                                             : condition_status;
    }
    status = mylite_dml_promote_expression_warnings(database, warning_start);
    if (status != MYLITE_OK) {
        return status;
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

static int evaluate_delete_order_values(mylite_db *database,
                                        const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        const struct mylite_dml_expression_callbacks *callbacks,
                                        struct mylite_update_row *row)
{
    if (order_plan->order_key_count == 0U) {
        return MYLITE_OK;
    }

    row->order_values = calloc(order_plan->order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_plan->order_key_count;

    for (size_t index = 0U; index < order_plan->order_key_count; ++index) {
        int status = evaluate_delete_order_key(database, table, row, &order_plan->order_keys[index],
                                               callbacks, &row->order_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_delete_order_key(mylite_db *database, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     const struct mylite_dml_expression_callbacks *callbacks,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
        .callbacks = callbacks,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = mylite_dml_resolve_update_expression_identifier,
        .eval_session_function = mylite_dml_evaluate_session_function,
    };
    size_t warning_start = database->warnings.count;
    int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                     &database->warnings, out_value);

    if (status != 0) {
        int condition_status = mylite_dml_set_expression_condition_error(database, warning_start);

        return condition_status == MYLITE_OK
                   ? mylite_dml_set_delete_unsupported_clause_error(database)
                   : condition_status;
    }
    return mylite_dml_promote_expression_warnings(database, warning_start);
}

static int set_where_predicate_eval_error(const struct mylite_dml_expression_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->set_where_predicate_eval_error == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    return callbacks->set_where_predicate_eval_error(callbacks->user_data);
}
