#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_temporal_format.h"

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"
#include "mylite_temporal_arithmetic.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    date_text_length = 10,
    datetime_text_length = mylite_execution_scalar_datetime_text_length,
    decimal_base = 10,
    date_minimum_year = 1000,
    date_maximum_year = 9999,
    date_months_per_year = 12,
    date_interval_diagnostic_capacity = 160,
    date_interval_nul_diagnostic_capacity = 96,
    date_interval_format_diagnostic_capacity = 96,
    date_interval_days_per_week = 7,
    date_interval_months_per_quarter = 3,
    time_text_minimum_length = 8,
    time_text_maximum_length = 10,
    time_minute_second_suffix_length = 6,
    time_minimum_three_digit_hour = 100,
    time_maximum_hour = 838,
    time_maximum_minute_or_second = 59,
    time_second_per_minute = 60,
    time_second_per_hour = 3600,
    time_arithmetic_sqlite_argument_count = 2,
};

enum scalar_time_arithmetic_input_kind {
    SCALAR_TIME_ARITHMETIC_INPUT_NULL = 0,
    SCALAR_TIME_ARITHMETIC_INPUT_TIME = 1,
    SCALAR_TIME_ARITHMETIC_INPUT_DATETIME = 2,
};

struct scalar_time_arithmetic_input {
    enum scalar_time_arithmetic_input_kind kind;
    int64_t time_seconds;
    struct mylite_temporal_datetime_parts datetime;
};

static int date_interval_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int timestampadd_second_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit
);
static int set_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
);
static int set_date_interval_second_unsupported_shape_error(
    struct mylite_db *database,
    const char *function_name
);
static int date_interval_second_temporal_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct mylite_temporal_datetime_parts *out_datetime,
    bool *out_has_time,
    bool *out_is_null
);
static const char *date_interval_literal_support_text(const char *function_name);
static int set_date_interval_argument_support_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
);
static int set_date_interval_argument_range_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
);
static int date_interval_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_apply_calendar_months(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_second_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_seconds,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_second_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    struct session_scalar_cell *out_cell
);
static int date_interval_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    bool result_has_time,
    struct session_scalar_cell *out_cell
);
static const char *time_arithmetic_function_name(enum mylite_sql_ast_node_kind kind);
static bool time_arithmetic_function_subtracts(enum mylite_sql_ast_node_kind kind);
static void addtime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void subtime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int time_arithmetic_sqlite_result(
    sqlite3_context *context,
    enum mylite_sql_ast_node_kind kind,
    sqlite3_value *first_value,
    sqlite3_value *second_value
);
static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int set_time_arithmetic_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
);
static bool time_arithmetic_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
);
static int time_arithmetic_first_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_second_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_decode_string_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_suffix,
    char **out_text,
    size_t *out_text_length
);
static int time_arithmetic_text_value(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *first_value,
    size_t first_value_length,
    bool first_is_null,
    const char *second_value,
    size_t second_value_length,
    bool second_is_null,
    char **out_text,
    bool *out_is_null
);
static int time_arithmetic_first_text_argument(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *value,
    size_t value_length,
    bool is_null,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_second_text_argument(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *value,
    size_t value_length,
    bool is_null,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_apply_datetime(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
);
static int time_arithmetic_apply_time(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
);
static int time_arithmetic_format_time(
    struct mylite_db *database,
    const char *function_name,
    int64_t seconds,
    struct session_scalar_cell *out_cell
);
static int copy_time_arithmetic_result(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    char **out_text
);
static bool time_arithmetic_seconds_in_range(int64_t seconds);
static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_negate(int64_t value, int64_t *out_result);
static int date_add_signed_integer_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
);
static bool date_add_signed_integer_literal(
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_out_of_range
);
static int date_add_signed_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
);
static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds);
static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
);
static bool time_text_has_canonical_shape(const char *text, size_t text_length);
static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
);
static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value);

int mylite_execution_scalar_date_interval_second_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return date_interval_value(database, expression, out_cell);
}

