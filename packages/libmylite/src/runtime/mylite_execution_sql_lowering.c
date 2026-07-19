#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_row_scalar_sql.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_sql_lowering_support.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_text_internal.h"
#include "mylite_execution_value.h"
#include "mylite_json.h"
#include "mylite_lexer.h"
#include "mylite_parser.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
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

static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column);
static int append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
);

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"

enum quantified_subquery_exists_kind {
    QUANTIFIED_SUBQUERY_EXISTS_MATCH = 0,
    QUANTIFIED_SUBQUERY_EXISTS_UNKNOWN = 1,
    QUANTIFIED_SUBQUERY_EXISTS_FALSE = 2,
};

#include "mylite_execution_sql_lowering_declarations.inc"

static bool column_aggregate_function_is_count(enum planned_column_aggregate_function function);
static bool grouped_aggregate_function_is_bitwise(enum planned_grouped_aggregate_function function);
static bool planned_select_predicate_has_expression(const struct planned_select_predicate *predicate
);
static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind);
static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind);
static bool row_scalar_numeric_extra_expression_needs_real_sql_context(
    const struct planned_row_scalar_expression *expression
);
static bool planned_update_has_multiple_assignments(const struct planned_update *plan);
static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment);
static bool column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
);
static int append_string_key_collation_sql(struct mylite_dynamic_string *string);
static int append_mysql_quoted_text(struct mylite_dynamic_string *string, const char *text);
static const char *union_modifier_sql(enum mylite_sql_ast_union_modifier modifier);
static const char *column_aggregate_sql_function(enum planned_column_aggregate_function function);
static const char *grouped_aggregate_sql_function(enum planned_grouped_aggregate_function function);
static const char *scalar_aggregate_sql_function(enum planned_column_aggregate_function function);
const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind);

#include "mylite_execution_aggregate_predicate_sql_builders.inc"
#include "mylite_execution_dml_sql_builders.inc"
#include "mylite_execution_insert_sql_builders.inc"
#include "mylite_execution_select_sql_builders.inc"

static bool column_aggregate_function_is_count(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_COUNT_STAR ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN) != 0;
}

static bool grouped_aggregate_function_is_bitwise(enum planned_grouped_aggregate_function function
) {
    return function == PLANNED_GROUPED_AGGREGATE_BIT_AND ||
           function == PLANNED_GROUPED_AGGREGATE_BIT_OR ||
           function == PLANNED_GROUPED_AGGREGATE_BIT_XOR;
}

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

static bool row_scalar_numeric_extra_expression_needs_real_sql_context(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA) {
        return false;
    }

    return expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE ||
           expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI;
}

static bool planned_update_has_multiple_assignments(const struct planned_update *plan) {
    return plan != NULL && plan->assignment_count > 1U;
}

static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment) {
    return assignment != NULL && assignment->generated_default_noop;
}

static int append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
) {
    return mylite_execution_append_json_table_source_sql(
        string,
        source,
        source_index,
        next_parameter
    );
}

static bool column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
) {
    return mylite_execution_column_descriptor_uses_string_key_collation(
        column,
        include_text_family
    );
}

static int append_string_key_collation_sql(struct mylite_dynamic_string *string) {
    return mylite_execution_append_string_key_collation_sql(string);
}

static int append_mysql_quoted_text(struct mylite_dynamic_string *string, const char *text) {
    return mylite_execution_append_mysql_quoted_text(string, text);
}

static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column) {
    return mylite_execution_column_descriptor_is_time(column);
}
