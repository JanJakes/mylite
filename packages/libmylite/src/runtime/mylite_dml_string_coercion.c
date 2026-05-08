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
    MYLITE_DML_STRING_FIXED_BINARY,
    MYLITE_DML_STRING_BINARY,
};

enum {
    MYLITE_DML_ASCII_MAX = 0x7FU,
    MYLITE_DML_UTF8_SECOND_BYTE_OFFSET = 1,
    MYLITE_DML_UTF8_CONTINUATION_START_OFFSET = 2,
    MYLITE_DML_UTF8_TWO_BYTE_LENGTH = 2,
    MYLITE_DML_UTF8_THREE_BYTE_LENGTH = 3,
    MYLITE_DML_UTF8_FOUR_BYTE_LENGTH = 4,
    MYLITE_DML_UTF8_TWO_BYTE_MIN = 0xC2U,
    MYLITE_DML_UTF8_TWO_BYTE_MAX = 0xDFU,
    MYLITE_DML_UTF8_E0 = 0xE0U,
    MYLITE_DML_UTF8_E0_SECOND_MIN = 0xA0U,
    MYLITE_DML_UTF8_E1_MIN = 0xE1U,
    MYLITE_DML_UTF8_EC_MAX = 0xECU,
    MYLITE_DML_UTF8_ED = 0xEDU,
    MYLITE_DML_UTF8_ED_SECOND_MAX = 0x9FU,
    MYLITE_DML_UTF8_EE_MIN = 0xEEU,
    MYLITE_DML_UTF8_EF_MAX = 0xEFU,
    MYLITE_DML_UTF8_F0 = 0xF0U,
    MYLITE_DML_UTF8_F0_SECOND_MIN = 0x90U,
    MYLITE_DML_UTF8_F1_MIN = 0xF1U,
    MYLITE_DML_UTF8_F3_MAX = 0xF3U,
    MYLITE_DML_UTF8_F4 = 0xF4U,
    MYLITE_DML_UTF8_F4_SECOND_MAX = 0x8FU,
    MYLITE_DML_UTF8_CONTINUATION_MIN = 0x80U,
    MYLITE_DML_UTF8_CONTINUATION_MAX = 0xBFU,
    MYLITE_DML_UTF8_CONTINUATION_MASK = 0xC0U,
    MYLITE_DML_UTF8_CONTINUATION_MARKER = 0x80U,
    MYLITE_DML_INVALID_UTF8_PREVIEW_BYTES = 16,
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

struct mylite_dml_utf8_validation {
    size_t valid_prefix_length;
    size_t invalid_offset;
    size_t invalid_length;
    bool valid;
};

struct mylite_dml_utf8_sequence {
    size_t length;
    unsigned char second_min;
    unsigned char second_max;
};

struct mylite_dml_utf8_sequence_range {
    size_t length;
    unsigned char first_min;
    unsigned char first_max;
    unsigned char second_min;
    unsigned char second_max;
};

static int coerce_insert_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    bool strict_truncation_is_data_truncated,
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

static bool column_data_type_is_text_bytes(const char *data_type);

static bool column_data_type_is_binary_string(const char *data_type);

static bool column_data_type_is_fixed_binary_string(const char *data_type);

static bool column_data_type_is_blob_bytes(const char *data_type);

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
    bool strict_truncation_is_data_truncated,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
);

static int validate_string_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
);

static bool column_requires_utf8_validation(
    const struct mylite_insert_table_column *column,
    bool *out_allow_four_byte
);

static int handle_invalid_utf8_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation,
    struct mylite_dml_string_output *out_output
);

static struct mylite_dml_utf8_validation validate_utf8mb4_text(
    const char *text,
    size_t length,
    bool allow_four_byte
);

static bool utf8_sequence_from_first(
    unsigned char first,
    bool allow_four_byte,
    struct mylite_dml_utf8_sequence *out_sequence
);

static bool utf8_continuation_byte(unsigned char character);

static int set_invalid_utf8_text_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
);

static int append_invalid_utf8_text_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
);

static int replace_with_utf8_prefix(
    mylite_db *database,
    const struct mylite_dml_string_text *text,
    size_t prefix_length,
    struct mylite_dml_string_output *out_output
);

static char *make_invalid_utf8_condition_message(
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
);

static char *make_invalid_utf8_preview(
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
);

static bool invalid_utf8_preview_byte_is_printable(unsigned char byte);

