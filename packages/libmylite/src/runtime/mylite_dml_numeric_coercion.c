#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mylite_dml_numeric_kind {
    MYLITE_DML_NUMERIC_NONE = 0,
    MYLITE_DML_NUMERIC_SIGNED_INTEGER,
    MYLITE_DML_NUMERIC_UNSIGNED_INTEGER,
    MYLITE_DML_NUMERIC_DECIMAL,
    MYLITE_DML_NUMERIC_FLOAT,
    MYLITE_DML_NUMERIC_DOUBLE,
};

enum mylite_dml_numeric_problem {
    MYLITE_DML_NUMERIC_PROBLEM_NONE = 0,
    MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED,
    MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED,
    MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER,
    MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL,
    MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE,
};

enum {
    mylite_dml_decimal_base = 10,
    mylite_dml_uint64_text_buffer_size = 32,
    mylite_dml_mediumint_signed_bits = 23,
    mylite_dml_mediumint_unsigned_bits = 24,
};

static const uint64_t mylite_dml_int64_min_magnitude = (uint64_t)INT64_MAX + UINT64_C(1);

struct mylite_dml_numeric_text_parse {
    double value;
    bool saw_number;
    bool range_error;
    bool trailing_garbage;
    bool allocation_failed;
};

struct mylite_dml_unsigned_integer_text_parse {
    uint64_t value;
    bool saw_digits;
    bool overflow;
    bool trailing_garbage;
};

struct mylite_dml_signed_integer_text_parse {
    uint64_t magnitude;
    bool negative;
    bool saw_digits;
    bool overflow;
    bool trailing_garbage;
};

struct mylite_dml_numeric_output {
    enum mylite_insert_bound_value_kind insert_kind;
    enum mylite_expression_value_kind expression_kind;
    int64_t integer_value;
    double real_value;
    char *text_value;
    size_t text_length;
    bool replace;
};

struct mylite_dml_integer_uint64_input {
    enum mylite_dml_numeric_kind kind;
    uint64_t value;
    bool overflow;
    enum mylite_dml_numeric_problem problem;
    uint64_t row_number;
    bool ignore;
};

struct mylite_dml_negative_integer_input {
    enum mylite_dml_numeric_kind kind;
    uint64_t magnitude;
    bool overflow;
    enum mylite_dml_numeric_problem problem;
    uint64_t row_number;
    bool ignore;
};

struct mylite_dml_integer_range {
    int64_t minimum;
    int64_t maximum;
    bool has_maximum;
    bool available;
};

static int coerce_insert_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
);

static int coerce_update_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
);

static enum mylite_dml_numeric_kind numeric_kind_for_column(
    const struct mylite_insert_table_column *column
);

static bool column_type_is_unsigned(const struct mylite_insert_table_column *column);

static bool column_data_type_is_signed_integer(const char *data_type);

static struct mylite_dml_integer_range integer_range_for_column(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind
);

static enum mylite_dml_numeric_problem integer_problem_for_range(
    bool out_of_range,
    enum mylite_dml_numeric_problem problem
);

static struct mylite_dml_integer_range signed_integer_range_for_type(const char *data_type);

static struct mylite_dml_integer_range unsigned_integer_range_for_type(const char *data_type);

static struct mylite_dml_integer_range integer_range_with_bounds(int64_t minimum, int64_t maximum);

static struct mylite_dml_integer_range integer_range_with_minimum(int64_t minimum);

static bool integer_range_exceeded(struct mylite_dml_integer_range range, int64_t value);

static int64_t clamp_integer_to_range(struct mylite_dml_integer_range range, int64_t value);

static int coerce_integer_uint64(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_integer_uint64_input input,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_integer_negative_magnitude(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_negative_integer_input input,
    struct mylite_dml_numeric_output *out_output
);

static bool integer_uint64_maximum_for_column(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t *out_maximum
);

static bool signed_integer_uint64_maximum_for_type(const char *data_type, uint64_t *out_maximum);

static bool unsigned_integer_uint64_maximum_for_type(const char *data_type, uint64_t *out_maximum);

static int set_integer_uint64_output(uint64_t value, struct mylite_dml_numeric_output *out_output);

static bool integer_negative_magnitude_limit_for_range(
    struct mylite_dml_integer_range range,
    uint64_t *out_magnitude,
    int64_t *out_clipped_value
);

static int coerce_numeric_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_numeric_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_negative_integer_text_if_needed(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    struct mylite_dml_signed_integer_text_parse signed_integer,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output,
    bool *out_handled
);

static struct mylite_dml_numeric_text_parse parse_numeric_text(const char *text, size_t length);

static struct mylite_dml_unsigned_integer_text_parse parse_unsigned_integer_text_prefix(
    const char *text,
    size_t length
);

static struct mylite_dml_signed_integer_text_parse parse_signed_integer_text_prefix(
    const char *text,
    size_t length
);

static int coerce_integer_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_decimal_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_approximate_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
);

