#include "mylite_execution_dml_numeric.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    decimal_base = 10,
    decimal_round_half_digit = 5,
    uint64_decimal_digit_capacity = 20,
    dml_integer_decimal_shift_limit = 1000000,
};

struct dml_integer_decimal_scan {
    size_t mantissa_start;
    size_t mantissa_end;
    size_t token_end;
    size_t digit_count;
    size_t leading_zero_count;
    size_t digits_after_dot;
    int64_t exponent;
    bool is_negative;
};

struct dml_integer_decimal_exponent_scan {
    size_t start;
    size_t end;
    bool is_negative;
};

static enum mylite_execution_dml_numeric_parse_result scan_dml_integer_decimal_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct dml_integer_decimal_scan *out_scan
);
static size_t scan_dml_integer_decimal_mantissa(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
);
static void record_dml_integer_decimal_mantissa_digit(
    struct dml_integer_decimal_scan *inout_scan,
    unsigned char byte,
    bool has_dot,
    bool *inout_has_nonzero_digit
);
static void scan_dml_integer_decimal_exponent_suffix(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
);
static int64_t scan_dml_integer_decimal_exponent(
    const char *text,
    const struct dml_integer_decimal_exponent_scan *exponent_scan
);
static int64_t dml_integer_decimal_shift(const struct dml_integer_decimal_scan *scan);
static enum mylite_execution_dml_numeric_parse_result round_dml_integer_decimal_scan(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
static bool append_dml_integer_decimal_scan_digits(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t integer_digit_count,
    uint64_t *inout_magnitude
);
static bool dml_integer_decimal_scan_digit_at(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t digit_offset,
    unsigned int *out_digit
);
static bool append_uint64_decimal_digit(uint64_t *inout_value, unsigned int digit);
static bool checked_uint64_increment(uint64_t *inout_value);
static bool dml_integer_decimal_token_has_only_zero_digits(
    const struct dml_integer_decimal_scan *scan
);
static bool dml_numeric_suffix_is_truncated(const char *text, size_t text_length, size_t token_end);
static bool ascii_decimal_digit(unsigned char byte);
static bool ascii_numeric_whitespace(unsigned char byte);
static enum mylite_execution_dml_numeric_parse_result scan_dml_numeric_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct mylite_execution_dml_numeric_token_scan *out_scan
);
static size_t scan_dml_numeric_optional_sign(
    const char *text,
    size_t index,
    struct mylite_execution_dml_numeric_token_scan *scan
);
static bool scan_dml_numeric_mantissa(
    const char *text,
    size_t text_length,
    size_t *inout_index,
    struct mylite_execution_dml_numeric_token_scan *scan
);
static void scan_dml_numeric_mantissa_digit(
    struct mylite_execution_dml_numeric_token_scan *scan,
    unsigned char byte,
    bool saw_dot
);
static void scan_dml_numeric_exponent_suffix(
    const char *text,
    size_t text_length,
    size_t exponent_marker_index,
    struct mylite_execution_dml_numeric_token_scan *scan
);
static int64_t scan_dml_numeric_exponent(
    const char *text,
    size_t exponent_start,
    size_t exponent_end,
    bool exponent_is_negative
);

enum mylite_execution_dml_numeric_parse_result mylite_execution_parse_dml_integer_string_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    struct dml_integer_decimal_scan scan = {0};
    size_t token_start = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;

    if (text == NULL || out_magnitude == NULL || out_is_negative == NULL || text_length == 0U) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    while (token_start < text_length &&
           ascii_numeric_whitespace((unsigned char)text[token_start])) {
        ++token_start;
    }
    if (token_start == text_length ||
        scan_dml_integer_decimal_token(text, text_length, token_start, &scan) !=
            MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    parse_result = round_dml_integer_decimal_scan(text, &scan, out_magnitude, out_is_negative);
    if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK) {
        return parse_result;
    }
    if (dml_numeric_suffix_is_truncated(text, text_length, scan.token_end)) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED;
    }
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

