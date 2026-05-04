#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool update_values_equal(const struct mylite_expression_value *left,
                                const struct mylite_expression_value *right);

int mylite_dml_copy_update_candidate_values(mylite_db *database,
                                            const struct mylite_update_row *row,
                                            struct mylite_update_row *candidate)
{
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

int mylite_dml_copy_insert_bound_value_to_expression(const struct mylite_insert_bound_value *value,
                                                     struct mylite_expression_value *out_value)
{
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
        out_value->text_length = value->text_value == NULL ? 0U : strlen(value->text_value);
        out_value->text_value = mylite_copy_span_text(value->text_value, out_value->text_length);
        if (out_value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

bool mylite_dml_update_expression_value_positive_uint64(const struct mylite_expression_value *value,
                                                        uint64_t *out_value)
{
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

bool mylite_dml_update_row_changed(const struct mylite_update_row *stored,
                                   const struct mylite_update_row *candidate)
{
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

static bool update_values_equal(const struct mylite_expression_value *left,
                                const struct mylite_expression_value *right)
{
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