static int handle_numeric_problem(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore
);

static int set_numeric_problem_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
);

static int append_numeric_problem_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
);

static int append_decimal_scale_note(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
);

static int make_numeric_problem_message(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    char **out_message
);

static unsigned int numeric_problem_code(enum mylite_dml_numeric_problem problem);

static int64_t round_half_away_to_int64(double value);

static double round_decimal_to_scale(double value, uint64_t scale);

static double decimal_scale_factor(uint64_t scale);

static uint64_t decimal_scale_for_column(const struct mylite_insert_table_column *column);

static int set_decimal_output(
    double value,
    uint64_t scale,
    struct mylite_dml_numeric_output *out_output
);

static int set_approximate_output(double value, struct mylite_dml_numeric_output *out_output);

static int replace_insert_numeric_value(
    const struct mylite_dml_numeric_output *output,
    struct mylite_insert_bound_value *value
);

static int replace_update_numeric_value(
    const struct mylite_dml_numeric_output *output,
    struct mylite_expression_value *value
);

static void numeric_output_deinit(struct mylite_dml_numeric_output *output);

int mylite_dml_coerce_insert_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
) {
    enum mylite_dml_numeric_kind kind = numeric_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_NUMERIC_NONE || value->kind == MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    return coerce_insert_numeric_value(database, column, kind, row_number, ignore, value);
}

int mylite_dml_coerce_update_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
) {
    enum mylite_dml_numeric_kind kind = numeric_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_NUMERIC_NONE || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    return coerce_update_numeric_value(database, column, kind, row_number, ignore, value);
}