static bool string_text_needs_fixed_binary_padding(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
);

static int replace_with_padded_fixed_binary(
    mylite_db *database,
    const struct mylite_dml_string_text *text,
    uint64_t padded_length,
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
    bool ignore,
    bool strict_truncation_is_data_truncated
);

static int set_string_truncated_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number
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
    const char *text,
    size_t length,
    struct mylite_insert_bound_value *value
);

static int replace_update_string_value(
    const char *text,
    size_t length,
    struct mylite_expression_value *value
);

static void string_text_deinit(struct mylite_dml_string_text *text);

static void string_output_deinit(struct mylite_dml_string_output *output);

int mylite_dml_coerce_insert_string_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    bool strict_truncation_is_data_truncated,
    struct mylite_insert_bound_value *value
) {
    enum mylite_dml_string_kind kind = string_kind_for_column(column);

    if (database == NULL || column == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (kind == MYLITE_DML_STRING_NONE || value->kind == MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    return coerce_insert_string_value(
        database,
        column,
        kind,
        row_number,
        ignore,
        strict_truncation_is_data_truncated,
        value
    );
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
    bool strict_truncation_is_data_truncated,
    struct mylite_insert_bound_value *value
) {
    struct mylite_dml_string_text text = {0};
    struct mylite_dml_string_output output = {0};
    int status = insert_value_to_string_text(value, &text);

    if (status == MYLITE_OK) {
        status = coerce_string_text(
            database,
            column,
            kind,
            row_number,
            ignore,
            strict_truncation_is_data_truncated,
            &text,
            &output
        );
    }
    if (status == MYLITE_OK && (output.replace || value->kind != MYLITE_INSERT_BOUND_TEXT)) {
        status = replace_insert_string_value(
            output.replace ? output.text : text.text,
            output.replace ? output.length : text.length,
            value
        );
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
        status =
            coerce_string_text(database, column, kind, row_number, ignore, false, &text, &output);
    }
    if (status == MYLITE_OK && (output.replace || value->kind != MYLITE_EXPRESSION_VALUE_TEXT)) {
        status = replace_update_string_value(
            output.replace ? output.text : text.text,
            output.replace ? output.length : text.length,
            value
        );
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
    if (column_data_type_is_text_bytes(column->data_type)) {
        return MYLITE_DML_STRING_TEXT_BYTES;
    }
    if (column_data_type_is_fixed_binary_string(column->data_type)) {
        return MYLITE_DML_STRING_FIXED_BINARY;
    }
    if (column_data_type_is_binary_string(column->data_type)) {
        return MYLITE_DML_STRING_BINARY;
    }
    if (column_data_type_is_blob_bytes(column->data_type)) {
        return MYLITE_DML_STRING_BINARY;
    }
    return MYLITE_DML_STRING_NONE;
}

static bool column_data_type_is_character_string(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "char") ||
           mylite_ascii_case_equal(data_type, "varchar");
}

static bool column_data_type_is_text_bytes(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "tinytext") ||
           mylite_ascii_case_equal(data_type, "text") ||
           mylite_ascii_case_equal(data_type, "mediumtext") ||
           mylite_ascii_case_equal(data_type, "longtext");
}

static bool column_data_type_is_binary_string(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "varbinary");
}

static bool column_data_type_is_fixed_binary_string(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "binary");
}

static bool column_data_type_is_blob_bytes(const char *data_type) {
    return mylite_ascii_case_equal(data_type, "tinyblob") ||
           mylite_ascii_case_equal(data_type, "blob") ||
           mylite_ascii_case_equal(data_type, "mediumblob") ||
           mylite_ascii_case_equal(data_type, "longblob");
}

