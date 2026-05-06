#include "mylite_temporal_functions.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const long mylite_temporal_nanoseconds_per_microsecond = 1000L;
static const long mylite_temporal_max_microsecond = 999999L;

static int ensure_statement_timestamp(mylite_stmt *stmt);

static void capture_current_utc_timestamp(struct mylite_statement_timestamp *out_timestamp);

static int set_current_temporal_datetime_value(
    mylite_stmt *stmt,
    unsigned int fsp,
    struct mylite_expression_value *out_value
);

static int set_current_temporal_date_value(
    mylite_stmt *stmt,
    struct mylite_expression_value *out_value
);

static int set_current_temporal_time_value(
    mylite_stmt *stmt,
    unsigned int fsp,
    struct mylite_expression_value *out_value
);

static int set_current_temporal_value(
    mylite_stmt *stmt,
    const char *format,
    size_t base_length,
    unsigned int fsp,
    struct mylite_expression_value *out_value
);

static bool temporal_fsp_from_literal(
    const struct mylite_sql_ast_node *argument,
    unsigned int *out_fsp
);

static bool temporal_function_name_matches_any(
    const struct mylite_sql_ast_node *name,
    const char *const *names,
    size_t name_count
);

int mylite_temporal_evaluate_current_function(
    mylite_stmt *stmt,
    const struct mylite_sql_ast_node *function_call,
    struct mylite_expression_value *out_value
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(function_call, 0U);
    unsigned int fsp = 0U;

    if (!mylite_temporal_current_function_fsp(function_call, &fsp)) {
        return -1;
    }
    if (function_call->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP ||
        mylite_temporal_function_name_is_current_datetime(name)) {
        return set_current_temporal_datetime_value(stmt, fsp, out_value);
    }
    if (mylite_temporal_function_name_is_current_date(name)) {
        return set_current_temporal_date_value(stmt, out_value);
    }
    if (mylite_temporal_function_name_is_current_time(name)) {
        return set_current_temporal_time_value(stmt, fsp, out_value);
    }
    return -1;
}

int mylite_temporal_statement_timestamp(
    mylite_stmt *stmt,
    const struct mylite_statement_timestamp **out_timestamp
) {
    int status = 0;

    if (out_timestamp == NULL) {
        return -1;
    }
    status = ensure_statement_timestamp(stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    *out_timestamp = &stmt->statement_timestamp;
    return MYLITE_OK;
}

bool mylite_temporal_current_function_fsp(
    const struct mylite_sql_ast_node *function_call,
    unsigned int *out_fsp
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    size_t arity = 0U;

    if (function_call == NULL || out_fsp == NULL) {
        return false;
    }
    if (function_call->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        *out_fsp = 0U;
        if (function_call->has_column_precision) {
            *out_fsp = (unsigned int)function_call->column_precision;
        }
        return true;
    }
    if (function_call->kind != MYLITE_SQL_AST_FUNCTION_CALL) {
        return false;
    }

    arguments = mylite_ast_child_at(function_call, 1U);
    arity = arguments == NULL ? 0U : mylite_sql_ast_node_child_count(arguments);
    if (arity == 0U) {
        *out_fsp = 0U;
        return true;
    }
    if (arity == 1U) {
        return temporal_fsp_from_literal(mylite_ast_child_at(arguments, 0U), out_fsp);
    }
    return false;
}

bool mylite_temporal_function_name_is_current(const struct mylite_sql_ast_node *name) {
    if (mylite_temporal_function_name_is_current_datetime(name) ||
        mylite_temporal_function_name_is_current_date(name)) {
        return true;
    }
    return mylite_temporal_function_name_is_current_time(name);
}

bool mylite_temporal_function_name_is_current_datetime(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"NOW", "LOCALTIME", "LOCALTIMESTAMP", "UTC_TIMESTAMP"};

    return temporal_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_temporal_function_name_is_current_date(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"CURDATE", "CURRENT_DATE", "UTC_DATE"};

    return temporal_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_temporal_function_name_is_current_time(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"CURTIME", "CURRENT_TIME", "UTC_TIME"};

    return temporal_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

static int ensure_statement_timestamp(mylite_stmt *stmt) {
    if (stmt == NULL) {
        return -1;
    }
    if (!stmt->has_statement_timestamp) {
        capture_current_utc_timestamp(&stmt->statement_timestamp);
        stmt->has_statement_timestamp = true;
    }
    return MYLITE_OK;
}

static void capture_current_utc_timestamp(struct mylite_statement_timestamp *out_timestamp) {
    time_t seconds = time(NULL);
    long microseconds = 0;

#ifdef TIME_UTC
    struct timespec now;

    if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
        seconds = now.tv_sec;
        microseconds = now.tv_nsec / mylite_temporal_nanoseconds_per_microsecond;
    }
#endif

    *out_timestamp = (struct mylite_statement_timestamp){
        .seconds = seconds,
        .microseconds = microseconds,
    };
}

