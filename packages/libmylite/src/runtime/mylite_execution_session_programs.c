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
#include "mylite_execution_completion.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_information_schema_join_plan.h"
#include "mylite_execution_information_schema_plan.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_analysis.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_session_programs.h"
#include "mylite_execution_session_programs_support.h"
#include "mylite_execution_session_system_variables_support.h"
#include "mylite_execution_set.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_transaction_control.h"
#include "mylite_execution_value.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"
#include "sqlite3.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#define MYLITE_EXECUTION_SESSION_PROGRAM_MODULE 1
#include "mylite_execution_declarations_10_statement.inc"
#undef MYLITE_EXECUTION_SESSION_PROGRAM_MODULE

int mylite_execution_execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_prepare_statement(database, statement, out_result);
}

int mylite_execution_execute_prepared_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_execute_statement(database, statement, out_result);
}

int mylite_execution_execute_deallocate_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_deallocate_prepare_statement(database, statement, out_result);
}

int mylite_execution_execute_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_create_procedure_statement(database, statement, out_result);
}

int mylite_execution_execute_drop_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_drop_procedure_statement(database, statement, out_result);
}

int mylite_execution_execute_call_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_call_statement(database, statement, out_result);
}

bool mylite_execution_prepared_statement_disallows_statement(
    const struct mylite_sql_ast_node *statement
) {
    return prepared_statement_disallows_statement(statement);
}

int mylite_execution_append_show_create_definer(
    struct mylite_dynamic_string *string,
    const char *identity
) {
    return append_show_create_definer(string, identity);
}

int mylite_execution_try_stored_procedure_local_variables_placeholder(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result,
    bool *out_handled
) {
    return try_execute_stored_procedure_local_variables_placeholder_statement(
        database,
        statement,
        out_result,
        out_handled
    );
}

int mylite_execution_execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_show_create_procedure_statement(database, statement, out_result);
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return mylite_execution_text_equals_ascii_case_insensitive(left, right);
}

static int start_cursor_execution(mylite_stmt *statement) {
    return mylite_execution_session_program_start_cursor_execution(statement);
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    return mylite_execution_session_program_finish_parse_failure(database, parse_result, parse_rc);
}

static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result) {
    return mylite_execution_session_program_execute_empty_statement(database, out_result);
}

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return mylite_execution_session_program_execute_parsed_statement(
        database,
        context,
        statement,
        out_result
    );
}

static bool statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
) {
    return mylite_execution_session_program_statement_result_is_select(statement, result);
}

static int finish_failed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    int rc,
    mylite_result **out_result
) {
    return mylite_execution_session_program_finish_failed_statement(
        database,
        completion,
        rc,
        out_result
    );
}

static int finish_completed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    bool completed_statement_is_select,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
) {
    return mylite_execution_session_program_finish_completed_statement(
        database,
        completion,
        completed_statement_is_select,
        completed_row_count,
        preserve_diagnostics_snapshot,
        out_result
    );
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_unwrap_parenthesized_expression(expression);
}

static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return mylite_execution_decode_sql_string_literal(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
}

static void session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    mylite_execution_session_scalar_cell_deinit(cell);
}

static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_execution_session_program_resolve_selected_schema(database, out_schema);
}

static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_execution_session_resolve_schema_name(database, schema_name, out_schema);
}

static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return mylite_execution_session_scalar_value(database, expression, out_cell);
}

static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t capacity,
    size_t *out_part_count,
    struct mylite_db *database
) {
    return mylite_execution_session_program_collect_identifier_parts(
        database,
        node,
        parts,
        capacity,
        out_part_count
    );
}

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    return mylite_execution_session_program_resolve_table_name(database, node, out_resolution);
}

static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_by_name(schema_name);
}

#include "mylite_execution_prepared_statement_execution.inc"
#include "mylite_execution_prepared_statement_support.inc"
#include "mylite_execution_stored_procedures.inc"
