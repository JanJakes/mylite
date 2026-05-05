#include "mylite_session_functions.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "mylite_temporal_functions.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char mylite_embedded_identity[] = "mylite@localhost";
static const int mylite_session_decimal_conversion_base = 10;
static const uint32_t mylite_rand_max_value = 0x3fffffffU;

static int
evaluate_last_insert_id_function(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int evaluate_unix_timestamp_function(mylite_stmt *stmt,
                                            struct mylite_expression_value *out_value);
static int evaluate_rand_function(mylite_stmt *stmt,
                                  const struct mylite_sql_ast_node *function_call,
                                  const struct mylite_expression_eval_context *expression_context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static const struct mylite_sql_ast_node *
rand_seed_argument(const struct mylite_sql_ast_node *function_call);
static int
evaluate_rand_dynamic_seed(const struct mylite_sql_ast_node *argument,
                           const struct mylite_expression_eval_context *expression_context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static void set_rand_output_value(struct mylite_rand_state *state,
                                  struct mylite_expression_value *out_value);
static int rand_state_for_function(mylite_stmt *stmt,
                                   const struct mylite_sql_ast_node *function_call,
                                   struct mylite_rand_state **out_state);
static int initialize_rand_state(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_rand_state *state);
static uint64_t unseeded_rand_seed(mylite_stmt *stmt,
                                   const struct mylite_sql_ast_node *function_call);
static void initialize_rand_seed(struct mylite_rand_state *state, uint64_t seed);
static double next_rand_value(struct mylite_rand_state *state);
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
    if (mylite_span_equal_ci(name->span, "UNIX_TIMESTAMP")) {
        return evaluate_unix_timestamp_function(stmt, out_value);
    }
    if (mylite_span_equal_ci(name->span, "RAND")) {
        return evaluate_rand_function(stmt, function_call, expression_context, warnings, out_value);
    }
    if (mylite_span_equal_ci(name->span, "ROW_COUNT")) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = database->previous_row_count};
        return 0;
    }
    if (mylite_span_equal_ci(name->span, "FOUND_ROWS")) {
        if (mylite_expression_warnings_append(
                warnings, MYLITE_MYSQL_ER_WARN_DEPRECATED_SYNTAX,
                "FOUND_ROWS() is deprecated and will be removed in a future release. "
                "Consider using COUNT(*) instead.") != 0) {
            return -1;
        }
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_UINT64,
            .uint64_value = database->previous_found_rows,
        };
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

static int evaluate_unix_timestamp_function(mylite_stmt *stmt,
                                            struct mylite_expression_value *out_value)
{
    const struct mylite_statement_timestamp *timestamp = NULL;
    int status = mylite_temporal_statement_timestamp(stmt, &timestamp);

    if (status != MYLITE_OK) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = (int64_t)timestamp->seconds};
    return MYLITE_OK;
}

static int evaluate_rand_function(mylite_stmt *stmt,
                                  const struct mylite_sql_ast_node *function_call,
                                  const struct mylite_expression_eval_context *expression_context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument = rand_seed_argument(function_call);
    struct mylite_rand_state *state = NULL;
    int status = MYLITE_OK;

    if (argument != NULL && !mylite_expression_is_cacheable_no_table(argument)) {
        return evaluate_rand_dynamic_seed(argument, expression_context, warnings, out_value);
    }

    status = rand_state_for_function(stmt, function_call, &state);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!state->initialized) {
        status = initialize_rand_state(stmt, function_call, expression_context, warnings, state);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    set_rand_output_value(state, out_value);
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *
rand_seed_argument(const struct mylite_sql_ast_node *function_call)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);

    return mylite_ast_child_at(arguments, 0U);
}

static int
evaluate_rand_dynamic_seed(const struct mylite_sql_ast_node *argument,
                           const struct mylite_expression_eval_context *expression_context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct mylite_expression_value seed_value = {0};
    struct mylite_rand_state state = {0};
    int status =
        mylite_expression_eval_with_context(argument, expression_context, warnings, &seed_value);

    if (status != MYLITE_OK) {
        mylite_expression_value_deinit(&seed_value);
        return status;
    }
    initialize_rand_seed(&state, session_function_value_to_uint64(&seed_value));
    mylite_expression_value_deinit(&seed_value);
    set_rand_output_value(&state, out_value);
    return MYLITE_OK;
}