static int date_interval_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    struct mylite_temporal_datetime_parts input = {0};
    struct mylite_temporal_datetime_parts output = {0};
    const char *function_name = "DATE_ADD";
    int64_t interval_value = 0;
    bool temporal_has_time = false;
    bool temporal_is_null = false;
    bool interval_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        !mylite_execution_is_date_interval_second_function_kind(expression->kind)) {
        return set_date_interval_second_unsupported_shape_error(database, function_name);
    }
    function_name = mylite_execution_date_interval_second_function_name(expression->kind);
    rc = mylite_execution_validate_date_interval_second_function_shape(
        database,
        expression,
        function_name
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = date_interval_second_temporal_argument(
        database,
        function_name,
        mylite_execution_date_interval_second_temporal_node(expression),
        &input,
        &temporal_has_time,
        &temporal_is_null
    );
    if (rc != MYLITE_OK || temporal_is_null) {
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        rc = mylite_execution_date_interval_unit_from_ast(
            database,
            mylite_execution_date_interval_unit_node(expression),
            function_name,
            &unit
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    rc = mylite_execution_date_interval_second_interval_argument(
        database,
        function_name,
        mylite_execution_date_interval_second_interval_node(expression),
        unit,
        &interval_value,
        &interval_is_null
    );
    if (rc != MYLITE_OK || interval_is_null) {
        return rc;
    }
    if (mylite_execution_date_interval_second_function_subtracts(expression->kind) &&
        checked_int64_negate(interval_value, &interval_value)) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }
    rc = date_interval_apply(database, function_name, &input, interval_value, unit, &output);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return date_interval_format(
        database,
        function_name,
        &output,
        (temporal_has_time || mylite_date_interval_unit_has_time_part(unit)) != 0,
        out_cell
    );
}

bool mylite_execution_is_date_interval_second_function_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
        return true;
    default:
        return false;
    }
}

const char *mylite_execution_date_interval_second_function_name(enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
        return "DATE_SUB";
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
        return "ADDDATE";
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
        return "SUBDATE";
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
        return "TIMESTAMPADD";
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    default:
        return "DATE_ADD";
    }
}

bool mylite_execution_date_interval_second_function_subtracts(enum mylite_sql_ast_node_kind kind) {
    return (kind == MYLITE_SQL_AST_DATE_SUB_FUNCTION || kind == MYLITE_SQL_AST_SUBDATE_FUNCTION) !=
           0;
}

int mylite_execution_validate_date_interval_second_function_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    size_t child_count = 0U;

    if (expression == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        if (child_count != 3U) {
            return set_date_interval_second_unsupported_shape_error(database, function_name);
        }
        return timestampadd_second_unit_from_ast(
            database,
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (child_count != 3U) {
        return set_date_interval_second_unsupported_shape_error(database, function_name);
    }
    return MYLITE_OK;
}

static int timestampadd_second_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = mylite_execution_copy_identifier_text(unit, unit_name, sizeof(unit_name), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(unit_name, "SECOND") ||
        mylite_execution_text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_SECOND")) {
        return MYLITE_OK;
    }
    mylite_execution_set_unsupported_error(
        database,
        "TIMESTAMPADD() supports only SECOND and SQL_TSI_SECOND units"
    );
    return MYLITE_ERROR;
}