enum mylite_execution_dml_numeric_parse_result mylite_execution_scan_dml_numeric_string_text(
    const char *text,
    size_t text_length,
    struct mylite_execution_dml_numeric_token_scan *out_scan
) {
    size_t token_start = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;

    if (text == NULL || out_scan == NULL || text_length == 0U) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }
    while (token_start < text_length &&
           ascii_numeric_whitespace((unsigned char)text[token_start])) {
        ++token_start;
    }
    if (token_start == text_length) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    parse_result = scan_dml_numeric_token(text, text_length, token_start, out_scan);
    if (parse_result != MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK) {
        return parse_result;
    }
    if (dml_numeric_suffix_is_truncated(text, text_length, out_scan->token_end)) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED;
    }
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

bool mylite_execution_dml_numeric_scan_has_nonzero_digits(
    const struct mylite_execution_dml_numeric_token_scan *scan
) {
    return (scan != NULL && scan->has_nonzero_digits) != 0;
}

enum mylite_execution_dml_numeric_parse_result mylite_execution_parse_signed_integer_text(
    const char *text,
    size_t text_length,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    uint64_t magnitude = 0U;
    size_t digit_start = 0U;
    bool overflow = false;

    if (text == NULL || out_magnitude == NULL || out_is_negative == NULL || text_length == 0U) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    if (text[0] == '+' || text[0] == '-') {
        *out_is_negative = text[0] == '-';
        digit_start = 1U;
    }
    if (digit_start == text_length) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    for (size_t index = digit_start; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
            continue;
        }
        if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }

    if (overflow) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW;
    }
    *out_magnitude = magnitude;
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

static enum mylite_execution_dml_numeric_parse_result scan_dml_integer_decimal_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct dml_integer_decimal_scan *out_scan
) {
    size_t index = token_start;
    struct dml_integer_decimal_scan scan = {0};

    if (text == NULL || out_scan == NULL || token_start >= text_length) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    if (text[index] == '+' || text[index] == '-') {
        scan.is_negative = text[index] == '-';
        ++index;
    }
    scan.mantissa_start = index;
    if (index >= text_length) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    index = scan_dml_integer_decimal_mantissa(text, text_length, &scan);
    if (scan.digit_count == 0U) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }
    scan.mantissa_end = index;
    scan.token_end = index;

    if (index < text_length && (text[index] == 'e' || text[index] == 'E')) {
        scan_dml_integer_decimal_exponent_suffix(text, text_length, &scan);
    }

    *out_scan = scan;
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

static size_t scan_dml_integer_decimal_mantissa(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
) {
    size_t index = 0U;
    bool has_dot = false;
    bool has_nonzero_digit = false;

    if (text == NULL || inout_scan == NULL) {
        return 0U;
    }

    index = inout_scan->mantissa_start;
    while (index < text_length) {
        unsigned char byte = (unsigned char)text[index];

        if (ascii_decimal_digit(byte)) {
            record_dml_integer_decimal_mantissa_digit(
                inout_scan,
                byte,
                has_dot,
                &has_nonzero_digit
            );
            ++index;
        } else if (!has_dot && byte == '.') {
            has_dot = true;
            ++index;
        } else {
            break;
        }
    }
    return index;
}

static void record_dml_integer_decimal_mantissa_digit(
    struct dml_integer_decimal_scan *inout_scan,
    unsigned char byte,
    bool has_dot,
    bool *inout_has_nonzero_digit
) {
    if (inout_scan == NULL || inout_has_nonzero_digit == NULL) {
        return;
    }

    ++inout_scan->digit_count;
    if (has_dot) {
        ++inout_scan->digits_after_dot;
    }
    if (!*inout_has_nonzero_digit && byte == '0') {
        ++inout_scan->leading_zero_count;
        return;
    }
    *inout_has_nonzero_digit = true;
}

