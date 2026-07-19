#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_row_scalar_sql.h"
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

#define MYLITE_EXECUTION_ROW_SCALAR_SQL_IMPLEMENTATION
#define MYLITE_EXECUTION_ROW_SCALAR_SQL_ONLY
#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_declarations_90_sql_builder.inc"

static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index);
static int append_size_literal(struct mylite_dynamic_string *string, size_t value);
static int append_string_key_collation_sql(struct mylite_dynamic_string *string);
static int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index);
static int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
);
static int append_row_scalar_regexp_string_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);
static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind);
static bool json_mutation_kind_is_merge(enum planned_json_mutation_kind mutation_kind);
static bool json_mutation_kind_preserves_first_null(enum planned_json_mutation_kind mutation_kind);
static bool row_scalar_control_flow_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);
static const char *row_scalar_digest_sql_function_name(
    enum planned_row_scalar_digest_kind digest_kind
);
static bool window_function_is_json_aggregate(enum planned_window_function_kind kind);

#include "mylite_execution_row_scalar_sql_core.inc"
#include "mylite_execution_row_scalar_sql_functions.inc"
#include "mylite_execution_row_scalar_sql_json_control.inc"

static int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index) {
    char parameter[integer_text_capacity];
    int written = snprintf(parameter, sizeof(parameter), "?%zu", parameter_index);

    if (written < 0 || (size_t)written >= sizeof(parameter)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append(string, parameter);
}

static int append_size_literal(struct mylite_dynamic_string *string, size_t value) {
    char text[integer_text_capacity];
    int written = snprintf(text, sizeof(text), "%zu", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append(string, text);
}

static int append_string_key_collation_sql(struct mylite_dynamic_string *string) {
    enum mylite_collation_kind kind = MYLITE_COLLATION_UTF8MB4_0900_AI_CI;
    const char *sqlite_name = NULL;
    int rc = MYLITE_OK;

    if (mylite_collation_kind_from_name(string_key_collation_name, &kind) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    sqlite_name = mylite_collation_sqlite_name(kind);
    if (sqlite_name == NULL) {
        return MYLITE_OK;
    }
    rc = mylite_dynamic_string_append(string, " COLLATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, sqlite_name);
    }
    return rc;
}

static int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index) {
    char alias[select_source_alias_capacity];
    int written = snprintf(alias, sizeof(alias), "_mylite_s%zu", source_index);

    if (written < 0 || (size_t)written >= sizeof(alias)) {
        return MYLITE_NOMEM;
    }
    return mylite_dynamic_string_append_quoted_identifier(string, alias);
}

static int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
) {
    return mylite_dynamic_string_append(
        string,
        direction == PLANNED_SELECT_ORDER_DESC ? " DESC" : " ASC"
    );
}

static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    default:
        return "=";
    }
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

static bool row_scalar_control_flow_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_LESS ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER ||
           operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
}

static const char *row_scalar_digest_sql_function_name(
    enum planned_row_scalar_digest_kind digest_kind
) {
    switch (digest_kind) {
    case PLANNED_ROW_SCALAR_DIGEST_MD5:
        return "_mylite_md5";
    case PLANNED_ROW_SCALAR_DIGEST_SHA:
        return "_mylite_sha";
    case PLANNED_ROW_SCALAR_DIGEST_SHA1:
        return "_mylite_sha1";
    case PLANNED_ROW_SCALAR_DIGEST_SHA2:
        return "_mylite_sha2";
    case PLANNED_ROW_SCALAR_DIGEST_NONE:
        return NULL;
    }
    return NULL;
}

static bool window_function_is_json_aggregate(enum planned_window_function_kind kind) {
    return kind == PLANNED_WINDOW_FUNCTION_JSON_ARRAYAGG ||
           kind == PLANNED_WINDOW_FUNCTION_JSON_OBJECTAGG;
}
