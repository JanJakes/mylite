#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_binary_literal.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"
#include "mylite_uint64_text.h"
#include "sql/mylite_expression.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool update_values_equal(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
);

static int resolve_update_implicit_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
);

static bool column_is_explicitly_assigned(
    size_t column_index,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count
);

static int apply_update_on_update_current_timestamps_to_changed_row(
    mylite_db *database,
    const struct mylite_insert_table *write_table,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    struct mylite_update_row *candidate,
    struct mylite_dml_timestamp_state *timestamp_state
);

static int resolve_on_update_current_timestamp_expression(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    struct mylite_expression_value *out_value
);

static int resolve_on_update_current_timestamp_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    struct mylite_insert_bound_value *out_value
);

static int copy_on_update_current_timestamp_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    char **out_text
);

static int ensure_dml_timestamp_state(struct mylite_dml_timestamp_state *timestamp_state);

static unsigned int on_update_current_timestamp_fsp(
    const struct mylite_insert_table_column *column
);

static bool parse_on_update_column_type_fsp(const char *column_type, unsigned int *out_fsp);

int mylite_dml_copy_update_candidate_values(
    mylite_db *database,
    const struct mylite_update_row *row,
    struct mylite_update_row *candidate
) {
    if (database == NULL || row == NULL || candidate == NULL) {
        return MYLITE_MISUSE;
    }

    candidate->rowid = row->rowid;
    candidate->values = calloc(row->value_count, sizeof(*candidate->values));
    if (candidate->values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    candidate->value_count = row->value_count;

    for (size_t index = 0U; index < row->value_count; ++index) {
        if (mylite_expression_value_copy(&row->values[index], &candidate->values[index]) != 0) {
            mylite_dml_update_row_deinit(candidate);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_apply_update_on_update_current_timestamps(
    mylite_db *database,
    const struct mylite_insert_table *write_table,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    const struct mylite_update_row *stored,
    struct mylite_update_row *candidate,
    struct mylite_dml_timestamp_state *timestamp_state,
    bool *out_row_changed
) {
    bool row_changed = false;

    if (database == NULL || write_table == NULL || stored == NULL || candidate == NULL ||
        timestamp_state == NULL || out_row_changed == NULL) {
        return MYLITE_MISUSE;
    }

    row_changed = mylite_dml_update_row_changed(stored, candidate);
    *out_row_changed = row_changed;
    if (!row_changed) {
        return MYLITE_OK;
    }

    return apply_update_on_update_current_timestamps_to_changed_row(
        database,
        write_table,
        explicit_column_indexes,
        explicit_column_count,
        candidate,
        timestamp_state
    );
}

int mylite_dml_apply_update_on_update_current_timestamps_for_changed_row(
    mylite_db *database,
    const struct mylite_insert_table *write_table,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    struct mylite_update_row *candidate,
    struct mylite_dml_timestamp_state *timestamp_state
) {
    if (database == NULL || write_table == NULL || candidate == NULL || timestamp_state == NULL) {
        return MYLITE_MISUSE;
    }

    return apply_update_on_update_current_timestamps_to_changed_row(
        database,
        write_table,
        explicit_column_indexes,
        explicit_column_count,
        candidate,
        timestamp_state
    );
}

static int apply_update_on_update_current_timestamps_to_changed_row(
    mylite_db *database,
    const struct mylite_insert_table *write_table,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    struct mylite_update_row *candidate,
    struct mylite_dml_timestamp_state *timestamp_state
) {
    for (size_t index = 0U; index < write_table->column_count; ++index) {
        const struct mylite_insert_table_column *column = &write_table->columns[index];
        struct mylite_expression_value value = {0};
        int status = MYLITE_OK;

        if (!column->on_update_current_timestamp ||
            column_is_explicitly_assigned(index, explicit_column_indexes, explicit_column_count)) {
            continue;
        }
        if (candidate->values == NULL || index >= candidate->value_count) {
            return MYLITE_MISUSE;
        }

        status = resolve_on_update_current_timestamp_expression(
            database,
            column,
            timestamp_state,
            &value
        );
        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&value);
            return status;
        }
        mylite_expression_value_deinit(&candidate->values[index]);
        candidate->values[index] = value;
    }
    return MYLITE_OK;
}

int mylite_dml_apply_insert_on_update_current_timestamps(
    mylite_db *database,
    const struct mylite_insert_table *table,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count,
    const struct mylite_insert_bound_value *stored,
    struct mylite_insert_bound_value *candidate,
    struct mylite_dml_timestamp_state *timestamp_state,
    bool *out_row_changed
) {
    bool row_changed = false;

    if (database == NULL || table == NULL || stored == NULL || candidate == NULL ||
        timestamp_state == NULL || out_row_changed == NULL) {
        return MYLITE_MISUSE;
    }

    row_changed = mylite_dml_insert_update_row_changed(stored, candidate, table->column_count);
    *out_row_changed = row_changed;
    if (!row_changed) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < table->column_count; ++index) {
        const struct mylite_insert_table_column *column = &table->columns[index];
        struct mylite_insert_bound_value value = {0};
        int status = MYLITE_OK;

        if (!column->on_update_current_timestamp ||
            column_is_explicitly_assigned(index, explicit_column_indexes, explicit_column_count)) {
            continue;
        }

        status = resolve_on_update_current_timestamp_bound_value(
            database,
            column,
            timestamp_state,
            &value
        );
        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_value_deinit(&value);
            return status;
        }
        mylite_dml_insert_bound_value_deinit(&candidate[index]);
        candidate[index] = value;
    }
    return MYLITE_OK;
}