int mylite_execution_date_interval_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit_node,
    const char *function_name,
    enum mylite_date_interval_unit *out_unit
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[date_interval_diagnostic_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (function_name == NULL || out_unit == NULL) {
        return MYLITE_MISUSE;
    }
    *out_unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    rc = mylite_execution_copy_identifier_text(unit_node, unit_name, sizeof(unit_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_date_interval_unit_from_name(unit_name, strlen(unit_name), out_unit) &&
        *out_unit != MYLITE_DATE_INTERVAL_UNIT_MICROSECOND) {
        return MYLITE_OK;
    }
    written = snprintf(
        message,
        sizeof(message),
        "%s() supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND "
        "interval units",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_second_temporal_node(
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return mylite_execution_child_at(expression, 2U);
    }
    return mylite_execution_child_at(expression, 0U);
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_second_interval_node(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_child_at(expression, 1U);
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_unit_node(
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return mylite_execution_child_at(expression, 0U);
    }
    return mylite_execution_child_at(expression, 2U);
}

static int set_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[date_interval_diagnostic_capacity];

    if (!mylite_execution_date_interval_second_message(
            message,
            sizeof(message),
            function_name,
            suffix
        )) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int set_date_interval_second_unsupported_shape_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[date_interval_diagnostic_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "%s() supports only %s(date, INTERVAL value unit)",
        function_name,
        function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

bool mylite_execution_date_interval_second_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || function_name == NULL || suffix == NULL) {
        return false;
    }
    written = snprintf(buffer, buffer_size, "%s() %s", function_name, suffix);
    return (written >= 0 && (size_t)written < buffer_size) != 0;
}

static int date_interval_second_temporal_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct mylite_temporal_datetime_parts *out_datetime,
    bool *out_has_time,
    bool *out_is_null
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_datetime == NULL || out_has_time == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_datetime = (struct mylite_temporal_datetime_parts){0};
    *out_has_time = false;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_date_interval_second_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            "supports only date or datetime string literals and NULL"
        ) ||
        !mylite_execution_date_interval_second_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "date literals do not support NUL bytes"
        )) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        mylite_execution_set_unsupported_error(database, nul_message);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !mylite_temporal_arithmetic_parse_datetime_text(text, text_length, out_datetime)) {
        rc = set_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values"
        );
    }
    if (rc == MYLITE_OK) {
        *out_has_time = text_length == datetime_text_length;
    }
    free(text);
    return rc;
}

int mylite_execution_date_interval_second_interval_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    enum mylite_date_interval_unit unit,
    int64_t *out_interval,
    bool *out_is_null
) {
    bool interval_matched = false;
    bool out_of_range = false;
    int rc = MYLITE_OK;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_interval == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_interval = 0;
    *out_is_null = false;

    if (expression == NULL) {
        return set_date_interval_argument_support_error(database, function_name, unit);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    rc = date_add_signed_integer_expression(
        database,
        expression,
        function_name,
        out_interval,
        &interval_matched,
        &out_of_range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!interval_matched) {
        if (out_of_range) {
            return set_date_interval_argument_range_error(database, function_name, unit);
        }
        return set_date_interval_argument_support_error(database, function_name, unit);
    }

    return MYLITE_OK;
}

static const char *date_interval_literal_support_text(const char *function_name) {
    if (function_name != NULL && strcmp(function_name, "TIMESTAMPADD") == 0) {
        return "signed integer literals and NULL";
    }
    return "signed integer literals, exact signed integer string literals, and NULL";
}

static int set_date_interval_argument_support_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
) {
    const char *literal_support = date_interval_literal_support_text(function_name);
    const char *unit_name = mylite_date_interval_unit_name(unit);
    char message[date_interval_diagnostic_capacity];
    int written = 0;

    if (unit_name == NULL) {
        unit_name = "unit";
    }
    written = snprintf(
        message,
        sizeof(message),
        "INTERVAL %s supports only %s",
        unit_name,
        literal_support
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    return set_date_interval_second_unsupported_error(database, function_name, message);
}

static int set_date_interval_argument_range_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
) {
    const char *unit_name = mylite_date_interval_unit_name(unit);
    char message[date_interval_diagnostic_capacity];
    int written = 0;

    if (unit_name == NULL) {
        unit_name = "unit";
    }
    written = snprintf(
        message,
        sizeof(message),
        "INTERVAL %s literals must fit the signed 64-bit range",
        unit_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    return set_date_interval_second_unsupported_error(database, function_name, message);
}

static int date_interval_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    int64_t interval_seconds = 0;

    if (input == NULL || out_datetime == NULL) {
        return MYLITE_MISUSE;
    }
    switch (unit) {
    case MYLITE_DATE_INTERVAL_UNIT_YEAR:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                date_months_per_year,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_QUARTER:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                date_interval_months_per_quarter,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MONTH:
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_value,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_WEEK:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                mylite_temporal_arithmetic_seconds_per_day() * date_interval_days_per_week,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_DAY:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                mylite_temporal_arithmetic_seconds_per_day(),
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_HOUR:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                time_second_per_hour,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MINUTE:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                time_second_per_minute,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_SECOND:
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_value,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MICROSECOND:
    default:
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND interval "
            "units"
        );
    }
}

