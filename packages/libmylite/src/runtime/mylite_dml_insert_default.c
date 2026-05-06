#include "mylite_dml_insert_default.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int set_insert_bound_text_value(
    mylite_db *database,
    const char *text,
    struct mylite_insert_bound_value *out_value
);

static int reserve_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    uint64_t first_value
);

static bool insert_column_uses_numeric_implicit_default(
    const struct mylite_insert_table_column *column
);

static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column);

static char *insert_current_timestamp_text(void);

int mylite_dml_resolve_insert_default_bound_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (database == NULL || column == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (column->auto_increment && state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment) {
        return mylite_dml_allocate_insert_auto_increment(
            database,
            statement_row_count,
            state,
            out_value
        );
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
            return MYLITE_OK;
        }
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    if (mylite_column_default_is_current_timestamp(column->default_text)) {
        char *timestamp = insert_current_timestamp_text();

        if (timestamp == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    }
    if (column->generated_default) {
        return mylite_dml_insert_set_unsupported_generated_default_error(database, column->name);
    }
    return mylite_dml_resolve_insert_text_value(
        database,
        column,
        column->default_text,
        statement_row_count,
        state,
        out_value
    );
}

int mylite_dml_resolve_insert_implicit_expression_default(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value
) {
    const char *text_default = "";

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    if (insert_column_uses_numeric_implicit_default(column)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }
    if (column != NULL && column->data_type != NULL) {
        if (mylite_ascii_case_equal(column->data_type, "date")) {
            text_default = "0000-00-00";
        } else if (mylite_ascii_case_equal(column->data_type, "time")) {
            text_default = "00:00:00";
        } else if (
            mylite_ascii_case_equal(column->data_type, "datetime") ||
            mylite_ascii_case_equal(column->data_type, "timestamp")
        ) {
            text_default = "0000-00-00 00:00:00";
        }
    }

    out_value->text_value = mylite_copy_span_text(text_default, strlen(text_default));
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

int mylite_dml_resolve_insert_current_timestamp_bound_value(
    mylite_db *database,
    struct mylite_insert_bound_value *out_value
) {
    char *timestamp = NULL;

    if (database == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }

    timestamp = insert_current_timestamp_text();
    if (timestamp == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_TEXT,
        .text_value = timestamp,
    };
    return MYLITE_OK;
}

uint64_t mylite_dml_insert_auto_increment_next_value(
    const struct mylite_insert_execution_state *state
) {
    if (state == NULL) {
        return 0U;
    }
    if (state->reserved_auto_increment_end > state->next_auto_increment) {
        return state->reserved_auto_increment_end;
    }
    return state->next_auto_increment;
}

int mylite_dml_resolve_insert_explicit_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (plan->ignore && !column->auto_increment && !column->nullable &&
        column->default_text == NULL) {
        int status = mylite_dml_insert_append_no_default_warning(database, column->name);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(
        database,
        column,
        statement_row_count,
        state,
        out_value
    );
}

int mylite_dml_resolve_insert_omitted_default_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    size_t column_index,
    struct mylite_insert_bound_value *out_value
) {
    if (plan->ignore && !column->auto_increment && !column->nullable &&
        column->default_text == NULL) {
        int status =
            mylite_dml_insert_append_no_default_warning_once(database, column, state, column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_dml_resolve_insert_implicit_expression_default(database, column, out_value);
    }
    return mylite_dml_resolve_insert_default_bound_value(
        database,
        column,
        statement_row_count,
        state,
        out_value
    );
}

int mylite_dml_resolve_insert_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    int64_t integer_value = 0;
    double real_value = 0.0;

    if (text == NULL) {
        if (!column->nullable) {
            return mylite_dml_set_not_null_column_error(database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    if (mylite_dml_parse_insert_integer_text(text, &integer_value)) {
        if (integer_value == 0 &&
            mylite_dml_insert_auto_increment_zero_generates(database, column)) {
            return mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                out_value
            );
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment) {
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }
    if (mylite_dml_parse_insert_real_text(text, &real_value)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    }

    return set_insert_bound_text_value(database, text, out_value);
}

int mylite_dml_resolve_insert_quoted_text_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    const char *text,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    if (text == NULL || !insert_column_uses_text_storage(column)) {
        return mylite_dml_resolve_insert_text_value(
            database,
            column,
            text,
            statement_row_count,
            state,
            out_value
        );
    }
    return set_insert_bound_text_value(database, text, out_value);
}

bool mylite_dml_insert_auto_increment_zero_generates(
    const mylite_db *database,
    const struct mylite_insert_table_column *column
) {
    return (column != NULL && column->auto_increment &&
            !mylite_connection_sql_mode_has_no_auto_value_on_zero(database)) != 0;
}

int mylite_dml_allocate_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *out_value
) {
    uint64_t value = 0U;
    int status = MYLITE_OK;

    if (state == NULL) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }

    value = state->next_auto_increment == 0U ? 1U : state->next_auto_increment;
    if (value > (uint64_t)INT64_MAX) {
        (void)
            mylite_diagnostics_set_error_message(database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    status = reserve_insert_auto_increment(database, statement_row_count, state, value);
    if (status != MYLITE_OK) {
        return status;
    }
    state->next_auto_increment = value + 1U;
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = (int64_t)value,
        .generated_auto_increment = true,
    };
    return MYLITE_OK;
}

static int set_insert_bound_text_value(
    mylite_db *database,
    const char *text,
    struct mylite_insert_bound_value *out_value
) {
    out_value->text_value = mylite_copy_span_text(text, strlen(text));
    if (out_value->text_value == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

static int reserve_insert_auto_increment(
    mylite_db *database,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    uint64_t first_value
) {
    if (state->reserved_auto_increment_end != 0U) {
        return MYLITE_OK;
    }
    if (statement_row_count > (uint64_t)INT64_MAX - first_value) {
        (void)
            mylite_diagnostics_set_error_message(database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    state->reserved_auto_increment_end = first_value + statement_row_count;
    return MYLITE_OK;
}

static bool insert_column_uses_numeric_implicit_default(
    const struct mylite_insert_table_column *column
) {
    static const char *const numeric_types[] = {
        "tinyint",
        "smallint",
        "mediumint",
        "int",
        "bigint",
        "decimal",
        "float",
        "double",
        "bool",
        "boolean",
        "year",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(numeric_types) / sizeof(numeric_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, numeric_types[index])) {
            return true;
        }
    }
    return false;
}

static bool insert_column_uses_text_storage(const struct mylite_insert_table_column *column) {
    static const char *const text_types[] = {
        "char",
        "varchar",
        "tinytext",
        "text",
        "mediumtext",
        "longtext",
        "binary",
        "varbinary",
        "tinyblob",
        "blob",
        "mediumblob",
        "longblob",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(text_types) / sizeof(text_types[0]); ++index) {
        if (mylite_ascii_case_equal(column->data_type, text_types[index])) {
            return true;
        }
    }
    return false;
}

static char *insert_current_timestamp_text(void) {
    enum { timestamp_length = 19U };

    time_t now = time(NULL);
    struct tm tm_value;
    char *timestamp = malloc(timestamp_length + 1U);

    if (timestamp == NULL) {
        return NULL;
    }
#ifdef _WIN32
    if (gmtime_s(&tm_value, &now) != 0) {
        free(timestamp);
        return NULL;
    }
#else
    if (gmtime_r(&now, &tm_value) == NULL) {
        free(timestamp);
        return NULL;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        return NULL;
    }
    return timestamp;
}