int mylite_dml_resolve_update_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_bound_value value = {0};
    int status = MYLITE_OK;

    if (!mylite_connection_sql_mode_is_strict(database) && column != NULL && !column->has_default) {
        status = mylite_dml_insert_append_no_default_warning(database, column->name);
        if (status == MYLITE_OK) {
            status = resolve_update_implicit_default_value(database, column, out_value);
        }
        return status;
    }

    status = mylite_dml_resolve_insert_default_bound_value(database, column, 0U, NULL, &value);

    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_bound_value_to_expression(&value, out_value);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    mylite_dml_insert_bound_value_deinit(&value);
    return status;
}

int mylite_dml_resolve_update_binary_literal_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const struct mylite_sql_ast_node *expression,
    bool ignore,
    struct mylite_expression_value *out_value
) {
    enum mylite_dml_binary_literal_kind literal_kind =
        mylite_dml_binary_literal_kind_for_ast(expression);
    struct mylite_insert_value value = {0};
    struct mylite_insert_bound_value bound = {0};
    int status = MYLITE_OK;

    if (database == NULL || column == NULL || expression == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (literal_kind == MYLITE_DML_BINARY_LITERAL_NONE) {
        return mylite_dml_set_update_unsupported_assignment_error(database);
    }
    value = (struct mylite_insert_value){
        .kind = literal_kind == MYLITE_DML_BINARY_LITERAL_HEX ? MYLITE_INSERT_VALUE_HEX_LITERAL
                                                              : MYLITE_INSERT_VALUE_BIT_LITERAL,
        .text = (char *)expression->span.text,
        .text_length = expression->span.length,
    };
    status = mylite_dml_resolve_insert_binary_literal_value(
        database,
        column,
        &value,
        1U,
        1U,
        NULL,
        ignore,
        &bound
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_bound_value_to_expression(&bound, out_value);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    mylite_dml_insert_bound_value_deinit(&bound);
    return status;
}

int mylite_dml_resolve_default_function_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_bound_value value = {0};
    int status = MYLITE_OK;

    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (!column->has_default) {
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return MYLITE_OK;
        }
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    if (mylite_column_default_is_current_timestamp(column->default_text)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    if (column->generated_default) {
        return mylite_dml_insert_set_default_function_generated_error(database);
    }

    status = mylite_dml_resolve_insert_text_value(
        database,
        column,
        column->default_text,
        column->default_text == NULL ? 0U : strlen(column->default_text),
        0U,
        1U,
        NULL,
        false,
        &value
    );
    if (status == MYLITE_OK) {
        status = mylite_dml_copy_insert_bound_value_to_expression(&value, out_value);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    mylite_dml_insert_bound_value_deinit(&value);
    return status;
}

int mylite_dml_copy_insert_bound_value_to_expression(
    const struct mylite_insert_bound_value *value,
    struct mylite_expression_value *out_value
) {
    if (value == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = value->integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_REAL:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = value->real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_TEXT:
    case MYLITE_INSERT_BOUND_BLOB:
        out_value->text_length = value->text_value == NULL ? 0U : value->text_length;
        out_value->text_value = mylite_copy_span_text(value->text_value, out_value->text_length);
        if (out_value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_dml_validate_update_assignment_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    bool ignore,
    struct mylite_expression_value *value
) {
    int64_t integer_value = 0;

    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        if (column->nullable) {
            return MYLITE_OK;
        }
        if (ignore || !mylite_connection_sql_mode_is_strict(database)) {
            int status = mylite_dml_insert_append_null_warning(database, column->name);

            if (status != MYLITE_OK) {
                return status;
            }
            return resolve_update_implicit_default_value(database, column, value);
        }
        return mylite_dml_set_not_null_column_error(database, column->name);
    }
    int status = mylite_dml_coerce_update_temporal_value(database, column, 1U, ignore, value);

    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_dml_coerce_update_numeric_value(database, column, 1U, ignore, value);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_dml_coerce_update_string_value(database, column, 1U, ignore, value);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!column->auto_increment) {
        return MYLITE_OK;
    }

    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64 &&
        value->uint64_value <= (uint64_t)INT64_MAX) {
        value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        value->int64_value = (int64_t)value->uint64_value;
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
        mylite_dml_parse_insert_integer_text(value->text_value, &integer_value)) {
        mylite_expression_value_deinit(value);
        *value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = integer_value,
        };
        return MYLITE_OK;
    }
    return mylite_dml_set_update_unsupported_assignment_error(database);
}

int mylite_dml_advance_update_auto_increment(
    mylite_db *database,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_row *candidate,
    uint64_t *next_auto_increment
) {
    uint64_t value = 0U;

    if (!write_table->has_auto_increment ||
        !mylite_dml_update_expression_value_positive_uint64(
            &candidate->values[write_table->auto_increment_column_index],
            &value
        )) {
        return MYLITE_OK;
    }
    if (value == UINT64_MAX) {
        (void)
            mylite_diagnostics_set_error_message(database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    if (value >= *next_auto_increment) {
        *next_auto_increment = value + 1U;
    }
    return MYLITE_OK;
}

bool mylite_dml_update_expression_value_positive_uint64(
    const struct mylite_expression_value *value,
    uint64_t *out_value
) {
    if (value == NULL || out_value == NULL) {
        return false;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64 && value->int64_value > 0) {
        *out_value = (uint64_t)value->int64_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64 && value->uint64_value > 0U) {
        *out_value = value->uint64_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
        mylite_parse_uint64_text(value->text_value, value->text_length, out_value) &&
        *out_value > 0U) {
        return true;
    }
    return false;
}

static int resolve_update_implicit_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_bound_value value = {0};
    int status = mylite_dml_resolve_insert_implicit_expression_default(database, column, &value);

    if (status == MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        status = mylite_dml_copy_insert_bound_value_to_expression(&value, out_value);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
    }
    mylite_dml_insert_bound_value_deinit(&value);
    return status;
}

static bool column_is_explicitly_assigned(
    size_t column_index,
    const size_t *explicit_column_indexes,
    size_t explicit_column_count
) {
    for (size_t index = 0U; index < explicit_column_count; ++index) {
        if (explicit_column_indexes[index] == column_index) {
            return true;
        }
    }
    return false;
}

static int resolve_on_update_current_timestamp_expression(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    struct mylite_expression_value *out_value
) {
    char *timestamp = NULL;
    int status =
        copy_on_update_current_timestamp_text(database, column, timestamp_state, &timestamp);

    if (status != MYLITE_OK) {
        return status;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_TEXT,
        .text_value = timestamp,
        .text_length = strlen(timestamp),
    };
    return MYLITE_OK;
}

static int resolve_on_update_current_timestamp_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    struct mylite_insert_bound_value *out_value
) {
    char *timestamp = NULL;
    int status =
        copy_on_update_current_timestamp_text(database, column, timestamp_state, &timestamp);

    if (status != MYLITE_OK) {
        return status;
    }
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = timestamp,
        .text_length = strlen(timestamp),
    };
    return MYLITE_OK;
}

