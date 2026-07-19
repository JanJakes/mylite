#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_parameter_binding.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar_numeric.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_text_internal.h"
#include "mylite_execution_value.h"
#include "mylite_json.h"
#include "mylite_lexer.h"
#include "mylite_parser.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_padding.h"
#include "mylite_string_search.h"
#include "mylite_string_substring_index.h"
#include "mylite_sys_functions.h"
#include "sqlite3.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MYLITE_EXECUTION_PARAMETER_BINDING_IMPLEMENTATION
#define MYLITE_EXECUTION_PARAMETER_BINDING_ONLY
#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_declarations_99_sqlite_binding.inc"

static int bind_select_exists_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_exists_subquery *subquery,
    int *parameter_index
);
static int bind_select_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_non_in_predicate_node_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_predicate_node_parameters_without_subqueries(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_row_scalar_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_literal_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_subquery_in_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_comparison_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_scalar_aggregate_subquery_between_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    int *parameter_index
);
static int bind_scalar_aggregate_subquery_or_value_parameters(
    sqlite3_stmt *statement,
    const struct planned_scalar_aggregate_subquery *subquery,
    const struct planned_value *value,
    int *parameter_index
);
static int bind_select_in_value_list_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_select_in_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_in_subquery *subquery,
    int *parameter_index
);
static int bind_select_quantified_subquery_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate_node *node,
    int *parameter_index
);
static int bind_row_scalar_regexp_string_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
static bool planned_select_predicate_has_expression(const struct planned_select_predicate *predicate
);
static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind);
static bool planned_update_has_multiple_assignments(const struct planned_update *plan);
static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment);
static bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
);
static bool json_mutation_kind_is_merge(enum planned_json_mutation_kind mutation_kind);
static bool json_mutation_kind_preserves_first_null(enum planned_json_mutation_kind mutation_kind);
static int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
);
static int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
);
static int bind_json_table_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_json_table_source *source,
    int *parameter_index
);

#include "mylite_execution_predicate_dml_parameter_binding.inc"
#include "mylite_execution_row_scalar_arithmetic_parameter_binding.inc"
#include "mylite_execution_row_scalar_control_flow_parameter_binding.inc"
#include "mylite_execution_row_scalar_conversion_parameter_binding.inc"
#include "mylite_execution_row_scalar_encoding_uuid_char_parameter_binding.inc"
#include "mylite_execution_row_scalar_expression_parameter_dispatch.inc"
#include "mylite_execution_row_scalar_json_parameter_binding.inc"
#include "mylite_execution_row_scalar_select_parameter_binding.inc"
#include "mylite_execution_row_scalar_string_regexp_parameter_binding.inc"
#include "mylite_execution_row_scalar_temporal_string_parameter_binding.inc"
#include "mylite_execution_row_scalar_window_parameter_binding.inc"

static bool planned_select_predicate_has_expression(const struct planned_select_predicate *predicate
) {
    return predicate != NULL && predicate->has_root;
}

static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE_BINARY;
}

static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE;
}

static bool planned_update_has_multiple_assignments(const struct planned_update *plan) {
    return plan != NULL && plan->assignment_count > 1U;
}

static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment) {
    return assignment != NULL && assignment->generated_default_noop;
}

static bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
) {
    if (plan == NULL || column == NULL || !column->on_update_current_timestamp) {
        return false;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            if (planned_update_assignment_is_noop(&plan->assignments[index])) {
                continue;
            }
            if (plan->assignments[index].column.column_id == column->column_id) {
                return false;
            }
        }
        return true;
    }
    return column->column_id != plan->assignment_column.column_id;
}

static bool json_mutation_kind_is_merge(enum planned_json_mutation_kind mutation_kind) {
    return mutation_kind == PLANNED_JSON_MUTATION_MERGE ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PATCH ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PRESERVE;
}

static bool json_mutation_kind_preserves_first_null(enum planned_json_mutation_kind mutation_kind) {
    return mutation_kind == PLANNED_JSON_MUTATION_MERGE ||
           mutation_kind == PLANNED_JSON_MUTATION_MERGE_PRESERVE;
}

static int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
) {
    return append_predicate_sql_work_item(
        items,
        item_count,
        (struct predicate_sql_work_item){
            .kind = PREDICATE_SQL_WORK_NODE,
            .node_index = node_index,
        }
    );
}

static int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
) {
    struct predicate_sql_work_item *grown_items = NULL;
    const size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        return MYLITE_NOMEM;
    }
    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        return MYLITE_NOMEM;
    }
    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;
    return MYLITE_OK;
}

static int bind_json_table_source_parameters(
    sqlite3_stmt *statement,
    const struct planned_json_table_source *source,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (source == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < source->row_count; ++row_index) {
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < source->column_count;
             ++column_index) {
            const size_t value_index = (row_index * source->column_count) + column_index;

            rc = bind_planned_value_parameter(
                statement,
                *parameter_index,
                &source->values[value_index]
            );
            if (rc == MYLITE_OK) {
                ++(*parameter_index);
            }
        }
    }
    return rc;
}