static int coerce_insert_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
) {
    struct mylite_dml_numeric_output output = {0};
    int status = MYLITE_OK;

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_INTEGER:
        status = coerce_numeric_double(
            database,
            column,
            kind,
            (double)value->integer_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_INSERT_BOUND_REAL:
        status = coerce_numeric_double(
            database,
            column,
            kind,
            value->real_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_INSERT_BOUND_TEXT:
        status = coerce_numeric_text(
            database,
            column,
            kind,
            value->text_value,
            value->text_length,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_INSERT_BOUND_BLOB:
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_NULL:
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && output.replace) {
        status = replace_insert_numeric_value(&output, value);
    }
    numeric_output_deinit(&output);
    return status;
}

static int coerce_update_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
) {
    struct mylite_dml_numeric_output output = {0};
    int status = MYLITE_OK;

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        status = coerce_numeric_double(
            database,
            column,
            kind,
            (double)value->int64_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (kind != MYLITE_DML_NUMERIC_DECIMAL) {
            status = coerce_integer_uint64(
                database,
                column,
                (struct mylite_dml_integer_uint64_input){
                    .kind = kind,
                    .value = value->uint64_value,
                    .overflow = false,
                    .problem = MYLITE_DML_NUMERIC_PROBLEM_NONE,
                    .row_number = row_number,
                    .ignore = ignore,
                },
                &output
            );
            break;
        }
        status = coerce_numeric_double(
            database,
            column,
            kind,
            value->uint64_value > (uint64_t)INT64_MAX ? (double)INT64_MAX
                                                      : (double)value->uint64_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        status = coerce_numeric_double(
            database,
            column,
            kind,
            value->real_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        status = coerce_numeric_text(
            database,
            column,
            kind,
            value->text_value,
            value->text_length,
            row_number,
            ignore,
            &output
        );
        break;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && output.replace) {
        status = replace_update_numeric_value(&output, value);
    }
    numeric_output_deinit(&output);
    return status;
}

static enum mylite_dml_numeric_kind numeric_kind_for_column(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || column->data_type == NULL) {
        return MYLITE_DML_NUMERIC_NONE;
    }
    if (mylite_ascii_case_equal(column->data_type, "decimal")) {
        return MYLITE_DML_NUMERIC_DECIMAL;
    }
    if (mylite_ascii_case_equal(column->data_type, "float")) {
        return MYLITE_DML_NUMERIC_FLOAT;
    }
    if (mylite_ascii_case_equal(column->data_type, "double")) {
        return MYLITE_DML_NUMERIC_DOUBLE;
    }
    if (column_data_type_is_signed_integer(column->data_type)) {
        return column_type_is_unsigned(column) ? MYLITE_DML_NUMERIC_UNSIGNED_INTEGER
                                               : MYLITE_DML_NUMERIC_SIGNED_INTEGER;
    }
    return MYLITE_DML_NUMERIC_NONE;
}

static bool column_type_is_unsigned(const struct mylite_insert_table_column *column) {
    return column != NULL && column->column_type != NULL &&
           mylite_text_contains_word(column->column_type, "unsigned");
}

static bool column_data_type_is_signed_integer(const char *data_type) {
    static const char *const integer_types[] = {
        "tinyint",
        "smallint",
        "mediumint",
        "int",
        "bigint",
        "bool",
        "boolean",
        "year",
    };

    if (data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(integer_types) / sizeof(integer_types[0]); ++index) {
        if (mylite_ascii_case_equal(data_type, integer_types[index])) {
            return true;
        }
    }
    return false;
}

static int coerce_numeric_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
) {
    if (kind == MYLITE_DML_NUMERIC_DECIMAL) {
        return coerce_decimal_double(
            database,
            column,
            value,
            problem,
            row_number,
            ignore,
            out_output
        );
    }
    if (kind == MYLITE_DML_NUMERIC_FLOAT || kind == MYLITE_DML_NUMERIC_DOUBLE) {
        return coerce_approximate_double(
            database,
            column,
            kind,
            value,
            problem,
            row_number,
            ignore,
            out_output
        );
    }
    return coerce_integer_double(
        database,
        column,
        kind,
        value,
        problem,
        row_number,
        ignore,
        out_output
    );
}

static int coerce_numeric_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
) {
    struct mylite_dml_numeric_text_parse parsed = parse_numeric_text(text, text_length);
    struct mylite_dml_signed_integer_text_parse signed_integer =
        parse_signed_integer_text_prefix(text, text_length);
    struct mylite_dml_unsigned_integer_text_parse unsigned_integer =
        parse_unsigned_integer_text_prefix(text, text_length);
    enum mylite_dml_numeric_problem problem = MYLITE_DML_NUMERIC_PROBLEM_NONE;
    bool handled_negative_integer = false;
    int status = MYLITE_OK;

    if (parsed.allocation_failed) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (kind == MYLITE_DML_NUMERIC_FLOAT || kind == MYLITE_DML_NUMERIC_DOUBLE) {
        bool approximate_out_of_range = false;

        if (!parsed.saw_number || parsed.trailing_garbage) {
            problem = MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED;
        }
        if (!parsed.saw_number) {
            parsed.value = 0.0;
        } else if (parsed.range_error) {
            problem = MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE;
        }
        approximate_out_of_range =
            parsed.saw_number && (isinf(parsed.value) || (kind == MYLITE_DML_NUMERIC_FLOAT &&
                                                          fabs(parsed.value) > (double)FLT_MAX));
        if (problem == MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED && approximate_out_of_range) {
            status = handle_numeric_problem(database, column, problem, row_number, ignore);
            if (status != MYLITE_OK) {
                return status;
            }
            problem = MYLITE_DML_NUMERIC_PROBLEM_NONE;
        }
        return coerce_numeric_double(
            database,
            column,
            kind,
            parsed.value,
            problem,
            row_number,
            ignore,
            out_output
        );
    }
    if (!parsed.saw_number) {
        problem = kind == MYLITE_DML_NUMERIC_DECIMAL ? MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL
                                                     : MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER;
    } else if (parsed.trailing_garbage) {
        problem = kind == MYLITE_DML_NUMERIC_DECIMAL ? MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED
                                                     : MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED;
    }
    status = coerce_negative_integer_text_if_needed(
        database,
        column,
        kind,
        signed_integer,
        row_number,
        ignore,
        out_output,
        &handled_negative_integer
    );
    if (status != MYLITE_OK || handled_negative_integer) {
        return status;
    }
    if (kind != MYLITE_DML_NUMERIC_DECIMAL && unsigned_integer.saw_digits) {
        bool value_exceeds_int64 = unsigned_integer.value > (uint64_t)INT64_MAX;

        if (unsigned_integer.overflow || value_exceeds_int64) {
            enum mylite_dml_numeric_problem integer_problem = MYLITE_DML_NUMERIC_PROBLEM_NONE;

            if (unsigned_integer.trailing_garbage) {
                integer_problem = MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED;
            }

            return coerce_integer_uint64(
                database,
                column,
                (struct mylite_dml_integer_uint64_input){
                    .kind = kind,
                    .value = unsigned_integer.value,
                    .overflow = unsigned_integer.overflow,
                    .problem = integer_problem,
                    .row_number = row_number,
                    .ignore = ignore,
                },
                out_output
            );
        }
    }
    return coerce_numeric_double(
        database,
        column,
        kind,
        parsed.value,
        problem,
        row_number,
        ignore,
        out_output
    );
}

