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
#include <time.h>

static const char mylite_embedded_identity[] = "mylite@localhost";
static const int mylite_session_decimal_conversion_base = 10;
static const uint32_t mylite_rand_max_value = 0x3fffffffU;
static const uint64_t mylite_uuid_epoch_offset_100ns = UINT64_C(122192928000000000);
static const uint64_t mylite_uuid_100ns_per_second = UINT64_C(10000000);
static const uint64_t mylite_uuid_short_startup_mask = UINT64_C(0xffffffff);
static const uint32_t mylite_uuid_short_counter_mask = 0x00ffffffU;
static const unsigned int mylite_uuid_short_server_shift = 56U;
static const unsigned int mylite_uuid_short_startup_shift = 24U;

enum mylite_uuid_constants {
    MYLITE_UUID_BINARY_LENGTH = 16,
    MYLITE_UUID_NODE_LENGTH = 6,
    MYLITE_UUID_TEXT_LENGTH = 36,
    MYLITE_UUID_RANDOM_BYTES = 8,
    MYLITE_UUID_TIME_LOW_TEXT_END = 8,
    MYLITE_UUID_TIME_MID_TEXT_END = 13,
    MYLITE_UUID_TIME_HIGH_TEXT_END = 18,
    MYLITE_UUID_CLOCK_SEQ_TEXT_END = 23,
    MYLITE_UUID_RANDOM_NODE_OFFSET = 2,
    MYLITE_UUID_NODE_OFFSET = 10,
    MYLITE_UUID_VERSION_1_MASK = 0x1000U,
    MYLITE_UUID_TIME_HIGH_MASK = 0x0fffU,
    MYLITE_UUID_CLOCK_SEQ_MASK = 0x3fffU,
    MYLITE_UUID_VARIANT_MASK = 0x80U,
    MYLITE_UUID_VARIANT_VALUE_MASK = 0x3fU,
    MYLITE_UUID_HEX_LOW_NIBBLE_MASK = 0x0fU,
    MYLITE_UUID_MULTICAST_NODE_MASK = 0x01U,
    MYLITE_UUID_SHORT_RANDOM_BYTES = 4,
    MYLITE_UUID_SHORT_SERVER_ID_MASK = 0x7fU,
    MYLITE_UUID_SHORT_SERVER_ID_FALLBACK = 1,
};

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
static int evaluate_uuid_function(mylite_stmt *stmt, struct mylite_expression_value *out_value);
static int evaluate_uuid_short_function(mylite_stmt *stmt,
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
static void next_uuid_bytes(mylite_db *database, unsigned char bytes[MYLITE_UUID_BINARY_LENGTH]);
static void initialize_uuid_state(struct mylite_uuid_state *state);
static uint64_t uuid_current_timestamp_100ns(void);
static void format_uuid_text(const unsigned char bytes[MYLITE_UUID_BINARY_LENGTH],
                             char text[MYLITE_UUID_TEXT_LENGTH + 1U]);
static uint64_t next_uuid_short_value(mylite_db *database);
static void initialize_uuid_short_state(struct mylite_uuid_short_state *state);
static uint64_t current_unix_seconds(void);
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
    if (mylite_span_equal_ci(name->span, "UUID")) {
        return evaluate_uuid_function(stmt, out_value);
    }
    if (mylite_span_equal_ci(name->span, "UUID_SHORT")) {
        return evaluate_uuid_short_function(stmt, out_value);
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
    if (mylite_span_equal_ci(name->span, "CURRENT_ROLE")) {
        return mylite_session_set_text_function_value(database, "NONE", out_value);
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

static int evaluate_uuid_function(mylite_stmt *stmt, struct mylite_expression_value *out_value)
{
    unsigned char bytes[MYLITE_UUID_BINARY_LENGTH] = {0};
    char text[MYLITE_UUID_TEXT_LENGTH + 1U] = {0};

    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }
    next_uuid_bytes(stmt->database, bytes);
    format_uuid_text(bytes, text);
    return mylite_session_set_text_function_value(stmt->database, text, out_value);
}

static int evaluate_uuid_short_function(mylite_stmt *stmt,
                                        struct mylite_expression_value *out_value)
{
    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_UINT64,
        .uint64_value = next_uuid_short_value(stmt->database),
    };
    return 0;
}

static void next_uuid_bytes(mylite_db *database, unsigned char bytes[MYLITE_UUID_BINARY_LENGTH])
{
    struct mylite_uuid_state *state = &database->uuid_state;
    uint64_t timestamp = uuid_current_timestamp_100ns();
    uint32_t time_low = 0U;
    uint16_t time_mid = 0U;
    uint16_t time_high = 0U;
    uint16_t clock_sequence = 0U;

    if (!state->initialized) {
        initialize_uuid_state(state);
    }
    if (timestamp <= state->last_timestamp) {
        state->clock_sequence =
            (uint16_t)((state->clock_sequence + 1U) & (uint16_t)MYLITE_UUID_CLOCK_SEQ_MASK);
        if (state->last_timestamp < UINT64_MAX) {
            timestamp = state->last_timestamp + 1U;
        }
    }
    state->last_timestamp = timestamp;

    time_low = (uint32_t)(timestamp & UINT64_C(0xffffffff));
    time_mid = (uint16_t)((timestamp >> 32U) & UINT64_C(0xffff));
    time_high =
        (uint16_t)(((timestamp >> 48U) & MYLITE_UUID_TIME_HIGH_MASK) | MYLITE_UUID_VERSION_1_MASK);
    clock_sequence = (uint16_t)(state->clock_sequence & MYLITE_UUID_CLOCK_SEQ_MASK);

    bytes[0] = (unsigned char)(time_low >> 24U);
    bytes[1] = (unsigned char)(time_low >> 16U);
    bytes[2] = (unsigned char)(time_low >> 8U);
    bytes[3] = (unsigned char)time_low;
    bytes[4] = (unsigned char)(time_mid >> 8U);
    bytes[5] = (unsigned char)time_mid;
    bytes[6] = (unsigned char)(time_high >> 8U);
    bytes[7] = (unsigned char)time_high;
    bytes[8] = (unsigned char)(((clock_sequence >> 8U) & MYLITE_UUID_VARIANT_VALUE_MASK) |
                               MYLITE_UUID_VARIANT_MASK);
    bytes[9] = (unsigned char)clock_sequence;
    memcpy(bytes + MYLITE_UUID_NODE_OFFSET, state->node, MYLITE_UUID_NODE_LENGTH);
}

static void initialize_uuid_state(struct mylite_uuid_state *state)
{
    unsigned char random[MYLITE_UUID_RANDOM_BYTES] = {0};

    sqlite3_randomness((int)sizeof(random), random);
    state->clock_sequence = (uint16_t)((((uint16_t)random[0] << 8U) | (uint16_t)random[1]) &
                                       (uint16_t)MYLITE_UUID_CLOCK_SEQ_MASK);
    memcpy(state->node, random + MYLITE_UUID_RANDOM_NODE_OFFSET, MYLITE_UUID_NODE_LENGTH);
    state->node[0] = (unsigned char)(state->node[0] | MYLITE_UUID_MULTICAST_NODE_MASK);
    state->initialized = true;
}

static uint64_t uuid_current_timestamp_100ns(void)
{
#ifdef TIME_UTC
    struct timespec timestamp;

    if (timespec_get(&timestamp, TIME_UTC) == TIME_UTC && timestamp.tv_sec >= (time_t)0 &&
        timestamp.tv_nsec >= 0L) {
        return mylite_uuid_epoch_offset_100ns +
               ((uint64_t)timestamp.tv_sec * mylite_uuid_100ns_per_second) +
               ((uint64_t)timestamp.tv_nsec / 100U);
    }
#endif
    {
        time_t seconds = time(NULL);

        if (seconds == (time_t)-1 || seconds < (time_t)0) {
            seconds = 0;
        }
        return mylite_uuid_epoch_offset_100ns + ((uint64_t)seconds * mylite_uuid_100ns_per_second);
    }
}

static void format_uuid_text(const unsigned char bytes[MYLITE_UUID_BINARY_LENGTH],
                             char text[MYLITE_UUID_TEXT_LENGTH + 1U])
{
    static const char digits[] = "0123456789abcdef";
    size_t output = 0U;

    for (size_t index = 0U; index < MYLITE_UUID_BINARY_LENGTH; ++index) {
        if (output == MYLITE_UUID_TIME_LOW_TEXT_END || output == MYLITE_UUID_TIME_MID_TEXT_END ||
            output == MYLITE_UUID_TIME_HIGH_TEXT_END || output == MYLITE_UUID_CLOCK_SEQ_TEXT_END) {
            text[output++] = '-';
        }
        text[output++] = digits[bytes[index] >> 4U];
        text[output++] = digits[bytes[index] & MYLITE_UUID_HEX_LOW_NIBBLE_MASK];
    }
    text[output] = '\0';
}

static uint64_t next_uuid_short_value(mylite_db *database)
{
    struct mylite_uuid_short_state *state = &database->uuid_short_state;
    uint32_t counter = 0U;
    uint64_t startup_seconds = 0U;
    uint64_t value = 0U;

    if (!state->initialized) {
        initialize_uuid_short_state(state);
    }

    counter = state->counter & mylite_uuid_short_counter_mask;
    startup_seconds = state->startup_seconds & mylite_uuid_short_startup_mask;
    value = ((uint64_t)state->server_id << mylite_uuid_short_server_shift) |
            (startup_seconds << mylite_uuid_short_startup_shift) | (uint64_t)counter;

    if (counter == mylite_uuid_short_counter_mask) {
        state->counter = 0U;
        if (state->startup_seconds < mylite_uuid_short_startup_mask) {
            ++state->startup_seconds;
        }
    } else {
        state->counter = counter + 1U;
    }
    return value;
}

static void initialize_uuid_short_state(struct mylite_uuid_short_state *state)
{
    unsigned char random[MYLITE_UUID_SHORT_RANDOM_BYTES] = {0};

    sqlite3_randomness((int)sizeof(random), random);
    state->server_id = (uint8_t)(random[0] & MYLITE_UUID_SHORT_SERVER_ID_MASK);
    if (state->server_id == 0U) {
        state->server_id = MYLITE_UUID_SHORT_SERVER_ID_FALLBACK;
    }
    state->startup_seconds = current_unix_seconds();
    state->counter =
        (((uint32_t)random[1] << 16U) | ((uint32_t)random[2] << 8U) | (uint32_t)random[3]) &
        mylite_uuid_short_counter_mask;
    state->initialized = true;
}

static uint64_t current_unix_seconds(void)
{
    time_t seconds = time(NULL);

    if (seconds == (time_t)-1 || seconds < (time_t)0) {
        return 0U;
    }
    return (uint64_t)seconds;
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
