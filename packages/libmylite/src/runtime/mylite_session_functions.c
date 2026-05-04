#include "mylite_session_functions.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_temporal_functions.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char mylite_embedded_identity[] = "mylite@localhost";
static const int mylite_session_decimal_conversion_base = 10;

static int
evaluate_last_insert_id_function(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static uint64_t session_function_value_to_uint64(const struct mylite_expression_value *value);

int mylite_session_evaluate_core_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    mylite_db *database = stmt == NULL ? NULL : stmt->database;
    const struct mylite_sql_ast_node *name = NULL;

    if (database == NULL || function_call == NULL) {
        return -1;
    }
    if (function_call->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return mylite_temporal_evaluate_current_function(stmt, function_call, out_value);
    }

    name = mylite_ast_child_at(function_call, 0U);
    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return -1;
    }
    if (mylite_temporal_function_name_is_current(name)) {
        return mylite_temporal_evaluate_current_function(stmt, function_call, out_value);
    }
    if (mylite_span_equal_ci(name->span, "DATABASE") ||
        mylite_span_equal_ci(name->span, "SCHEMA")) {
        if (database->selected_schema == NULL) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return 0;
        }
        return mylite_session_set_text_function_value(database, database->selected_schema,
                                                      out_value);
    }
    if (mylite_span_equal_ci(name->span, "VERSION")) {
        return mylite_session_set_text_function_value(database, mylite_version(), out_value);
    }
    if (mylite_span_equal_ci(name->span, "LAST_INSERT_ID")) {
        return evaluate_last_insert_id_function(stmt, function_call, expression_context, warnings,
                                                out_value);
    }
    if (mylite_span_equal_ci(name->span, "ROW_COUNT")) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = database->previous_row_count};
        return 0;
    }
    if (mylite_span_equal_ci(name->span, "CONNECTION_ID")) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = database->connection_id};
        return 0;
    }
    if (mylite_span_equal_ci(name->span, "USER") ||
        mylite_span_equal_ci(name->span, "SESSION_USER") ||
        mylite_span_equal_ci(name->span, "SYSTEM_USER") ||
        mylite_span_equal_ci(name->span, "CURRENT_USER")) {
        return mylite_session_set_text_function_value(database, mylite_embedded_identity,
                                                      out_value);
    }
    return -1;
}

int mylite_session_set_text_function_value(mylite_db *database, const char *text,
                                           struct mylite_expression_value *out_value)
{
    size_t length = text == NULL ? 0U : strlen(text);

    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = mylite_copy_span_text(text == NULL ? "" : text, length);
    out_value->text_length = length;
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int
evaluate_last_insert_id_function(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_expression_value value = {0};
    int status = 0;

    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }
    if (argument == NULL) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_UINT64, .uint64_value = stmt->database->last_insert_id};
        return 0;
    }

    status = mylite_expression_eval_with_context(argument, expression_context, warnings, &value);
    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (value.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        stmt->database->last_insert_id = 0U;
        mylite_expression_value_deinit(&value);
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }

    stmt->database->last_insert_id = session_function_value_to_uint64(&value);
    mylite_expression_value_deinit(&value);
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                  .uint64_value = stmt->database->last_insert_id};
    return 0;
}

static uint64_t session_function_value_to_uint64(const struct mylite_expression_value *value)
{
    if (value == NULL) {
        return 0U;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return 0U;
    case MYLITE_EXPRESSION_VALUE_INT64:
        return (uint64_t)value->int64_value;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return value->uint64_value;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return (uint64_t)value->real_value;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return value->text_value == NULL
                   ? 0U
                   : (uint64_t)strtoull(value->text_value, NULL,
                                        mylite_session_decimal_conversion_base);
    }
    return 0U;
}
