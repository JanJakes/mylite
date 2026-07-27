#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_PROGRAMS_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_PROGRAMS_SUPPORT_H

#include "mylite_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_schema_descriptor;
struct mylite_db;
struct mylite_result;
struct mylite_sql_ast_node;
struct mylite_sql_parse_result;
struct mylite_statement_completion;
struct mylite_statement_context;
struct mylite_stmt;
struct table_name_resolution;

int mylite_execution_session_program_start_cursor_execution(struct mylite_stmt *statement);
int mylite_execution_session_program_finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
);
int mylite_execution_session_program_execute_empty_statement(
    struct mylite_db *database,
    struct mylite_result **out_result
);
int mylite_execution_session_program_validate_alter_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
int mylite_execution_session_program_execute_non_prepared_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
int mylite_execution_session_program_execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    struct mylite_result **out_result
);
bool mylite_execution_session_program_statement_result_is_select(
    const struct mylite_sql_ast_node *statement,
    const struct mylite_result *result
);
int mylite_execution_session_program_finish_failed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    int rc,
    struct mylite_result **out_result
);
int mylite_execution_session_program_finish_completed_statement(
    struct mylite_db *database,
    struct mylite_statement_completion *completion,
    bool completed_statement_is_select,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    struct mylite_result **out_result
);
int mylite_execution_session_program_resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_execution_session_program_collect_identifier_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t capacity,
    size_t *out_part_count
);
int mylite_execution_session_program_resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);

#endif
