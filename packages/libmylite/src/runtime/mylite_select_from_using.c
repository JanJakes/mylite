#include "mylite_select_from_using.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_select.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int add_select_using_request_name(mylite_db *database,
                                         struct mylite_select_join_using_request *request,
                                         const struct mylite_sql_ast_node *column);
static bool select_using_column_name_seen(const struct mylite_select_join_using_request *request,
                                          const char *name);
static int resolve_select_using_request(mylite_db *database, struct mylite_select_plan *plan,
                                        const struct mylite_select_join_using_request *request);
static int resolve_select_using_request_name(mylite_db *database, struct mylite_select_plan *plan,
                                             const struct mylite_select_join_using_request *request,
                                             const char *name);
static int resolve_select_using_request_column(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const char *name,
                                               struct mylite_select_table_range range,
                                               size_t *out_column);
static int set_select_unknown_from_column_error(mylite_db *database, const char *name);
static int add_select_using_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *name, size_t left_column_index,
                                   size_t right_column_index, size_t coalesced_column_index,
                                   size_t first_table, size_t table_count);

int mylite_select_from_add_using_request(mylite_db *database, struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *condition,
                                         size_t left_first_table, size_t left_table_count,
                                         size_t right_first_table, size_t right_table_count,
                                         enum mylite_sql_ast_join_type join_type)
{
    const struct mylite_sql_ast_node *columns = mylite_ast_child_at(condition, 0U);
    struct mylite_select_join_using_request request = {
        .left_first_table = left_first_table,
        .left_table_count = left_table_count,
        .right_first_table = right_first_table,
        .right_table_count = right_table_count,
        .join_type = join_type,
    };
    struct mylite_select_join_using_request *requests = NULL;

    if (columns == NULL || columns->kind != MYLITE_SQL_AST_USING_COLUMN_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = columns->first_child; item != NULL;
         item = item->next_sibling) {
        int status =
            add_select_using_request_name(database, &request, mylite_ast_child_at(item, 0U));

        if (status != MYLITE_OK) {
            for (size_t index = 0U; index < request.name_count; ++index) {
                free(request.names[index]);
            }
            free((void *)request.names);
            return status;
        }
    }

    requests = realloc(plan->using_requests,
                       (plan->using_request_count + 1U) * sizeof(*plan->using_requests));
    if (requests == NULL) {
        for (size_t index = 0U; index < request.name_count; ++index) {
            free(request.names[index]);
        }
        free((void *)request.names);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->using_requests = requests;
    plan->using_requests[plan->using_request_count++] = request;
    return MYLITE_OK;
}

int mylite_select_from_resolve_using_requests(mylite_db *database, struct mylite_select_plan *plan)
{
    for (size_t request_index = 0U; request_index < plan->using_request_count; ++request_index) {
        int status =
            resolve_select_using_request(database, plan, &plan->using_requests[request_index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_select_using_request_name(mylite_db *database,
                                         struct mylite_select_join_using_request *request,
                                         const struct mylite_sql_ast_node *column)
{
    char *name = mylite_copy_identifier_span(column);
    char **names = NULL;

    if (name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (select_using_column_name_seen(request, name)) {
        free(name);
        return MYLITE_OK;
    }

    names = (char **)realloc((void *)request->names,
                             (request->name_count + 1U) * sizeof(*request->names));
    if (names == NULL) {
        free(name);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    request->names = names;
    request->names[request->name_count++] = name;
    return MYLITE_OK;
}

static bool select_using_column_name_seen(const struct mylite_select_join_using_request *request,
                                          const char *name)
{
    for (size_t index = 0U; index < request->name_count; ++index) {
        if (mylite_ascii_case_equal(request->names[index], name)) {
            return true;
        }
    }
    return false;
}

static int resolve_select_using_request(mylite_db *database, struct mylite_select_plan *plan,
                                        const struct mylite_select_join_using_request *request)
{
    for (size_t name_index = 0U; name_index < request->name_count; ++name_index) {
        int status =
            resolve_select_using_request_name(database, plan, request, request->names[name_index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_select_using_request_name(mylite_db *database, struct mylite_select_plan *plan,
                                             const struct mylite_select_join_using_request *request,
                                             const char *name)
{
    struct mylite_select_table_range left_range = {
        .first_table = request->left_first_table,
        .table_count = request->left_table_count,
    };
    struct mylite_select_table_range right_range = {
        .first_table = request->right_first_table,
        .table_count = request->right_table_count,
    };
    size_t left_column = mylite_select_plan_column_count(plan);
    size_t right_column = mylite_select_plan_column_count(plan);
    size_t coalesced_column = 0U;
    int status =
        resolve_select_using_request_column(database, plan, name, left_range, &left_column);

    if (status == MYLITE_OK) {
        status =
            resolve_select_using_request_column(database, plan, name, right_range, &right_column);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    coalesced_column = request->join_type == MYLITE_SQL_AST_JOIN_RIGHT ? right_column : left_column;
    return add_select_using_column(database, plan, name, left_column, right_column,
                                   coalesced_column, request->left_first_table,
                                   request->left_table_count + request->right_table_count);
}

static int resolve_select_using_request_column(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const char *name,
                                               struct mylite_select_table_range range,
                                               size_t *out_column)
{
    size_t match_count =
        mylite_select_count_column_parts_using_matches(plan, name, range, out_column);

    for (size_t index = 0U; index < range.table_count; ++index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(plan, range.first_table + index);
        size_t column_index = mylite_select_plan_column_count(plan);

        if (mylite_select_resolve_column_in_table(plan, table, name, &column_index) != MYLITE_OK ||
            mylite_select_column_index_is_using_column_in_range(plan, column_index, range)) {
            continue;
        }
        *out_column = column_index;
        ++match_count;
    }
    if (match_count == 1U) {
        return MYLITE_OK;
    }
    if (match_count > 1U) {
        return mylite_select_set_ambiguous_column_error(database, name, "from clause");
    }
    return set_select_unknown_from_column_error(database, name);
}

static int set_select_unknown_from_column_error(mylite_db *database, const char *name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '", name,
                                                            "' in 'from clause'");

    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                                 mylite_error_message(database));
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int add_select_using_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *name, size_t left_column_index,
                                   size_t right_column_index, size_t coalesced_column_index,
                                   size_t first_table, size_t table_count)
{
    struct mylite_select_join_using_column *columns = NULL;
    char *name_copy = mylite_copy_span_text(name, strlen(name));

    if (name_copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    columns = realloc(plan->using_columns,
                      (plan->using_column_count + 1U) * sizeof(*plan->using_columns));
    if (columns == NULL) {
        free(name_copy);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->using_columns = columns;
    plan->using_columns[plan->using_column_count++] = (struct mylite_select_join_using_column){
        .name = name_copy,
        .left_column_index = left_column_index,
        .right_column_index = right_column_index,
        .coalesced_column_index = coalesced_column_index,
        .first_table = first_table,
        .table_count = table_count,
    };
    return MYLITE_OK;
}