static void set_rand_output_value(struct mylite_rand_state *state,
                                  struct mylite_expression_value *out_value)
{
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = next_rand_value(state),
        .compact_real_text = true,
    };
}

static int rand_state_for_function(mylite_stmt *stmt,
                                   const struct mylite_sql_ast_node *function_call,
                                   struct mylite_rand_state **out_state)
{
    struct mylite_rand_state *states = NULL;
    size_t next_count = 0U;

    if (stmt == NULL || out_state == NULL) {
        return -1;
    }
    for (size_t index = 0U; index < stmt->rand_state_count; ++index) {
        if (stmt->rand_states[index].function_call == function_call) {
            *out_state = &stmt->rand_states[index];
            return MYLITE_OK;
        }
    }
    if (stmt->rand_state_count >= SIZE_MAX / sizeof(*stmt->rand_states)) {
        return -1;
    }
    next_count = stmt->rand_state_count + 1U;
    states = realloc(stmt->rand_states, next_count * sizeof(*stmt->rand_states));
    if (states == NULL) {
        if (stmt->database != NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return MYLITE_NOMEM;
    }
    stmt->rand_states = states;
    stmt->rand_states[stmt->rand_state_count] = (struct mylite_rand_state){
        .function_call = function_call,
    };
    *out_state = &stmt->rand_states[stmt->rand_state_count];
    stmt->rand_state_count = next_count;
    return MYLITE_OK;
}

static int initialize_rand_state(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_rand_state *state)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_expression_value seed_value = {0};
    uint64_t seed = 0U;
    int status = 0;

    if (argument == NULL) {
        initialize_rand_seed(state, unseeded_rand_seed(stmt, function_call));
        return MYLITE_OK;
    }

    status =
        mylite_expression_eval_with_context(argument, expression_context, warnings, &seed_value);
    if (status != 0) {
        mylite_expression_value_deinit(&seed_value);
        return status;
    }
    seed = session_function_value_to_uint64(&seed_value);
    mylite_expression_value_deinit(&seed_value);
    initialize_rand_seed(state, seed);
    return MYLITE_OK;
}

static uint64_t unseeded_rand_seed(mylite_stmt *stmt,
                                   const struct mylite_sql_ast_node *function_call)
{
    const struct mylite_statement_timestamp *timestamp = NULL;
    uint64_t seed = (uint64_t)(uintptr_t)function_call ^ (uint64_t)(uintptr_t)stmt;

    if (stmt != NULL && stmt->database != NULL) {
        seed ^= stmt->database->connection_id;
    }
    if (mylite_temporal_statement_timestamp(stmt, &timestamp) == MYLITE_OK && timestamp != NULL) {
        seed ^= (uint64_t)timestamp->seconds;
        seed ^= ((uint64_t)timestamp->microseconds << 24U);
    }
    return seed;
}

static void initialize_rand_seed(struct mylite_rand_state *state, uint64_t seed)
{
    uint64_t reduced = seed % (uint64_t)mylite_rand_max_value;

    state->seed1 = (uint32_t)(((reduced * UINT64_C(0x10001)) + UINT64_C(55555555)) %
                              (uint64_t)mylite_rand_max_value);
    state->seed2 = (uint32_t)((reduced * UINT64_C(0x10000001)) % (uint64_t)mylite_rand_max_value);
    state->initialized = true;
}

static double next_rand_value(struct mylite_rand_state *state)
{
    state->seed1 = (uint32_t)((((uint64_t)state->seed1 * 3U) + state->seed2) %
                              (uint64_t)mylite_rand_max_value);
    state->seed2 =
        (uint32_t)(((uint64_t)state->seed1 + state->seed2 + 33U) % (uint64_t)mylite_rand_max_value);
    return (double)state->seed1 / (double)mylite_rand_max_value;
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
