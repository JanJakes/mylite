#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <ctype.h>
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
};

enum mylite_dml_numeric_problem {
    MYLITE_DML_NUMERIC_PROBLEM_NONE = 0,
    MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED,
    MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED,
    MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER,
    MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL,
    MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE,
};

struct mylite_dml_numeric_text_parse {
    double value;
    bool saw_number;
    bool trailing_garbage;
    bool allocation_failed;
};

struct mylite_dml_numeric_output {
    enum mylite_insert_bound_value_kind insert_kind;
    enum mylite_expression_value_kind expression_kind;
    int64_t integer_value;
    char *text_value;
    size_t text_length;
    bool replace;
};

static int coerce_insert_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    struct mylite_insert_bound_value *value
);

static int coerce_update_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
    struct mylite_expression_value *value
);

static enum mylite_dml_numeric_kind numeric_kind_for_column(
    const struct mylite_insert_table_column *column
);

static bool column_type_is_unsigned(const struct mylite_insert_table_column *column);

static bool column_data_type_is_signed_integer(const char *data_type);

static int coerce_numeric_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_numeric_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
);

static struct mylite_dml_numeric_text_parse parse_numeric_text(const char *text, size_t length);

static int coerce_integer_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
);

static int coerce_decimal_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
);

static int handle_numeric_problem(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
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
    struct mylite_insert_bound_value *value
) {
    enum mylite_dml_numeric_kind kind = numeric_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_NUMERIC_NONE || value->kind == MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    return coerce_insert_numeric_value(database, column, kind, row_number, value);
}

int mylite_dml_coerce_update_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    struct mylite_expression_value *value
) {
    enum mylite_dml_numeric_kind kind = numeric_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_NUMERIC_NONE || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    return coerce_update_numeric_value(database, column, kind, row_number, value);
}

static int coerce_insert_numeric_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    uint64_t row_number,
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
            &output
        );
        break;
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
            &output
        );
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (kind == MYLITE_DML_NUMERIC_UNSIGNED_INTEGER) {
            return MYLITE_OK;
        }
        status = coerce_numeric_double(
            database,
            column,
            kind,
            value->uint64_value > (uint64_t)INT64_MAX ? (double)INT64_MAX
                                                      : (double)value->uint64_value,
            MYLITE_DML_NUMERIC_PROBLEM_NONE,
            row_number,
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
    struct mylite_dml_numeric_output *out_output
) {
    if (kind == MYLITE_DML_NUMERIC_DECIMAL) {
        return coerce_decimal_double(database, column, value, problem, row_number, out_output);
    }
    return coerce_integer_double(database, column, kind, value, problem, row_number, out_output);
}

static int coerce_numeric_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    const char *text,
    size_t text_length,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
) {
    struct mylite_dml_numeric_text_parse parsed = parse_numeric_text(text, text_length);
    enum mylite_dml_numeric_problem problem = MYLITE_DML_NUMERIC_PROBLEM_NONE;

    if (parsed.allocation_failed) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (!parsed.saw_number) {
        problem = kind == MYLITE_DML_NUMERIC_DECIMAL ? MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_DECIMAL
                                                     : MYLITE_DML_NUMERIC_PROBLEM_INCORRECT_INTEGER;
    } else if (parsed.trailing_garbage) {
        problem = kind == MYLITE_DML_NUMERIC_DECIMAL ? MYLITE_DML_NUMERIC_PROBLEM_DECIMAL_TRUNCATED
                                                     : MYLITE_DML_NUMERIC_PROBLEM_TRUNCATED;
    }
    return coerce_numeric_double(
        database,
        column,
        kind,
        parsed.value,
        problem,
        row_number,
        out_output
    );
}

static struct mylite_dml_numeric_text_parse parse_numeric_text(const char *text, size_t length) {
    struct mylite_dml_numeric_text_parse parsed = {0};
    char *copy = mylite_copy_span_text(text == NULL ? "" : text, text == NULL ? 0U : length);
    char *start = NULL;
    char *end = NULL;

    if (copy == NULL) {
        parsed.allocation_failed = true;
        return parsed;
    }
    start = copy;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    parsed.value = strtod(start, &end);
    parsed.saw_number = end != start;
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    parsed.trailing_garbage = parsed.saw_number && end != NULL && *end != '\0';
    free(copy);
    return parsed;
}

static int coerce_integer_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_kind kind,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
) {
    int status = handle_numeric_problem(database, column, problem, row_number);
    int64_t rounded = 0;

    if (status != MYLITE_OK) {
        return status;
    }
    rounded = round_half_away_to_int64(value);
    if (kind == MYLITE_DML_NUMERIC_UNSIGNED_INTEGER && rounded < 0) {
        status = handle_numeric_problem(
            database,
            column,
            MYLITE_DML_NUMERIC_PROBLEM_OUT_OF_RANGE,
            row_number
        );
        if (status != MYLITE_OK) {
            return status;
        }
        rounded = 0;
    }
    *out_output = (struct mylite_dml_numeric_output){
        .insert_kind = MYLITE_INSERT_BOUND_INTEGER,
        .expression_kind = MYLITE_EXPRESSION_VALUE_INT64,
        .integer_value = rounded,
        .replace = true,
    };
    return MYLITE_OK;
}

static int coerce_decimal_double(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    double value,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number,
    struct mylite_dml_numeric_output *out_output
) {
    uint64_t scale = decimal_scale_for_column(column);
    double rounded = round_decimal_to_scale(value, scale);
    int status = handle_numeric_problem(database, column, problem, row_number);

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

static int handle_numeric_problem(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_numeric_problem problem,
    uint64_t row_number
) {
    if (problem == MYLITE_DML_NUMERIC_PROBLEM_NONE) {
        return MYLITE_OK;
    }
    if (mylite_connection_sql_mode_is_strict(database)) {
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
    return MYLITE_OK;
}

static void numeric_output_deinit(struct mylite_dml_numeric_output *output) {
    if (output == NULL) {
        return;
    }
    free(output->text_value);
    *output = (struct mylite_dml_numeric_output){0};
}
