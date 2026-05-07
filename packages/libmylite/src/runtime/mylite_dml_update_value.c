#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool update_values_equal(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
);

static int resolve_update_implicit_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
);

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

int mylite_dml_resolve_update_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_expression_value *out_value
) {
    struct mylite_insert_bound_value value = {0};
    int status = MYLITE_OK;

    if (!mylite_connection_sql_mode_is_strict(database) && column != NULL &&
        !column->auto_increment && !column->nullable && column->default_text == NULL) {
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
        NULL,
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