static int insert_value_to_string_text(
    const struct mylite_insert_bound_value *value,
    struct mylite_dml_string_text *out_text
) {
    char buffer[64];
    int length = 0;

    switch (value->kind) {
    case MYLITE_INSERT_BOUND_TEXT:
    case MYLITE_INSERT_BOUND_BLOB:
        *out_text = (struct mylite_dml_string_text){
            .text = value->text_value,
            .length = value->text_length,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_INTEGER:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->integer_value);
        break;
    case MYLITE_INSERT_BOUND_REAL:
        length = mylite_format_storage_real_text(value->real_value, buffer, sizeof(buffer));
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
    char buffer[64];
    int length = 0;

    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        *out_text = (struct mylite_dml_string_text){
            .text = value->text_value,
            .length = value->text_length,
        };
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        length = mylite_format_storage_real_text(value->real_value, buffer, sizeof(buffer));
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
    bool strict_truncation_is_data_truncated,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
) {
    struct mylite_dml_string_output utf8_output = {0};
    struct mylite_dml_string_text effective_text = *text;
    size_t truncated_length = 0U;
    int status = MYLITE_OK;

    status = validate_string_text(database, column, kind, row_number, ignore, text, &utf8_output);
    if (status != MYLITE_OK) {
        return status;
    }
    if (utf8_output.replace) {
        effective_text.text = utf8_output.text;
        effective_text.length = utf8_output.length;
    }

    if (!string_text_exceeds_column_length(
            kind,
            &effective_text,
            column->character_maximum_length
        )) {
        if (string_text_needs_fixed_binary_padding(
                kind,
                &effective_text,
                column->character_maximum_length
            )) {
            string_output_deinit(&utf8_output);
            return replace_with_padded_fixed_binary(
                database,
                &effective_text,
                column->character_maximum_length,
                out_output
            );
        }
        if (utf8_output.replace) {
            *out_output = utf8_output;
        }
        return MYLITE_OK;
    }
    status = handle_string_truncation(
        database,
        column,
        row_number,
        ignore,
        strict_truncation_is_data_truncated
    );
    if (status != MYLITE_OK) {
        string_output_deinit(&utf8_output);
        return status;
    }
    truncated_length =
        string_text_truncated_byte_length(kind, &effective_text, column->character_maximum_length);
    out_output->text = mylite_copy_span_text(effective_text.text, truncated_length);
    if (out_output->text == NULL) {
        string_output_deinit(&utf8_output);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_output->length = truncated_length;
    out_output->replace = true;
    string_output_deinit(&utf8_output);
    return MYLITE_OK;
}

static int validate_string_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    enum mylite_dml_string_kind kind,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    struct mylite_dml_string_output *out_output
) {
    struct mylite_dml_utf8_validation validation = {0};
    bool allow_four_byte = true;

    if (kind != MYLITE_DML_STRING_CHARACTER && kind != MYLITE_DML_STRING_TEXT_BYTES) {
        return MYLITE_OK;
    }
    if (!column_requires_utf8_validation(column, &allow_four_byte)) {
        return MYLITE_OK;
    }
    validation = validate_utf8mb4_text(
        text == NULL ? NULL : text->text,
        text == NULL ? 0U : text->length,
        allow_four_byte
    );
    if (validation.valid) {
        return MYLITE_OK;
    }
    return handle_invalid_utf8_text(
        database,
        column,
        row_number,
        ignore,
        text,
        &validation,
        out_output
    );
}

static bool column_requires_utf8_validation(
    const struct mylite_insert_table_column *column,
    bool *out_allow_four_byte
) {
    const char *character_set_name = column == NULL ? NULL : column->character_set_name;

    if (out_allow_four_byte == NULL) {
        return false;
    }
    *out_allow_four_byte = true;
    if (character_set_name == NULL || character_set_name[0] == '\0') {
        return true;
    }
    if (mylite_ascii_case_equal(character_set_name, "utf8mb4")) {
        return true;
    }
    if (mylite_ascii_case_equal(character_set_name, "utf8mb3") ||
        mylite_ascii_case_equal(character_set_name, "utf8")) {
        *out_allow_four_byte = false;
        return true;
    }
    return false;
}

static int handle_invalid_utf8_text(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    bool ignore,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation,
    struct mylite_dml_string_output *out_output
) {
    int status = MYLITE_OK;

    if (!ignore && mylite_connection_sql_mode_is_strict(database)) {
        return set_invalid_utf8_text_error(database, column, row_number, text, validation);
    }
    status = append_invalid_utf8_text_warning(database, column, row_number, text, validation);
    if (status != MYLITE_OK) {
        return status;
    }
    return replace_with_utf8_prefix(database, text, validation->valid_prefix_length, out_output);
}

static struct mylite_dml_utf8_validation validate_utf8mb4_text(
    const char *text,
    size_t length,
    bool allow_four_byte
) {
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text);
    size_t index = 0U;

    if (text == NULL) {
        length = 0U;
    }
    while (index < length) {
        unsigned char first = source[index];
        struct mylite_dml_utf8_sequence sequence = {0};

        if (first <= MYLITE_DML_ASCII_MAX) {
            ++index;
            continue;
        }
        if (!utf8_sequence_from_first(first, allow_four_byte, &sequence) ||
            index + sequence.length > length ||
            source[index + MYLITE_DML_UTF8_SECOND_BYTE_OFFSET] < sequence.second_min ||
            source[index + MYLITE_DML_UTF8_SECOND_BYTE_OFFSET] > sequence.second_max) {
            return (struct mylite_dml_utf8_validation){
                .valid_prefix_length = index,
                .invalid_offset = index,
                .invalid_length = sequence.length == 0U || index + sequence.length > length
                                      ? length - index
                                      : sequence.length,
            };
        }
        for (size_t offset = MYLITE_DML_UTF8_CONTINUATION_START_OFFSET; offset < sequence.length;
             ++offset) {
            if (!utf8_continuation_byte(source[index + offset])) {
                return (struct mylite_dml_utf8_validation){
                    .valid_prefix_length = index,
                    .invalid_offset = index,
                    .invalid_length = offset + 1U,
                };
            }
        }
        index += sequence.length;
    }
    return (struct mylite_dml_utf8_validation){
        .valid_prefix_length = length,
        .valid = true,
    };
}