static int date_interval_apply_calendar_months(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    bool applied =
        mylite_temporal_arithmetic_add_calendar_months(input, interval_months, out_datetime);

    if (applied) {
        return MYLITE_OK;
    }
    return set_date_interval_second_unsupported_error(
        database,
        function_name,
        "result is outside the supported datetime range"
    );
}

static int date_interval_second_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_seconds,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    const int64_t seconds_per_day = mylite_temporal_arithmetic_seconds_per_day();
    struct mylite_temporal_day_second result_day_second = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t result_seconds = 0;
    int64_t result_day_seconds = 0;

    if (input == NULL || out_datetime == NULL) {
        return MYLITE_MISUSE;
    }
    *out_datetime = (struct mylite_temporal_datetime_parts){0};

    days = mylite_temporal_arithmetic_days_from_datetime(input);
    day_seconds = ((int64_t)input->hour * (int64_t)time_second_per_hour) +
                  ((int64_t)input->minute * (int64_t)time_second_per_minute) +
                  (int64_t)input->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!mylite_temporal_arithmetic_checked_add_int64(
            base_seconds,
            interval_seconds,
            &result_seconds
        )) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }

    result_day_second = mylite_temporal_arithmetic_floor_divmod_seconds(result_seconds);
    mylite_temporal_arithmetic_civil_from_days(result_day_second.days, out_datetime);
    if (out_datetime->year < date_minimum_year || out_datetime->year > date_maximum_year) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }

    result_day_seconds = result_day_second.day_seconds;
    out_datetime->hour = (uint32_t)(result_day_seconds / time_second_per_hour);
    result_day_seconds %= time_second_per_hour;
    out_datetime->minute = (uint32_t)(result_day_seconds / time_second_per_minute);
    out_datetime->second = (uint32_t)(result_day_seconds % time_second_per_minute);
    return MYLITE_OK;
}

static int date_interval_second_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    struct session_scalar_cell *out_cell
) {
    return date_interval_format(database, function_name, datetime, true, out_cell);
}

static int date_interval_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    bool result_has_time,
    struct session_scalar_cell *out_cell
) {
    char message[date_interval_format_diagnostic_capacity];
    int expected_length = date_text_length;
    int written = 0;

    if (datetime == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    if (result_has_time) {
        expected_length = datetime_text_length;
        written = snprintf(
            out_cell->datetime_text,
            sizeof(out_cell->datetime_text),
            "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
            datetime->year,
            datetime->month,
            datetime->day,
            datetime->hour,
            datetime->minute,
            datetime->second
        );
    } else {
        written = snprintf(
            out_cell->datetime_text,
            sizeof(out_cell->datetime_text),
            "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32,
            datetime->year,
            datetime->month,
            datetime->day
        );
    }
    if (written != expected_length) {
        written = snprintf(message, sizeof(message), "failed to format %s() result", function_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            mylite_execution_set_runtime_error(
                database,
                "failed to format temporal function result"
            );
            return MYLITE_ERROR;
        }
        mylite_execution_set_runtime_error(database, message);
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->datetime_text;
    return MYLITE_OK;
}

int mylite_execution_scalar_addtime_subtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_time_arithmetic_input first = {0};
    struct scalar_time_arithmetic_input second = {0};
    const char *function_name = "ADDTIME";
    int64_t second_seconds = 0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        !mylite_execution_is_time_arithmetic_function_kind(expression->kind) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    function_name = time_arithmetic_function_name(expression->kind);

    rc = time_arithmetic_first_argument(
        database,
        function_name,
        mylite_execution_child_at(expression, 0U),
        &first
    );
    if (rc != MYLITE_OK || first.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        return rc;
    }
    rc = time_arithmetic_second_argument(
        database,
        function_name,
        mylite_execution_child_at(expression, 1U),
        &second
    );
    if (rc != MYLITE_OK || second.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        return rc;
    }

    second_seconds = second.time_seconds;
    if (time_arithmetic_function_subtracts(expression->kind) &&
        checked_int64_negate(second_seconds, &second_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    if (first.kind == SCALAR_TIME_ARITHMETIC_INPUT_DATETIME) {
        return time_arithmetic_apply_datetime(
            database,
            function_name,
            &first,
            second_seconds,
            out_cell
        );
    }
    return time_arithmetic_apply_time(database, function_name, &first, second_seconds, out_cell);
}

int mylite_execution_addtime_subtime_text_value(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *first_value,
    size_t first_value_length,
    bool first_is_null,
    const char *second_value,
    size_t second_value_length,
    bool second_is_null,
    char **out_text,
    bool *out_is_null
) {
    if (!mylite_execution_is_time_arithmetic_function_kind(kind)) {
        return set_time_arithmetic_unsupported_error(
            database,
            "ADDTIME",
            "supports only ADDTIME() and SUBTIME()"
        );
    }
    return time_arithmetic_text_value(
        database,
        kind,
        first_value,
        first_value_length,
        first_is_null,
        second_value,
        second_value_length,
        second_is_null,
        out_text,
        out_is_null
    );
}

int mylite_sqlite_register_time_arithmetic_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_addtime",
            .argument_count = time_arithmetic_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = addtime_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_subtime",
            .argument_count = time_arithmetic_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = subtime_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

bool mylite_execution_is_time_arithmetic_function_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return true;
    default:
        return false;
    }
}

static const char *time_arithmetic_function_name(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return "SUBTIME";
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    default:
        return "ADDTIME";
    }
}

