#include "mylite_dml.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mylite_dml_string_kind {
    MYLITE_DML_STRING_NONE = 0,
    MYLITE_DML_STRING_CHARACTER,
    MYLITE_DML_STRING_TEXT_BYTES,
    MYLITE_DML_STRING_BINARY,
};

struct mylite_dml_string_text {
    char *text;
    size_t length;
    bool owned;
};

struct mylite_dml_string_output {
    char *text;
    size_t length;
    bool replace;
};

static int coerce_insert_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
);

static int coerce_update_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
);

static enum mylite_dml_string_kind string_kind_for_column(
    const struct mylite_insert_table_column *column
);

static bool column_data_type_is_character_string(const char *data_type);

static bool column_data_type_is_tinytext(const char *data_type);

static bool column_data_type_is_binary_string(const char *data_type);

static bool column_data_type_is_tinyblob(const char *data_type);

static int insert_value_to_string_text(
    const struct mylite_insert_bound_value *value,
    struct mylite_dml_string_text *out_text
);

static int expression_value_to_string_text(
    const struct mylite_expression_value *value,
    struct mylite_dml_string_text *out_text
);

static int coerce_string_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
);

static bool string_text_exceeds_column_length(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
);

static size_t string_text_truncated_byte_length(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
);

static size_t utf8_prefix_for_byte_limit(const char *text, size_t length, uint64_t byte_limit);

static size_t utf8_prefix_byte_length(const char *text, size_t length, uint64_t character_limit);

static int handle_string_truncation(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore
);

static int set_string_too_long_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
);

static int append_string_truncation_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
);

static char *make_string_condition_message(
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const char *format
);

static int replace_insert_string_value(
    const struct mylite_dml_string_output *output,
    struct mylite_insert_bound_value *value
);

static int replace_update_string_value(
    const struct mylite_dml_string_output *output,
    struct mylite_expression_value *value
);

static void string_text_deinit(struct mylite_dml_string_text *text);

static void string_output_deinit(struct mylite_dml_string_output *output);

int mylite_dml_coerce_insert_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
) {
    enum mylite_dml_string_kind kind = string_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_STRING_NONE || value->kind == MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    return coerce_insert_string_value(database, column, kind, row_number, ignore, value);
}

int mylite_dml_coerce_update_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
) {
    enum mylite_dml_string_kind kind = string_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_STRING_NONE || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    return coerce_update_string_value(database, column, kind, row_number, ignore, value);
}

static int coerce_insert_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_insert_bound_value *value
) {
    struct mylite_dml_string_text text = {0};
    struct mylite_dml_string_output output = {0};
    int status = insert_value_to_string_text(value, &text);

    if (status == MYLITE_OK) {
        status = coerce_string_text(database, column, kind, row_number, ignore, &text, &output);
    }
    if (status == MYLITE_OK && output.replace) {
        status = replace_insert_string_value(&output, value);
    }
    string_output_deinit(&output);
    string_text_deinit(&text);
    return status;
}

static int coerce_update_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    struct mylite_expression_value *value
) {
    struct mylite_dml_string_text text = {0};
    struct mylite_dml_string_output output = {0};
    int status = expression_value_to_string_text(value, &text);

    if (status == MYLITE_OK) {
        status = coerce_string_text(database, column, kind, row_number, ignore, &text, &output);
    }
    if (status == MYLITE_OK && output.replace) {
        status = replace_update_string_value(&output, value);
    }
    string_output_deinit(&output);
    string_text_deinit(&text);
    return status;
}

static enum mylite_dml_string_kind string_kind_for_column(
    const struct mylite_insert_table_column *column
) {
    if (column == NULL || column->data_type == NULL || !column->has_character_maximum_length) {
        return MYLITE_DML_STRING_NONE;
    }
    if (column_data_type_is_character_string(column->data_type)) {
        return MYLITE_DML_STRING_CHARACTER;
    }
    if (column_data_type_is_tinytext(column->data_type)) {
        return MYLITE_DML_STRING_TEXT_BYTES;
    }
    if (column_data_type_is_binary_string(column->data_type)) {
        return MYLITE_DML_STRING_BINARY;
    }
    if (column_data_type_is_tinyblob(column->data_type)) {
        return MYLITE_DML_STRING_BINARY;
    }
    return MYLITE_DML_STRING_NONE;
}

static bool column_data_type_is_character_string(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "char") ||
           mylite_ascii_case_equal(data_type, "varchar");
}

static bool column_data_type_is_tinytext(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "tinytext");
}

static bool column_data_type_is_binary_string(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "binary") ||
           mylite_ascii_case_equal(data_type, "varbinary");
}

static bool column_data_type_is_tinyblob(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "tinyblob");
}

static int insert_value_to_string_text(
    const struct mylite_insert_bound_value *value,
    struct mylite_dml_string_text *out_text
) {
    char buffer[64];
    int length = 0;

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_TEXT:
        *out_text = (struct mylite_dml_string_text){
            .text = value->text_value,
            .length = value->text_length,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_INTEGER:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->integer_value);
        break;
    case MYLITE_INSERT_BOUND_REAL:
        length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);
        break;
    case MYLITE_INSERT_BOUND_NULL:
        return MYLITE_OK;
    }
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return MYLITE_NOMEM;
    }
    out_text->text = mylite_copy_span_text(buffer, (size_t)length);
    if (out_text->text == NULL) {
        return MYLITE_NOMEM;
    }
    out_text->length = (size_t)length;
    out_text->owned = true;
    return MYLITE_OK;
}

