#include "mylite_select_aggregate.h"

#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_span.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int append_aggregate_numeric_conversion_warning(struct mylite_expression_warnings *warnings,
                                                       const char *text);

int mylite_select_aggregate_value_to_double(struct mylite_expression_warnings *warnings,
                                            const struct mylite_expression_value *value,
                                            struct mylite_aggregate_numeric_value *out_value)
{
    char *copy = NULL;
    char *start = NULL;
    char *end = NULL;

    *out_value = (struct mylite_aggregate_numeric_value){0};
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        out_value->value = (double)value->int64_value;
        out_value->integral = true;
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        out_value->value = (double)value->uint64_value;
        out_value->integral = true;
        out_value->unsigned_value = true;
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        out_value->value = value->real_value;
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }

    copy = mylite_copy_span_text(value->text_value == NULL ? "" : value->text_value,
                                 value->text_value == NULL ? 0U : value->text_length);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    start = copy;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    out_value->value = strtod(start, &end);
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        int status = append_aggregate_numeric_conversion_warning(warnings, copy);

        if (end == start) {
            out_value->value = 0.0;
        }
        free(copy);
        return status;
    }
    free(copy);
    return MYLITE_OK;
}

int mylite_select_aggregate_format_double(double value, struct mylite_expression_value *out_value)
{
    enum { double_buffer_size = 64 };
    char buffer[double_buffer_size];
    int length = snprintf(buffer, sizeof(buffer), "%.16g", value);

    if (length < 0) {
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = mylite_copy_span_text(buffer, (size_t)length);
    out_value->text_length = (size_t)length;
    return out_value->text_value == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int append_aggregate_numeric_conversion_warning(struct mylite_expression_warnings *warnings,
                                                       const char *text)
{
    enum {
        message_size = 256,
        excerpt_length = 128,
    };
    char message[message_size];
    int length = snprintf(message, sizeof(message), "Truncated incorrect DOUBLE value: '%.*s'",
                          excerpt_length, text == NULL ? "" : text);

    if (length < 0) {
        return MYLITE_NOMEM;
    }
    return mylite_expression_warnings_append(warnings, MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE,
                                             message) == 0
               ? MYLITE_OK
               : MYLITE_NOMEM;
}