static int coerce_negative_integer_text_if_needed(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    struct mylite_dml_signed_integer_text_parse signed_integer,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output,
    bool *out_handled
) {
    bool magnitude_exceeds_int64 = signed_integer.overflow;
    enum mylite_dml_numeric_problem problem = MYLITE_DML_NUMERIC_PROBLEM_NONE;

    *out_handled = false;
    if (kind == MYLITE_DML_NUMERIC_DECIMAL || !signed_integer.negative ||
        !signed_integer.saw_digits) {
        return MYLITE_OK;
    }
    if (!magnitude_exceeds_int64 && signed_integer.magnitude > (uint64_t)INT64_MAX) {
        magnitude_exceeds_int64 = true;
    }
    if (!magnitude_exceeds_int64) {
        return MYLITE_OK;
    }
    if (signed_integer.trailing_garbage) {
        problem = MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED;
    }
    *out_handled = true;
    return coerce_integer_negative_magnitude(
        database,
        column,
        (struct mylite_dml_negative_integer_input){
            .kind = kind,
            .magnitude = signed_integer.magnitude,
            .overflow = signed_integer.overflow,
            .problem = problem,
            .row_number = row_number,
            .ignore = ignore,
        },
        out_output
    );
}

static struct mylite_dml_numeric_text_parse parse_numeric_text(const char *text, size_t length) {
    struct mylite_dml_numeric_text_parse parsed = {0};
    char *copy = mylite_copy_span_text(text == NULL ? "" : text, text == NULL ? 0U : length);
    char *copy_end = copy == NULL ? NULL : copy + (text == NULL ? 0U : length);
    char *start = NULL;
    char *end = NULL;
    int parse_errno = 0;