static int set_current_temporal_datetime_value(
    mylite_stmt *stmt,
    unsigned int fsp,
    struct mylite_expression_value *out_value
) {
    int status = set_current_temporal_value(
        stmt,
        "%Y-%m-%d %H:%M:%S",
        (size_t)mylite_mysql_datetime_display_length,
        fsp,
        out_value
    );

    if (status == MYLITE_OK) {
        out_value->temporal_type = MYLITE_EXPRESSION_TEMPORAL_DATETIME;
    }
    return status;
}

static int set_current_temporal_date_value(
    mylite_stmt *stmt,
    struct mylite_expression_value *out_value
) {
    int status = set_current_temporal_value(
        stmt,
        "%Y-%m-%d",
        (size_t)mylite_mysql_date_display_length,
        0U,
        out_value
    );

    if (status == MYLITE_OK) {
        out_value->temporal_type = MYLITE_EXPRESSION_TEMPORAL_DATE;
    }
    return status;
}

static int set_current_temporal_time_value(
    mylite_stmt *stmt,
    unsigned int fsp,
    struct mylite_expression_value *out_value
) {
    int status = set_current_temporal_value(
        stmt,
        "%H:%M:%S",
        (size_t)mylite_mysql_current_time_display_length,
        fsp,
        out_value
    );

    if (status == MYLITE_OK) {
        out_value->temporal_type = MYLITE_EXPRESSION_TEMPORAL_TIME;
    }
    return status;
}

static int set_current_temporal_value(
    mylite_stmt *stmt,
    const char *format,
    size_t base_length,
    unsigned int fsp,
    struct mylite_expression_value *out_value
) {
    enum { microsecond_text_length = 6U };

    char microsecond_text[microsecond_text_length + 1U];
    struct tm tm_value;
    size_t text_length = base_length + (fsp == 0U ? 0U : 1U + fsp);
    long microseconds = 0;
    char *text = NULL;
    int status = ensure_statement_timestamp(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    text = malloc(text_length + 1U);
    if (text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
#ifdef _WIN32
    if (gmtime_s(&tm_value, &stmt->statement_timestamp.seconds) != 0) {
        free(text);
        return -1;
    }
#else
    if (gmtime_r(&stmt->statement_timestamp.seconds, &tm_value) == NULL) {
        free(text);
        return -1;
    }
#endif
    if (strftime(text, base_length + 1U, format, &tm_value) != base_length) {
        free(text);
        return -1;
    }
    if (fsp != 0U) {
        microseconds = stmt->statement_timestamp.microseconds;
        if (microseconds < 0L) {
            microseconds = 0L;
        } else if (microseconds > mylite_temporal_max_microsecond) {
            microseconds = mylite_temporal_max_microsecond;
        }
        if (snprintf(microsecond_text, sizeof(microsecond_text), "%06ld", microseconds) !=
            microsecond_text_length) {
            free(text);
            return -1;
        }
        text[base_length] = '.';
        memcpy(text + base_length + 1U, microsecond_text, fsp);
        text[text_length] = '\0';
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .preserve_temporal_fraction_digits = true,
        .text_value = text,
        .text_length = text_length,
    };
    return MYLITE_OK;
}

static bool temporal_fsp_from_literal(
    const struct mylite_sql_ast_node *argument,
    unsigned int *out_fsp
) {
    enum { max_fsp = 6U, decimal_base = 10U };

    unsigned int value = 0U;

    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER || out_fsp == NULL) {
        return false;
    }
    for (size_t index = 0U; index < argument->span.length; ++index) {
        char character = argument->span.text[index];

        if (character < '0' || character > '9') {
            return false;
        }
        value = (value * decimal_base) + (unsigned int)(character - '0');
        if (value > max_fsp) {
            return false;
        }
    }
    *out_fsp = value;
    return true;
}

static bool temporal_function_name_matches_any(
    const struct mylite_sql_ast_node *name,
    const char *const *names,
    size_t name_count
) {
    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }
    for (size_t index = 0U; index < name_count; ++index) {
        if (mylite_span_equal_ci(name->span, names[index])) {
            return true;
        }
    }
    return false;
}