static bool time_arithmetic_function_subtracts(enum mylite_sql_ast_node_kind kind) {
    return kind == MYLITE_SQL_AST_SUBTIME_FUNCTION;
}

static void addtime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (context == NULL || argc != time_arithmetic_sqlite_argument_count || argv == NULL ||
        argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite ADDTIME callback", -1);
        return;
    }
    (void)time_arithmetic_sqlite_result(context, MYLITE_SQL_AST_ADDTIME_FUNCTION, argv[0], argv[1]);
}

static void subtime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (context == NULL || argc != time_arithmetic_sqlite_argument_count || argv == NULL ||
        argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SUBTIME callback", -1);
        return;
    }
    (void)time_arithmetic_sqlite_result(context, MYLITE_SQL_AST_SUBTIME_FUNCTION, argv[0], argv[1]);
}

static int time_arithmetic_sqlite_result(
    sqlite3_context *context,
    enum mylite_sql_ast_node_kind kind,
    sqlite3_value *first_value,
    sqlite3_value *second_value
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    const char *first_text = NULL;
    const char *second_text = NULL;
    size_t first_text_length = 0U;
    size_t second_text_length = 0U;
    bool first_is_null = false;
    bool second_is_null = false;
    bool result_is_null = false;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite time arithmetic owner", -1);
        return MYLITE_ERROR;
    }

    rc = sqlite_value_text_pointer(
        context,
        first_value,
        &first_text,
        &first_text_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = sqlite_value_text_pointer(
            context,
            second_value,
            &second_text,
            &second_text_length,
            &second_is_null
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = time_arithmetic_text_value(
        database,
        kind,
        first_text,
        first_text_length,
        first_is_null,
        second_text,
        second_text_length,
        second_is_null,
        &result,
        &result_is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite time arithmetic failed", -1);
        }
        free(result);
        return rc;
    }
    if (result_is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    }
    free(result);
    return MYLITE_OK;
}

static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    *out_text = (const char *)text;
    *out_text_length = (size_t)text_length;
    return MYLITE_OK;
}

static int set_time_arithmetic_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (!time_arithmetic_message(message, sizeof(message), function_name, suffix)) {
        mylite_execution_set_runtime_error(database, "failed to format time arithmetic diagnostic");
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static bool time_arithmetic_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || function_name == NULL || suffix == NULL) {
        return false;
    }
    written = snprintf(buffer, buffer_size, "%s() %s", function_name, suffix);
    return (written >= 0 && (size_t)written < buffer_size) != 0;
}

