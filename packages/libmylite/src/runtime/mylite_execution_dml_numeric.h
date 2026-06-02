#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_DML_NUMERIC_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_DML_NUMERIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_execution_dml_numeric_parse_result {
    MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK,
    MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID,
    MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED,
    MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW,
};

struct mylite_execution_dml_numeric_token_scan {
    size_t token_start;
    size_t token_end;
    size_t mantissa_start;
    size_t mantissa_end;
    size_t integer_digit_count;
    size_t digit_count;
    size_t first_nonzero_digit_index;
    size_t last_nonzero_digit_index;
    int64_t exponent;
    bool has_nonzero_digits;
    bool is_negative;
};

enum mylite_execution_dml_numeric_parse_result mylite_execution_parse_dml_integer_string_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
enum mylite_execution_dml_numeric_parse_result mylite_execution_scan_dml_numeric_string_text(
    const char *text,
    size_t text_length,
    struct mylite_execution_dml_numeric_token_scan *out_scan
);
bool mylite_execution_dml_numeric_scan_has_nonzero_digits(
    const struct mylite_execution_dml_numeric_token_scan *scan
);
enum mylite_execution_dml_numeric_parse_result mylite_execution_parse_signed_integer_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
);

#endif