static int expression_value_to_string_text(
    const struct mylite_expression_value *value,
    struct mylite_dml_string_text *out_text
) {
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        *out_text = (struct mylite_dml_string_text){
            .text = value->text_value,
            .length = value->text_length,
        };
        return MYLITE_OK;
    }
    out_text->text = mylite_expression_value_to_text(value);
    if (out_text->text == NULL) {
        return MYLITE_NOMEM;
    }
    out_text->length = strlen(out_text->text);
    out_text->owned = true;
    return MYLITE_OK;
}

static int coerce_string_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
) {
    size_t truncated_length = 0U;
    int status = MYLITE_OK;

    if (!string_text_exceeds_column_length(kind, text, column->character_maximum_length)) {
        return MYLITE_OK;
    }
    status = handle_string_truncation(database, column, row_number, ignore);
    if (status != MYLITE_OK) {
        return status;
    }
    truncated_length =
        string_text_truncated_byte_length(kind, text, column->character_maximum_length);
    out_output->text = mylite_copy_span_text(text->text, truncated_length);
    if (out_output->text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_output->length = truncated_length;
    out_output->replace = true;
    return MYLITE_OK;
}

static bool string_text_exceeds_column_length(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
) {
    if (text == NULL) {
        return false;
    }
    if (kind == MYLITE_DML_STRING_BINARY || kind == MYLITE_DML_STRING_TEXT_BYTES) {
        return (uint64_t)text->length > maximum_length;
    }
    return utf8_prefix_byte_length(text->text, text->length, maximum_length) < text->length;
}

static size_t string_text_truncated_byte_length(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
) {
    if (text == NULL) {
        return 0U;
    }
    if (kind == MYLITE_DML_STRING_BINARY) {
        return maximum_length > (uint64_t)SIZE_MAX ? text->length : (size_t)maximum_length;
    }
    if (kind == MYLITE_DML_STRING_TEXT_BYTES) {
        return utf8_prefix_for_byte_limit(text->text, text->length, maximum_length);
    }
    return utf8_prefix_byte_length(text->text, text->length, maximum_length);
}

static size_t utf8_prefix_for_byte_limit(const char *text, size_t length, uint64_t byte_limit) {
    size_t index = 0U;

    if (text == NULL) {
        return 0U;
    }
    if (byte_limit >= (uint64_t)length) {
        return length;
    }
    index = byte_limit > (uint64_t)SIZE_MAX ? length : (size_t)byte_limit;
    while (index > 0U && ((unsigned char)text[index] & 0xC0U) == 0x80U) {
        --index;
    }
    return index;
}

static size_t utf8_prefix_byte_length(const char *text, size_t length, uint64_t character_limit) {
    uint64_t characters = 0U;
    size_t index = 0U;

    if (text == NULL) {
        return 0U;
    }
    while (index < length && characters < character_limit) {
        unsigned char byte = (unsigned char)text[index++];

        if ((byte & 0xC0U) != 0x80U) {
            ++characters;
        }
    }
    while (index < length && ((unsigned char)text[index] & 0xC0U) == 0x80U) {
        ++index;
    }
    return index;
}

static int handle_string_truncation(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore
) {
    if (!ignore && mylite_connection_sql_mode_is_strict(database)) {
        return set_string_too_long_error(database, column, row_number);
    }
    return append_string_truncation_warning(database, column, row_number);
}

static int set_string_too_long_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
) {
    char *message = make_string_condition_message(
        column,
        row_number,
        "Data too long for column '%q' at row %llu"
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_DATA_TOO_LONG, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_string_truncation_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
) {
    char *message = make_string_condition_message(
        column,
        row_number,
        "Data truncated for column '%q' at row %llu"
    );
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status =
        mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED, message);
    sqlite3_free(message);
    return status;
}

static char *make_string_condition_message(
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const char *format
) {
    return sqlite3_mprintf(
        format,
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
}

static int replace_insert_string_value(
    const struct mylite_dml_string_output *output,
    struct mylite_insert_bound_value *value
) {
    mylite_dml_insert_bound_value_deinit(value);
    value->text_value = mylite_copy_span_text(output->text, output->length);
    if (value->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    value->kind = MYLITE_INSERT_BOUND_TEXT;
    value->text_length = output->length;
    return MYLITE_OK;
}

static int replace_update_string_value(
    const struct mylite_dml_string_output *output,
    struct mylite_expression_value *value
) {
    mylite_expression_value_deinit(value);
    value->text_value = mylite_copy_span_text(output->text, output->length);
    if (value->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    value->text_length = output->length;
    return MYLITE_OK;
}

static void string_text_deinit(struct mylite_dml_string_text *text) {
    if (text == NULL) {
        return;
    }
    if (text->owned) {
        free(text->text);
    }
    *text = (struct mylite_dml_string_text){0};
}

static void string_output_deinit(struct mylite_dml_string_output *output) {
    if (output == NULL) {
        return;
    }
    free(output->text);
    *output = (struct mylite_dml_string_output){0};
}