static int copy_on_update_current_timestamp_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_timestamp_state *timestamp_state,
    char **out_text
) {
    enum {
        timestamp_length = 19U,
        microsecond_length = 6U,
    };

    unsigned int fsp = on_update_current_timestamp_fsp(column);
    size_t text_length = timestamp_length + (fsp == 0U ? 0U : 1U + fsp);
    char *timestamp = NULL;
    time_t seconds = 0;
    struct tm tm_value;
    int status = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;

    status = ensure_dml_timestamp_state(timestamp_state);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    timestamp = malloc(text_length + 1U);
    if (timestamp == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    seconds = (time_t)timestamp_state->seconds;
#ifdef _WIN32
    if (gmtime_s(&tm_value, &seconds) != 0) {
        free(timestamp);
        (void)mylite_diagnostics_set_error_message(database, "Could not format timestamp value");
        return MYLITE_EXEC_ERROR;
    }
#else
    if (gmtime_r(&seconds, &tm_value) == NULL) {
        free(timestamp);
        (void)mylite_diagnostics_set_error_message(database, "Could not format timestamp value");
        return MYLITE_EXEC_ERROR;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        (void)mylite_diagnostics_set_error_message(database, "Could not format timestamp value");
        return MYLITE_EXEC_ERROR;
    }
    if (fsp > 0U) {
        char microsecond_text[microsecond_length + 1U];

        if (snprintf(
                microsecond_text,
                sizeof(microsecond_text),
                "%06ld",
                timestamp_state->microseconds
            ) != microsecond_length) {
            free(timestamp);
            (void)
                mylite_diagnostics_set_error_message(database, "Could not format timestamp value");
            return MYLITE_EXEC_ERROR;
        }
        timestamp[timestamp_length] = '.';
        memcpy(timestamp + timestamp_length + 1U, microsecond_text, fsp);
        timestamp[text_length] = '\0';
    }

    *out_text = timestamp;
    return MYLITE_OK;
}

static int ensure_dml_timestamp_state(struct mylite_dml_timestamp_state *timestamp_state) {
    time_t now = time(NULL);
    long microseconds = 0;

    if (timestamp_state == NULL) {
        return MYLITE_MISUSE;
    }
    if (timestamp_state->initialized) {
        return MYLITE_OK;
    }
#ifdef TIME_UTC
    {
        struct timespec timespec_now;

        if (timespec_get(&timespec_now, TIME_UTC) == TIME_UTC) {
            now = timespec_now.tv_sec;
            microseconds = timespec_now.tv_nsec / 1000L;
        }
    }
#endif
    timestamp_state->seconds = (int64_t)now;
    timestamp_state->microseconds = microseconds;
    timestamp_state->initialized = true;
    return MYLITE_OK;
}

static unsigned int on_update_current_timestamp_fsp(
    const struct mylite_insert_table_column *column
) {
    unsigned int fsp = 0U;

    if (column == NULL || (!mylite_ascii_case_equal(column->data_type, "datetime") &&
                           !mylite_ascii_case_equal(column->data_type, "timestamp"))) {
        return 0U;
    }
    (void)parse_on_update_column_type_fsp(column->column_type, &fsp);
    return fsp > 6U ? 6U : fsp;
}

static bool parse_on_update_column_type_fsp(const char *column_type, unsigned int *out_fsp) {
    const char *open = column_type == NULL ? NULL : strchr(column_type, '(');
    const char *close = open == NULL ? NULL : strchr(open, ')');
    unsigned int fsp = 0U;

    if (out_fsp != NULL) {
        *out_fsp = 0U;
    }
    if (open == NULL || close == NULL || close <= open + 1) {
        return false;
    }
    for (const char *cursor = open + 1; cursor < close; ++cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return false;
        }
        fsp = (fsp * 10U) + (unsigned int)(*cursor - '0');
    }
    if (out_fsp != NULL) {
        *out_fsp = fsp;
    }
    return true;
}

bool mylite_dml_update_row_changed(
    const struct mylite_update_row *stored,
    const struct mylite_update_row *candidate
) {
    if (stored == NULL || candidate == NULL || stored->value_count != candidate->value_count) {
        return true;
    }
    for (size_t index = 0U; index < stored->value_count; ++index) {
        if (!update_values_equal(&stored->values[index], &candidate->values[index])) {
            return true;
        }
    }
    return false;
}

static bool update_values_equal(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
) {
    if (left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return true;
    case MYLITE_EXPRESSION_VALUE_INT64:
        return left->int64_value == right->int64_value;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return left->uint64_value == right->uint64_value;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return left->real_value == right->real_value;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        if (left->text_value == NULL || right->text_value == NULL) {
            return left->text_value == right->text_value;
        }
        return (left->text_length == right->text_length &&
                memcmp(left->text_value, right->text_value, left->text_length) == 0) != 0;
    }
    return false;
}