static int time_arithmetic_first_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical datetime string literals, canonical time string literals, "
            "and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical datetime string literals, canonical time string literals, "
            "and NULL"
        );
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }

    rc = time_arithmetic_decode_string_argument(
        database,
        function_name,
        expression,
        "supports only canonical datetime string literals, canonical time string literals, "
        "and NULL",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && text_length == datetime_text_length &&
        mylite_temporal_arithmetic_parse_datetime_text(text, text_length, &out_input->datetime)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_DATETIME;
    } else if (rc == MYLITE_OK &&
               time_text_to_seconds(text, text_length, &out_input->time_seconds)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
    } else if (rc == MYLITE_OK) {
        rc = set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical YYYY-MM-DD HH:MM:SS datetime or canonical [-]HH:MM:SS time "
            "values"
        );
    }

    free(text);
    return rc;
}

static int time_arithmetic_second_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical time string literals and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical time string literals and NULL"
        );
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }

    rc = time_arithmetic_decode_string_argument(
        database,
        function_name,
        expression,
        "time argument supports only canonical time string literals and NULL",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && time_text_to_seconds(text, text_length, &out_input->time_seconds)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
    } else if (rc == MYLITE_OK) {
        rc = set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical [-]HH:MM:SS time values"
        );
    }

    free(text);
    return rc;
}

static int time_arithmetic_decode_string_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_suffix,
    char **out_text,
    size_t *out_text_length
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_time_arithmetic_unsupported_error(database, function_name, unsupported_suffix);
    }
    if (!time_arithmetic_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            unsupported_suffix
        ) ||
        !time_arithmetic_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "time literals do not support NUL bytes"
        )) {
        mylite_execution_set_runtime_error(database, "failed to format time arithmetic diagnostic");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(database, nul_message);
        free(*out_text);
        *out_text = NULL;
        *out_text_length = 0U;
        return MYLITE_ERROR;
    }
    return rc;
}