    if (copy == NULL) {
        parsed.allocation_failed = true;
        return parsed;
    }
    start = copy;
    while (start < copy_end && isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    parsed.value = strtod(start, &end);
    parse_errno = errno;
    parsed.saw_number = end != start;
    parsed.range_error = parsed.saw_number && parse_errno == ERANGE && isinf(parsed.value);
    while (end != NULL && end < copy_end && isspace((unsigned char)*end)) {
        ++end;
    }
    parsed.trailing_garbage = parsed.saw_number && end != NULL && end < copy_end;
    free(copy);
    return parsed;
}

static struct mylite_dml_unsigned_integer_text_parse parse_unsigned_integer_text_prefix(
    const char *text,
    size_t length
) {
    struct mylite_dml_unsigned_integer_text_parse parsed = {0};
    size_t offset = 0U;

    if (text == NULL) {
        return parsed;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < length && text[offset] == '+') {
        ++offset;
    } else if (offset < length && text[offset] == '-') {
        return parsed;
    }
    while (offset < length && isdigit((unsigned char)text[offset])) {
        uint64_t digit = (uint64_t)(text[offset] - '0');

        parsed.saw_digits = true;
        if (parsed.value > (UINT64_MAX - digit) / mylite_dml_decimal_base) {
            parsed.overflow = true;
            parsed.value = UINT64_MAX;
        } else if (!parsed.overflow) {
            parsed.value = (parsed.value * mylite_dml_decimal_base) + digit;
        }
        ++offset;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    parsed.trailing_garbage = (parsed.saw_digits && offset < length) != 0;
    return parsed;
}

static struct mylite_dml_signed_integer_text_parse parse_signed_integer_text_prefix(
    const char *text,
    size_t length
) {
    struct mylite_dml_signed_integer_text_parse parsed = {0};
    size_t offset = 0U;

    if (text == NULL) {
        return parsed;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < length && text[offset] == '+') {
        ++offset;
    } else if (offset < length && text[offset] == '-') {
        parsed.negative = true;
        ++offset;
    }
    while (offset < length && isdigit((unsigned char)text[offset])) {
        uint64_t digit = (uint64_t)(text[offset] - '0');

        parsed.saw_digits = true;
        if (parsed.magnitude > (UINT64_MAX - digit) / mylite_dml_decimal_base) {
            parsed.overflow = true;
            parsed.magnitude = UINT64_MAX;
        } else if (!parsed.overflow) {
            parsed.magnitude = (parsed.magnitude * mylite_dml_decimal_base) + digit;
        }
        ++offset;
    }
    while (offset < length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    parsed.trailing_garbage = (parsed.saw_digits && offset < length) != 0;
    return parsed;
}

static int coerce_integer_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
) {
    struct mylite_dml_integer_range range = integer_range_for_column(column, kind);
    int64_t rounded = 0;
    bool out_of_range = false;
    int status = MYLITE_OK;

    rounded = round_half_away_to_int64(value);
    out_of_range = integer_range_exceeded(range, rounded);
    status = handle_numeric_problem(
        database,
        column,
        integer_problem_for_range(out_of_range, problem),
        row_number,
        ignore
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_of_range) {
        rounded = clamp_integer_to_range(range, rounded);
    }
    *out_output = (struct mylite_dml_numeric_output){
        .insert_kind = MYLITE_INSERT_BOUND_INTEGER,
        .expression_kind = MYLITE_EXPRESSION_VALUE_INT64,
        .integer_value = rounded,
        .replace = true,
    };
    return MYLITE_OK;
}

static int coerce_integer_negative_magnitude(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_negative_integer_input input,
    struct mylite_dml_numeric_output *out_output
) {
    struct mylite_dml_integer_range range = integer_range_for_column(column, input.kind);
    uint64_t maximum_magnitude = 0U;
    int64_t clipped_value = 0;
    int64_t result = 0;
    bool has_limit =
        integer_negative_magnitude_limit_for_range(range, &maximum_magnitude, &clipped_value);
    bool out_of_range = input.overflow;
    int status = MYLITE_OK;

    if (!out_of_range && has_limit && input.magnitude > maximum_magnitude) {
        out_of_range = true;
    }
    status = handle_numeric_problem(
        database,
        column,
        integer_problem_for_range(out_of_range, input.problem),
        input.row_number,
        input.ignore
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_of_range && has_limit) {
        result = clipped_value;
    } else if (input.magnitude == mylite_dml_int64_min_magnitude) {
        result = INT64_MIN;
    } else {
        result = -(int64_t)input.magnitude;
    }
    *out_output = (struct mylite_dml_numeric_output){
        .insert_kind = MYLITE_INSERT_BOUND_INTEGER,
        .expression_kind = MYLITE_EXPRESSION_VALUE_INT64,
        .integer_value = result,
        .replace = true,
    };
    return MYLITE_OK;
}

static int coerce_integer_uint64(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_dml_integer_uint64_input input,
    struct mylite_dml_numeric_output *out_output
) {
    uint64_t maximum = 0U;
    bool has_maximum = integer_uint64_maximum_for_column(column, input.kind, &maximum);
    bool out_of_range = input.overflow;
    int status = MYLITE_OK;

    if (!out_of_range && has_maximum && input.value > maximum) {
        out_of_range = true;
    }
    status = handle_numeric_problem(
        database,
        column,
        integer_problem_for_range(out_of_range, input.problem),
        input.row_number,
        input.ignore
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_of_range && has_maximum) {
        input.value = maximum;
    }
    return set_integer_uint64_output(input.value, out_output);
}

static struct mylite_dml_integer_range integer_range_for_column(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind
) {
    if (column == NULL || column->data_type == NULL) {
        return (struct mylite_dml_integer_range){0};
    }
    if (kind == MYLITE_DML_NUMERIC_UNSIGNED_INTEGER) {
        return unsigned_integer_range_for_type(column->data_type);
    }
    return signed_integer_range_for_type(column->data_type);
}

static enum mylite_dml_numeric_problem integer_problem_for_range(
    bool out_of_range,
    enum mylite_dml_numeric_problem problem
) {
    if (out_of_range) {
        return MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE;
    }
    return problem;
}

static struct mylite_dml_integer_range signed_integer_range_for_type(const char *data_type) {
    if (mylite_ascii_case_equal(data_type, "tinyint") ||
        mylite_ascii_case_equal(data_type, "bool") ||
        mylite_ascii_case_equal(data_type, "boolean")) {
        return integer_range_with_bounds(SCHAR_MIN, SCHAR_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "smallint")) {
        return integer_range_with_bounds(SHRT_MIN, SHRT_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "mediumint")) {
        return integer_range_with_bounds(
            -(INT64_C(1) << mylite_dml_mediumint_signed_bits),
            (INT64_C(1) << mylite_dml_mediumint_signed_bits) - 1
        );
    }
    if (mylite_ascii_case_equal(data_type, "int")) {
        return integer_range_with_bounds(INT_MIN, INT_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "bigint")) {
        return integer_range_with_bounds(INT64_MIN, INT64_MAX);
    }
    return (struct mylite_dml_integer_range){0};
}

static struct mylite_dml_integer_range unsigned_integer_range_for_type(const char *data_type) {
    if (mylite_ascii_case_equal(data_type, "tinyint") ||
        mylite_ascii_case_equal(data_type, "bool") ||
        mylite_ascii_case_equal(data_type, "boolean")) {
        return integer_range_with_bounds(0, UCHAR_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "smallint")) {
        return integer_range_with_bounds(0, USHRT_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "mediumint")) {
        return integer_range_with_bounds(0, (INT64_C(1) << mylite_dml_mediumint_unsigned_bits) - 1);
    }
    if (mylite_ascii_case_equal(data_type, "int")) {
        return integer_range_with_bounds(0, (int64_t)UINT_MAX);
    }
    if (mylite_ascii_case_equal(data_type, "bigint")) {
        return integer_range_with_minimum(0);
    }
    return (struct mylite_dml_integer_range){0};
}

static struct mylite_dml_integer_range integer_range_with_bounds(int64_t minimum, int64_t maximum) {
    return (struct mylite_dml_integer_range){
        .minimum = minimum,
        .maximum = maximum,
        .has_maximum = true,
        .available = true,
    };
}

static struct mylite_dml_integer_range integer_range_with_minimum(int64_t minimum) {
    return (struct mylite_dml_integer_range){
        .minimum = minimum,
        .maximum = 0,
        .has_maximum = false,
        .available = true,
    };
}

static bool integer_range_exceeded(struct mylite_dml_integer_range range, int64_t value) {
    if (!range.available) {
        return false;
    }
    if (value < range.minimum) {
        return true;
    }
    if (!range.has_maximum) {
        return false;
    }
    return value > range.maximum;
}

static int64_t clamp_integer_to_range(struct mylite_dml_integer_range range, int64_t value) {
    if (!range.available) {
        return value;
    }
    if (value < range.minimum) {
        return range.minimum;
    }
    if (range.has_maximum) {
        if (value > range.maximum) {
            return range.maximum;
        }
    }
    return value;
}

static bool integer_uint64_maximum_for_column(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t *out_maximum
) {
    if (column == NULL || column->data_type == NULL || out_maximum == NULL) {
        return false;
    }
    if (kind == MYLITE_DML_NUMERIC_SIGNED_INTEGER) {
        return signed_integer_uint64_maximum_for_type(column->data_type, out_maximum);
    }
    if (kind == MYLITE_DML_NUMERIC_UNSIGNED_INTEGER) {
        return unsigned_integer_uint64_maximum_for_type(column->data_type, out_maximum);
    }
    return false;
}

static bool signed_integer_uint64_maximum_for_type(const char *data_type, uint64_t *out_maximum) {
    if (mylite_ascii_case_equal(data_type, "tinyint") ||
        mylite_ascii_case_equal(data_type, "bool") ||
        mylite_ascii_case_equal(data_type, "boolean")) {
        *out_maximum = (uint64_t)SCHAR_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "smallint")) {
        *out_maximum = (uint64_t)SHRT_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "mediumint")) {
        *out_maximum = (uint64_t)((INT64_C(1) << mylite_dml_mediumint_signed_bits) - 1);
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "int")) {
        *out_maximum = (uint64_t)INT_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "bigint")) {
        *out_maximum = (uint64_t)INT64_MAX;
        return true;
    }
    return false;
}