static bool utf8_sequence_from_first(
    unsigned char first,
    bool allow_four_byte,
    struct mylite_dml_utf8_sequence *out_sequence
) {
    static const struct mylite_dml_utf8_sequence_range ranges[] = {
        {.length = MYLITE_DML_UTF8_TWO_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_TWO_BYTE_MIN,
         .first_max = MYLITE_DML_UTF8_TWO_BYTE_MAX,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_E0,
         .first_max = MYLITE_DML_UTF8_E0,
         .second_min = MYLITE_DML_UTF8_E0_SECOND_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_E1_MIN,
         .first_max = MYLITE_DML_UTF8_EC_MAX,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_ED,
         .first_max = MYLITE_DML_UTF8_ED,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_ED_SECOND_MAX},
        {.length = MYLITE_DML_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_EE_MIN,
         .first_max = MYLITE_DML_UTF8_EF_MAX,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_F0,
         .first_max = MYLITE_DML_UTF8_F0,
         .second_min = MYLITE_DML_UTF8_F0_SECOND_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_F1_MIN,
         .first_max = MYLITE_DML_UTF8_F3_MAX,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_DML_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_DML_UTF8_F4,
         .first_max = MYLITE_DML_UTF8_F4,
         .second_min = MYLITE_DML_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_DML_UTF8_F4_SECOND_MAX},
    };

    for (size_t index = 0U; index < sizeof(ranges) / sizeof(ranges[0]); ++index) {
        const struct mylite_dml_utf8_sequence_range *range = &ranges[index];

        if (first >= range->first_min && first <= range->first_max &&
            (allow_four_byte || range->length != MYLITE_DML_UTF8_FOUR_BYTE_LENGTH)) {
            *out_sequence = (struct mylite_dml_utf8_sequence){
                .length = range->length,
                .second_min = range->second_min,
                .second_max = range->second_max,
            };
            return true;
        }
    }
    return false;
}

static bool utf8_continuation_byte(unsigned char character) {
    return (character & MYLITE_DML_UTF8_CONTINUATION_MASK) == MYLITE_DML_UTF8_CONTINUATION_MARKER;
}

static int set_invalid_utf8_text_error(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
) {
    char *message = make_invalid_utf8_condition_message(column, row_number, text, validation);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
            message
        );
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int append_invalid_utf8_text_warning(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
) {
    char *message = make_invalid_utf8_condition_message(column, row_number, text, validation);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_warning(
        database,
        MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
        message
    );
    sqlite3_free(message);
    return status;
}