static void scan_dml_integer_decimal_exponent_suffix(
    const char *text,
    size_t text_length,
    struct dml_integer_decimal_scan *inout_scan
) {
    struct dml_integer_decimal_exponent_scan exponent_scan = {0};
    size_t index = 0U;

    if (text == NULL || inout_scan == NULL) {
        return;
    }

    index = inout_scan->token_end + 1U;
    if (index < text_length && (text[index] == '+' || text[index] == '-')) {
        exponent_scan.is_negative = text[index] == '-';
        ++index;
    }
    exponent_scan.start = index;
    while (index < text_length && ascii_decimal_digit((unsigned char)text[index])) {
        ++index;
    }
    exponent_scan.end = index;

    if (exponent_scan.end > exponent_scan.start) {
        inout_scan->exponent = scan_dml_integer_decimal_exponent(text, &exponent_scan);
        inout_scan->token_end = exponent_scan.end;
    } else if (!dml_numeric_suffix_is_truncated(text, text_length, index)) {
        inout_scan->token_end = index;
    }
}

static int64_t scan_dml_integer_decimal_exponent(
    const char *text,
    const struct dml_integer_decimal_exponent_scan *exponent_scan
) {
    int64_t exponent = 0;

    if (text == NULL || exponent_scan == NULL) {
        return 0;
    }

    for (size_t index = exponent_scan->start; index < exponent_scan->end; ++index) {
        int digit = text[index] - '0';

        if (exponent > (dml_integer_decimal_shift_limit - digit) / decimal_base) {
            if (exponent_scan->is_negative) {
                return -dml_integer_decimal_shift_limit;
            }
            return dml_integer_decimal_shift_limit;
        }
        exponent = (exponent * decimal_base) + digit;
    }
    if (exponent_scan->is_negative) {
        return -exponent;
    }
    return exponent;
}

static int64_t dml_integer_decimal_shift(const struct dml_integer_decimal_scan *scan) {
    int64_t scale = scan->digits_after_dot > (size_t)dml_integer_decimal_shift_limit
                        ? dml_integer_decimal_shift_limit
                        : (int64_t)scan->digits_after_dot;

    if (scan->exponent >= dml_integer_decimal_shift_limit) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent <= -dml_integer_decimal_shift_limit) {
        return -dml_integer_decimal_shift_limit;
    }
    if (scan->exponent > dml_integer_decimal_shift_limit - scale) {
        return dml_integer_decimal_shift_limit;
    }
    if (scan->exponent < -dml_integer_decimal_shift_limit + scale) {
        return -dml_integer_decimal_shift_limit;
    }
    return scan->exponent - scale;
}

static enum mylite_execution_dml_numeric_parse_result round_dml_integer_decimal_scan(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    uint64_t *out_magnitude,
    bool *out_is_negative
) {
    uint64_t magnitude = 0U;
    int64_t decimal_shift = 0;
    int64_t integer_digit_count = 0;
    unsigned int round_digit = 0U;
    size_t significant_digit_count = 0U;
    bool round_up = false;

    if (text == NULL || scan == NULL || out_magnitude == NULL || out_is_negative == NULL) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }

    *out_magnitude = 0U;
    *out_is_negative = scan->is_negative;
    if (dml_integer_decimal_token_has_only_zero_digits(scan)) {
        *out_is_negative = false;
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
    }

    significant_digit_count = scan->digit_count - scan->leading_zero_count;
    decimal_shift = dml_integer_decimal_shift(scan);
    if (decimal_shift >= 0) {
        if (significant_digit_count > (size_t)dml_integer_decimal_shift_limit ||
            decimal_shift > dml_integer_decimal_shift_limit ||
            significant_digit_count + (size_t)decimal_shift >
                (size_t)uint64_decimal_digit_capacity) {
            return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW;
        }
        integer_digit_count = (int64_t)significant_digit_count + decimal_shift;
    } else if ((uint64_t)(-decimal_shift) >= significant_digit_count) {
        integer_digit_count = (int64_t)significant_digit_count + decimal_shift;
    } else {
        integer_digit_count = (int64_t)(significant_digit_count - (size_t)(-decimal_shift));
    }

    if (integer_digit_count > uint64_decimal_digit_capacity) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW;
    }
    if (integer_digit_count > 0 && !append_dml_integer_decimal_scan_digits(
                                       text,
                                       scan,
                                       (size_t)integer_digit_count,
                                       &magnitude
                                   )) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW;
    }

    if (integer_digit_count == 0) {
        round_up = (dml_integer_decimal_scan_digit_at(text, scan, 0U, &round_digit) &&
                    round_digit >= decimal_round_half_digit) != 0;
    } else if (integer_digit_count > 0 && (size_t)integer_digit_count < significant_digit_count) {
        round_up = (dml_integer_decimal_scan_digit_at(
                        text,
                        scan,
                        (size_t)integer_digit_count,
                        &round_digit
                    ) &&
                    round_digit >= decimal_round_half_digit) != 0;
    }

    if (round_up && !checked_uint64_increment(&magnitude)) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW;
    }

    *out_magnitude = magnitude;
    *out_is_negative = (scan->is_negative && magnitude != 0U) != 0;
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