static bool unsigned_integer_uint64_maximum_for_type(const char *data_type, uint64_t *out_maximum) {
    if (mylite_ascii_case_equal(data_type, "tinyint") ||
        mylite_ascii_case_equal(data_type, "bool") ||
        mylite_ascii_case_equal(data_type, "boolean")) {
        *out_maximum = UCHAR_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "smallint")) {
        *out_maximum = USHRT_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "mediumint")) {
        *out_maximum = (UINT64_C(1) << mylite_dml_mediumint_unsigned_bits) - UINT64_C(1);
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "int")) {
        *out_maximum = UINT_MAX;
        return true;
    }
    if (mylite_ascii_case_equal(data_type, "bigint")) {
        *out_maximum = UINT64_MAX;
        return true;
    }
    return false;
}

static int set_integer_uint64_output(uint64_t value, struct mylite_dml_numeric_output *out_output) {
    char buffer[mylite_dml_uint64_text_buffer_size];
    int length = 0;

    if (value <= (uint64_t)INT64_MAX) {
        *out_output = (struct mylite_dml_numeric_output){
            .insert_kind = MYLITE_INSERT_BOUND_INTEGER,
            .expression_kind = MYLITE_EXPRESSION_VALUE_INT64,
            .integer_value = (int64_t)value,
            .replace = true,
        };
        return MYLITE_OK;
    }

    length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    out_output->text_value = mylite_copy_span_text(buffer, (size_t)length);
    if (out_output->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    out_output->insert_kind = MYLITE_INSERT_BOUND_TEXT;
    out_output->expression_kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_output->text_length = (size_t)length;
    out_output->replace = true;
    return MYLITE_OK;
}

static bool integer_negative_magnitude_limit_for_range(
    struct mylite_dml_integer_range range,
    uint64_t *out_magnitude,
    int64_t *out_clipped_value
) {
    if (!range.available || out_magnitude == NULL || out_clipped_value == NULL) {
        return false;
    }
    *out_clipped_value = range.minimum;
    if (range.minimum >= 0) {
        *out_magnitude = 0U;
        return true;
    }
    if (range.minimum == INT64_MIN) {
        *out_magnitude = mylite_dml_int64_min_magnitude;
        return true;
    }
    *out_magnitude = (uint64_t)(-range.minimum);
    return true;
}

static int coerce_decimal_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
) {
    uint64_t scale = decimal_scale_for_column(column);
    double rounded = round_decimal_to_scale(value, scale);
    int status = handle_numeric_problem(database, column, problem, row_number, ignore);

    if (status != MYLITE_OK) {
        return status;
    }
    if (fabs(value - rounded) > 0.0) {
        status = append_decimal_scale_note(database, column, row_number);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return set_decimal_output(rounded, scale, out_output);
}

static int coerce_approximate_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore,
    struct mylite_dml_numeric_output *out_output
) {
    double maximum = kind == MYLITE_DML_NUMERIC_FLOAT ? (double)FLT_MAX : DBL_MAX;
    bool negative = signbit(value) != 0;
    bool out_of_range = isinf(value) || fabs(value) > maximum;
    int status = handle_numeric_problem(
        database,
        column,
        out_of_range ? MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE : problem,
        row_number,
        ignore
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (out_of_range) {
        value = negative ? -maximum : maximum;
    }
    if (kind == MYLITE_DML_NUMERIC_FLOAT) {
        value = (double)(float)value;
    }
    return set_approximate_output(value, out_output);
}

static int handle_numeric_problem(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    bool ignore
) {
    if (problem == MYLITE_DML_NUMERIC_PROBLEM_NONE) {
        return MYLITE_OK;
    }
    if (!ignore && mylite_connection_sql_mode_is_strict(database)) {
        return set_numeric_problem_error(database, column, problem, row_number);
    }
    return append_numeric_problem_warning(database, column, problem, row_number);
}

static int set_numeric_problem_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
) {
    char *message = NULL;
    int status = make_numeric_problem_message(column, problem, row_number, &message);

    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return status;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, numeric_problem_code(problem), message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_numeric_problem_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
) {
    if (problem == MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED) {
        return append_decimal_scale_note(database, column, row_number);
    }
    char *message = NULL;
    int status = make_numeric_problem_message(column, problem, row_number, &message);

    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return status;
    }
    status = mylite_diagnostics_append_warning(database, numeric_problem_code(problem), message);
    sqlite3_free(message);
    return status;
}