static int replace_with_utf8_prefix(
    mylite_db *database,
    const struct mylite_dml_string_text *text,
    size_t prefix_length,
    struct mylite_dml_string_output *out_output
) {
    out_output->text = mylite_copy_span_text(text == NULL ? "" : text->text, prefix_length);
    if (out_output->text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_output->length = prefix_length;
    out_output->replace = true;
    return MYLITE_OK;
}

static char *make_invalid_utf8_condition_message(
    const struct mylite_insert_table_column *column,
    uint64_t row_number,
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
) {
    char *preview = make_invalid_utf8_preview(text, validation);
    char *message = NULL;

    if (preview == NULL) {
        return NULL;
    }
    message = sqlite3_mprintf(
        "Incorrect string value: '%q' for column '%q' at row %llu",
        preview,
        column->name,
        (unsigned long long)(row_number == 0U ? 1U : row_number)
    );
    sqlite3_free(preview);
    return message;
}

static char *make_invalid_utf8_preview(
    const struct mylite_dml_string_text *text,
    const struct mylite_dml_utf8_validation *validation
) {
    static const char digits[] = "0123456789ABCDEF";
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text->text);
    size_t text_length = text == NULL ? 0U : text->length;
    size_t remaining =
        validation->invalid_offset >= text_length ? 0U : text_length - validation->invalid_offset;
    size_t preview_length =
        validation->invalid_length < remaining ? validation->invalid_length : remaining;
    size_t output = 0U;
    char *preview = NULL;

    if (preview_length > MYLITE_DML_INVALID_UTF8_PREVIEW_BYTES) {
        preview_length = MYLITE_DML_INVALID_UTF8_PREVIEW_BYTES;
    }
    preview = sqlite3_malloc64((sqlite3_uint64)((preview_length * 4U) + 1U));
    if (preview == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < preview_length; ++index) {
        unsigned char byte = source[validation->invalid_offset + index];

        if (invalid_utf8_preview_byte_is_printable(byte)) {
            preview[output++] = (char)byte;
            continue;
        }
        preview[output++] = '\\';
        preview[output++] = 'x';
        preview[output++] = digits[byte >> 4U];
        preview[output++] = digits[byte & 0x0FU];
    }
    preview[output] = '\0';
    return preview;
}

static bool invalid_utf8_preview_byte_is_printable(unsigned char byte) {
    return byte >= 0x20U && byte <= 0x7EU && byte != '\'' && byte != '\\';
}

static bool string_text_needs_fixed_binary_padding(
    enum mylite_dml_string_kind kind,
    const struct mylite_dml_string_text *text,
    uint64_t maximum_length
) {
    if (kind != MYLITE_DML_STRING_FIXED_BINARY || text == NULL) {
        return false;
    }
    return (uint64_t)text->length < maximum_length;
}

static int replace_with_padded_fixed_binary(
    mylite_db *database,
    const struct mylite_dml_string_text *text,
    uint64_t padded_length,
    struct mylite_dml_string_output *out_output
) {
    char *padded = NULL;

    if (text == NULL || out_output == NULL || padded_length > (uint64_t)SIZE_MAX - 1U) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    padded = malloc((size_t)padded_length + 1U);
    if (padded == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (text->length > 0U && text->text != NULL) {
        memcpy(padded, text->text, text->length);
    }
    memset(padded + text->length, 0, (size_t)padded_length - text->length + 1U);

    out_output->text = padded;
    out_output->length = (size_t)padded_length;
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
    if (kind == MYLITE_DML_STRING_FIXED_BINARY || kind == MYLITE_DML_STRING_BINARY ||
        kind == MYLITE_DML_STRING_TEXT_BYTES) {
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
    if (kind == MYLITE_DML_STRING_FIXED_BINARY || kind == MYLITE_DML_STRING_BINARY) {
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
    bool ignore,
    bool strict_truncation_is_data_truncated
) {
    if (!ignore && mylite_connection_sql_mode_is_strict(database)) {
        const char *data_type = column == NULL ? NULL : column->data_type;

        if (strict_truncation_is_data_truncated && !column_data_type_is_text_bytes(data_type) &&
            !column_data_type_is_blob_bytes(data_type)) {
            return set_string_truncated_error(database, column, row_number);
        }
        return set_string_too_long_error(database, column, row_number);
    }
    return append_string_truncation_warning(database, column, row_number);
}

static int set_string_truncated_error(
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
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WARN_DATA_TRUNCATED, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
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
    const char *text,
    size_t length,
    struct mylite_insert_bound_value *value
) {
    mylite_dml_insert_bound_value_deinit(value);
    value->text_value = mylite_copy_span_text(text, length);
    if (value->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    value->kind = MYLITE_INSERT_BOUND_TEXT;
    value->text_length = length;
    return MYLITE_OK;
}

static int replace_update_string_value(
    const char *text,
    size_t length,
    struct mylite_expression_value *value
) {
    mylite_expression_value_deinit(value);
    value->text_value = mylite_copy_span_text(text, length);
    if (value->text_value == NULL) {
        return MYLITE_NOMEM;
    }
    value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    value->text_length = length;
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