static int time_arithmetic_text_value(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *first_value,
    size_t first_value_length,
    bool first_is_null,
    const char *second_value,
    size_t second_value_length,
    bool second_is_null,
    char **out_text,
    bool *out_is_null
) {
    struct scalar_time_arithmetic_input first = {0};
    struct scalar_time_arithmetic_input second = {0};
    struct session_scalar_cell cell = {0};
    const char *function_name = time_arithmetic_function_name(kind);
    int64_t second_seconds = 0;
    bool subtract = time_arithmetic_function_subtracts(kind);
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;

    rc = time_arithmetic_first_text_argument(
        database,
        kind,
        first_value,
        first_value_length,
        first_is_null,
        &first
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (first.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = time_arithmetic_second_text_argument(
        database,
        kind,
        second_value,
        second_value_length,
        second_is_null,
        &second
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (second.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    second_seconds = second.time_seconds;
    if (subtract && checked_int64_negate(second_seconds, &second_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    if (first.kind == SCALAR_TIME_ARITHMETIC_INPUT_DATETIME) {
        rc = time_arithmetic_apply_datetime(database, function_name, &first, second_seconds, &cell);
    } else {
        rc = time_arithmetic_apply_time(database, function_name, &first, second_seconds, &cell);
    }
    if (rc == MYLITE_OK) {
        rc = copy_time_arithmetic_result(database, &cell, out_text);
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    return rc;
}

static int time_arithmetic_first_text_argument(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *value,
    size_t value_length,
    bool is_null,
    struct scalar_time_arithmetic_input *out_input
) {
    const char *function_name = time_arithmetic_function_name(kind);

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};
    if (is_null) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }
    if (value == NULL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical datetime strings, canonical time strings, and NULL"
        );
    }
    if (value_length == datetime_text_length &&
        mylite_temporal_arithmetic_parse_datetime_text(value, value_length, &out_input->datetime)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_DATETIME;
        return MYLITE_OK;
    }
    if (time_text_to_seconds(value, value_length, &out_input->time_seconds)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
        return MYLITE_OK;
    }
    return set_time_arithmetic_unsupported_error(
        database,
        function_name,
        "supports only canonical YYYY-MM-DD HH:MM:SS datetime or canonical [-]HH:MM:SS time "
        "values"
    );
}

static int time_arithmetic_second_text_argument(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind kind,
    const char *value,
    size_t value_length,
    bool is_null,
    struct scalar_time_arithmetic_input *out_input
) {
    const char *function_name = time_arithmetic_function_name(kind);

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};
    if (is_null) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }
    if (value == NULL || !time_text_to_seconds(value, value_length, &out_input->time_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical [-]HH:MM:SS time values"
        );
    }
    out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
    return MYLITE_OK;
}

static int time_arithmetic_apply_datetime(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
) {
    struct mylite_temporal_datetime_parts output = {0};
    int rc = MYLITE_OK;

    if (first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = date_interval_second_apply(
        database,
        function_name,
        &first->datetime,
        second_seconds,
        &output
    );
    if (rc != MYLITE_OK) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    return date_interval_second_format(database, function_name, &output, out_cell);
}

static int time_arithmetic_apply_time(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
) {
    int64_t result_seconds = 0;

    if (first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (checked_int64_add(first->time_seconds, second_seconds, &result_seconds) ||
        !time_arithmetic_seconds_in_range(result_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    return time_arithmetic_format_time(database, function_name, result_seconds, out_cell);
}

static int time_arithmetic_format_time(
    struct mylite_db *database,
    const char *function_name,
    int64_t seconds,
    struct session_scalar_cell *out_cell
) {
    char buffer[sizeof("-838:59:59")];
    bool is_negative = seconds < 0;
    int64_t magnitude = seconds;
    int64_t hour = 0;
    int64_t minute = 0;
    int64_t second = 0;
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (is_negative) {
        magnitude = -magnitude;
    }
    hour = magnitude / time_second_per_hour;
    magnitude %= time_second_per_hour;
    minute = magnitude / time_second_per_minute;
    second = magnitude % time_second_per_minute;
    written = snprintf(
        buffer,
        sizeof(buffer),
        "%s%02" PRId64 ":%02" PRId64 ":%02" PRId64,
        is_negative ? "-" : "",
        hour,
        minute,
        second
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        char message[date_interval_format_diagnostic_capacity];

        written = snprintf(message, sizeof(message), "failed to format %s() result", function_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            mylite_execution_set_runtime_error(database, "failed to format time arithmetic result");
            return MYLITE_ERROR;
        }
        mylite_execution_set_runtime_error(database, message);
        return MYLITE_ERROR;
    }

    out_cell->owned_text = (char *)malloc((size_t)written + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, buffer, (size_t)written + 1U);
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static int copy_time_arithmetic_result(
    struct mylite_db *database,
    const struct session_scalar_cell *cell,
    char **out_text
) {
    size_t text_length = 0U;
    char *copy = NULL;

    if (cell == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (cell->value == NULL) {
        return MYLITE_OK;
    }
    text_length = strlen(cell->value);
    copy = (char *)malloc(text_length + 1U);
    if (copy == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, cell->value, text_length + 1U);
    *out_text = copy;
    return MYLITE_OK;
}

static bool time_arithmetic_seconds_in_range(int64_t seconds) {
    const int64_t maximum = ((int64_t)time_maximum_hour * time_second_per_hour) +
                            ((int64_t)time_maximum_minute_or_second * time_second_per_minute) +
                            (int64_t)time_maximum_minute_or_second;

    return (seconds >= -maximum && seconds <= maximum) != 0;
}

static int date_add_signed_integer_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
) {
    if (out_value == NULL || out_matched == NULL || out_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;
    if (date_add_signed_integer_literal(expression, out_value, out_out_of_range)) {
        *out_matched = true;
        return MYLITE_OK;
    }
    if (*out_out_of_range) {
        return MYLITE_OK;
    }
    if (function_name != NULL && strcmp(function_name, "TIMESTAMPADD") == 0) {
        return MYLITE_OK;
    }
    return date_add_signed_integer_string_literal(
        database,
        expression,
        out_value,
        out_matched,
        out_out_of_range
    );
}

static bool date_add_signed_integer_literal(
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_out_of_range
) {
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (out_value == NULL || out_out_of_range == NULL) {
        return false;
    }
    *out_value = 0;
    *out_out_of_range = false;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return false;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    } else {
        literal = expression;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        *out_out_of_range = true;
        return false;
    }

    if (is_negative) {
        if (magnitude > signed_negative_abs_max) {
            *out_out_of_range = true;
            return false;
        }
        if (magnitude == signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return true;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        *out_out_of_range = true;
        return false;
    }
    *out_value = (int64_t)magnitude;
    return true;
}

static int date_add_signed_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
) {
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    const char unsupported_message[] = "DATE interval string literal expected";
    const char nul_message[] = "DATE interval string literal does not support NUL bytes";
    char *text = NULL;
    const char *digits = NULL;
    size_t digit_count = 0U;
    size_t text_length = 0U;
    uint64_t magnitude = 0U;
    uint64_t limit = (uint64_t)INT64_MAX;
    bool is_negative = false;
    bool is_positive = false;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_matched == NULL || out_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_matched = false;
    *out_out_of_range = false;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        return MYLITE_OK;
    }
    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    if (text_length == 0U) {
        free(text);
        return MYLITE_OK;
    }
    is_negative = text[0] == '-';
    is_positive = text[0] == '+';
    digits = text + (is_negative || is_positive ? 1U : 0U);
    digit_count = text_length - (is_negative || is_positive ? 1U : 0U);
    if (digit_count == 0U) {
        free(text);
        return MYLITE_OK;
    }
    if (is_negative) {
        limit = signed_negative_abs_max;
    }
    for (size_t digit_index = 0U; digit_index < digit_count; ++digit_index) {
        unsigned char byte = (unsigned char)digits[digit_index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            free(text);
            return MYLITE_OK;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (limit - digit) / decimal_base) {
            *out_out_of_range = true;
            free(text);
            return MYLITE_OK;
        }
        magnitude = (magnitude * decimal_base) + digit;
    }
    if (is_negative) {
        *out_value = magnitude == signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    *out_matched = true;
    free(text);
    return MYLITE_OK;
}

static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result) {
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return true;
    }
    *out_result = left + right;
    return false;
}

static bool checked_int64_negate(int64_t value, int64_t *out_result) {
    if (value == INT64_MIN) {
        return true;
    }
    *out_result = -value;
    return false;
}

static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds) {
    bool is_negative = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int64_t total = 0;

    if (out_seconds == NULL ||
        !time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        return false;
    }
    if (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second) {
        return false;
    }
    if (!time_text_uses_canonical_hour_width(text, text_length, &hour)) {
        return false;
    }
    if (hour == 0U && minute == 0U && second == 0U && is_negative) {
        return false;
    }
    if (hour > time_maximum_hour ||
        (hour == time_maximum_hour &&
         (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second))) {
        return false;
    }

    total = ((int64_t)hour * time_second_per_hour) + ((int64_t)minute * time_second_per_minute) +
            (int64_t)second;
    if (is_negative) {
        total = -total;
    }
    *out_seconds = total;
    return true;
}

static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (out_is_negative == NULL || out_hour == NULL || out_minute == NULL || out_second == NULL ||
        !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (!date_component_text_to_u32(text + hour_offset, first_separator - hour_offset, out_hour) ||
        !date_component_text_to_u32(text + first_separator + 1U, 2U, out_minute) ||
        !date_component_text_to_u32(text + second_separator + 1U, 2U, out_second)) {
        return false;
    }

    *out_is_negative = is_negative;
    return true;
}

static bool time_text_has_canonical_shape(const char *text, size_t text_length) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (text == NULL || text_length < time_text_minimum_length ||
        text_length > time_text_maximum_length) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    if (is_negative && text_length == time_text_minimum_length) {
        return false;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (first_separator <= hour_offset || text[first_separator] != ':' ||
        text[second_separator] != ':') {
        return false;
    }
    if (first_separator - hour_offset < 2U || first_separator - hour_offset > 3U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if ((index == 0U && is_negative) || index == first_separator || index == second_separator) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }

    return true;
}

static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t hour_digit_count = 0U;

    if (hour == NULL || !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }

    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    hour_digit_count = first_separator - hour_offset;

    return (hour_digit_count == 2U ||
            (hour_digit_count == 3U && *hour >= time_minimum_three_digit_hour)) != 0;
}

static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value) {
    uint32_t value = 0U;

    if (text == NULL || out_value == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        value = (value * decimal_base) + (uint32_t)(byte - '0');
    }
    *out_value = value;
    return true;
}
