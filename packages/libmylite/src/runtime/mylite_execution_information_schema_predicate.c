#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_information_schema_predicate.h"
#include "mylite_execution_information_schema_predicate_support.h"
#include "mylite_execution_information_schema_values.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_show_filter.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_text_internal.h"
#include "mylite_numeric_locale.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"

static int information_schema_evaluate_predicate_instruction(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    struct information_schema_truth_stack *truth_stack
);
static void information_schema_truth_stack_deinit(struct information_schema_truth_stack *stack);
static int information_schema_truth_stack_push(
    struct mylite_db *database,
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value value
);
static bool information_schema_truth_stack_pop(
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value *out_value
);
static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_predicate_row_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int information_schema_compound_predicate_matches(
    struct mylite_db *database,
    enum information_schema_predicate_instruction_kind predicate_kind,
    struct information_schema_truth_stack *truth_stack,
    enum information_schema_truth_value *out_truth
);
static enum information_schema_truth_value information_schema_truth_from_bool(bool condition);
static bool information_schema_truth_is_true(enum information_schema_truth_value truth);
static int information_schema_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_like_comparison_truth(
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum information_schema_truth_value *out_truth
);
static int information_schema_null_safe_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_regular_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_nonnull_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
);
static int information_schema_like_comparison_matches(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    char escape_character,
    bool *out_matches
);
static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_instruction *instruction,
    const char *const *row,
    enum information_schema_truth_value *out_truth
);
static int information_schema_compare_nonnull_value_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
);
static int information_schema_between_bound_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
);
static int information_schema_numeric_comparison_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool *out_matches
);
static int information_schema_text_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool right_is_numeric,
    bool *out_matches
);

#include "mylite_execution_information_schema_predicate_comparison.inc"
#include "mylite_execution_information_schema_predicate_evaluation.inc"