static bool append_dml_integer_decimal_scan_digits(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t integer_digit_count,
    uint64_t *inout_magnitude
) {
    size_t appended_count = 0U;
    bool in_significant_digits = false;

    if (text == NULL || scan == NULL || inout_magnitude == NULL) {
        return false;
    }

    for (size_t index = scan->mantissa_start;
         index < scan->mantissa_end && appended_count < integer_digit_count;
         ++index) {
        unsigned char byte = (unsigned char)text[index];
        unsigned int digit = 0U;

        if (byte < '0' || byte > '9') {
            continue;
        }
        digit = (unsigned int)(byte - '0');
        if (!in_significant_digits && digit == 0U) {
            continue;
        }
        in_significant_digits = true;
        if (!append_uint64_decimal_digit(inout_magnitude, digit)) {
            return false;
        }
        ++appended_count;
    }

    while (appended_count < integer_digit_count) {
        if (!append_uint64_decimal_digit(inout_magnitude, 0U)) {
            return false;
        }
        ++appended_count;
    }

    return true;
}

static bool dml_integer_decimal_scan_digit_at(
    const char *text,
    const struct dml_integer_decimal_scan *scan,
    size_t digit_offset,
    unsigned int *out_digit
) {
    size_t significant_seen = 0U;
    bool in_significant_digits = false;

    if (text == NULL || scan == NULL || out_digit == NULL) {
        return false;
    }

    for (size_t index = scan->mantissa_start; index < scan->mantissa_end; ++index) {
        unsigned char byte = (unsigned char)text[index];
        unsigned int digit = 0U;

        if (byte < '0' || byte > '9') {
            continue;
        }
        digit = (unsigned int)(byte - '0');
        if (!in_significant_digits && digit == 0U) {
            continue;
        }
        in_significant_digits = true;
        if (significant_seen == digit_offset) {
            *out_digit = digit;
            return true;
        }
        ++significant_seen;
    }
    return false;
}

static bool append_uint64_decimal_digit(uint64_t *inout_value, unsigned int digit) {
    if (inout_value == NULL || digit >= (unsigned int)decimal_base) {
        return false;
    }
    if (*inout_value > (UINT64_MAX - digit) / decimal_base) {
        return false;
    }
    *inout_value = (*inout_value * decimal_base) + digit;
    return true;
}

static bool checked_uint64_increment(uint64_t *inout_value) {
    if (inout_value == NULL || *inout_value == UINT64_MAX) {
        return false;
    }
    ++*inout_value;
    return true;
}

static bool dml_integer_decimal_token_has_only_zero_digits(
    const struct dml_integer_decimal_scan *scan
) {
    if (scan == NULL) {
        return true;
    }
    return scan->leading_zero_count == scan->digit_count;
}

static bool dml_numeric_suffix_is_truncated(
    const char *text,
    size_t text_length,
    size_t token_end
) {
    if (text == NULL || token_end > text_length) {
        return false;
    }
    for (size_t index = token_end; index < text_length; ++index) {
        if (!ascii_numeric_whitespace((unsigned char)text[index])) {
            return true;
        }
    }
    return false;
}

static bool ascii_decimal_digit(unsigned char byte) {
    return (byte >= '0' && byte <= '9') != 0;
}

static bool ascii_numeric_whitespace(unsigned char byte) {
    return (byte == ' ' || (byte >= '\t' && byte <= '\r')) != 0;
}