static int append_decimal_scale_note(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
) {
    char *message = sqlite3_mprintf(
        "Data truncated for column '%q' at row %llu",
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_note(database, MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED, message);
    sqlite3_free(message);
    return status;
}

static int make_numeric_problem_message(
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    char **out_message
) {
    const char *format = NULL;

    if (out_message == NULL) {
        return MYLITE_MISUSE;
    }
    switch (problem) {
    case MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED:
        format = "Data truncated for column '%q' at row %llu";
        break;
    case MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED:
        format = "Incorrect decimal value for column '%q' at row %llu";
        break;
    case MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER:
        format = "Incorrect integer value for column '%q' at row %llu";
        break;
    case MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL:
        format = "Incorrect decimal value for column '%q' at row %llu";
        break;
    case MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE:
        format = "Out of range value for column '%q' at row %llu";
        break;
    case MYLITE_DML_NUMERIC_PROBLEM_NONE:
        return MYLITE_MISUSE;
    }
    *out_message = sqlite3_mprintf(
        format,
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
    return *out_message == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static unsigned int numeric_problem_code(enum mylite_dml_numeric_problem problem) {
    switch (problem) {
    case MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED:
        return MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED;
    case MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED:
        return MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE_FOR_FIELD;
    case MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER:
    case MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL:
        return MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE_FOR_FIELD;
    case MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE:
        return MYLITE_MYSQL_ER_WARN_DATA_OUT_OF_RANGE;
    case MYLITE_DML_NUMERIC_PROBLEM_NONE:
        break;
    }
    return MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED;
}

static int64_t round_half_away_to_int64(double value) {
    double rounded = 0.0;

    if (value >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (value <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    rounded = value >= 0.0 ? floor(value + 0.5) : ceil(value - 0.5);
    if (rounded >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (rounded <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    return (int64_t)rounded;
}

static double round_decimal_to_scale(double value, uint64_t scale) {
    double factor = decimal_scale_factor(scale);
    double scaled = value * factor;
    double rounded = scaled >= 0.0 ? floor(scaled + 0.5) : ceil(scaled - 0.5);

    return rounded / factor;
}

static double decimal_scale_factor(uint64_t scale) {
    double factor = 1.0;

    for (uint64_t index = 0U; index < scale && index < 30U; ++index) {
        factor *= 10.0;
    }
    return factor;
}

static uint64_t decimal_scale_for_column(const struct mylite_insert_table_column *column) {
    return column != NULL && column->has_numeric_scale ? column->numeric_scale : 0U;
}

static int set_decimal_output(
    double value,
    uint64_t scale,
    struct mylite_dml_numeric_output *out_output
) {
    char buffer[128];
    int length = snprintf(buffer, sizeof(buffer), "%.*f", (int)(scale > 30U ? 30U : scale), value);

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    out_output->text_value = mylite_copy_span_text(buffer, (size_t)length);
    if (out_output->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    out_output->insert_kind = MYLITE_INSERT_BOUND_TEXT;
    out_output->expression_kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_output->text_length = (size_t)length;
    out_output->replace = true;
    return MYLITE_OK;
}

static int set_approximate_output(double value, struct mylite_dml_numeric_output *out_output) {
    *out_output = (struct mylite_dml_numeric_output){
        .insert_kind = MYLITE_INSERT_BOUND_REAL,
        .expression_kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = value,
        .replace = true,
    };
    return MYLITE_OK;
}

static int replace_insert_numeric_value(
    const struct mylite_dml_numeric_output *output,
    struct mylite_insert_bound_value *value
) {
    mylite_dml_insert_bound_value_deinit(value);
    if (output->insert_kind == MYLITE_INSERT_BOUND_TEXT) {
        value->text_value = mylite_copy_span_text(output->text_value, output->text_length);
        if (value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
        value->text_length = output->text_length;
    }
    value->kind = output->insert_kind;
    value->integer_value = output->integer_value;
    value->real_value = output->real_value;
    return MYLITE_OK;
}

static int replace_update_numeric_value(
    const struct mylite_dml_numeric_output *output,
    struct mylite_expression_value *value
) {
    mylite_expression_value_deinit(value);
    if (output->expression_kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        value->text_value = mylite_copy_span_text(output->text_value, output->text_length);
        if (value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
        value->text_length = output->text_length;
    }
    value->kind = output->expression_kind;
    value->int64_value = output->integer_value;
    value->real_value = output->real_value;
    return MYLITE_OK;
}

static void numeric_output_deinit(struct mylite_dml_numeric_output *output) {
    if (output == NULL) {
        return;
    }
    free(output->text_value);
    *output = (struct mylite_dml_numeric_output){0};
}