static enum mylite_execution_dml_numeric_parse_result scan_dml_numeric_token(
    const char *text,
    size_t text_length,
    size_t token_start,
    struct mylite_execution_dml_numeric_token_scan *out_scan
) {
    struct mylite_execution_dml_numeric_token_scan scan = {0};
    size_t index = token_start;

    if (text == NULL || out_scan == NULL || token_start >= text_length) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }
    scan.token_start = token_start;
    index = scan_dml_numeric_optional_sign(text, index, &scan);
    scan.mantissa_start = index;
    if (!scan_dml_numeric_mantissa(text, text_length, &index, &scan)) {
        return MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
    }
    scan.mantissa_end = index;
    scan.token_end = index;
    scan_dml_numeric_exponent_suffix(text, text_length, index, &scan);

    *out_scan = scan;
    return MYLITE_EXECUTION_DML_NUMERIC_PARSE_OK;
}

static size_t scan_dml_numeric_optional_sign(
    const char *text,
    size_t index,
    struct mylite_execution_dml_numeric_token_scan *scan
) {
    if (text[index] == '+' || text[index] == '-') {
        scan->is_negative = text[index] == '-';
        return index + 1U;
    }
    return index;
}

static bool scan_dml_numeric_mantissa(
    const char *text,
    size_t text_length,
    size_t *inout_index,
    struct mylite_execution_dml_numeric_token_scan *scan
) {
    size_t index = inout_index == NULL ? 0U : *inout_index;
    bool saw_dot = false;
    bool saw_digit = false;

    if (text == NULL || inout_index == NULL || scan == NULL) {
        return false;
    }
    while (index < text_length) {
        unsigned char byte = (unsigned char)text[index];

        if (ascii_decimal_digit(byte)) {
            scan_dml_numeric_mantissa_digit(scan, byte, saw_dot);
            saw_digit = true;
            ++index;
        } else if (!saw_dot && byte == '.') {
            saw_dot = true;
            ++index;
        } else {
            break;
        }
    }
    *inout_index = index;
    return saw_digit;
}

static void scan_dml_numeric_mantissa_digit(
    struct mylite_execution_dml_numeric_token_scan *scan,
    unsigned char byte,
    bool saw_dot
) {
    if (scan == NULL) {
        return;
    }
    if (!saw_dot) {
        ++scan->integer_digit_count;
    }
    if (byte != '0') {
        if (!scan->has_nonzero_digits) {
            scan->first_nonzero_digit_index = scan->digit_count;
        }
        scan->last_nonzero_digit_index = scan->digit_count;
        scan->has_nonzero_digits = true;
    }
    ++scan->digit_count;
}

static void scan_dml_numeric_exponent_suffix(
    const char *text,
    size_t text_length,
    size_t index,
    struct mylite_execution_dml_numeric_token_scan *scan
) {
    size_t exponent_start = 0U;
    bool exponent_is_negative = false;

    if (text == NULL || scan == NULL || index >= text_length ||
        (text[index] != 'e' && text[index] != 'E')) {
        return;
    }
    ++index;
    if (index < text_length && (text[index] == '+' || text[index] == '-')) {
        exponent_is_negative = text[index] == '-';
        ++index;
    }
    exponent_start = index;
    while (index < text_length && ascii_decimal_digit((unsigned char)text[index])) {
        ++index;
    }
    if (index > exponent_start) {
        scan->exponent =
            scan_dml_numeric_exponent(text, exponent_start, index, exponent_is_negative);
        scan->token_end = index;
    }
}

static int64_t scan_dml_numeric_exponent(
    const char *text,
    size_t start,
    size_t end,
    bool is_negative
) {
    int64_t exponent = 0;

    if (text == NULL || end < start) {
        return 0;
    }
    for (size_t index = start; index < end; ++index) {
        int digit = text[index] - '0';

        if (exponent > (dml_integer_decimal_shift_limit - digit) / decimal_base) {
            if (is_negative) {
                return -(int64_t)dml_integer_decimal_shift_limit;
            }
            return (int64_t)dml_integer_decimal_shift_limit;
        }
        exponent = (exponent * decimal_base) + digit;
    }
    if (is_negative) {
        return -exponent;
    }
    return exponent;
}
