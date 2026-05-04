#include "mylite_expression.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(misc-no-recursion, readability-implicit-bool-conversion)

enum {
    MYLITE_WARNING_UNKNOWN = 1105,
    MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS = 1210,
    MYLITE_WARNING_TRUNCATED_WRONG_VALUE = 1292,
    MYLITE_WARNING_INVALID_CHARACTER_STRING = 1300,
    MYLITE_WARNING_DIVISION_BY_ZERO = 1365,
    MYLITE_WARNING_INCORRECT_STRING_VALUE = 1411,
    MYLITE_WARNING_UNKNOWN_LOCALE = 1649,
    MYLITE_WARNING_OUT_OF_RANGE = 1690,
    MYLITE_WARNING_INVALID_ARGUMENT_FOR_LOGARITHM = 3020,
    MYLITE_EXPRESSION_TEXT_BUFFER_SIZE = 64,
    MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE = 128,
    MYLITE_EXPRESSION_FORMAT_TEXT_BUFFER_SIZE = 512,
    MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT = 30,
    MYLITE_EXPRESSION_DECIMAL_BASE = 10,
    MYLITE_TEMPORAL_SHORT_DATE_DIGITS = 6,
    MYLITE_TEMPORAL_LONG_DATE_DIGITS = 8,
    MYLITE_TEMPORAL_SHORT_DATETIME_DIGITS = 12,
    MYLITE_TEMPORAL_LONG_DATETIME_DIGITS = 14,
    MYLITE_TEMPORAL_MAX_YEAR = 9999,
    MYLITE_TEMPORAL_MAX_FSP = 6,
    MYLITE_TEMPORAL_DAYS_PER_WEEK = 7,
    MYLITE_TEMPORAL_SECONDS_PER_MINUTE = 60,
    MYLITE_TEMPORAL_SECONDS_PER_HOUR = 3600,
    MYLITE_TEMPORAL_SECONDS_PER_DAY = 86400,
    MYLITE_TEMPORAL_MICROSECOND_LIMIT = 1000000,
    MYLITE_TEMPORAL_MONTHS_PER_YEAR = 12,
    MYLITE_TEMPORAL_FEBRUARY = 2,
    MYLITE_TEMPORAL_MAX_MONTH_DAY = 31,
    MYLITE_TEMPORAL_MAX_HOUR = 23,
    MYLITE_TEMPORAL_MAX_MINUTE_SECOND = 59,
    MYLITE_TEMPORAL_LEAP_FEBRUARY_DAYS = 29,
    MYLITE_TEMPORAL_DAYS_PER_COMMON_YEAR = 365,
    MYLITE_TEMPORAL_LEAP_YEAR_INTERVAL = 4,
    MYLITE_TEMPORAL_CENTURY_YEARS = 100,
    MYLITE_TEMPORAL_QUADRICENTENNIAL_YEARS = 400,
    MYLITE_TEMPORAL_TWO_DIGIT_YEAR_PIVOT = 70,
    MYLITE_TEMPORAL_TWO_DIGIT_YEAR_HIGH_CENTURY = 1900,
    MYLITE_TEMPORAL_TWO_DIGIT_YEAR_LOW_CENTURY = 2000,
    MYLITE_EXPRESSION_UINT64_DIGITS = 19,
    MYLITE_EXPRESSION_BITS_PER_BYTE = 8,
    MYLITE_EXPRESSION_BITS_PER_UINT64 = 64,
    MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE = 256,
    MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW = 160,
    MYLITE_EXPRESSION_ORD_BYTE_BASE = 256,
    MYLITE_EXPRESSION_HEX_ALPHA_OFFSET = 10,
    MYLITE_EXPRESSION_HEX_LOW_NIBBLE_MASK = 0x0FU,
    MYLITE_EXPRESSION_BASE64_INPUT_GROUP = 3,
    MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP = 4,
    MYLITE_EXPRESSION_BASE64_LINE_LENGTH = 76,
    MYLITE_EXPRESSION_BASE64_SHIFT_TWO = 2,
    MYLITE_EXPRESSION_BASE64_SHIFT_FOUR = 4,
    MYLITE_EXPRESSION_BASE64_SHIFT_SIX = 6,
    MYLITE_EXPRESSION_BASE64_TWO_BIT_MASK = 0x03U,
    MYLITE_EXPRESSION_BASE64_FOUR_BIT_MASK = 0x0FU,
    MYLITE_EXPRESSION_BASE64_SIX_BIT_MASK = 0x3FU,
    MYLITE_EXPRESSION_BASE64_LOWER_ALPHA_OFFSET = 26,
    MYLITE_EXPRESSION_BASE64_DIGIT_OFFSET = 52,
    MYLITE_EXPRESSION_BASE64_PLUS_VALUE = 62,
    MYLITE_EXPRESSION_BASE64_SLASH_VALUE = 63,
    MYLITE_EXPRESSION_BINARY_BASE = 2,
    MYLITE_EXPRESSION_OCTAL_BASE = 8,
    MYLITE_EXPRESSION_MIN_BASE = 2,
    MYLITE_EXPRESSION_MAX_BASE = 36,
    MYLITE_EXPRESSION_BASE_CONVERSION_BUFFER_SIZE = 66,
    MYLITE_EXPRESSION_IPV4_PART_COUNT = 4,
    MYLITE_EXPRESSION_IPV4_OCTET_MAX = 255,
    MYLITE_EXPRESSION_IPV4_FIRST_OCTET_SHIFT = 24,
    MYLITE_EXPRESSION_IPV4_SECOND_OCTET_SHIFT = 16,
    MYLITE_EXPRESSION_IPV4_THIRD_OCTET_SHIFT = 8,
    MYLITE_EXPRESSION_IPV4_NTOA_BUFFER_SIZE = 16,
    MYLITE_EXPRESSION_UUID_TEXT_HEX_LENGTH = 32,
    MYLITE_EXPRESSION_UUID_CANONICAL_TEXT_LENGTH = 36,
    MYLITE_EXPRESSION_UUID_BRACED_TEXT_LENGTH = 38,
    MYLITE_EXPRESSION_UUID_BINARY_LENGTH = 16,
    MYLITE_EXPRESSION_UUID_FIRST_DASH = 8,
    MYLITE_EXPRESSION_UUID_SECOND_DASH = 13,
    MYLITE_EXPRESSION_UUID_THIRD_DASH = 18,
    MYLITE_EXPRESSION_UUID_FOURTH_DASH = 23,
    MYLITE_EXPRESSION_UUID_TIME_LOW_OFFSET = 0,
    MYLITE_EXPRESSION_UUID_TIME_MID_OFFSET = 4,
    MYLITE_EXPRESSION_UUID_TIME_HIGH_OFFSET = 6,
    MYLITE_EXPRESSION_UUID_REST_OFFSET = 8,
    MYLITE_EXPRESSION_UUID_TIME_LOW_LENGTH = 4,
    MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH = 2,
    MYLITE_EXPRESSION_CHAR_VALUE_BYTES = 4,
    MYLITE_EXPRESSION_CHAR_SHIFT_8 = 8,
    MYLITE_EXPRESSION_CHAR_SHIFT_16 = 16,
    MYLITE_EXPRESSION_CHAR_SHIFT_24 = 24,
    MYLITE_ASCII_MAX = 0x7FU,
    MYLITE_UTF8_SECOND_BYTE_OFFSET = 1,
    MYLITE_UTF8_CONTINUATION_START_OFFSET = 2,
    MYLITE_UTF8_TWO_BYTE_LENGTH = 2,
    MYLITE_UTF8_THREE_BYTE_LENGTH = 3,
    MYLITE_UTF8_FOUR_BYTE_LENGTH = 4,
    MYLITE_UTF8_TWO_BYTE_MIN = 0xC2U,
    MYLITE_UTF8_TWO_BYTE_MAX = 0xDFU,
    MYLITE_UTF8_E0 = 0xE0U,
    MYLITE_UTF8_E0_SECOND_MIN = 0xA0U,
    MYLITE_UTF8_E1_MIN = 0xE1U,
    MYLITE_UTF8_EC_MAX = 0xECU,
    MYLITE_UTF8_ED = 0xEDU,
    MYLITE_UTF8_ED_SECOND_MAX = 0x9FU,
    MYLITE_UTF8_EE_MIN = 0xEEU,
    MYLITE_UTF8_EF_MAX = 0xEFU,
    MYLITE_UTF8_F0 = 0xF0U,
    MYLITE_UTF8_F0_SECOND_MIN = 0x90U,
    MYLITE_UTF8_F1_MIN = 0xF1U,
    MYLITE_UTF8_F3_MAX = 0xF3U,
    MYLITE_UTF8_F4 = 0xF4U,
    MYLITE_UTF8_F4_SECOND_MAX = 0x8FU,
    MYLITE_UTF8_CONTINUATION_MIN = 0x80U,
    MYLITE_UTF8_CONTINUATION_MAX = 0xBFU,
    MYLITE_UTF8_CONTINUATION_MASK = 0xC0U,
    MYLITE_UTF8_CONTINUATION_MARKER = 0x80U,
    MYLITE_ASCII_CONTROL_Z = 0x1A,
};

static const char mylite_pi_text[] = "3.141593";
static const double mylite_pi_double_value = 3.141592653589793238462643383279502884;
static const double mylite_angle_straight_degrees = 180.0;
static const uint64_t mylite_expression_int64_min_magnitude = (uint64_t)INT64_MAX + UINT64_C(1);
static const uint64_t mylite_expression_ipv4_u32_max = UINT32_MAX;
static const uint32_t mylite_expression_crc32_initial = UINT32_C(0xFFFFFFFF);
static const uint32_t mylite_expression_crc32_polynomial = UINT32_C(0xEDB88320);
static const double mylite_expression_round_half = 0.5;

struct numeric_value {
    double real_value;
    int64_t int64_value;
    uint64_t uint64_value;
    bool is_integer;
    bool is_unsigned;
};

struct numeric_text_input {
    const char *start;
    const char *text;
};

struct numeric_text_parse_input {
    char *text;
    char *start;
};

struct between_truth {
    int low;
    int high;
};

struct substring_range {
    int64_t start;
    int64_t take;
    bool empty;
};

struct substring_context {
    int64_t char_count;
    size_t arity;
};

struct substring_index_input {
    const char *text;
    size_t text_length;
    const char *delimiter;
    size_t delimiter_length;
    uint64_t requested;
    bool negative_count;
};

struct trim_match {
    const char *source;
    const char *remove;
    size_t remove_length;
};

struct locate_texts {
    const char *text;
    const char *needle;
};

struct locate_search {
    const char *text;
    const char *needle;
    size_t start_offset;
    int64_t start_position;
};

enum field_comparison_mode {
    FIELD_COMPARISON_STRING = 0,
    FIELD_COMPARISON_NUMERIC = 1,
};

struct field_match_input {
    const struct mylite_expression_value *search;
    const struct mylite_expression_value *candidates;
    size_t candidate_count;
};

struct find_in_set_input {
    const char *needle;
    const char *list;
};

struct text_compare_input {
    const char *left;
    size_t left_length;
    const char *right;
    size_t right_length;
};

struct greatest_least_argument {
    struct mylite_expression_value value;
    char *text;
    char *compare_text;
    size_t text_length;
};

struct greatest_least_eval_state {
    bool string_domain;
    bool null_result;
};

struct insert_range {
    int64_t position;
    int64_t length;
};

struct insert_operands {
    struct mylite_expression_value text;
    struct mylite_expression_value replacement;
    struct mylite_expression_value position;
    struct mylite_expression_value length;
};

struct insert_text_input {
    const char *text;
    const char *replacement;
    struct insert_range range;
};

struct cast_integer_parse {
    uint64_t magnitude;
    bool negative;
    bool saw_digit;
    bool trailing_garbage;
    bool overflow;
};

enum char_function_charset {
    CHAR_FUNCTION_CHARSET_BINARY = 0,
    CHAR_FUNCTION_CHARSET_LATIN1 = 1,
    CHAR_FUNCTION_CHARSET_UTF8MB4 = 2,
    CHAR_FUNCTION_CHARSET_UTF8MB3 = 3,
    CHAR_FUNCTION_CHARSET_ASCII = 4,
    CHAR_FUNCTION_CHARSET_UNKNOWN = 5,
};

struct char_integer_parse {
    uint64_t magnitude;
    bool negative;
    bool saw_digit;
    bool trailing_garbage;
    bool overflow;
};

struct char_invalid_string_warning {
    const char *charset_name;
    const char *text;
    size_t text_length;
};

struct utf8_sequence_range {
    size_t length;
    unsigned char first_min;
    unsigned char first_max;
    unsigned char second_min;
    unsigned char second_max;
    bool requires_four_byte;
};

struct utf8_sequence {
    size_t length;
    unsigned char second_min;
    unsigned char second_max;
};

struct base_conversion_parse {
    uint64_t bits;
    bool saw_digit;
    bool overflow;
};

struct base_conversion_parse_input {
    const char *text;
    size_t text_length;
    uint64_t from_base;
    bool signed_input;
};

struct base_conversion_format_input {
    uint64_t bits;
    uint64_t to_base;
    bool signed_output;
};

struct inet_aton_parse {
    uint64_t parts[MYLITE_EXPRESSION_IPV4_PART_COUNT];
    size_t part_count;
};

struct decimal_text_parts {
    const char *integer;
    size_t integer_length;
    const char *fraction;
    size_t fraction_length;
    bool negative;
};

struct round_exact_argument_text {
    char *text;
    size_t text_length;
    bool bound_signed;
    bool bound_unsigned;
};

enum format_grouping {
    FORMAT_GROUPING_NONE = 0,
    FORMAT_GROUPING_WESTERN,
    FORMAT_GROUPING_INDIAN,
};

struct format_locale {
    const char *name;
    const char *decimal_separator;
    const char *group_separator;
    enum format_grouping grouping;
};

struct format_numeric_input {
    char *exact_text;
    size_t exact_text_length;
    struct numeric_value number;
    bool has_exact_text;
};

struct format_exact_round_input {
    const char *text;
    size_t text_length;
    int scale;
};

struct format_approximate_round_input {
    double value;
    int scale;
};

enum temporal_date_warning_kind {
    TEMPORAL_DATE_WARNING_NONE = 0,
    TEMPORAL_DATE_WARNING_INCORRECT,
    TEMPORAL_DATE_WARNING_TRUNCATED_DATE,
    TEMPORAL_DATE_WARNING_TRUNCATED_DATETIME,
};

enum temporal_part_kind {
    TEMPORAL_PART_NONE = 0,
    TEMPORAL_PART_YEAR,
    TEMPORAL_PART_MONTH,
    TEMPORAL_PART_DAY,
    TEMPORAL_PART_HOUR,
    TEMPORAL_PART_MINUTE,
    TEMPORAL_PART_SECOND,
};

struct temporal_date_value {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int microsecond;
    unsigned int fraction_digits;
    enum temporal_date_warning_kind warning_kind;
    bool has_time;
    bool preserve_fraction_digits;
};

struct temporal_date_source {
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    char warning_buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    const char *text;
    size_t length;
    const char *warning_text;
    size_t warning_length;
    bool numeric;
    bool approximate_fraction;
};

struct temporal_digit_range {
    size_t offset;
    size_t count;
};

struct temporal_digit_width {
    size_t minimum;
    size_t maximum;
};

struct temporal_year_parts {
    int year;
    size_t digit_count;
};

enum mylite_scalar_function_id {
    MYLITE_SCALAR_FUNCTION_UNKNOWN = 0,
    MYLITE_SCALAR_FUNCTION_CONCAT = 1,
    MYLITE_SCALAR_FUNCTION_CONCAT_WS = 2,
    MYLITE_SCALAR_FUNCTION_LENGTH = 3,
    MYLITE_SCALAR_FUNCTION_CHAR_LENGTH = 4,
    MYLITE_SCALAR_FUNCTION_LOWER = 5,
    MYLITE_SCALAR_FUNCTION_UPPER = 6,
    MYLITE_SCALAR_FUNCTION_LEFT = 7,
    MYLITE_SCALAR_FUNCTION_RIGHT = 8,
    MYLITE_SCALAR_FUNCTION_SUBSTRING = 9,
    MYLITE_SCALAR_FUNCTION_TRIM = 10,
    MYLITE_SCALAR_FUNCTION_LTRIM = 11,
    MYLITE_SCALAR_FUNCTION_RTRIM = 12,
    MYLITE_SCALAR_FUNCTION_REPLACE = 13,
    MYLITE_SCALAR_FUNCTION_ABS = 14,
    MYLITE_SCALAR_FUNCTION_SIGN = 15,
    MYLITE_SCALAR_FUNCTION_FLOOR = 16,
    MYLITE_SCALAR_FUNCTION_CEIL = 17,
    MYLITE_SCALAR_FUNCTION_MOD = 18,
    MYLITE_SCALAR_FUNCTION_PI = 19,
    MYLITE_SCALAR_FUNCTION_IF = 20,
    MYLITE_SCALAR_FUNCTION_IFNULL = 21,
    MYLITE_SCALAR_FUNCTION_NULLIF = 22,
    MYLITE_SCALAR_FUNCTION_COALESCE = 23,
    MYLITE_SCALAR_FUNCTION_ISNULL = 24,
    MYLITE_SCALAR_FUNCTION_DATABASE = 25,
    MYLITE_SCALAR_FUNCTION_SCHEMA = 26,
    MYLITE_SCALAR_FUNCTION_VERSION = 27,
    MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID = 28,
    MYLITE_SCALAR_FUNCTION_ROW_COUNT = 29,
    MYLITE_SCALAR_FUNCTION_ASCII = 30,
    MYLITE_SCALAR_FUNCTION_ORD = 31,
    MYLITE_SCALAR_FUNCTION_LOCATE = 32,
    MYLITE_SCALAR_FUNCTION_INSTR = 33,
    MYLITE_SCALAR_FUNCTION_INSERT = 34,
    MYLITE_SCALAR_FUNCTION_REPEAT = 35,
    MYLITE_SCALAR_FUNCTION_SPACE = 36,
    MYLITE_SCALAR_FUNCTION_REVERSE = 37,
    MYLITE_SCALAR_FUNCTION_LPAD = 38,
    MYLITE_SCALAR_FUNCTION_RPAD = 39,
    MYLITE_SCALAR_FUNCTION_QUOTE = 40,
    MYLITE_SCALAR_FUNCTION_ELT = 41,
    MYLITE_SCALAR_FUNCTION_FIELD = 42,
    MYLITE_SCALAR_FUNCTION_FIND_IN_SET = 43,
    MYLITE_SCALAR_FUNCTION_MAKE_SET = 44,
    MYLITE_SCALAR_FUNCTION_HEX = 45,
    MYLITE_SCALAR_FUNCTION_UNHEX = 46,
    MYLITE_SCALAR_FUNCTION_TO_BASE64 = 47,
    MYLITE_SCALAR_FUNCTION_FROM_BASE64 = 48,
    MYLITE_SCALAR_FUNCTION_BIN = 49,
    MYLITE_SCALAR_FUNCTION_OCT = 50,
    MYLITE_SCALAR_FUNCTION_CONV = 51,
    MYLITE_SCALAR_FUNCTION_CHAR = 52,
    MYLITE_SCALAR_FUNCTION_CHARSET = 53,
    MYLITE_SCALAR_FUNCTION_COLLATION = 54,
    MYLITE_SCALAR_FUNCTION_COERCIBILITY = 55,
    MYLITE_SCALAR_FUNCTION_CONNECTION_ID = 56,
    MYLITE_SCALAR_FUNCTION_USER = 57,
    MYLITE_SCALAR_FUNCTION_CURRENT_USER = 58,
    MYLITE_SCALAR_FUNCTION_BIT_COUNT = 59,
    MYLITE_SCALAR_FUNCTION_BIT_LENGTH = 60,
    MYLITE_SCALAR_FUNCTION_INET_ATON = 61,
    MYLITE_SCALAR_FUNCTION_INET_NTOA = 62,
    MYLITE_SCALAR_FUNCTION_CRC32 = 63,
    MYLITE_SCALAR_FUNCTION_IS_UUID = 64,
    MYLITE_SCALAR_FUNCTION_UUID_TO_BIN = 65,
    MYLITE_SCALAR_FUNCTION_BIN_TO_UUID = 66,
    MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX = 67,
    MYLITE_SCALAR_FUNCTION_ROUND = 68,
    MYLITE_SCALAR_FUNCTION_TRUNCATE = 69,
    MYLITE_SCALAR_FUNCTION_POWER = 70,
    MYLITE_SCALAR_FUNCTION_SQRT = 71,
    MYLITE_SCALAR_FUNCTION_EXP = 72,
    MYLITE_SCALAR_FUNCTION_LN = 73,
    MYLITE_SCALAR_FUNCTION_LOG = 74,
    MYLITE_SCALAR_FUNCTION_LOG2 = 75,
    MYLITE_SCALAR_FUNCTION_LOG10 = 76,
    MYLITE_SCALAR_FUNCTION_SIN = 77,
    MYLITE_SCALAR_FUNCTION_COS = 78,
    MYLITE_SCALAR_FUNCTION_TAN = 79,
    MYLITE_SCALAR_FUNCTION_COT = 80,
    MYLITE_SCALAR_FUNCTION_DEGREES = 81,
    MYLITE_SCALAR_FUNCTION_RADIANS = 82,
    MYLITE_SCALAR_FUNCTION_ACOS = 83,
    MYLITE_SCALAR_FUNCTION_ASIN = 84,
    MYLITE_SCALAR_FUNCTION_ATAN = 85,
    MYLITE_SCALAR_FUNCTION_ATAN2 = 86,
    MYLITE_SCALAR_FUNCTION_GREATEST = 87,
    MYLITE_SCALAR_FUNCTION_LEAST = 88,
    MYLITE_SCALAR_FUNCTION_STRCMP = 89,
    MYLITE_SCALAR_FUNCTION_FORMAT = 90,
    MYLITE_SCALAR_FUNCTION_NOW = 91,
    MYLITE_SCALAR_FUNCTION_CURDATE = 92,
    MYLITE_SCALAR_FUNCTION_CURRENT_DATE = 93,
    MYLITE_SCALAR_FUNCTION_CURTIME = 94,
    MYLITE_SCALAR_FUNCTION_CURRENT_TIME = 95,
    MYLITE_SCALAR_FUNCTION_LOCALTIME = 96,
    MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP = 97,
    MYLITE_SCALAR_FUNCTION_DATE = 98,
    MYLITE_SCALAR_FUNCTION_DATEDIFF = 99,
    MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF = 100,
    MYLITE_SCALAR_FUNCTION_DATE_ADD = 101,
    MYLITE_SCALAR_FUNCTION_DATE_SUB = 102,
    MYLITE_SCALAR_FUNCTION_ADDDATE = 103,
    MYLITE_SCALAR_FUNCTION_SUBDATE = 104,
    MYLITE_SCALAR_FUNCTION_YEAR = 105,
    MYLITE_SCALAR_FUNCTION_MONTH = 106,
    MYLITE_SCALAR_FUNCTION_DAY = 107,
    MYLITE_SCALAR_FUNCTION_HOUR = 108,
    MYLITE_SCALAR_FUNCTION_MINUTE = 109,
    MYLITE_SCALAR_FUNCTION_SECOND = 110,
    MYLITE_SCALAR_FUNCTION_EXTRACT = 111,
};

struct angle_conversion_input {
    enum mylite_scalar_function_id function_id;
    double input;
};

struct inverse_trigonometric_input {
    enum mylite_scalar_function_id function_id;
    double input;
};

struct atan_input {
    double y;
    double x;
};

struct trigonometric_input {
    enum mylite_scalar_function_id function_id;
    double input;
};

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value);
static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value);
static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings);
static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings);
static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value);
static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_case_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_simple_case_expression(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static int eval_searched_case_expression(const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value);
static int eval_case_default(const struct mylite_sql_ast_node *expression,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_cast_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_signed_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_unsigned_cast(const struct mylite_expression_value *value,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_decimal_cast(const struct mylite_sql_ast_node *target,
                             const struct mylite_expression_value *value,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_char_cast(const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          struct mylite_expression_warnings *warnings,
                          struct mylite_expression_value *out_value);
static int eval_binary_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_value *out_value);
static int eval_function_call(const struct mylite_sql_ast_node *node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_date_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_datediff_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static int eval_timestampdiff_function(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static bool timestampdiff_value(enum mylite_sql_ast_interval_unit unit,
                                const struct temporal_date_value *start,
                                const struct temporal_date_value *end, int64_t *out_value);
static int64_t timestampdiff_months(const struct temporal_date_value *start,
                                    const struct temporal_date_value *end);
static int timestampdiff_compare_day_time(const struct temporal_date_value *left,
                                          const struct temporal_date_value *right);
static int64_t timestampdiff_seconds(const struct temporal_date_value *start,
                                     const struct temporal_date_value *end);
static int temporal_time_compare(const struct temporal_date_value *left,
                                 const struct temporal_date_value *right);
static int eval_temporal_part_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static bool temporal_part_from_function(enum mylite_scalar_function_id function_id,
                                        enum mylite_sql_ast_interval_unit unit,
                                        enum temporal_part_kind *out_part);
static int temporal_part_value(const struct temporal_date_value *date,
                               enum temporal_part_kind part);
static int eval_date_arithmetic_function(enum mylite_scalar_function_id function_id,
                                         const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value);
static bool apply_date_interval_arithmetic(enum mylite_scalar_function_id function_id,
                                           enum mylite_sql_ast_interval_unit unit,
                                           struct temporal_date_value *date, int64_t amount,
                                           bool *out_datetime_result);
static bool multiply_interval_amount(int64_t amount, int64_t factor, int64_t *out_value);
static bool add_temporal_months(struct temporal_date_value *date, int64_t months);
static bool add_temporal_days(struct temporal_date_value *date, int64_t days);
static bool add_temporal_seconds(struct temporal_date_value *date, int64_t seconds);
static bool temporal_date_from_day_number(int64_t day_number, struct temporal_date_value *out_date);
static bool interval_unit_has_time_part(enum mylite_sql_ast_interval_unit unit);
static int set_temporal_datetime_text_value(const struct temporal_date_value *date,
                                            struct mylite_expression_value *out_value);
static int temporal_date_from_value(const struct mylite_expression_value *value,
                                    bool warn_approximate_fraction,
                                    struct mylite_expression_warnings *warnings,
                                    struct temporal_date_value *out_date, bool *out_valid);
static int temporal_date_from_value_with_mode(const struct mylite_expression_value *value,
                                              bool warn_approximate_fraction,
                                              bool allow_incomplete_date,
                                              struct mylite_expression_warnings *warnings,
                                              struct temporal_date_value *out_date,
                                              bool *out_valid);
static int temporal_date_source_from_value(const struct mylite_expression_value *value,
                                           struct temporal_date_source *out_source);
static bool temporal_date_source_has_fraction(const struct temporal_date_source *source);
static bool parse_temporal_date_source(const struct temporal_date_source *source,
                                       bool warn_approximate_fraction, bool allow_incomplete_date,
                                       struct temporal_date_value *out_date);
static bool parse_temporal_delimited_date(const char *text, size_t length,
                                          bool allow_incomplete_date,
                                          struct temporal_date_value *out_date);
static bool parse_temporal_compact_date(const char *text, size_t length, bool numeric,
                                        bool allow_incomplete_date,
                                        struct temporal_date_value *out_date);
static bool temporal_text_is_digits(const char *text, size_t length);
static bool prepare_temporal_compact_digits(const char *text, size_t length, bool numeric,
                                            char *padded, const char **out_digits,
                                            size_t *out_digit_length);
static bool temporal_numeric_compact_digit_length(size_t length, size_t *out_digit_length);
static bool temporal_compact_digit_length(size_t length, bool numeric, size_t *out_digit_length);
static size_t temporal_compact_year_digit_count(size_t digit_length);
static bool temporal_compact_datetime_has_time(size_t digit_length);
static bool parse_temporal_compact_time(const char *digits, size_t offset,
                                        struct temporal_date_value *out_date);
static bool parse_temporal_time_suffix(const char *text, size_t length, size_t offset,
                                       struct temporal_date_value *date);
static bool parse_temporal_fixed_digits(const char *text, struct temporal_digit_range range,
                                        int *out_value);
static bool parse_temporal_unsigned_part(const char *text, size_t length, size_t *offset,
                                         struct temporal_digit_width width, int *out_value);
static bool parse_temporal_fraction(const char *text, size_t length, size_t *offset,
                                    int *out_microsecond, unsigned int *out_digits);
static bool temporal_date_parts_are_valid(int year, int month, int day);
static bool temporal_date_parts_are_valid_for_mode(int year, int month, int day,
                                                   bool allow_incomplete_date);
static bool temporal_date_parts_are_all_zero(int year, int month, int day);
static bool temporal_time_parts_are_valid(int hour, int minute, int second);
static bool temporal_year_is_leap(int year);
static int temporal_month_day_limit(int year, int month);
static int temporal_normalized_year(struct temporal_year_parts parts);
static int64_t temporal_day_number(const struct temporal_date_value *date);
static int64_t temporal_days_before_year(int year);
static int temporal_days_before_month(int year, int month);
static int set_temporal_date_text_value(const struct temporal_date_value *date,
                                        struct mylite_expression_value *out_value);
static int append_temporal_date_warning(struct mylite_expression_warnings *warnings,
                                        enum temporal_date_warning_kind warning_kind,
                                        const char *text, size_t text_length);
static int eval_concat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_concat_ws_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int eval_unary_string_function(enum mylite_scalar_function_id function_id,
                                      const struct mylite_sql_ast_node *arguments,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value);
static int eval_leftmost_code_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static int eval_left_right_function(enum mylite_scalar_function_id function_id,
                                    const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value);
static int eval_substring_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static struct substring_range
substring_range_from_arguments(const struct mylite_expression_value *values,
                               struct substring_context context);
static int eval_substring_index_function(const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value);
static int substring_index_count_from_value(const struct mylite_expression_value *value,
                                            struct mylite_expression_warnings *warnings,
                                            uint64_t *out_requested, bool *out_negative);
static int substring_index_count_from_text(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_requested, bool *out_negative);
static int substring_index_text_value(struct substring_index_input input,
                                      struct mylite_expression_value *out_value);
static int substring_index_positive_value(struct substring_index_input input, uint64_t requested,
                                          struct mylite_expression_value *out_value);
static int substring_index_negative_value(struct substring_index_input input, uint64_t requested,
                                          struct mylite_expression_value *out_value);
static size_t count_substring_index_delimiters(struct substring_index_input input);
static bool find_next_substring_index_delimiter(struct substring_index_input input,
                                                size_t start_offset, size_t *out_offset);
static int eval_trim_function(enum mylite_scalar_function_id function_id,
                              const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_trim_operands(enum mylite_sql_ast_trim_direction direction,
                              const struct mylite_sql_ast_node *source_node,
                              const struct mylite_sql_ast_node *remove_node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int trim_text_value(const char *text, const char *remove,
                           enum mylite_sql_ast_trim_direction direction,
                           struct mylite_expression_value *out_value);
static size_t trim_leading_offset(struct trim_match match);
static size_t trim_trailing_length(const char *text, size_t text_length, const char *remove,
                                   size_t remove_length);
static int eval_replace_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int eval_insert_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_insert_operands(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct insert_operands *operands, struct insert_range *out_range,
                                bool *out_null_result);
static int eval_insert_nonnull_argument(const struct mylite_sql_ast_node *argument,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value,
                                        bool *out_null_result);
static void insert_operands_deinit(struct insert_operands *operands);
static int insert_text_value(struct insert_text_input input,
                             struct mylite_expression_value *out_value);
static int eval_quote_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int quote_text_value(const char *text, struct mylite_expression_value *out_value);
static int eval_repeat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int repeat_text_value(const char *text, int64_t count,
                             struct mylite_expression_value *out_value);
static int eval_space_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int space_text_value(int64_t count, struct mylite_expression_value *out_value);
static int eval_reverse_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int reverse_text_value(const char *text, struct mylite_expression_value *out_value);
static int eval_pad_function(enum mylite_scalar_function_id function_id,
                             const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int pad_text_value(enum mylite_scalar_function_id function_id, const char *text,
                          int64_t target_length, const char *pad,
                          struct mylite_expression_value *out_value);
static int append_padding_chars(char **result, size_t *result_length, const char *pad,
                                int64_t pad_chars, int64_t needed_chars);
static int eval_locate_function(enum mylite_scalar_function_id function_id,
                                const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_locate_arguments(enum mylite_scalar_function_id function_id,
                                 const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value values[3],
                                 const struct mylite_sql_ast_node **out_start_node);
static bool locate_arguments_are_null(const struct mylite_expression_value values[3],
                                      const struct mylite_sql_ast_node *start_node);
static int set_locate_function_result(struct locate_texts texts, int64_t start,
                                      struct mylite_expression_value *out_value);
static int eval_elt_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_field_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int eval_field_candidates(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *candidates,
                                 size_t candidate_count);
static int field_match_position(struct field_match_input input, enum field_comparison_mode mode,
                                struct mylite_expression_warnings *warnings, int64_t *out_position);
static enum field_comparison_mode field_comparison_mode_from_values(struct field_match_input input);
static int field_string_match_position(struct field_match_input input, int64_t *out_position);
static int field_numeric_match_position(struct field_match_input input,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_position);
static int eval_find_in_set_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value);
static int64_t find_in_set_position(struct find_in_set_input input);
static int eval_make_set_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static int make_set_bits_from_value(const struct mylite_expression_value *value,
                                    struct mylite_expression_warnings *warnings,
                                    uint64_t *out_bits);
static int make_set_bits_from_string(const char *text, struct mylite_expression_warnings *warnings,
                                     uint64_t *out_bits);
static bool make_set_member_is_selected(uint64_t bits, size_t index);
static int eval_char_function(const struct mylite_sql_ast_node *function_call,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int char_argument_value(const struct mylite_expression_value *value,
                               const struct mylite_sql_ast_node *argument,
                               struct mylite_expression_warnings *warnings, uint32_t *out_value);
static int char_integer_literal_overflow_value(const struct mylite_sql_ast_node *argument,
                                               struct mylite_expression_warnings *warnings,
                                               uint32_t *out_value, bool *out_handled);
static int char_text_value(const char *text, size_t text_length,
                           struct mylite_expression_warnings *warnings, uint32_t *out_value);
static struct char_integer_parse parse_char_integer_text(const char *text, size_t text_length);
static int append_char_integer_warning(struct mylite_expression_warnings *warnings,
                                       const char *type_name, const char *text, size_t text_length);
static int append_char_bytes(char **result, size_t *result_length, uint32_t value);
static int set_char_result(enum char_function_charset charset, const char *charset_name,
                           const char *text, size_t text_length,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static bool char_text_is_valid_for_charset(enum char_function_charset charset, const char *text,
                                           size_t text_length);
static bool char_text_is_ascii(const char *text, size_t text_length);
static bool char_text_is_utf8(const char *text, size_t text_length, bool allow_four_byte);
static bool utf8_sequence_from_first(unsigned char first, bool allow_four_byte,
                                     struct utf8_sequence *out_sequence);
static bool utf8_continuation_byte(unsigned char character);
static int append_invalid_char_string_warning(struct mylite_expression_warnings *warnings,
                                              struct char_invalid_string_warning warning);
static char *copy_charset_node_name(const struct mylite_sql_ast_node *node);
static char *copy_unquoted_identifier_text(struct mylite_sql_source_span span);
static enum char_function_charset char_function_charset_from_name(const char *name);
static bool char_charset_name_is(const char *text, size_t text_length, const char *expected);
static int eval_hex_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int hex_numeric_value(const struct mylite_expression_value *value,
                             const struct mylite_sql_ast_node *argument,
                             struct mylite_expression_warnings *warnings, uint64_t *out_number);
static int64_t hex_real_to_signed_integer(double value, const struct mylite_sql_ast_node *argument);
static bool numeric_argument_uses_exact_rounding(const struct mylite_sql_ast_node *argument);
static int64_t cast_real_to_signed_integer_half_even(double value);
static int set_hex_uint64_value(uint64_t number, struct mylite_expression_value *out_value);
static int set_hex_bytes_value(const char *text, size_t text_length,
                               struct mylite_expression_value *out_value);
static int eval_unhex_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int unhex_argument_to_text(const struct mylite_expression_value *value,
                                  const struct mylite_sql_ast_node *argument, char **out_text,
                                  size_t *out_length);
static bool unhex_argument_uses_exact_decimal_text(const struct mylite_sql_ast_node *argument);
static int unhex_text_value(const char *text, size_t text_length,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int append_unhex_warning(struct mylite_expression_warnings *warnings, const char *text,
                                size_t text_length);
static int hex_digit_value(unsigned char character);
static int eval_to_base64_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int base64_argument_to_text(const struct mylite_expression_value *value,
                                   const struct mylite_sql_ast_node *argument, char **out_text,
                                   size_t *out_length);
static int to_base64_text_value(const char *text, size_t text_length,
                                struct mylite_expression_value *out_value);
static size_t base64_encoded_length(size_t text_length);
static int eval_from_base64_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value);
static int from_base64_text_value(const char *text, size_t text_length,
                                  struct mylite_expression_value *out_value);
static char *copy_base64_clean_text(const char *text, size_t text_length, size_t *out_length);
static bool base64_decode_group(const unsigned char *source, bool is_last_group, char *result,
                                size_t *output);
static int base64_digit_value(unsigned char character);
static bool base64_ignored_whitespace(unsigned char character);
static int eval_base_conversion_function(enum mylite_scalar_function_id function_id,
                                         const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value);
static int eval_bit_count_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int bit_count_value_bits(const struct mylite_expression_value *value,
                                struct mylite_expression_warnings *warnings, uint64_t *out_bits);
static int bit_count_string_bits(const char *text, struct mylite_expression_warnings *warnings,
                                 uint64_t *out_bits);
static unsigned int uint64_bit_count(uint64_t value);
static int eval_bit_length_function(const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value);
static int eval_crc32_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int crc32_argument_to_text(const struct mylite_expression_value *value,
                                  const struct mylite_sql_ast_node *argument, char **out_text,
                                  size_t *out_length);
static void normalize_crc32_exact_decimal_text(char *text, size_t *length);
static int crc32_real_to_text(double value, char **out_text, size_t *out_length);
static void remove_positive_exponent_marker(char *text, size_t *length);
static uint32_t crc32_bytes(const char *text, size_t text_length);
static int eval_inet_aton_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int eval_inet_ntoa_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int inet_aton_input_to_text(const struct mylite_expression_value *value, char **out_text,
                                   size_t *out_length, bool *out_was_text);
static bool parse_inet_aton_text(const char *text, size_t text_length, uint64_t *out_address);
static bool parse_inet_aton_parts(const char *text, size_t text_length,
                                  struct inet_aton_parse *out_parse);
static bool inet_aton_part_limit(size_t part_count, size_t part_index, uint64_t *out_limit);
static int append_inet_aton_warning(struct mylite_expression_warnings *warnings, const char *text,
                                    size_t text_length, bool was_text);
static int inet_ntoa_value_to_address(const struct mylite_expression_value *value,
                                      const struct mylite_sql_ast_node *argument,
                                      struct mylite_expression_warnings *warnings,
                                      uint32_t *out_address);
static int inet_ntoa_text_to_address(const char *text, struct mylite_expression_warnings *warnings,
                                     uint32_t *out_address);
static int append_inet_ntoa_range_warning(struct mylite_expression_warnings *warnings,
                                          const struct mylite_expression_value *value,
                                          const struct mylite_sql_ast_node *argument);
static int append_inet_ntoa_negative_magnitude_warning(struct mylite_expression_warnings *warnings,
                                                       uint64_t magnitude);
static int
append_inet_ntoa_negative_integer_span_warning(struct mylite_expression_warnings *warnings,
                                               const char *text, bool *out_handled);
static int append_inet_ntoa_range_text_warning(struct mylite_expression_warnings *warnings,
                                               const char *text);
static int set_inet_ntoa_result(uint32_t address, struct mylite_expression_value *out_value);
static int eval_is_uuid_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int eval_uuid_to_bin_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value);
static int eval_bin_to_uuid_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value);
static int eval_uuid_first_argument(const struct mylite_sql_ast_node *argument,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value, char **out_text,
                                    size_t *out_length);
static int eval_uuid_swap_flag(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings, bool *out_swap);
static bool parse_uuid_text(const char *text, size_t text_length,
                            unsigned char out_bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH]);
static bool parse_uuid_unbraced_text(const char *text, size_t text_length,
                                     unsigned char out_bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH]);
static bool uuid_canonical_dash_position(size_t index);
static void swap_uuid_time_parts(unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH]);
static void unswap_uuid_time_parts(unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH]);
static int set_uuid_binary_value(const unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH],
                                 struct mylite_expression_value *out_value);
static int set_uuid_text_value(const unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH],
                               struct mylite_expression_value *out_value);
static int append_uuid_incorrect_string_error(struct mylite_expression_warnings *warnings,
                                              const char *function_name, const char *text,
                                              size_t text_length);
static int eval_bin_oct_function(const struct mylite_sql_ast_node *argument, uint64_t to_base,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int eval_conv_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_conv_conversion(const struct mylite_expression_value *number, int64_t from_base,
                                const struct mylite_sql_ast_node *number_argument, int64_t to_base,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static bool base_conversion_abs_base(int64_t base, uint64_t *out_abs_base);
static int base_argument_to_signed_integer(const struct mylite_expression_value *value,
                                           const struct mylite_sql_ast_node *argument,
                                           struct mylite_expression_warnings *warnings,
                                           int64_t *out_base);
static bool base_argument_uses_exact_rounding(const struct mylite_sql_ast_node *argument);
static int base_conversion_input_to_text(const struct mylite_expression_value *value,
                                         const struct mylite_sql_ast_node *argument,
                                         char **out_text, size_t *out_length);
static int base_conversion_real_to_text(double value, char **out_text, size_t *out_length);
static int base_conversion_exact_numeric_literal_to_text(const struct mylite_sql_ast_node *argument,
                                                         char **out_text, size_t *out_length,
                                                         bool *out_matched);
static int copy_base_conversion_literal_text(char sign, const struct mylite_sql_ast_node *literal,
                                             char **out_text, size_t *out_length);
static int parse_base_conversion_input(const char *text, size_t text_length, uint64_t from_base,
                                       bool signed_input,
                                       struct mylite_expression_warnings *warnings,
                                       uint64_t *out_bits);
static struct base_conversion_parse
parse_base_conversion_digits(struct base_conversion_parse_input input);
static int append_base_conversion_warning(struct mylite_expression_warnings *warnings,
                                          const char *text, size_t text_length);
static int base_digit_value(unsigned char character);
static int set_base_conversion_value(struct base_conversion_format_input input,
                                     struct mylite_expression_value *out_value);
static int eval_mod_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_power_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int eval_exp_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_log_function(enum mylite_scalar_function_id function_id,
                             const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_unary_log_function(enum mylite_scalar_function_id function_id,
                                   const struct mylite_sql_ast_node *argument,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static int eval_binary_log_function(const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value);
static int eval_sqrt_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_trigonometric_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static int trigonometric_function_result(struct trigonometric_input input,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_result);
static bool trigonometric_result_is_out_of_range(enum mylite_scalar_function_id function_id,
                                                 double result);
static int eval_inverse_trigonometric_function(enum mylite_scalar_function_id function_id,
                                               const struct mylite_sql_ast_node *arguments,
                                               const struct mylite_expression_eval_context *context,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value);
static int eval_atan_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_atan_argument(const struct mylite_sql_ast_node *argument_node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *argument,
                              struct numeric_value *number, bool *out_null_result);
static double atan_function_result(struct atan_input input, size_t arity);
static int eval_inverse_trigonometric_argument(const struct mylite_sql_ast_node *argument_node,
                                               const struct mylite_expression_eval_context *context,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *argument,
                                               struct numeric_value *number, bool *out_null_result);
static int inverse_trigonometric_function_result(struct inverse_trigonometric_input input,
                                                 double *out_result);
static bool inverse_trigonometric_input_is_out_of_domain(double value);
static bool inverse_trigonometric_result_is_null(double result);
static int eval_angle_conversion_function(enum mylite_scalar_function_id function_id,
                                          const struct mylite_sql_ast_node *arguments,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_warnings *warnings,
                                          struct mylite_expression_value *out_value);
static int eval_angle_conversion_argument(const struct mylite_sql_ast_node *argument_node,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_warnings *warnings,
                                          struct mylite_expression_value *argument,
                                          struct numeric_value *number, bool *out_null_result);
static int angle_conversion_result(struct angle_conversion_input conversion, double *out_result);
static bool angle_conversion_result_is_out_of_range(double result);
static const char *angle_conversion_function_name(enum mylite_scalar_function_id function_id);
static bool trigonometric_pi_expression_value(const struct mylite_sql_ast_node *node,
                                              double *out_value);
static bool trigonometric_pi_expression_value_impl(const struct mylite_sql_ast_node *node,
                                                   double *out_value, bool *out_contains_pi);
static bool trigonometric_pi_literal_value(const struct mylite_sql_ast_node *node,
                                           double *out_value);
static bool trigonometric_expression_is_pi_call(const struct mylite_sql_ast_node *node);
static int eval_round_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value);
static int eval_format_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int format_locale_from_argument(const struct mylite_sql_ast_node *locale_argument,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       const struct format_locale **out_locale);
static bool format_locale_argument_is_literal(const struct mylite_sql_ast_node *locale_argument);
static int format_input_from_value(const struct mylite_sql_ast_node *argument,
                                   const struct mylite_expression_value *value,
                                   struct mylite_expression_warnings *warnings,
                                   struct format_numeric_input *out_input);
static int format_scale_from_value(const struct mylite_expression_value *value,
                                   struct mylite_expression_warnings *warnings, int *out_scale);
static int format_rounded_text_from_input(const struct format_numeric_input *input, int scale,
                                          char **out_text, size_t *out_length);
static int format_round_exact_decimal_text(struct format_exact_round_input input, char **out_text,
                                           size_t *out_length);
static int format_round_approximate_text(struct format_approximate_round_input input,
                                         char **out_text, size_t *out_length);
static int format_apply_locale(const char *rounded_text, size_t rounded_length,
                               const struct format_locale *locale,
                               struct mylite_expression_value *out_value);
static int format_append_grouped_integer(char **result, size_t *result_length, const char *integer,
                                         size_t integer_length, const struct format_locale *locale);
static int format_append_western_grouped_integer(char **result, size_t *result_length,
                                                 const char *integer, size_t integer_length,
                                                 const char *separator, size_t separator_length);
static int format_append_indian_grouped_integer(char **result, size_t *result_length,
                                                const char *integer, size_t integer_length,
                                                const char *separator, size_t separator_length);
static const struct format_locale *format_locale_by_name(const char *name, size_t name_length);
static const struct format_locale *format_default_locale(void);
static int append_format_unknown_locale_warning(struct mylite_expression_warnings *warnings,
                                                const char *locale, size_t locale_length);
static void format_numeric_input_deinit(struct format_numeric_input *input);
static int eval_truncate_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static int eval_round_scale(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings, int *out_scale);
static int round_scale_from_value(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_scale);
static int round_exact_argument_value(const struct mylite_sql_ast_node *argument,
                                      const struct mylite_expression_value *value, int scale,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value, bool *out_handled);
static int round_exact_argument_text(const struct mylite_sql_ast_node *argument,
                                     const struct mylite_expression_value *value,
                                     struct round_exact_argument_text *out_text);
static int round_check_integer_result_bound(struct round_exact_argument_text input,
                                            struct mylite_expression_warnings *warnings,
                                            struct mylite_expression_value *out_value);
static int round_exact_decimal_text(const char *text, size_t text_length,
                                    struct mylite_expression_value *out_value, int scale);
static int round_exact_decimal_positive_scale(const struct decimal_text_parts *parts, int scale,
                                              struct mylite_expression_value *out_value);
static int round_exact_decimal_negative_scale(const struct decimal_text_parts *parts, int scale,
                                              struct mylite_expression_value *out_value);
static int round_append_signed_decimal_result(const struct decimal_text_parts *parts,
                                              const char *digits, size_t digits_length,
                                              size_t fraction_length,
                                              struct mylite_expression_value *out_value);
static int round_approximate_value(const struct mylite_expression_value *value, int scale,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value);
static long double round_half_even_long_double(long double value);
static int set_round_approximate_text(long double value, struct mylite_expression_value *out_value,
                                      int scale);
static int truncate_exact_decimal_text(const char *text, size_t text_length,
                                       struct mylite_expression_value *out_value, int scale);
static int truncate_exact_decimal_positive_scale(const struct decimal_text_parts *parts, int scale,
                                                 struct mylite_expression_value *out_value);
static int truncate_exact_decimal_negative_scale(const struct decimal_text_parts *parts, int scale,
                                                 struct mylite_expression_value *out_value);
static int truncate_approximate_numeric(const struct numeric_value *number, int scale,
                                        struct mylite_expression_value *out_value);
static int set_truncate_approximate_text(long double value,
                                         struct mylite_expression_value *out_value, int scale);
static bool round_argument_exact_literal_text(const struct mylite_sql_ast_node *argument,
                                              char **out_text, size_t *out_length,
                                              bool *out_integer_literal);
static bool parse_decimal_text_parts(char *text, struct decimal_text_parts *out_parts);
static void trim_leading_decimal_zeros(const char **digits, size_t *length);
static bool decimal_digits_all_zero(const char *digits, size_t length);
static bool decimal_text_exceeds_bound(const char *text, const char *bound);
static int increment_decimal_digits(char **digits, size_t *length);
static int append_round_out_of_range_error(struct mylite_expression_warnings *warnings,
                                           bool unsigned_value);
static int eval_numeric_unary_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static void eval_abs_function(const struct numeric_value *number,
                              struct mylite_expression_value *out_value);
static int eval_if_function(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_ifnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_nullif_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_coalesce_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static int eval_greatest_least_function(enum mylite_scalar_function_id function_id,
                                        const struct mylite_sql_ast_node *arguments,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value);
static int eval_greatest_least_arguments(const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct greatest_least_argument *values,
                                         struct greatest_least_eval_state *out_state);
static int set_greatest_least_result(enum mylite_scalar_function_id function_id,
                                     struct greatest_least_argument *values, size_t value_count,
                                     bool string_domain,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value);
static int set_greatest_least_string_result(enum mylite_scalar_function_id function_id,
                                            struct greatest_least_argument *values,
                                            size_t value_count,
                                            struct mylite_expression_value *out_value);
static int set_greatest_least_numeric_result(enum mylite_scalar_function_id function_id,
                                             struct greatest_least_argument *values,
                                             size_t value_count,
                                             struct mylite_expression_warnings *warnings,
                                             struct mylite_expression_value *out_value);
static int greatest_least_argument_to_text(struct greatest_least_argument *argument);
static int greatest_least_argument_prepare_compare_text(struct greatest_least_argument *argument,
                                                        size_t length);
static int compare_greatest_least_text(const struct greatest_least_argument *left,
                                       const struct greatest_least_argument *right);
static int compare_greatest_least_numeric_values(const struct mylite_expression_value *left,
                                                 const struct mylite_expression_value *right,
                                                 struct mylite_expression_warnings *warnings,
                                                 int *out_compare);
static bool greatest_least_candidate_replaces_selected(enum mylite_scalar_function_id function_id,
                                                       int selected_vs_candidate);
static void greatest_least_arguments_deinit(struct greatest_least_argument *values,
                                            size_t value_count);
static int eval_isnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value);
static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth);
static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value);
static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value);
static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *node);
static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *node);
static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind);
static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *node);
static int eval_quantified_comparison(const struct mylite_sql_ast_node *node,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value);
static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth);
static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare);
static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric);
static int text_value_to_numeric(const struct mylite_expression_value *value,
                                 struct mylite_expression_warnings *warnings,
                                 struct numeric_value *out_numeric);
static int text_value_to_numeric_without_warnings(const struct mylite_expression_value *value,
                                                  struct numeric_value *out_numeric);
static bool numeric_text_prefix_is_integer(const char *start, const char *end);
static bool numeric_text_has_digit(const char *start);
static bool numeric_text_is_hex_like(const char *start);
static int parse_numeric_text_double(struct numeric_text_parse_input input,
                                     struct mylite_expression_warnings *warnings,
                                     struct numeric_value *out_numeric);
static void clamp_numeric_text_range(struct numeric_value *numeric);
static int64_t numeric_real_to_truncated_int64(double value);
static int append_numeric_text_without_digits_warning(struct mylite_expression_warnings *warnings,
                                                      struct numeric_text_input input);
static int cast_value_to_signed_integer(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_integer);
static int cast_value_to_unsigned_integer(const struct mylite_expression_value *value,
                                          struct mylite_expression_warnings *warnings,
                                          uint64_t *out_integer);
static int cast_string_to_signed_integer(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         int64_t *out_integer);
static int cast_string_to_unsigned_integer(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_integer);
static struct cast_integer_parse parse_cast_integer_text(const char *text);
static int64_t signed_integer_from_uint64(uint64_t value);
static uint64_t unsigned_complement_from_magnitude(uint64_t magnitude);
static int cast_value_to_decimal_double(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        double *out_number);
static int cast_string_to_decimal_double(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_number);
static int cast_value_to_string(const struct mylite_expression_value *value, char **out_text);
static int cast_real_to_string(double value, char **out_text);
static int64_t cast_real_to_signed_integer(double value);
static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target);
static double absolute_real_value(double value);
static int64_t floor_real_value(double value);
static int64_t ceil_real_value(double value);
static int value_to_string(const struct mylite_expression_value *value, char **out_text);
static int value_to_string_with_length(const struct mylite_expression_value *value, char **out_text,
                                       size_t *out_length);
static int format_compact_real_text(double value, char *buffer, size_t buffer_size);
static bool compact_real_text_round_trips(double value, const char *text);
static void normalize_real_exponent_text(char *text);
static int set_text_value(const char *text, size_t length,
                          struct mylite_expression_value *out_value);
static int append_text(char **text, size_t *length, const char *addition, size_t addition_length);
static bool ascii_text_equal_ci(struct text_compare_input input);
static int utf8_char_count(const char *text, int64_t *out_count);
static size_t utf8_offset_for_chars(const char *text, int64_t char_count);
static size_t utf8_first_character_length(const char *text);
static int64_t find_text_match_position(struct locate_search search);
static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message);
static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text);
static int append_cast_truncation_warning(struct mylite_expression_warnings *warnings,
                                          const char *type_name, const char *text);
static int append_power_out_of_range_error(struct mylite_expression_warnings *warnings);
static int append_exp_out_of_range_error(struct mylite_expression_warnings *warnings);
static int append_cot_out_of_range_error(struct mylite_expression_warnings *warnings);
static int append_angle_conversion_out_of_range_error(struct mylite_expression_warnings *warnings,
                                                      const char *function_name);
static int append_invalid_logarithm_warning(struct mylite_expression_warnings *warnings);
static int append_char_truncation_warning(struct mylite_expression_warnings *warnings,
                                          uint64_t length, const char *text);
static int append_signed_complement_warning(struct mylite_expression_warnings *warnings);
static int append_unsigned_complement_warning(struct mylite_expression_warnings *warnings);
static char *copy_span_text(const char *text, size_t length);
static char *decode_string_literal(const struct mylite_sql_ast_node *node);
static bool decode_string_escape(char escaped, char *out_character);
static const struct mylite_sql_ast_node *
unwrap_parenthesized_node(const struct mylite_sql_ast_node *node);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static size_t child_count(const struct mylite_sql_ast_node *node);
static bool expression_is_supported_no_table(const struct mylite_sql_ast_node *expression,
                                             bool require_cacheable);
static enum mylite_scalar_function_id scalar_function_id(const struct mylite_sql_ast_node *node);
static enum mylite_scalar_function_id
scalar_function_id_from_span(struct mylite_sql_source_span span);
static bool scalar_function_depends_on_session(enum mylite_scalar_function_id function_id);
static bool ascii_span_equals(struct mylite_sql_source_span span, const char *text);
static bool is_null(const struct mylite_expression_value *value);
static bool is_numeric_kind(enum mylite_expression_value_kind kind);
static bool like_match(const char *value, const char *pattern, char escape);
static bool like_match_here(const char *value, const char *pattern, char escape);
static int ascii_case_fold(int character);

void mylite_expression_value_deinit(struct mylite_expression_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text_value);
    *value = (struct mylite_expression_value){0};
}

void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings)
{
    if (warnings == NULL) {
        return;
    }

    for (size_t index = 0U; index < warnings->count; ++index) {
        free(warnings->items[index].message);
    }
    free(warnings->items);
    *warnings = (struct mylite_expression_warnings){0};
}

int mylite_expression_warnings_append(struct mylite_expression_warnings *warnings,
                                      unsigned int code, const char *message)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_WARNING, code, message);
}

int mylite_expression_warnings_append_condition(struct mylite_expression_warnings *warnings,
                                                enum mylite_expression_warning_level level,
                                                unsigned int code, const char *message)
{
    struct mylite_expression_warning *items = NULL;
    char *copy = NULL;

    if (warnings == NULL) {
        return 0;
    }
    copy = copy_span_text(message == NULL ? "" : message, message == NULL ? 0U : strlen(message));
    if (copy == NULL) {
        return -1;
    }
    items = realloc(warnings->items, (warnings->count + 1U) * sizeof(*warnings->items));
    if (items == NULL) {
        free(copy);
        return -1;
    }
    warnings->items = items;
    warnings->items[warnings->count++] =
        (struct mylite_expression_warning){.code = code, .message = copy, .level = level};
    return 0;
}

int mylite_expression_eval(const struct mylite_sql_ast_node *expression,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    return mylite_expression_eval_with_context(expression, NULL, warnings, out_value);
}

int mylite_expression_eval_with_context(const struct mylite_sql_ast_node *expression,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value)
{
    if (out_value == NULL) {
        return -1;
    }

    *out_value = (struct mylite_expression_value){0};
    return eval_node(expression, context, warnings, out_value);
}

int mylite_expression_value_copy(const struct mylite_expression_value *value,
                                 struct mylite_expression_value *out_value)
{
    *out_value = *value;
    out_value->text_value = NULL;
    if (value->text_value != NULL) {
        out_value->text_value = copy_span_text(value->text_value, value->text_length);
        if (out_value->text_value == NULL) {
            return -1;
        }
    }
    return 0;
}

char *mylite_expression_value_to_text(const struct mylite_expression_value *value)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return NULL;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return copy_span_text(value->text_value,
                              value->text_value == NULL ? 0U : value->text_length);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        return length <= 0 || (size_t)length >= sizeof(buffer)
                   ? NULL
                   : copy_span_text(buffer, (size_t)length);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        int length =
            snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        return length <= 0 || (size_t)length >= sizeof(buffer)
                   ? NULL
                   : copy_span_text(buffer, (size_t)length);
    }

    int length = value->compact_real_text
                     ? format_compact_real_text(value->real_value, buffer, sizeof(buffer))
                     : snprintf(buffer, sizeof(buffer), "%.4f", value->real_value);
    return length <= 0 || (size_t)length >= sizeof(buffer) ? NULL
                                                           : copy_span_text(buffer, (size_t)length);
}

int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value)
{
    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        return value->int64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        return value->uint64_value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value->uint64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        return (int64_t)value->real_value;
    }
    return value->text_value == NULL
               ? 0
               : strtoll(value->text_value, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
}

int mylite_expression_value_compare(const struct mylite_expression_value *left,
                                    const struct mylite_expression_value *right,
                                    struct mylite_expression_warnings *warnings, int *out_compare)
{
    if (out_compare == NULL) {
        return -1;
    }
    return compare_values(left, right, warnings, out_compare);
}

int mylite_expression_value_truth(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_truth)
{
    if (out_truth == NULL) {
        return -1;
    }
    return truth_value(value, warnings, out_truth);
}

bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression)
{
    return expression_is_supported_no_table(expression, false);
}

bool mylite_expression_is_cacheable_no_table(const struct mylite_sql_ast_node *expression)
{
    return expression_is_supported_no_table(expression, true);
}

static bool expression_is_supported_no_table(const struct mylite_sql_ast_node *expression,
                                             bool require_cacheable)
{
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        switch (expression->literal_kind) {
        case MYLITE_SQL_AST_LITERAL_NULL:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
        case MYLITE_SQL_AST_LITERAL_FLOAT:
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
            return true;
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
        case MYLITE_SQL_AST_LITERAL_NONE:
            return false;
        }
        return false;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            if (!expression_is_supported_no_table(child, require_cacheable)) {
                return false;
            }
        }
        return true;
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return expression_is_supported_no_table(child_at(expression, 0U), require_cacheable);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        if (!mylite_expression_is_supported_function_call(expression)) {
            return false;
        }
        if (require_cacheable &&
            scalar_function_depends_on_session(scalar_function_id(expression))) {
            return false;
        }
        if (require_cacheable && scalar_function_id(expression) == MYLITE_SCALAR_FUNCTION_CHAR &&
            child_at(expression, 2U) != NULL) {
            return false;
        }
        for (const struct mylite_sql_ast_node *child =
                 child_at(expression, 1U) == NULL ? NULL : child_at(expression, 1U)->first_child;
             child != NULL; child = child->next_sibling) {
            if (!expression_is_supported_no_table(child, require_cacheable)) {
                return false;
            }
        }
        return true;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return !require_cacheable;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    default:
        return false;
    }
}

bool mylite_expression_is_supported_function_call(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = child_at(expression, 1U);
    enum mylite_scalar_function_id function_id = scalar_function_id(expression);
    size_t arity = child_count(arguments);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FUNCTION_CALL ||
        arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
        return arity >= 1U;
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
        return arity >= 2U;
    case MYLITE_SCALAR_FUNCTION_STRCMP:
        return arity == 2U;
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
        return arity == 1U;
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
        return arity == 1U || arity == 2U;
    case MYLITE_SCALAR_FUNCTION_FORMAT:
        return arity == 2U || arity == 3U;
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
        return arity == 2U;
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
        return arity == 2U && expression->interval_spec &&
               expression->interval_unit != MYLITE_SQL_AST_INTERVAL_UNIT_NONE;
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
        return arity == 1U && expression->interval_spec &&
               temporal_part_from_function(function_id, expression->interval_unit, NULL);
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return arity == 1U || arity == 2U;
    case MYLITE_SCALAR_FUNCTION_INSERT:
        return arity == 4U;
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
        return arity == 2U || arity == 3U;
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
        return arity == 3U;
    case MYLITE_SCALAR_FUNCTION_TRIM:
        return arguments->trim_spec ? (arity == 1U || arity == 2U) : arity == 1U;
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_CONV:
        return arity == 3U;
    case MYLITE_SCALAR_FUNCTION_REPEAT:
        return arity == 2U;
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
        return arity == 1U;
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
        return arity == 0U;
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
        return arity == 0U || arity == 1U;
    case MYLITE_SCALAR_FUNCTION_COALESCE:
        return arity >= 1U;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
        return false;
    }
    return false;
}

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    if (node == NULL) {
        return -1;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return -1;
        }
    }
    if (context != NULL && context->eval_constant != NULL &&
        mylite_expression_is_cacheable_no_table(node)) {
        return context->eval_constant(context->user_data, node, warnings, out_value);
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return eval_literal(node, out_value);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (context != NULL && context->resolve_identifier != NULL) {
            return context->resolve_identifier(context->user_data, node, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return eval_unary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return eval_binary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return eval_ternary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return eval_case_expression(node, context, warnings, out_value);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return eval_cast_expression(node, context, warnings, out_value);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        if (context != NULL && context->eval_aggregate != NULL) {
            return context->eval_aggregate(context->user_data, node, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        if (context != NULL && context->eval_subquery != NULL) {
            return context->eval_subquery(context->user_data, node, warnings, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return eval_quantified_comparison(node, context, warnings, out_value);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return eval_function_call(node, context, warnings, out_value);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return context == NULL || context->eval_session_function == NULL
                   ? -1
                   : context->eval_session_function(context->user_data, node, context, warnings,
                                                    out_value);
    default:
        return -1;
    }
}

static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value operand = {0};
    int status = 0;

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return eval_is_expression(node->operator_kind, child_at(node, 0U), context, warnings,
                                  out_value);
    default:
        break;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &operand);
    if (status != 0) {
        return status;
    }
    status = eval_numeric_unary(node->operator_kind, &operand, warnings, out_value);
    mylite_expression_value_deinit(&operand);
    return status;
}

static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = 0;

    if (binary_expression_is_row_subquery(node)) {
        return context == NULL || context->eval_row_subquery == NULL
                   ? -1
                   : context->eval_row_subquery(context->user_data, node, context, warnings,
                                                out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        return eval_in(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        return eval_logical_and(node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        return eval_logical_or(node, context, warnings, out_value);
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &right);
    }
    if (status != 0) {
        goto cleanup;
    }

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        status = eval_arithmetic(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        status = eval_bitwise(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        status = eval_comparison(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        status = eval_logical(node->operator_kind, &left, &right, warnings, out_value);
        break;
    default:
        status = -1;
        break;
    }

cleanup:
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), false)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 0) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), true)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 1) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 1) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings)
{
    int status = 0;

    if (node == NULL) {
        return 0;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return 0;
        }
    }
    if ((node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION ||
         node->kind == MYLITE_SQL_AST_TERNARY_EXPRESSION) &&
        (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE)) {
        status = validate_like_escape_before_shortcut(node, context, warnings);
        if (status != 0) {
            return status;
        }
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        status = validate_boolean_shortcut_operand(child, context, warnings);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}

static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings)
{
    const struct mylite_sql_ast_node *escape = child_at(node, 2U);
    struct mylite_expression_value escape_value = {0};
    char *escape_text = NULL;
    int status = 0;

    if (escape == NULL) {
        return 0;
    }
    status = eval_node(escape, context, warnings, &escape_value);
    if (status != 0 || is_null(&escape_value)) {
        mylite_expression_value_deinit(&escape_value);
        return status;
    }

    status = value_to_string(&escape_value, &escape_text);
    if (status == 0 && strlen(escape_text) != 1U) {
        status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                "Incorrect arguments to ESCAPE");
        if (status == 0) {
            status = -1;
        }
    }

    free(escape_text);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return !expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        char *text = copy_span_text(node->span.text, node->span.length);
        char *end = NULL;
        long long value = 0;
        bool matches = false;

        if (text == NULL) {
            return false;
        }
        errno = 0;
        value = strtoll(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
        if (errno == 0 && end != text && *end == '\0') {
            matches = (value != 0) == expected_value;
        }
        free(text);
        return matches;
    }
    return false;
}

static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN) {
        return eval_between(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    return -1;
}

static int eval_case_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    if (node->case_expression_simple) {
        return eval_simple_case_expression(node, context, warnings, out_value);
    }
    return eval_searched_case_expression(node, context, warnings, out_value);
}

static int eval_simple_case_expression(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *base_expression = child_at(node, 0U);
    const struct mylite_sql_ast_node *when_list = child_at(node, 1U);
    const struct mylite_sql_ast_node *else_expression = child_at(node, 2U);
    struct mylite_expression_value base = {0};
    int status = eval_node(base_expression, context, warnings, &base);

    if (status != 0 || when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        mylite_expression_value_deinit(&base);
        return status == 0 ? -1 : status;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        const struct mylite_sql_ast_node *compare_expression = child_at(arm, 0U);
        const struct mylite_sql_ast_node *result_expression = child_at(arm, 1U);
        struct mylite_expression_value compare = {0};
        int comparison = 0;
        bool matches = false;

        status = eval_node(compare_expression, context, warnings, &compare);
        if (status == 0 && !is_null(&base) && !is_null(&compare)) {
            status = compare_values(&base, &compare, warnings, &comparison);
            matches = status == 0 && comparison == 0;
        }
        mylite_expression_value_deinit(&compare);
        if (status != 0) {
            break;
        }
        if (matches) {
            status = eval_node(result_expression, context, warnings, out_value);
            mylite_expression_value_deinit(&base);
            return status;
        }
    }

    mylite_expression_value_deinit(&base);
    if (status != 0) {
        return status;
    }
    return eval_case_default(else_expression, context, warnings, out_value);
}

static int eval_searched_case_expression(const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *when_list = child_at(node, 0U);
    const struct mylite_sql_ast_node *else_expression = child_at(node, 1U);

    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        return -1;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        const struct mylite_sql_ast_node *condition_expression = child_at(arm, 0U);
        const struct mylite_sql_ast_node *result_expression = child_at(arm, 1U);
        struct mylite_expression_value condition = {0};
        int truth = -1;
        int status = eval_node(condition_expression, context, warnings, &condition);

        if (status == 0) {
            status = truth_value(&condition, warnings, &truth);
        }
        mylite_expression_value_deinit(&condition);
        if (status != 0) {
            return status;
        }
        if (truth == 1) {
            return eval_node(result_expression, context, warnings, out_value);
        }
    }

    return eval_case_default(else_expression, context, warnings, out_value);
}

static int eval_case_default(const struct mylite_sql_ast_node *expression,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    if (expression == NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    return eval_node(expression, context, warnings, out_value);
}

static int eval_cast_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *source = child_at(node, 0U);
    const struct mylite_sql_ast_node *target = child_at(node, 1U);
    struct mylite_expression_value value = {0};
    int status = eval_node(source, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }
    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        mylite_expression_value_deinit(&value);
        return -1;
    }

    switch (target->column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        if (target->column_type_unsigned) {
            status = eval_unsigned_cast(&value, warnings, out_value);
        } else {
            status = eval_signed_cast(&value, warnings, out_value);
        }
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        status = eval_decimal_cast(target, &value, warnings, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        status = eval_char_cast(target, &value, warnings, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        status = eval_binary_cast(&value, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        status = -1;
        break;
    }

    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_signed_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    int64_t integer = 0;
    int status = cast_value_to_signed_integer(value, warnings, &integer);

    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = integer};
    return 0;
}

static int eval_unsigned_cast(const struct mylite_expression_value *value,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    uint64_t integer = 0;
    int status = cast_value_to_unsigned_integer(value, warnings, &integer);

    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                  .uint64_value = integer};
    return 0;
}

static int eval_decimal_cast(const struct mylite_sql_ast_node *target,
                             const struct mylite_expression_value *value,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    char buffer[MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE];
    unsigned int scale = cast_decimal_scale(target);
    double number = 0.0;
    int length = 0;
    int status = cast_value_to_decimal_double(value, warnings, &number);

    if (status != 0) {
        return status;
    }
    length = snprintf(buffer, sizeof(buffer), "%.*f", (int)scale, number);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    return set_text_value(buffer, (size_t)length, out_value);
}

static int eval_char_cast(const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          struct mylite_expression_warnings *warnings,
                          struct mylite_expression_value *out_value)
{
    char *text = NULL;
    int status = cast_value_to_string(value, &text);

    if (status != 0) {
        return status;
    }
    if (target->has_column_length) {
        int64_t character_count = 0;

        status = utf8_char_count(text, &character_count);
        if (status == 0 && character_count > (int64_t)target->column_length) {
            size_t offset = utf8_offset_for_chars(text, (int64_t)target->column_length);

            status = append_char_truncation_warning(warnings, target->column_length, text);
            if (status == 0) {
                text[offset] = '\0';
            }
        }
    }
    if (status == 0) {
        status = set_text_value(text, strlen(text), out_value);
    }
    free(text);
    return status;
}

static int eval_binary_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_value *out_value)
{
    char *text = NULL;
    int status = cast_value_to_string(value, &text);

    if (status == 0) {
        status = set_text_value(text, strlen(text), out_value);
    }
    free(text);
    return status;
}

static int eval_function_call(const struct mylite_sql_ast_node *node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(node, 1U);
    enum mylite_scalar_function_id function_id = scalar_function_id(node);

    if (function_id == MYLITE_SCALAR_FUNCTION_UNKNOWN ||
        !mylite_expression_is_supported_function_call(node)) {
        return -1;
    }
    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_CONCAT:
        return eval_concat_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
        return eval_concat_ws_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
        return eval_unary_string_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
        return eval_leftmost_code_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
        return eval_left_right_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
        return eval_substring_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
        return eval_substring_index_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
        return eval_trim_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_REPLACE:
        return eval_replace_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_INSERT:
        return eval_insert_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_QUOTE:
        return eval_quote_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_REPEAT:
        return eval_repeat_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_SPACE:
        return eval_space_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_REVERSE:
        return eval_reverse_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
        return eval_pad_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
        return eval_locate_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ELT:
        return eval_elt_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_FIELD:
        return eval_field_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
        return eval_find_in_set_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
        return eval_make_set_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_CHAR:
        return eval_char_function(node, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_HEX:
        return eval_hex_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_UNHEX:
        return eval_unhex_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
        return eval_to_base64_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
        return eval_from_base64_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
        return eval_base_conversion_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
        return eval_bit_count_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
        return eval_bit_length_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_CRC32:
        return eval_crc32_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
        return eval_inet_aton_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
        return eval_inet_ntoa_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
        return eval_is_uuid_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
        return eval_uuid_to_bin_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return eval_bin_to_uuid_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_MOD:
        return eval_mod_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_POWER:
        return eval_power_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_EXP:
        return eval_exp_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
        return eval_log_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_SQRT:
        return eval_sqrt_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
        return eval_trigonometric_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
        return eval_inverse_trigonometric_function(function_id, arguments, context, warnings,
                                                   out_value);
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
        return eval_angle_conversion_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
        return eval_numeric_unary_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ROUND:
        return eval_round_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_FORMAT:
        return eval_format_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
        return eval_truncate_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_DATE:
        return eval_date_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
        return eval_datediff_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
        return eval_timestampdiff_function(node, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
        return eval_temporal_part_function(function_id, node, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
        return eval_date_arithmetic_function(function_id, node, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_PI:
        return set_text_value(mylite_pi_text, strlen(mylite_pi_text), out_value);
    case MYLITE_SCALAR_FUNCTION_IF:
        return eval_if_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_IFNULL:
        return eval_ifnull_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_NULLIF:
        return eval_nullif_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_COALESCE:
        return eval_coalesce_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
        return eval_greatest_least_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ISNULL:
        return eval_isnull_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
        return context == NULL || context->eval_session_function == NULL
                   ? -1
                   : context->eval_session_function(context->user_data, node, context, warnings,
                                                    out_value);
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
        break;
    }
    return -1;
}

static int eval_date_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value argument = {0};
    struct temporal_date_value date = {0};
    bool valid = false;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &argument);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&argument)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = temporal_date_from_value(&argument, false, warnings, &date, &valid);
    if (status != 0) {
        goto cleanup;
    }
    if (!valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = set_temporal_date_text_value(&date, out_value);

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int eval_datediff_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    struct temporal_date_value left_date = {0};
    struct temporal_date_value right_date = {0};
    bool left_valid = false;
    bool right_valid = false;
    bool left_null = false;
    bool right_null = false;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &left);

    if (status != 0) {
        goto cleanup;
    }
    status = eval_node(child_at(arguments, 1U), context, warnings, &right);
    if (status != 0) {
        goto cleanup;
    }

    left_null = is_null(&left);
    right_null = is_null(&right);
    if (!left_null) {
        status = temporal_date_from_value(&left, true, warnings, &left_date, &left_valid);
        if (status != 0) {
            goto cleanup;
        }
    }
    if (!right_null) {
        status = temporal_date_from_value(&right, true, warnings, &right_date, &right_valid);
        if (status != 0) {
            goto cleanup;
        }
    }
    if (left_null || right_null || !left_valid || !right_valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = temporal_day_number(&left_date) -
                                                                 temporal_day_number(&right_date)};

cleanup:
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_timestampdiff_function(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(node, 1U);
    struct mylite_expression_value start = {0};
    struct mylite_expression_value end = {0};
    struct temporal_date_value start_date = {0};
    struct temporal_date_value end_date = {0};
    int64_t diff = 0;
    bool start_valid = false;
    bool end_valid = false;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &start);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&start)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = temporal_date_from_value(&start, true, warnings, &start_date, &start_valid);
    if (status != 0) {
        goto cleanup;
    }
    if (!start_valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = eval_node(child_at(arguments, 1U), context, warnings, &end);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&end)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = temporal_date_from_value(&end, true, warnings, &end_date, &end_valid);
    if (status != 0) {
        goto cleanup;
    }
    if (!end_valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (!timestampdiff_value(node->interval_unit, &start_date, &end_date, &diff)) {
        status = -1;
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = diff};

cleanup:
    mylite_expression_value_deinit(&start);
    mylite_expression_value_deinit(&end);
    return status;
}

static bool timestampdiff_value(enum mylite_sql_ast_interval_unit unit,
                                const struct temporal_date_value *start,
                                const struct temporal_date_value *end, int64_t *out_value)
{
    int64_t diff = 0;

    if (start == NULL || end == NULL || out_value == NULL) {
        return false;
    }

    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
        *out_value = timestampdiff_seconds(start, end) / MYLITE_TEMPORAL_SECONDS_PER_DAY;
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        *out_value = timestampdiff_seconds(start, end) /
                     ((int64_t)MYLITE_TEMPORAL_SECONDS_PER_DAY * MYLITE_TEMPORAL_DAYS_PER_WEEK);
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
        *out_value = timestampdiff_months(start, end);
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
        *out_value = timestampdiff_months(start, end) / MYLITE_TEMPORAL_MONTHS_PER_YEAR;
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
        diff = timestampdiff_seconds(start, end);
        *out_value = diff / MYLITE_TEMPORAL_SECONDS_PER_HOUR;
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
        diff = timestampdiff_seconds(start, end);
        *out_value = diff / MYLITE_TEMPORAL_SECONDS_PER_MINUTE;
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
        *out_value = timestampdiff_seconds(start, end);
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
        return false;
    }
    return false;
}

static int64_t timestampdiff_months(const struct temporal_date_value *start,
                                    const struct temporal_date_value *end)
{
    int64_t months =
        (((int64_t)end->year - (int64_t)start->year) * MYLITE_TEMPORAL_MONTHS_PER_YEAR) +
        ((int64_t)end->month - (int64_t)start->month);
    int boundary_compare = timestampdiff_compare_day_time(end, start);

    if (months > 0 && boundary_compare < 0) {
        --months;
    } else if (months < 0 && boundary_compare > 0) {
        ++months;
    }
    return months;
}

static int timestampdiff_compare_day_time(const struct temporal_date_value *left,
                                          const struct temporal_date_value *right)
{
    if (left->day < right->day) {
        return -1;
    }
    if (left->day > right->day) {
        return 1;
    }
    return temporal_time_compare(left, right);
}

static int64_t timestampdiff_seconds(const struct temporal_date_value *start,
                                     const struct temporal_date_value *end)
{
    int64_t day_diff = temporal_day_number(end) - temporal_day_number(start);
    int64_t start_seconds = ((int64_t)start->hour * MYLITE_TEMPORAL_SECONDS_PER_HOUR) +
                            ((int64_t)start->minute * MYLITE_TEMPORAL_SECONDS_PER_MINUTE) +
                            (int64_t)start->second;
    int64_t end_seconds = ((int64_t)end->hour * MYLITE_TEMPORAL_SECONDS_PER_HOUR) +
                          ((int64_t)end->minute * MYLITE_TEMPORAL_SECONDS_PER_MINUTE) +
                          (int64_t)end->second;
    int64_t seconds = (day_diff * MYLITE_TEMPORAL_SECONDS_PER_DAY) + end_seconds - start_seconds;

    if (seconds > 0 && end->microsecond < start->microsecond) {
        --seconds;
    } else if (seconds < 0 && end->microsecond > start->microsecond) {
        ++seconds;
    }
    return seconds;
}

static int temporal_time_compare(const struct temporal_date_value *left,
                                 const struct temporal_date_value *right)
{
    if (left->hour != right->hour) {
        return left->hour < right->hour ? -1 : 1;
    }
    if (left->minute != right->minute) {
        return left->minute < right->minute ? -1 : 1;
    }
    if (left->second != right->second) {
        return left->second < right->second ? -1 : 1;
    }
    if (left->microsecond != right->microsecond) {
        return left->microsecond < right->microsecond ? -1 : 1;
    }
    return 0;
}

static int eval_temporal_part_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(node, 1U);
    struct mylite_expression_value argument = {0};
    struct temporal_date_value date = {0};
    enum temporal_part_kind part = TEMPORAL_PART_NONE;
    bool valid = false;
    int status = 0;

    if (!temporal_part_from_function(function_id, node->interval_unit, &part)) {
        return -1;
    }

    status = eval_node(child_at(arguments, 0U), context, warnings, &argument);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&argument)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = temporal_date_from_value_with_mode(&argument, true, true, warnings, &date, &valid);
    if (status != 0) {
        goto cleanup;
    }
    if (!valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = temporal_part_value(&date, part),
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static bool temporal_part_from_function(enum mylite_scalar_function_id function_id,
                                        enum mylite_sql_ast_interval_unit unit,
                                        enum temporal_part_kind *out_part)
{
    enum temporal_part_kind part = TEMPORAL_PART_NONE;

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_YEAR:
        part = TEMPORAL_PART_YEAR;
        break;
    case MYLITE_SCALAR_FUNCTION_MONTH:
        part = TEMPORAL_PART_MONTH;
        break;
    case MYLITE_SCALAR_FUNCTION_DAY:
        part = TEMPORAL_PART_DAY;
        break;
    case MYLITE_SCALAR_FUNCTION_HOUR:
        part = TEMPORAL_PART_HOUR;
        break;
    case MYLITE_SCALAR_FUNCTION_MINUTE:
        part = TEMPORAL_PART_MINUTE;
        break;
    case MYLITE_SCALAR_FUNCTION_SECOND:
        part = TEMPORAL_PART_SECOND;
        break;
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
        switch (unit) {
        case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
            part = TEMPORAL_PART_YEAR;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
            part = TEMPORAL_PART_MONTH;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
            part = TEMPORAL_PART_DAY;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
            part = TEMPORAL_PART_HOUR;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
            part = TEMPORAL_PART_MINUTE;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
            part = TEMPORAL_PART_SECOND;
            break;
        case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
        case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
            return false;
        }
        break;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
        return false;
    }
    if (out_part != NULL) {
        *out_part = part;
    }
    return part != TEMPORAL_PART_NONE;
}

static int temporal_part_value(const struct temporal_date_value *date, enum temporal_part_kind part)
{
    switch (part) {
    case TEMPORAL_PART_YEAR:
        return date == NULL ? 0 : date->year;
    case TEMPORAL_PART_MONTH:
        return date == NULL ? 0 : date->month;
    case TEMPORAL_PART_DAY:
        return date == NULL ? 0 : date->day;
    case TEMPORAL_PART_HOUR:
        return date == NULL ? 0 : date->hour;
    case TEMPORAL_PART_MINUTE:
        return date == NULL ? 0 : date->minute;
    case TEMPORAL_PART_SECOND:
        return date == NULL ? 0 : date->second;
    case TEMPORAL_PART_NONE:
        return 0;
    }
    return 0;
}

static int eval_date_arithmetic_function(enum mylite_scalar_function_id function_id,
                                         const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(node, 1U);
    struct mylite_expression_value temporal = {0};
    struct mylite_expression_value amount_value = {0};
    struct temporal_date_value date = {0};
    int64_t amount = 0;
    bool valid = false;
    bool datetime_result = false;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &temporal);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&temporal)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = temporal_date_from_value(&temporal, false, warnings, &date, &valid);
    if (status != 0) {
        goto cleanup;
    }
    if (!valid) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (!date.preserve_fraction_digits) {
        date.fraction_digits = date.microsecond == 0 ? 0U : MYLITE_TEMPORAL_MAX_FSP;
    }

    status = eval_node(child_at(arguments, 1U), context, warnings, &amount_value);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&amount_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = cast_value_to_signed_integer(&amount_value, warnings, &amount);
    if (status != 0) {
        goto cleanup;
    }
    if (!apply_date_interval_arithmetic(function_id, node->interval_unit, &date, amount,
                                        &datetime_result)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = datetime_result ? set_temporal_datetime_text_value(&date, out_value)
                             : set_temporal_date_text_value(&date, out_value);

cleanup:
    mylite_expression_value_deinit(&temporal);
    mylite_expression_value_deinit(&amount_value);
    return status;
}

static bool apply_date_interval_arithmetic(enum mylite_scalar_function_id function_id,
                                           enum mylite_sql_ast_interval_unit unit,
                                           struct temporal_date_value *date, int64_t amount,
                                           bool *out_datetime_result)
{
    int64_t value = amount;
    int64_t scaled = 0;

    if (date == NULL || out_datetime_result == NULL) {
        return false;
    }
    if (function_id == MYLITE_SCALAR_FUNCTION_DATE_SUB ||
        function_id == MYLITE_SCALAR_FUNCTION_SUBDATE) {
        if (value == INT64_MIN) {
            return false;
        }
        value = -value;
    }

    *out_datetime_result = date->has_time || interval_unit_has_time_part(unit);
    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
        return add_temporal_days(date, value);
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        return multiply_interval_amount(value, MYLITE_TEMPORAL_DAYS_PER_WEEK, &scaled) &&
               add_temporal_days(date, scaled);
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
        return add_temporal_months(date, value);
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
        return multiply_interval_amount(value, MYLITE_TEMPORAL_MONTHS_PER_YEAR, &scaled) &&
               add_temporal_months(date, scaled);
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
        return multiply_interval_amount(value, MYLITE_TEMPORAL_SECONDS_PER_HOUR, &scaled) &&
               add_temporal_seconds(date, scaled);
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
        return multiply_interval_amount(value, MYLITE_TEMPORAL_SECONDS_PER_MINUTE, &scaled) &&
               add_temporal_seconds(date, scaled);
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
        return add_temporal_seconds(date, value);
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
        return false;
    }
    return false;
}

static bool multiply_interval_amount(int64_t amount, int64_t factor, int64_t *out_value)
{
    if (out_value == NULL || factor <= 0) {
        return false;
    }
    if (amount > 0 && amount > INT64_MAX / factor) {
        return false;
    }
    if (amount < 0 && amount < INT64_MIN / factor) {
        return false;
    }
    *out_value = amount * factor;
    return true;
}

static bool add_temporal_months(struct temporal_date_value *date, int64_t months)
{
    int64_t month_index = 0;
    int day_limit = 0;

    if (date == NULL) {
        return false;
    }
    month_index =
        ((int64_t)date->year * MYLITE_TEMPORAL_MONTHS_PER_YEAR) + (int64_t)(date->month - 1);
    if ((months > 0 && month_index > INT64_MAX - months) ||
        (months < 0 && month_index < INT64_MIN - months)) {
        return false;
    }
    month_index += months;
    if (month_index < 0 ||
        month_index > ((int64_t)MYLITE_TEMPORAL_MAX_YEAR * MYLITE_TEMPORAL_MONTHS_PER_YEAR) +
                          (MYLITE_TEMPORAL_MONTHS_PER_YEAR - 1)) {
        return false;
    }

    date->year = (int)(month_index / MYLITE_TEMPORAL_MONTHS_PER_YEAR);
    date->month = (int)(month_index % MYLITE_TEMPORAL_MONTHS_PER_YEAR) + 1;
    day_limit = temporal_month_day_limit(date->year, date->month);
    if (date->day > day_limit) {
        date->day = day_limit;
    }
    return true;
}

static bool add_temporal_days(struct temporal_date_value *date, int64_t days)
{
    int64_t day_number = 0;

    if (date == NULL) {
        return false;
    }
    day_number = temporal_day_number(date);
    if ((days > 0 && day_number > INT64_MAX - days) ||
        (days < 0 && day_number < INT64_MIN - days)) {
        return false;
    }
    return temporal_date_from_day_number(day_number + days, date);
}

static bool add_temporal_seconds(struct temporal_date_value *date, int64_t seconds)
{
    enum { seconds_per_minute = 60, seconds_per_hour = 3600, seconds_per_day = 86400 };
    int64_t current = 0;
    int64_t total = 0;
    int64_t day_delta = 0;
    int64_t remainder = 0;

    if (date == NULL) {
        return false;
    }
    current = ((int64_t)date->hour * seconds_per_hour) +
              ((int64_t)date->minute * seconds_per_minute) + date->second;
    if ((seconds > 0 && current > INT64_MAX - seconds) ||
        (seconds < 0 && current < INT64_MIN - seconds)) {
        return false;
    }
    total = current + seconds;
    day_delta = total / seconds_per_day;
    remainder = total % seconds_per_day;
    if (remainder < 0) {
        remainder += seconds_per_day;
        --day_delta;
    }
    if (!add_temporal_days(date, day_delta)) {
        return false;
    }
    date->hour = (int)(remainder / seconds_per_hour);
    remainder %= seconds_per_hour;
    date->minute = (int)(remainder / seconds_per_minute);
    date->second = (int)(remainder % seconds_per_minute);
    date->has_time = true;
    return true;
}

static bool temporal_date_from_day_number(int64_t day_number, struct temporal_date_value *out_date)
{
    int low = 0;
    int high = MYLITE_TEMPORAL_MAX_YEAR;
    int year = 0;
    int month = 1;
    int64_t max_day_number = 0;
    int64_t remaining = 0;

    if (out_date == NULL) {
        return false;
    }
    max_day_number = temporal_day_number(&(const struct temporal_date_value){
        .year = MYLITE_TEMPORAL_MAX_YEAR,
        .month = MYLITE_TEMPORAL_MONTHS_PER_YEAR,
        .day = MYLITE_TEMPORAL_MAX_MONTH_DAY,
    });
    if (day_number < 0 || day_number > max_day_number) {
        return false;
    }

    while (low <= high) {
        int mid = low + ((high - low) / 2);
        int64_t start = temporal_days_before_year(mid);

        if (start <= day_number) {
            year = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    remaining = day_number - temporal_days_before_year(year);
    for (month = 1; month <= MYLITE_TEMPORAL_MONTHS_PER_YEAR; ++month) {
        int days = temporal_month_day_limit(year, month);

        if (remaining < days) {
            break;
        }
        remaining -= days;
    }

    out_date->year = year;
    out_date->month = month;
    out_date->day = (int)remaining + 1;
    return true;
}

static bool interval_unit_has_time_part(enum mylite_sql_ast_interval_unit unit)
{
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
        return true;
    }
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE) {
        return true;
    }
    return unit == MYLITE_SQL_AST_INTERVAL_UNIT_SECOND;
}

static int set_temporal_datetime_text_value(const struct temporal_date_value *date,
                                            struct mylite_expression_value *out_value)
{
    enum {
        temporal_datetime_text_length = 19U,
        temporal_fraction_text_length = 6U,
        temporal_datetime_fraction_text_length = 26U,
    };
    unsigned int fraction_digits = date == NULL ? 0U : date->fraction_digits;
    char buffer[temporal_datetime_fraction_text_length + 1U];
    int length = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                          date == NULL ? 0 : date->year, date == NULL ? 0 : date->month,
                          date == NULL ? 0 : date->day, date == NULL ? 0 : date->hour,
                          date == NULL ? 0 : date->minute, date == NULL ? 0 : date->second);

    if (length != temporal_datetime_text_length) {
        return -1;
    }
    if (fraction_digits > MYLITE_TEMPORAL_MAX_FSP) {
        fraction_digits = MYLITE_TEMPORAL_MAX_FSP;
    }
    if (fraction_digits != 0U) {
        char fraction[temporal_fraction_text_length + 1U];
        int fraction_length =
            snprintf(fraction, sizeof(fraction), "%06d", date == NULL ? 0 : date->microsecond);

        if (fraction_length != temporal_fraction_text_length ||
            length + 1 + (int)fraction_digits >= (int)sizeof(buffer)) {
            return -1;
        }
        buffer[length] = '.';
        ++length;
        memcpy(buffer + length, fraction, fraction_digits);
        length += (int)fraction_digits;
        buffer[length] = '\0';
    }
    int status = set_text_value(buffer, (size_t)length, out_value);

    if (status == 0 && date != NULL) {
        out_value->preserve_temporal_fraction_digits = date->preserve_fraction_digits;
    }
    return status;
}

static int temporal_date_from_value(const struct mylite_expression_value *value,
                                    bool warn_approximate_fraction,
                                    struct mylite_expression_warnings *warnings,
                                    struct temporal_date_value *out_date, bool *out_valid)
{
    return temporal_date_from_value_with_mode(value, warn_approximate_fraction, false, warnings,
                                              out_date, out_valid);
}

static int temporal_date_from_value_with_mode(const struct mylite_expression_value *value,
                                              bool warn_approximate_fraction,
                                              bool allow_incomplete_date,
                                              struct mylite_expression_warnings *warnings,
                                              struct temporal_date_value *out_date, bool *out_valid)
{
    struct temporal_date_source source = {0};
    bool valid = false;
    int status = 0;

    if (out_date == NULL || out_valid == NULL) {
        return -1;
    }
    *out_date = (struct temporal_date_value){0};
    *out_valid = false;

    status = temporal_date_source_from_value(value, &source);
    if (status != 0) {
        return status;
    }

    valid = parse_temporal_date_source(&source, warn_approximate_fraction, allow_incomplete_date,
                                       out_date);
    if (!valid && out_date->warning_kind == TEMPORAL_DATE_WARNING_NONE) {
        out_date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
    }
    if (out_date->warning_kind != TEMPORAL_DATE_WARNING_NONE) {
        status = append_temporal_date_warning(warnings, out_date->warning_kind, source.warning_text,
                                              source.warning_length);
        if (status != 0) {
            return status;
        }
    }
    out_date->preserve_fraction_digits = value->preserve_temporal_fraction_digits;
    *out_valid = valid;
    return 0;
}

static int temporal_date_source_from_value(const struct mylite_expression_value *value,
                                           struct temporal_date_source *out_source)
{
    int length = 0;

    if (value == NULL || out_source == NULL) {
        return -1;
    }
    *out_source = (struct temporal_date_source){0};
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_TEXT:
        out_source->text = value->text_value == NULL ? "" : value->text_value;
        out_source->length = value->text_value == NULL ? 0U : value->text_length;
        out_source->warning_text = out_source->text;
        out_source->warning_length = out_source->length;
        return 0;
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(out_source->buffer, sizeof(out_source->buffer), "%lld",
                          (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(out_source->buffer, sizeof(out_source->buffer), "%llu",
                          (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL: {
        int original_length = format_compact_real_text(
            value->real_value, out_source->warning_buffer, sizeof(out_source->warning_buffer));
        char *dot = NULL;

        if (original_length <= 0 || (size_t)original_length >= sizeof(out_source->buffer)) {
            return -1;
        }
        memcpy(out_source->buffer, out_source->warning_buffer, (size_t)original_length + 1U);
        out_source->approximate_fraction = temporal_date_source_has_fraction(out_source);
        out_source->warning_text = out_source->warning_buffer;
        out_source->warning_length = (size_t)original_length;
        dot = strchr(out_source->buffer, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        out_source->text = out_source->buffer;
        out_source->length = strlen(out_source->buffer);
        out_source->numeric = true;
        return 0;
    }
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }
    if (length <= 0 || (size_t)length >= sizeof(out_source->buffer)) {
        return -1;
    }
    out_source->text = out_source->buffer;
    out_source->length = (size_t)length;
    out_source->warning_text = out_source->buffer;
    out_source->warning_length = (size_t)length;
    out_source->numeric = true;
    return 0;
}

static bool temporal_date_source_has_fraction(const struct temporal_date_source *source)
{
    const char *dot = source == NULL ? NULL : strchr(source->buffer, '.');

    if (dot == NULL) {
        return false;
    }
    for (++dot; *dot != '\0'; ++dot) {
        if (*dot >= '1' && *dot <= '9') {
            return true;
        }
    }
    return false;
}

static bool parse_temporal_date_source(const struct temporal_date_source *source,
                                       bool warn_approximate_fraction, bool allow_incomplete_date,
                                       struct temporal_date_value *out_date)
{
    if (source == NULL || out_date == NULL || source->text == NULL || source->length == 0U) {
        return false;
    }
    if (source->numeric) {
        bool valid = parse_temporal_compact_date(source->text, source->length, true,
                                                 allow_incomplete_date, out_date);

        if (valid && warn_approximate_fraction && source->approximate_fraction &&
            out_date->warning_kind == TEMPORAL_DATE_WARNING_NONE) {
            out_date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        }
        return valid;
    }
    if (parse_temporal_delimited_date(source->text, source->length, allow_incomplete_date,
                                      out_date)) {
        return true;
    }
    return parse_temporal_compact_date(source->text, source->length, false, allow_incomplete_date,
                                       out_date);
}

static bool parse_temporal_delimited_date(const char *text, size_t length,
                                          bool allow_incomplete_date,
                                          struct temporal_date_value *out_date)
{
    size_t offset = 0U;
    size_t year_start = 0U;
    int year = 0;
    int month = 0;
    int day = 0;
    bool all_zero = false;

    if (text == NULL || out_date == NULL) {
        return false;
    }
    while (offset < length && text[offset] >= '0' && text[offset] <= '9') {
        year = (year * MYLITE_EXPRESSION_DECIMAL_BASE) + (int)(text[offset] - '0');
        ++offset;
    }
    if ((offset - year_start != 2U && offset - year_start != 4U) || offset >= length ||
        text[offset] != '-') {
        return false;
    }
    year = temporal_normalized_year((struct temporal_year_parts){
        .year = year,
        .digit_count = offset - year_start,
    });
    ++offset;
    if (!parse_temporal_unsigned_part(text, length, &offset,
                                      (struct temporal_digit_width){.minimum = 1U, .maximum = 2U},
                                      &month) ||
        offset >= length || text[offset] != '-') {
        return false;
    }
    ++offset;
    if (!parse_temporal_unsigned_part(text, length, &offset,
                                      (struct temporal_digit_width){.minimum = 1U, .maximum = 2U},
                                      &day)) {
        return false;
    }
    all_zero = allow_incomplete_date && temporal_date_parts_are_all_zero(year, month, day);
    if (!temporal_date_parts_are_valid_for_mode(year, month, day, allow_incomplete_date) &&
        !all_zero) {
        out_date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
        return false;
    }

    *out_date = (struct temporal_date_value){
        .year = year,
        .month = month,
        .day = day,
    };
    if (!parse_temporal_time_suffix(text, length, offset, out_date)) {
        return false;
    }
    if (all_zero && !out_date->has_time) {
        out_date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
        return false;
    }
    return true;
}

static bool parse_temporal_compact_date(const char *text, size_t length, bool numeric,
                                        bool allow_incomplete_date,
                                        struct temporal_date_value *out_date)
{
    char padded[MYLITE_TEMPORAL_LONG_DATETIME_DIGITS + 1U];
    const char *digits = text;
    size_t digit_length = length;
    size_t year_digits = 0U;
    size_t offset = 0U;
    int year = 0;
    int month = 0;
    int day = 0;
    bool has_time = false;
    struct temporal_date_value time = {0};

    if (text == NULL || out_date == NULL || length == 0U) {
        return false;
    }
    if (!temporal_text_is_digits(text, length) ||
        !prepare_temporal_compact_digits(text, length, numeric, padded, &digits, &digit_length)) {
        return false;
    }

    year_digits = temporal_compact_year_digit_count(digit_length);
    if (!parse_temporal_fixed_digits(digits,
                                     (struct temporal_digit_range){
                                         .offset = offset,
                                         .count = year_digits,
                                     },
                                     &year)) {
        return false;
    }
    year = temporal_normalized_year((struct temporal_year_parts){
        .year = year,
        .digit_count = year_digits,
    });
    offset += year_digits;
    if (!parse_temporal_fixed_digits(
            digits, (struct temporal_digit_range){.offset = offset, .count = 2U}, &month) ||
        !parse_temporal_fixed_digits(
            digits, (struct temporal_digit_range){.offset = offset + 2U, .count = 2U}, &day)) {
        return false;
    }
    offset += 4U;
    has_time = temporal_compact_datetime_has_time(digit_length);
    if (has_time && !parse_temporal_compact_time(digits, offset, &time)) {
        out_date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
        return false;
    }
    if (!temporal_date_parts_are_valid_for_mode(year, month, day, allow_incomplete_date)) {
        out_date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
        return false;
    }

    *out_date = (struct temporal_date_value){
        .year = year,
        .month = month,
        .day = day,
    };
    if (has_time) {
        out_date->hour = time.hour;
        out_date->minute = time.minute;
        out_date->second = time.second;
        out_date->has_time = true;
    }
    return true;
}

static bool temporal_text_is_digits(const char *text, size_t length)
{
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

static bool prepare_temporal_compact_digits(const char *text, size_t length, bool numeric,
                                            char *padded, const char **out_digits,
                                            size_t *out_digit_length)
{
    size_t digit_length = length;

    if (text == NULL || padded == NULL || out_digits == NULL || out_digit_length == NULL ||
        !temporal_compact_digit_length(length, numeric, &digit_length)) {
        return false;
    }
    if (!numeric) {
        *out_digits = text;
        *out_digit_length = digit_length;
        return true;
    }

    memset(padded, '0', digit_length - length);
    memcpy(padded + (digit_length - length), text, length);
    padded[digit_length] = '\0';
    *out_digits = padded;
    *out_digit_length = digit_length;
    return true;
}

static bool temporal_numeric_compact_digit_length(size_t length, size_t *out_digit_length)
{
    if (out_digit_length == NULL) {
        return false;
    }
    if (length <= MYLITE_TEMPORAL_SHORT_DATE_DIGITS) {
        *out_digit_length = MYLITE_TEMPORAL_SHORT_DATE_DIGITS;
        return true;
    }
    if (length <= MYLITE_TEMPORAL_LONG_DATE_DIGITS) {
        *out_digit_length = MYLITE_TEMPORAL_LONG_DATE_DIGITS;
        return true;
    }
    if (length <= MYLITE_TEMPORAL_SHORT_DATETIME_DIGITS) {
        *out_digit_length = MYLITE_TEMPORAL_SHORT_DATETIME_DIGITS;
        return true;
    }
    if (length <= MYLITE_TEMPORAL_LONG_DATETIME_DIGITS) {
        *out_digit_length = MYLITE_TEMPORAL_LONG_DATETIME_DIGITS;
        return true;
    }
    return false;
}

static bool temporal_compact_digit_length(size_t length, bool numeric, size_t *out_digit_length)
{
    if (numeric) {
        return temporal_numeric_compact_digit_length(length, out_digit_length);
    }
    if (out_digit_length == NULL) {
        return false;
    }
    if (length != MYLITE_TEMPORAL_SHORT_DATE_DIGITS && length != MYLITE_TEMPORAL_LONG_DATE_DIGITS &&
        length != MYLITE_TEMPORAL_SHORT_DATETIME_DIGITS &&
        length != MYLITE_TEMPORAL_LONG_DATETIME_DIGITS) {
        return false;
    }
    *out_digit_length = length;
    return true;
}

static size_t temporal_compact_year_digit_count(size_t digit_length)
{
    if (digit_length == MYLITE_TEMPORAL_LONG_DATE_DIGITS ||
        digit_length == MYLITE_TEMPORAL_LONG_DATETIME_DIGITS) {
        return 4U;
    }
    return 2U;
}

static bool temporal_compact_datetime_has_time(size_t digit_length)
{
    return digit_length == MYLITE_TEMPORAL_SHORT_DATETIME_DIGITS ||
           digit_length == MYLITE_TEMPORAL_LONG_DATETIME_DIGITS;
}

static bool parse_temporal_compact_time(const char *digits, size_t offset,
                                        struct temporal_date_value *out_date)
{
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (!parse_temporal_fixed_digits(
            digits, (struct temporal_digit_range){.offset = offset, .count = 2U}, &hour) ||
        !parse_temporal_fixed_digits(
            digits, (struct temporal_digit_range){.offset = offset + 2U, .count = 2U}, &minute) ||
        !parse_temporal_fixed_digits(
            digits, (struct temporal_digit_range){.offset = offset + 4U, .count = 2U}, &second) ||
        !temporal_time_parts_are_valid(hour, minute, second)) {
        return false;
    }
    if (out_date != NULL) {
        out_date->hour = hour;
        out_date->minute = minute;
        out_date->second = second;
        out_date->has_time = true;
    }
    return true;
}

static bool parse_temporal_time_suffix(const char *text, size_t length, size_t offset,
                                       struct temporal_date_value *date)
{
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (offset >= length) {
        return true;
    }
    if (text[offset] != ' ' && text[offset] != 'T') {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        return true;
    }
    ++offset;
    while (offset < length && text[offset] == ' ') {
        ++offset;
    }
    if (offset >= length || text[offset] < '0' || text[offset] > '9') {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        return true;
    }
    if (!parse_temporal_unsigned_part(text, length, &offset,
                                      (struct temporal_digit_width){.minimum = 1U, .maximum = 2U},
                                      &hour) ||
        offset >= length || text[offset] != ':') {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        return true;
    }
    ++offset;
    if (!parse_temporal_unsigned_part(text, length, &offset,
                                      (struct temporal_digit_width){.minimum = 1U, .maximum = 2U},
                                      &minute) ||
        offset >= length || text[offset] != ':') {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        return true;
    }
    ++offset;
    if (!parse_temporal_unsigned_part(text, length, &offset,
                                      (struct temporal_digit_width){.minimum = 1U, .maximum = 2U},
                                      &second)) {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATE;
        return true;
    }
    if (!temporal_time_parts_are_valid(hour, minute, second)) {
        date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
        return false;
    }
    date->hour = hour;
    date->minute = minute;
    date->second = second;
    date->has_time = true;
    if (offset < length && text[offset] == '.') {
        int microsecond = 0;
        unsigned int fraction_digits = 0U;

        ++offset;
        if (!parse_temporal_fraction(text, length, &offset, &microsecond, &fraction_digits)) {
            date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATETIME;
            return true;
        }
        if (microsecond >= MYLITE_TEMPORAL_MICROSECOND_LIMIT) {
            microsecond = 0;
            fraction_digits = 0U;
            if (!add_temporal_seconds(date, 1)) {
                date->warning_kind = TEMPORAL_DATE_WARNING_INCORRECT;
                return false;
            }
        }
        date->microsecond = microsecond;
        date->fraction_digits = fraction_digits;
    }
    if (offset < length) {
        date->warning_kind = TEMPORAL_DATE_WARNING_TRUNCATED_DATETIME;
    }
    return true;
}

static bool parse_temporal_fixed_digits(const char *text, struct temporal_digit_range range,
                                        int *out_value)
{
    int value = 0;

    if (text == NULL || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < range.count; ++index) {
        char character = text[range.offset + index];

        if (character < '0' || character > '9') {
            return false;
        }
        value = (value * MYLITE_EXPRESSION_DECIMAL_BASE) + (int)(character - '0');
    }
    *out_value = value;
    return true;
}

static bool parse_temporal_unsigned_part(const char *text, size_t length, size_t *offset,
                                         struct temporal_digit_width width, int *out_value)
{
    size_t cursor = offset == NULL ? 0U : *offset;
    size_t digits = 0U;
    int value = 0;

    if (text == NULL || offset == NULL || out_value == NULL) {
        return false;
    }
    while (cursor < length && digits < width.maximum && text[cursor] >= '0' &&
           text[cursor] <= '9') {
        value = (value * MYLITE_EXPRESSION_DECIMAL_BASE) + (int)(text[cursor] - '0');
        ++cursor;
        ++digits;
    }
    if (digits < width.minimum) {
        return false;
    }
    *offset = cursor;
    *out_value = value;
    return true;
}

static bool parse_temporal_fraction(const char *text, size_t length, size_t *offset,
                                    int *out_microsecond, unsigned int *out_digits)
{
    size_t cursor = offset == NULL ? 0U : *offset;
    size_t digits = 0U;
    int microsecond = 0;
    bool round_up = false;

    if (text == NULL || offset == NULL || out_microsecond == NULL || out_digits == NULL) {
        return false;
    }
    while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9') {
        if (digits < MYLITE_TEMPORAL_MAX_FSP) {
            microsecond =
                (microsecond * MYLITE_EXPRESSION_DECIMAL_BASE) + (int)(text[cursor] - '0');
        } else if (digits == MYLITE_TEMPORAL_MAX_FSP && text[cursor] >= '5') {
            round_up = true;
        }
        ++cursor;
        ++digits;
    }
    if (digits == 0U) {
        return false;
    }
    for (size_t index = digits; index < MYLITE_TEMPORAL_MAX_FSP; ++index) {
        microsecond *= MYLITE_EXPRESSION_DECIMAL_BASE;
    }
    if (round_up) {
        ++microsecond;
    }
    *offset = cursor;
    *out_microsecond = microsecond;
    *out_digits = digits > MYLITE_TEMPORAL_MAX_FSP ? MYLITE_TEMPORAL_MAX_FSP : (unsigned int)digits;
    return true;
}

static bool temporal_date_parts_are_valid(int year, int month, int day)
{
    if (year < 0 || year > MYLITE_TEMPORAL_MAX_YEAR || month < 1 ||
        month > MYLITE_TEMPORAL_MONTHS_PER_YEAR || day < 1) {
        return false;
    }
    return day <= temporal_month_day_limit(year, month);
}

static bool temporal_date_parts_are_valid_for_mode(int year, int month, int day,
                                                   bool allow_incomplete_date)
{
    if (!allow_incomplete_date) {
        return temporal_date_parts_are_valid(year, month, day);
    }
    if (year < 0 || year > MYLITE_TEMPORAL_MAX_YEAR || month < 0 ||
        month > MYLITE_TEMPORAL_MONTHS_PER_YEAR || day < 0 || day > MYLITE_TEMPORAL_MAX_MONTH_DAY) {
        return false;
    }
    if (year == 0 && month == 0 && day == 0) {
        return false;
    }
    if (month == 0 || day == 0) {
        return true;
    }
    return day <= temporal_month_day_limit(year, month);
}

static bool temporal_date_parts_are_all_zero(int year, int month, int day)
{
    return year == 0 && month == 0 && day == 0;
}

static bool temporal_time_parts_are_valid(int hour, int minute, int second)
{
    return hour >= 0 && hour <= MYLITE_TEMPORAL_MAX_HOUR && minute >= 0 &&
           minute <= MYLITE_TEMPORAL_MAX_MINUTE_SECOND && second >= 0 &&
           second <= MYLITE_TEMPORAL_MAX_MINUTE_SECOND;
}

static bool temporal_year_is_leap(int year)
{
    if (year <= 0) {
        return false;
    }
    return (year % MYLITE_TEMPORAL_LEAP_YEAR_INTERVAL == 0 &&
            year % MYLITE_TEMPORAL_CENTURY_YEARS != 0) ||
           year % MYLITE_TEMPORAL_QUADRICENTENNIAL_YEARS == 0;
}

static int temporal_month_day_limit(int year, int month)
{
    static const int days_by_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == MYLITE_TEMPORAL_FEBRUARY && temporal_year_is_leap(year)) {
        return MYLITE_TEMPORAL_LEAP_FEBRUARY_DAYS;
    }
    if (month < 1 || month > MYLITE_TEMPORAL_MONTHS_PER_YEAR) {
        return 0;
    }
    return days_by_month[month - 1];
}

static int temporal_normalized_year(struct temporal_year_parts parts)
{
    if (parts.digit_count != 2U) {
        return parts.year;
    }
    return parts.year >= MYLITE_TEMPORAL_TWO_DIGIT_YEAR_PIVOT
               ? MYLITE_TEMPORAL_TWO_DIGIT_YEAR_HIGH_CENTURY + parts.year
               : MYLITE_TEMPORAL_TWO_DIGIT_YEAR_LOW_CENTURY + parts.year;
}

static int64_t temporal_day_number(const struct temporal_date_value *date)
{
    if (date == NULL) {
        return 0;
    }
    return temporal_days_before_year(date->year) +
           temporal_days_before_month(date->year, date->month) + (int64_t)(date->day - 1);
}

static int64_t temporal_days_before_year(int year)
{
    int64_t previous = year <= 0 ? 0 : (int64_t)year - 1;

    return ((int64_t)year * MYLITE_TEMPORAL_DAYS_PER_COMMON_YEAR) +
           (previous / MYLITE_TEMPORAL_LEAP_YEAR_INTERVAL) -
           (previous / MYLITE_TEMPORAL_CENTURY_YEARS) +
           (previous / MYLITE_TEMPORAL_QUADRICENTENNIAL_YEARS);
}

static int temporal_days_before_month(int year, int month)
{
    static const int common[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    static const int leap[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

    if (month < 1 || month > MYLITE_TEMPORAL_MONTHS_PER_YEAR) {
        return 0;
    }
    return temporal_year_is_leap(year) ? leap[month - 1] : common[month - 1];
}

static int set_temporal_date_text_value(const struct temporal_date_value *date,
                                        struct mylite_expression_value *out_value)
{
    enum { temporal_date_text_length = 10U };
    char buffer[temporal_date_text_length + 1U];
    int length = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", date == NULL ? 0 : date->year,
                          date == NULL ? 0 : date->month, date == NULL ? 0 : date->day);

    if (length != temporal_date_text_length) {
        return -1;
    }
    return set_text_value(buffer, temporal_date_text_length, out_value);
}

static int append_temporal_date_warning(struct mylite_expression_warnings *warnings,
                                        enum temporal_date_warning_kind warning_kind,
                                        const char *text, size_t text_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    const char *format = "Incorrect datetime value: '%.*s'";
    int preview_length = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                             ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                             : (int)text_length;
    int length = 0;

    if (warning_kind == TEMPORAL_DATE_WARNING_TRUNCATED_DATE) {
        format = "Truncated incorrect date value: '%.*s'";
    } else if (warning_kind == TEMPORAL_DATE_WARNING_TRUNCATED_DATETIME) {
        format = "Truncated incorrect datetime value: '%.*s'";
    }
    length = snprintf(message, sizeof(message), format, preview_length, text == NULL ? "" : text);
    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int eval_concat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    char *result = copy_span_text("", 0U);
    size_t result_length = 0U;

    if (result == NULL) {
        return -1;
    }
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        char *text = NULL;
        int status = eval_node(argument, context, warnings, &value);

        if (status == 0 && is_null(&value)) {
            free(result);
            mylite_expression_value_deinit(&value);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return 0;
        }
        if (status == 0) {
            status = value_to_string(&value, &text);
        }
        if (status == 0) {
            status = append_text(&result, &result_length, text, strlen(text));
        }
        free(text);
        mylite_expression_value_deinit(&value);
        if (status != 0) {
            free(result);
            return status;
        }
    }
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = result_length;
    return 0;
}

static int eval_concat_ws_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value separator_value = {0};
    char *separator = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    bool appended = false;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &separator_value);

    if (status != 0 || is_null(&separator_value)) {
        mylite_expression_value_deinit(&separator_value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string(&separator_value, &separator);
    mylite_expression_value_deinit(&separator_value);
    if (status != 0) {
        return status;
    }

    result = copy_span_text("", 0U);
    if (result == NULL) {
        free(separator);
        return -1;
    }
    for (const struct mylite_sql_ast_node *argument = child_at(arguments, 1U); argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        char *text = NULL;

        status = eval_node(argument, context, warnings, &value);
        if (status == 0 && is_null(&value)) {
            mylite_expression_value_deinit(&value);
            continue;
        }
        if (status == 0) {
            status = value_to_string(&value, &text);
        }
        if (status == 0 && appended) {
            status = append_text(&result, &result_length, separator, strlen(separator));
        }
        if (status == 0) {
            status = append_text(&result, &result_length, text, strlen(text));
            appended = true;
        }
        free(text);
        mylite_expression_value_deinit(&value);
        if (status != 0) {
            free(separator);
            free(result);
            return status;
        }
    }
    free(separator);
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = result_length;
    return 0;
}

static int eval_unary_string_function(enum mylite_scalar_function_id function_id,
                                      const struct mylite_sql_ast_node *arguments,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        mylite_expression_value_deinit(&value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string_with_length(&value, &text, &text_length);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_LENGTH:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (int64_t)text_length};
        free(text);
        return 0;
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH: {
        int64_t count = 0;

        status = utf8_char_count(text, &count);
        free(text);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = count};
        }
        return status;
    }
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER: {
        bool lower = function_id == MYLITE_SCALAR_FUNCTION_LOWER;

        for (char *cursor = text; *cursor != '\0'; ++cursor) {
            unsigned char character = (unsigned char)*cursor;

            if (character >= 'A' && character <= 'Z' && lower) {
                *cursor = (char)(character - 'A' + 'a');
            } else if (character >= 'a' && character <= 'z' && !lower) {
                *cursor = (char)(character - 'a' + 'A');
            }
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = text;
        out_value->text_length = strlen(text);
        return 0;
    }
    default:
        free(text);
        return -1;
    }
}

static int eval_leftmost_code_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        mylite_expression_value_deinit(&value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string(&value, &text);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }

    if (text[0] == '\0') {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
    } else if (function_id == MYLITE_SCALAR_FUNCTION_ASCII) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (unsigned char)text[0]};
    } else {
        uint64_t result = 0U;
        size_t character_length = utf8_first_character_length(text);

        for (size_t index = 0U; index < character_length; ++index) {
            result = (result * MYLITE_EXPRESSION_ORD_BYTE_BASE) + (unsigned char)text[index];
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (int64_t)result};
    }
    free(text);
    return 0;
}

static int eval_left_right_function(enum mylite_scalar_function_id function_id,
                                    const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value count = {0};
    char *text = NULL;
    int64_t char_count = 0;
    int64_t requested = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &count);
    }
    if (status != 0 || is_null(&value) || is_null(&count)) {
        mylite_expression_value_deinit(&value);
        mylite_expression_value_deinit(&count);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string(&value, &text);
    if (status == 0) {
        status = utf8_char_count(text, &char_count);
    }
    if (status == 0) {
        requested = mylite_expression_value_to_int64(&count);
        if (requested <= 0) {
            status = set_text_value("", 0U, out_value);
        } else if (function_id == MYLITE_SCALAR_FUNCTION_LEFT) {
            status = set_text_value(text, utf8_offset_for_chars(text, requested), out_value);
        } else {
            int64_t skip = requested >= char_count ? 0 : char_count - requested;
            size_t offset = utf8_offset_for_chars(text, skip);

            status = set_text_value(text + offset, strlen(text + offset), out_value);
        }
    }
    free(text);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&count);
    return status;
}

static int eval_substring_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    struct substring_range range = {0};
    char *text = NULL;
    size_t arity = child_count(arguments);
    int64_t char_count = 0;
    int status = 0;

    for (size_t index = 0U; index < arity; ++index) {
        status = eval_node(child_at(arguments, index), context, warnings, &values[index]);
        if (status != 0) {
            goto cleanup;
        }
        if (is_null(&values[index])) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            goto cleanup;
        }
    }

    status = value_to_string(&values[0], &text);
    if (status != 0) {
        goto cleanup;
    }
    status = utf8_char_count(text, &char_count);
    if (status != 0) {
        goto cleanup;
    }

    range = substring_range_from_arguments(values, (struct substring_context){
                                                       .char_count = char_count,
                                                       .arity = arity,
                                                   });
    if (range.empty) {
        status = set_text_value("", 0U, out_value);
        goto cleanup;
    }

    {
        size_t start_offset = utf8_offset_for_chars(text, range.start);
        size_t end_offset = utf8_offset_for_chars(text, range.start + range.take);

        status = set_text_value(text + start_offset, end_offset - start_offset, out_value);
    }

cleanup:
    free(text);
    for (size_t index = 0U; index < arity; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static struct substring_range
substring_range_from_arguments(const struct mylite_expression_value *values,
                               struct substring_context context)
{
    struct substring_range range = {.empty = true};
    int64_t position = mylite_expression_value_to_int64(&values[1]);

    if (position == 0 || context.char_count == 0) {
        return range;
    }
    if (position > 0) {
        range.start = position - 1;
        if (range.start >= context.char_count) {
            return range;
        }
    } else {
        if (position < -context.char_count) {
            return range;
        }
        range.start = context.char_count + position;
    }

    range.take = context.char_count - range.start;
    if (context.arity == 3U) {
        int64_t requested = mylite_expression_value_to_int64(&values[2]);

        if (requested <= 0) {
            return range;
        }
        if (requested < range.take) {
            range.take = requested;
        }
    }
    range.empty = false;
    return range;
}

static int eval_substring_index_function(const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    char *text = NULL;
    char *delimiter = NULL;
    size_t text_length = 0U;
    size_t delimiter_length = 0U;
    uint64_t requested = 0U;
    bool negative_count = false;
    int status = 0;

    for (size_t index = 0U; index < 3U; ++index) {
        status = eval_node(child_at(arguments, index), context, warnings, &values[index]);
        if (status != 0) {
            goto cleanup;
        }
    }
    if (is_null(&values[0]) || is_null(&values[1]) || is_null(&values[2])) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string_with_length(&values[0], &text, &text_length);
    if (status == 0) {
        status = value_to_string_with_length(&values[1], &delimiter, &delimiter_length);
    }
    if (status == 0) {
        status =
            substring_index_count_from_value(&values[2], warnings, &requested, &negative_count);
    }
    if (status == 0) {
        status = substring_index_text_value(
            (struct substring_index_input){
                .text = text == NULL ? "" : text,
                .text_length = text_length,
                .delimiter = delimiter == NULL ? "" : delimiter,
                .delimiter_length = delimiter_length,
                .requested = requested,
                .negative_count = negative_count,
            },
            out_value);
    }

cleanup:
    free(text);
    free(delimiter);
    for (size_t index = 0U; index < 3U; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static int substring_index_count_from_value(const struct mylite_expression_value *value,
                                            struct mylite_expression_warnings *warnings,
                                            uint64_t *out_requested, bool *out_negative)
{
    int64_t signed_count = 0;

    if (value == NULL || out_requested == NULL || out_negative == NULL) {
        return -1;
    }
    *out_requested = 0U;
    *out_negative = false;
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_requested = value->uint64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return substring_index_count_from_text(value->text_value == NULL ? "" : value->text_value,
                                               warnings, out_requested, out_negative);
    }
    if (cast_value_to_signed_integer(value, warnings, &signed_count) != 0) {
        return -1;
    }
    if (signed_count < 0) {
        *out_negative = true;
        *out_requested = (uint64_t)(-(signed_count + 1)) + 1U;
        return 0;
    }
    *out_requested = (uint64_t)signed_count;
    return 0;
}

static int substring_index_count_from_text(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_requested, bool *out_negative)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_requested = 0U;
        *out_negative = false;
        return 0;
    }
    if (parsed.negative) {
        *out_requested = parsed.magnitude;
        *out_negative = parsed.magnitude != 0U;
        return 0;
    }

    int64_t signed_count = signed_integer_from_uint64(parsed.magnitude);

    if (signed_count < 0) {
        *out_requested = (uint64_t)(-(signed_count + 1)) + 1U;
        *out_negative = true;
        return 0;
    }
    *out_requested = (uint64_t)signed_count;
    *out_negative = false;
    return 0;
}

static int substring_index_text_value(struct substring_index_input input,
                                      struct mylite_expression_value *out_value)
{
    if (input.requested == 0U || input.delimiter_length == 0U) {
        return set_text_value("", 0U, out_value);
    }
    if (!input.negative_count) {
        return substring_index_positive_value(input, input.requested, out_value);
    }
    return substring_index_negative_value(input, input.requested, out_value);
}

static int substring_index_positive_value(struct substring_index_input input, uint64_t requested,
                                          struct mylite_expression_value *out_value)
{
    uint64_t matches = 0U;
    size_t offset = 0U;
    size_t match_offset = 0U;

    while (find_next_substring_index_delimiter(input, offset, &match_offset)) {
        ++matches;
        if (matches == requested) {
            return set_text_value(input.text, match_offset, out_value);
        }
        offset = match_offset + input.delimiter_length;
    }
    return set_text_value(input.text, input.text_length, out_value);
}

static int substring_index_negative_value(struct substring_index_input input, uint64_t requested,
                                          struct mylite_expression_value *out_value)
{
    size_t delimiter_count = count_substring_index_delimiters(input);
    size_t offset = 0U;
    size_t match_offset = 0U;
    size_t target = 0U;

    if (requested > (uint64_t)delimiter_count) {
        return set_text_value(input.text, input.text_length, out_value);
    }
    target = delimiter_count - (size_t)requested;
    for (size_t index = 0U; index <= target; ++index) {
        if (!find_next_substring_index_delimiter(input, offset, &match_offset)) {
            return set_text_value(input.text, input.text_length, out_value);
        }
        offset = match_offset + input.delimiter_length;
    }
    return set_text_value(input.text + offset, input.text_length - offset, out_value);
}

static size_t count_substring_index_delimiters(struct substring_index_input input)
{
    size_t count = 0U;
    size_t offset = 0U;
    size_t match_offset = 0U;

    while (find_next_substring_index_delimiter(input, offset, &match_offset)) {
        ++count;
        offset = match_offset + input.delimiter_length;
    }
    return count;
}

static bool find_next_substring_index_delimiter(struct substring_index_input input,
                                                size_t start_offset, size_t *out_offset)
{
    if (input.delimiter_length == 0U || input.delimiter_length > input.text_length ||
        start_offset > input.text_length - input.delimiter_length) {
        return false;
    }
    for (size_t offset = start_offset; offset <= input.text_length - input.delimiter_length;
         ++offset) {
        if (memcmp(input.text + offset, input.delimiter, input.delimiter_length) == 0) {
            *out_offset = offset;
            return true;
        }
    }
    return false;
}

static int eval_trim_function(enum mylite_scalar_function_id function_id,
                              const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    enum mylite_sql_ast_trim_direction direction = MYLITE_SQL_AST_TRIM_DIRECTION_BOTH;
    const struct mylite_sql_ast_node *source = child_at(arguments, 0U);
    const struct mylite_sql_ast_node *remove = NULL;

    if (function_id == MYLITE_SCALAR_FUNCTION_LTRIM) {
        direction = MYLITE_SQL_AST_TRIM_DIRECTION_LEADING;
    } else if (function_id == MYLITE_SCALAR_FUNCTION_RTRIM) {
        direction = MYLITE_SQL_AST_TRIM_DIRECTION_TRAILING;
    } else if (arguments->trim_spec) {
        direction = arguments->trim_direction;
        remove = child_at(arguments, 1U);
    }

    return eval_trim_operands(direction, source, remove, context, warnings, out_value);
}

static int eval_trim_operands(enum mylite_sql_ast_trim_direction direction,
                              const struct mylite_sql_ast_node *source_node,
                              const struct mylite_sql_ast_node *remove_node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value source = {0};
    struct mylite_expression_value remove = {0};
    char *text = NULL;
    char *remove_text = NULL;
    const char *remove_string = " ";
    int status = eval_node(source_node, context, warnings, &source);

    if (status != 0 || is_null(&source)) {
        mylite_expression_value_deinit(&source);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    if (remove_node != NULL) {
        status = eval_node(remove_node, context, warnings, &remove);
        if (status != 0 || is_null(&remove)) {
            mylite_expression_value_deinit(&source);
            mylite_expression_value_deinit(&remove);
            if (status == 0) {
                *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            }
            return status;
        }
    }

    status = value_to_string(&source, &text);
    if (status == 0 && remove_node != NULL) {
        status = value_to_string(&remove, &remove_text);
        remove_string = remove_text;
    }
    if (status == 0) {
        status = trim_text_value(text, remove_string, direction, out_value);
    }

    free(text);
    free(remove_text);
    mylite_expression_value_deinit(&source);
    mylite_expression_value_deinit(&remove);
    return status;
}

static int trim_text_value(const char *text, const char *remove,
                           enum mylite_sql_ast_trim_direction direction,
                           struct mylite_expression_value *out_value)
{
    size_t text_length = strlen(text == NULL ? "" : text);
    size_t remove_length = strlen(remove == NULL ? "" : remove);
    size_t start = 0U;
    size_t end = text_length;

    if (remove_length == 0U) {
        return set_text_value(text == NULL ? "" : text, text_length, out_value);
    }
    if (direction == MYLITE_SQL_AST_TRIM_DIRECTION_BOTH ||
        direction == MYLITE_SQL_AST_TRIM_DIRECTION_LEADING) {
        start = trim_leading_offset((struct trim_match){
            .source = text,
            .remove = remove,
            .remove_length = remove_length,
        });
    }
    if (direction == MYLITE_SQL_AST_TRIM_DIRECTION_BOTH ||
        direction == MYLITE_SQL_AST_TRIM_DIRECTION_TRAILING) {
        end = trim_trailing_length((text == NULL ? "" : text) + start, text_length - start, remove,
                                   remove_length) +
              start;
    }
    return set_text_value((text == NULL ? "" : text) + start, end - start, out_value);
}

static size_t trim_leading_offset(struct trim_match match)
{
    const char *source = match.source == NULL ? "" : match.source;
    const char *remove = match.remove == NULL ? "" : match.remove;
    size_t offset = 0U;

    while (strncmp(source + offset, remove, match.remove_length) == 0) {
        offset += match.remove_length;
    }
    return offset;
}

static size_t trim_trailing_length(const char *text, size_t text_length, const char *remove,
                                   size_t remove_length)
{
    const char *source = text == NULL ? "" : text;
    size_t length = text_length;

    while (length >= remove_length &&
           memcmp(source + length - remove_length, remove, remove_length) == 0) {
        length -= remove_length;
    }
    return length;
}

static int eval_replace_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    char *text = NULL;
    char *from = NULL;
    char *replacement = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    int status = 0;

    for (size_t index = 0U; index < 3U; ++index) {
        status = eval_node(child_at(arguments, index), context, warnings, &values[index]);
        if (status != 0) {
            goto cleanup;
        }
        if (is_null(&values[index])) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            goto cleanup;
        }
    }
    status = value_to_string(&values[0], &text);
    if (status == 0) {
        status = value_to_string(&values[1], &from);
    }
    if (status == 0) {
        status = value_to_string(&values[2], &replacement);
    }
    if (status != 0) {
        goto cleanup;
    }
    result = copy_span_text("", 0U);
    if (result == NULL) {
        status = -1;
        goto cleanup;
    }
    for (const char *cursor = text; *cursor != '\0';) {
        size_t from_length = strlen(from);

        if (from_length != 0U && strncmp(cursor, from, from_length) == 0) {
            status = append_text(&result, &result_length, replacement, strlen(replacement));
            cursor += from_length;
        } else {
            status = append_text(&result, &result_length, cursor, 1U);
            ++cursor;
        }
        if (status != 0) {
            break;
        }
    }
    if (status == 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = result;
        out_value->text_length = result_length;
        result = NULL;
    }

cleanup:
    free(text);
    free(from);
    free(replacement);
    free(result);
    for (size_t index = 0U; index < 3U; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static int eval_insert_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct insert_operands operands = {0};
    struct insert_range range = {0};
    char *text = NULL;
    char *replacement = NULL;
    bool null_result = false;
    int status =
        eval_insert_operands(arguments, context, warnings, &operands, &range, &null_result);

    if (status != 0) {
        goto cleanup;
    }
    if (null_result) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&operands.text, &text);
    if (status == 0) {
        status = value_to_string(&operands.replacement, &replacement);
    }
    if (status == 0) {
        status = insert_text_value(
            (struct insert_text_input){.text = text, .replacement = replacement, .range = range},
            out_value);
    }

cleanup:
    free(text);
    free(replacement);
    insert_operands_deinit(&operands);
    return status;
}

static int eval_insert_operands(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct insert_operands *operands, struct insert_range *out_range,
                                bool *out_null_result)
{
    int status = eval_insert_nonnull_argument(child_at(arguments, 0U), context, warnings,
                                              &operands->text, out_null_result);

    if (status != 0 || *out_null_result) {
        return status;
    }
    status = eval_insert_nonnull_argument(child_at(arguments, 3U), context, warnings,
                                          &operands->replacement, out_null_result);
    if (status != 0 || *out_null_result) {
        return status;
    }
    status = eval_insert_nonnull_argument(child_at(arguments, 1U), context, warnings,
                                          &operands->position, out_null_result);
    if (status != 0 || *out_null_result) {
        return status;
    }
    status = cast_value_to_signed_integer(&operands->position, warnings, &out_range->position);
    if (status != 0) {
        return status;
    }
    status = eval_insert_nonnull_argument(child_at(arguments, 2U), context, warnings,
                                          &operands->length, out_null_result);
    if (status != 0 || *out_null_result) {
        return status;
    }
    return cast_value_to_signed_integer(&operands->length, warnings, &out_range->length);
}

static int eval_insert_nonnull_argument(const struct mylite_sql_ast_node *argument,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value,
                                        bool *out_null_result)
{
    int status = eval_node(argument, context, warnings, out_value);

    *out_null_result = status == 0 && is_null(out_value);
    return status;
}

static void insert_operands_deinit(struct insert_operands *operands)
{
    mylite_expression_value_deinit(&operands->text);
    mylite_expression_value_deinit(&operands->replacement);
    mylite_expression_value_deinit(&operands->position);
    mylite_expression_value_deinit(&operands->length);
}

static int insert_text_value(struct insert_text_input input,
                             struct mylite_expression_value *out_value)
{
    const char *source = input.text == NULL ? "" : input.text;
    const char *new_text = input.replacement == NULL ? "" : input.replacement;
    int64_t source_chars = 0;
    int64_t replaced_chars = 0;
    size_t prefix_length = 0U;
    size_t suffix_offset = 0U;
    char *result = NULL;
    size_t result_length = 0U;
    int status = utf8_char_count(source, &source_chars);

    if (status != 0) {
        return status;
    }
    if (input.range.position <= 0 || input.range.position > source_chars) {
        return set_text_value(source, strlen(source), out_value);
    }

    replaced_chars = source_chars - (input.range.position - 1);
    if (input.range.length >= 0 && input.range.length < replaced_chars) {
        replaced_chars = input.range.length;
    }

    prefix_length = utf8_offset_for_chars(source, input.range.position - 1);
    suffix_offset = utf8_offset_for_chars(source, input.range.position - 1 + replaced_chars);
    result = copy_span_text("", 0U);
    if (result == NULL) {
        return -1;
    }

    status = append_text(&result, &result_length, source, prefix_length);
    if (status == 0) {
        status = append_text(&result, &result_length, new_text, strlen(new_text));
    }
    if (status == 0) {
        status = append_text(&result, &result_length, source + suffix_offset,
                             strlen(source + suffix_offset));
    }
    if (status == 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = result;
        out_value->text_length = result_length;
        result = NULL;
    }
    free(result);
    return status;
}

static int eval_quote_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value)) {
        status = set_text_value("NULL", 4U, out_value);
        goto cleanup;
    }

    status = value_to_string(&value, &text);
    if (status == 0) {
        status = quote_text_value(text, out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int quote_text_value(const char *text, struct mylite_expression_value *out_value)
{
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text);
    size_t source_length = strlen((const char *)source);
    char *result = copy_span_text("'", 1U);
    size_t result_length = 1U;
    int status = 0;

    if (result == NULL) {
        return -1;
    }
    for (size_t index = 0U; index < source_length; ++index) {
        char escaped[2] = {'\\', (char)source[index]};

        if (source[index] == '\'' || source[index] == '\\') {
            status = append_text(&result, &result_length, escaped, sizeof(escaped));
        } else if (source[index] == MYLITE_ASCII_CONTROL_Z) {
            escaped[1] = 'Z';
            status = append_text(&result, &result_length, escaped, sizeof(escaped));
        } else {
            const char character = (char)source[index];

            status = append_text(&result, &result_length, &character, 1U);
        }
        if (status != 0) {
            free(result);
            return status;
        }
    }
    status = append_text(&result, &result_length, "'", 1U);
    if (status != 0) {
        free(result);
        return status;
    }

    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = result_length;
    return 0;
}

static int eval_repeat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value text_value = {0};
    struct mylite_expression_value count_value = {0};
    char *text = NULL;
    int64_t count = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &text_value);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &count_value);
    }
    if (status == 0 && !is_null(&count_value)) {
        status = cast_value_to_signed_integer(&count_value, warnings, &count);
    }
    if (status != 0 || is_null(&text_value) || is_null(&count_value)) {
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        goto cleanup;
    }

    status = value_to_string(&text_value, &text);
    if (status == 0) {
        status = repeat_text_value(text, count, out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&text_value);
    mylite_expression_value_deinit(&count_value);
    return status;
}

static int repeat_text_value(const char *text, int64_t count,
                             struct mylite_expression_value *out_value)
{
    const char *source = text == NULL ? "" : text;
    size_t source_length = strlen(source);
    size_t output_length = 0U;
    char *result = NULL;
    char *cursor = NULL;

    if (count < 1 || source_length == 0U) {
        return set_text_value("", 0U, out_value);
    }
    if ((uint64_t)count > (uint64_t)(((size_t)PTRDIFF_MAX - 1U) / source_length)) {
        return -1;
    }

    output_length = source_length * (size_t)count;
    result = malloc(output_length + 1U);
    if (result == NULL) {
        return -1;
    }
    cursor = result;
    for (int64_t index = 0; index < count; ++index) {
        memcpy(cursor, source, source_length);
        cursor += source_length;
    }
    result[output_length] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = output_length;
    return 0;
}

static int eval_space_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value count_value = {0};
    int64_t count = 0;
    int status = eval_node(arguments->first_child, context, warnings, &count_value);

    if (status != 0 || is_null(&count_value)) {
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        mylite_expression_value_deinit(&count_value);
        return status;
    }

    status = cast_value_to_signed_integer(&count_value, warnings, &count);
    if (status == 0) {
        status = space_text_value(count, out_value);
    }
    mylite_expression_value_deinit(&count_value);
    return status;
}

static int space_text_value(int64_t count, struct mylite_expression_value *out_value)
{
    char *result = NULL;

    if (count < 1) {
        return set_text_value("", 0U, out_value);
    }
    if ((uint64_t)count > (uint64_t)((size_t)PTRDIFF_MAX - 1U)) {
        return -1;
    }
    result = malloc((size_t)count + 1U);
    if (result == NULL) {
        return -1;
    }
    memset(result, ' ', (size_t)count);
    result[(size_t)count] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = (size_t)count;
    return 0;
}

static int eval_reverse_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        mylite_expression_value_deinit(&value);
        return status;
    }

    status = value_to_string(&value, &text);
    if (status == 0) {
        status = reverse_text_value(text, out_value);
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int reverse_text_value(const char *text, struct mylite_expression_value *out_value)
{
    const char *source = text == NULL ? "" : text;
    size_t source_length = strlen(source);
    size_t read_offset = source_length;
    size_t output_offset = 0U;
    char *result = malloc(source_length + 1U);

    if (result == NULL) {
        return -1;
    }
    while (read_offset > 0U) {
        size_t start_offset = read_offset - 1U;
        size_t character_length = 0U;

        while (start_offset > 0U &&
               (((unsigned char)source[start_offset] & MYLITE_UTF8_CONTINUATION_MASK) ==
                MYLITE_UTF8_CONTINUATION_MARKER)) {
            --start_offset;
        }
        character_length = read_offset - start_offset;
        memcpy(result + output_offset, source + start_offset, character_length);
        output_offset += character_length;
        read_offset = start_offset;
    }
    result[output_offset] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = output_offset;
    return 0;
}

static int eval_pad_function(enum mylite_scalar_function_id function_id,
                             const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    char *text = NULL;
    char *pad = NULL;
    int64_t target_length = 0;
    int status = 0;

    for (size_t index = 0U; index < 3U; ++index) {
        status = eval_node(child_at(arguments, index), context, warnings, &values[index]);
        if (status != 0) {
            goto cleanup;
        }
    }
    if (!is_null(&values[1])) {
        status = cast_value_to_signed_integer(&values[1], warnings, &target_length);
        if (status != 0) {
            goto cleanup;
        }
    }
    if (is_null(&values[0]) || is_null(&values[1]) || is_null(&values[2])) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&values[0], &text);
    if (status == 0) {
        status = value_to_string(&values[2], &pad);
    }
    if (status == 0) {
        status = pad_text_value(function_id, text, target_length, pad, out_value);
    }

cleanup:
    free(text);
    free(pad);
    for (size_t index = 0U; index < 3U; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static int pad_text_value(enum mylite_scalar_function_id function_id, const char *text,
                          int64_t target_length, const char *pad,
                          struct mylite_expression_value *out_value)
{
    const char *source = text == NULL ? "" : text;
    const char *padding = pad == NULL ? "" : pad;
    int64_t source_chars = 0;
    int64_t padding_chars = 0;
    int64_t needed_chars = 0;
    char *result = NULL;
    size_t result_length = 0U;
    int status = utf8_char_count(source, &source_chars);

    if (status != 0) {
        return status;
    }
    if (target_length < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (target_length == 0) {
        return set_text_value("", 0U, out_value);
    }
    if (source_chars >= target_length) {
        return set_text_value(source, utf8_offset_for_chars(source, target_length), out_value);
    }

    status = utf8_char_count(padding, &padding_chars);
    if (status != 0) {
        return status;
    }
    if (padding_chars == 0) {
        return set_text_value("", 0U, out_value);
    }

    result = copy_span_text("", 0U);
    if (result == NULL) {
        return -1;
    }

    needed_chars = target_length - source_chars;
    if (function_id == MYLITE_SCALAR_FUNCTION_LPAD) {
        status =
            append_padding_chars(&result, &result_length, padding, padding_chars, needed_chars);
        if (status == 0) {
            status = append_text(&result, &result_length, source, strlen(source));
        }
    } else {
        status = append_text(&result, &result_length, source, strlen(source));
        if (status == 0) {
            status =
                append_padding_chars(&result, &result_length, padding, padding_chars, needed_chars);
        }
    }

    if (status == 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = result;
        out_value->text_length = result_length;
        result = NULL;
    }
    free(result);
    return status;
}

static int append_padding_chars(char **result, size_t *result_length, const char *pad,
                                int64_t pad_chars, int64_t needed_chars)
{
    size_t pad_length = strlen(pad == NULL ? "" : pad);
    int64_t full_repetitions = pad_chars == 0 ? 0 : needed_chars / pad_chars;
    int64_t partial_chars = pad_chars == 0 ? 0 : needed_chars % pad_chars;
    int status = 0;

    for (int64_t index = 0; index < full_repetitions; ++index) {
        status = append_text(result, result_length, pad, pad_length);
        if (status != 0) {
            return status;
        }
    }
    if (partial_chars != 0) {
        status = append_text(result, result_length, pad, utf8_offset_for_chars(pad, partial_chars));
    }
    return status;
}

static int eval_locate_function(enum mylite_scalar_function_id function_id,
                                const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    char *needle = NULL;
    char *text = NULL;
    size_t arity = child_count(arguments);
    size_t value_count = arity < 3U ? arity : 3U;
    const struct mylite_sql_ast_node *start_node = NULL;
    int64_t start = 1;
    int status =
        eval_locate_arguments(function_id, arguments, context, warnings, values, &start_node);

    if (status != 0) {
        goto cleanup;
    }
    if (locate_arguments_are_null(values, start_node)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&values[0], &needle);
    if (status == 0) {
        status = value_to_string(&values[1], &text);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (start_node != NULL) {
        start = mylite_expression_value_to_int64(&values[2]);
    }

    status = set_locate_function_result((struct locate_texts){.text = text, .needle = needle},
                                        start, out_value);

cleanup:
    free(needle);
    free(text);
    for (size_t index = 0U; index < value_count; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static int eval_locate_arguments(enum mylite_scalar_function_id function_id,
                                 const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value values[3],
                                 const struct mylite_sql_ast_node **out_start_node)
{
    const struct mylite_sql_ast_node *needle_node = child_at(arguments, 0U);
    const struct mylite_sql_ast_node *text_node = child_at(arguments, 1U);
    int status = 0;

    *out_start_node = function_id == MYLITE_SCALAR_FUNCTION_INSTR ? NULL : child_at(arguments, 2U);
    if (function_id == MYLITE_SCALAR_FUNCTION_INSTR) {
        text_node = child_at(arguments, 0U);
        needle_node = child_at(arguments, 1U);
        status = eval_node(text_node, context, warnings, &values[1]);
        return status == 0 ? eval_node(needle_node, context, warnings, &values[0]) : status;
    }

    status = eval_node(needle_node, context, warnings, &values[0]);
    if (status == 0) {
        status = eval_node(text_node, context, warnings, &values[1]);
    }
    if (status == 0 && *out_start_node != NULL) {
        status = eval_node(*out_start_node, context, warnings, &values[2]);
    }
    return status;
}

static bool locate_arguments_are_null(const struct mylite_expression_value values[3],
                                      const struct mylite_sql_ast_node *start_node)
{
    return is_null(&values[0]) || is_null(&values[1]) ||
           (start_node != NULL && is_null(&values[2]));
}

static int set_locate_function_result(struct locate_texts texts, int64_t start,
                                      struct mylite_expression_value *out_value)
{
    const char *text = texts.text == NULL ? "" : texts.text;
    const char *needle = texts.needle == NULL ? "" : texts.needle;
    int64_t char_count = 0;
    int status = utf8_char_count(text, &char_count);

    if (status != 0) {
        return status;
    }
    if (start <= 0 || start > char_count + 1) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    }
    if (needle[0] == '\0') {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = start};
        return 0;
    }
    if (start > char_count) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    }

    {
        size_t start_offset = utf8_offset_for_chars(text, start - 1);
        int64_t position = find_text_match_position((struct locate_search){
            .text = text,
            .needle = needle,
            .start_offset = start_offset,
            .start_position = start,
        });

        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = position};
    }
    return 0;
}

static int eval_elt_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_value index_value = {0};
    struct mylite_expression_value selected_value = {0};
    char *text = NULL;
    size_t arity = child_count(arguments);
    int64_t position = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &index_value);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&index_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = cast_value_to_signed_integer(&index_value, warnings, &position);
    if (status != 0) {
        goto cleanup;
    }
    if (position < 1 || (uint64_t)position > (uint64_t)(arity - 1U)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = eval_node(child_at(arguments, (size_t)position), context, warnings, &selected_value);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&selected_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = value_to_string(&selected_value, &text);
    if (status == 0) {
        status = set_text_value(text, strlen(text), out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&index_value);
    mylite_expression_value_deinit(&selected_value);
    return status;
}

static int eval_field_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value search = {0};
    struct mylite_expression_value *candidates = NULL;
    size_t argument_count = child_count(arguments);
    size_t candidate_count = argument_count > 1U ? argument_count - 1U : 0U;
    int64_t position = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &search);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&search)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        goto cleanup;
    }
    if (candidate_count == 0U) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        goto cleanup;
    }

    candidates = calloc(candidate_count, sizeof(*candidates));
    if (candidates == NULL) {
        status = -1;
        goto cleanup;
    }

    status = eval_field_candidates(arguments, context, warnings, candidates, candidate_count);
    if (status == 0) {
        struct field_match_input input = {
            .search = &search,
            .candidates = candidates,
            .candidate_count = candidate_count,
        };

        status = field_match_position(input, field_comparison_mode_from_values(input), warnings,
                                      &position);
    }
    if (status == 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = position};
    }

cleanup:
    if (candidates != NULL) {
        for (size_t index = 0U; index < candidate_count; ++index) {
            mylite_expression_value_deinit(&candidates[index]);
        }
    }
    free(candidates);
    mylite_expression_value_deinit(&search);
    return status;
}

static int eval_field_candidates(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *candidates, size_t candidate_count)
{
    for (size_t index = 0U; index < candidate_count; ++index) {
        int status =
            eval_node(child_at(arguments, index + 1U), context, warnings, &candidates[index]);

        if (status != 0) {
            return status;
        }
    }
    return 0;
}

static int field_match_position(struct field_match_input input, enum field_comparison_mode mode,
                                struct mylite_expression_warnings *warnings, int64_t *out_position)
{
    *out_position = 0;
    if (mode == FIELD_COMPARISON_STRING) {
        return field_string_match_position(input, out_position);
    }
    return field_numeric_match_position(input, warnings, out_position);
}

static enum field_comparison_mode field_comparison_mode_from_values(struct field_match_input input)
{
    bool saw_string = input.search->kind == MYLITE_EXPRESSION_VALUE_TEXT;
    bool saw_numeric = is_numeric_kind(input.search->kind);

    for (size_t index = 0U; index < input.candidate_count; ++index) {
        if (is_null(&input.candidates[index])) {
            continue;
        }
        saw_string = saw_string || input.candidates[index].kind == MYLITE_EXPRESSION_VALUE_TEXT;
        saw_numeric = saw_numeric || is_numeric_kind(input.candidates[index].kind);
    }
    return saw_string && !saw_numeric ? FIELD_COMPARISON_STRING : FIELD_COMPARISON_NUMERIC;
}

static int field_string_match_position(struct field_match_input input, int64_t *out_position)
{
    char *search_text = NULL;
    int status = value_to_string(input.search, &search_text);

    if (status != 0) {
        return status;
    }
    for (size_t index = 0U; index < input.candidate_count; ++index) {
        char *candidate_text = NULL;

        if (is_null(&input.candidates[index])) {
            continue;
        }
        status = value_to_string(&input.candidates[index], &candidate_text);
        if (status == 0 && ascii_text_equal_ci((struct text_compare_input){
                               .left = search_text,
                               .left_length = strlen(search_text),
                               .right = candidate_text,
                               .right_length = strlen(candidate_text),
                           })) {
            *out_position = (int64_t)index + 1;
            free(candidate_text);
            break;
        }
        free(candidate_text);
        if (status != 0) {
            break;
        }
    }
    free(search_text);
    return status;
}

static int field_numeric_match_position(struct field_match_input input,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_position)
{
    struct numeric_value search_number = {0};
    int status = value_to_numeric(input.search, warnings, &search_number);

    if (status != 0) {
        return status;
    }
    for (size_t index = 0U; index < input.candidate_count; ++index) {
        struct numeric_value candidate_number = {0};

        if (is_null(&input.candidates[index])) {
            continue;
        }
        status = value_to_numeric(&input.candidates[index], warnings, &candidate_number);
        if (status != 0) {
            return status;
        }
        if (search_number.real_value == candidate_number.real_value) {
            *out_position = (int64_t)index + 1;
            return 0;
        }
    }
    return 0;
}

static int eval_find_in_set_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value needle_value = {0};
    struct mylite_expression_value list_value = {0};
    char *needle = NULL;
    char *list = NULL;
    int64_t position = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &needle_value);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&needle_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = value_to_string(&needle_value, &needle);
    if (status != 0) {
        goto cleanup;
    }

    status = eval_node(child_at(arguments, 1U), context, warnings, &list_value);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&list_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = value_to_string(&list_value, &list);
    if (status == 0) {
        position = find_in_set_position((struct find_in_set_input){
            .needle = needle,
            .list = list,
        });
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = position};
    }

cleanup:
    free(needle);
    free(list);
    mylite_expression_value_deinit(&needle_value);
    mylite_expression_value_deinit(&list_value);
    return status;
}

static int64_t find_in_set_position(struct find_in_set_input input)
{
    const char *target = input.needle == NULL ? "" : input.needle;
    const char *source = input.list == NULL ? "" : input.list;
    size_t target_length = strlen(target);
    const char *token = source;
    int64_t position = 1;

    if (source[0] == '\0' || strchr(target, ',') != NULL) {
        return 0;
    }
    for (const char *cursor = source;; ++cursor) {
        if (*cursor == ',' || *cursor == '\0') {
            size_t token_length = (size_t)(cursor - token);

            if (ascii_text_equal_ci((struct text_compare_input){
                    .left = target,
                    .left_length = target_length,
                    .right = token,
                    .right_length = token_length,
                })) {
                return position;
            }
            if (*cursor == '\0') {
                break;
            }
            token = cursor + 1;
            ++position;
        }
    }
    return 0;
}

static int eval_make_set_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    struct mylite_expression_value bits_value = {0};
    char *result = copy_span_text("", 0U);
    size_t result_length = 0U;
    uint64_t bits = 0U;
    bool appended = false;
    int status =
        result == NULL ? -1 : eval_node(child_at(arguments, 0U), context, warnings, &bits_value);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&bits_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = make_set_bits_from_value(&bits_value, warnings, &bits);
    if (status != 0) {
        goto cleanup;
    }

    size_t member_index = 0U;
    for (const struct mylite_sql_ast_node *member = child_at(arguments, 1U); member != NULL;
         member = member->next_sibling, ++member_index) {
        struct mylite_expression_value member_value = {0};
        char *member_text = NULL;

        if (!make_set_member_is_selected(bits, member_index)) {
            continue;
        }

        status = eval_node(member, context, warnings, &member_value);
        if (status != 0) {
            mylite_expression_value_deinit(&member_value);
            goto cleanup;
        }
        if (is_null(&member_value)) {
            mylite_expression_value_deinit(&member_value);
            continue;
        }

        status = value_to_string(&member_value, &member_text);
        if (status == 0 && appended) {
            status = append_text(&result, &result_length, ",", 1U);
        }
        if (status == 0) {
            status = append_text(&result, &result_length, member_text, strlen(member_text));
            appended = true;
        }
        free(member_text);
        mylite_expression_value_deinit(&member_value);
        if (status != 0) {
            goto cleanup;
        }
    }

    status = set_text_value(result, result_length, out_value);

cleanup:
    free(result);
    mylite_expression_value_deinit(&bits_value);
    return status;
}

static int make_set_bits_from_value(const struct mylite_expression_value *value,
                                    struct mylite_expression_warnings *warnings, uint64_t *out_bits)
{
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return make_set_bits_from_string(value->text_value == NULL ? "" : value->text_value,
                                         warnings, out_bits);
    }
    return cast_value_to_unsigned_integer(value, warnings, out_bits);
}

static int make_set_bits_from_string(const char *text, struct mylite_expression_warnings *warnings,
                                     uint64_t *out_bits)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_bits = 0U;
        return 0;
    }
    *out_bits =
        parsed.negative ? unsigned_complement_from_magnitude(parsed.magnitude) : parsed.magnitude;
    return 0;
}

static bool make_set_member_is_selected(uint64_t bits, size_t index)
{
    if (index >= (size_t)MYLITE_EXPRESSION_BITS_PER_UINT64) {
        return false;
    }
    return (bits & (UINT64_C(1) << index)) != 0U;
}

static int eval_char_function(const struct mylite_sql_ast_node *function_call,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(function_call, 1U);
    const struct mylite_sql_ast_node *charset_node = child_at(function_call, 2U);
    char *charset_name = copy_charset_node_name(charset_node);
    enum char_function_charset charset = char_function_charset_from_name(charset_name);
    char *result = NULL;
    size_t result_length = 0U;
    int status = 0;

    if (charset_name == NULL) {
        return -1;
    }
    if (charset == CHAR_FUNCTION_CHARSET_UNKNOWN) {
        free(charset_name);
        return -1;
    }

    result = copy_span_text("", 0U);
    if (result == NULL) {
        free(charset_name);
        return -1;
    }
    for (const struct mylite_sql_ast_node *argument = arguments->first_child;
         status == 0 && argument != NULL; argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        uint32_t char_value = 0U;

        status = eval_node(argument, context, warnings, &value);
        if (status == 0 && !is_null(&value)) {
            status = char_argument_value(&value, argument, warnings, &char_value);
            if (status == 0) {
                status = append_char_bytes(&result, &result_length, char_value);
            }
        }
        mylite_expression_value_deinit(&value);
    }
    if (status == 0) {
        status = set_char_result(charset, charset_name, result, result_length, warnings, out_value);
    }
    free(result);
    free(charset_name);
    return status;
}

static int char_argument_value(const struct mylite_expression_value *value,
                               const struct mylite_sql_ast_node *argument,
                               struct mylite_expression_warnings *warnings, uint32_t *out_value)
{
    bool handled_literal = false;
    int status =
        char_integer_literal_overflow_value(argument, warnings, out_value, &handled_literal);

    if (status != 0 || handled_literal) {
        return status;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        *out_value = (uint32_t)(uint64_t)value->int64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        *out_value = (uint32_t)value->uint64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        *out_value =
            (uint32_t)(uint64_t)(numeric_argument_uses_exact_rounding(argument)
                                     ? cast_real_to_signed_integer(value->real_value)
                                     : cast_real_to_signed_integer_half_even(value->real_value));
        return 0;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return char_text_value(value->text_value == NULL ? "" : value->text_value,
                               value->text_value == NULL ? 0U : value->text_length, warnings,
                               out_value);
    case MYLITE_EXPRESSION_VALUE_NULL:
        break;
    }
    return -1;
}

static int char_integer_literal_overflow_value(const struct mylite_sql_ast_node *argument,
                                               struct mylite_expression_warnings *warnings,
                                               uint32_t *out_value, bool *out_handled)
{
    const struct mylite_sql_ast_node *node = argument;
    bool negative = false;
    struct char_integer_parse parsed = {0};
    bool effective_negative = false;

    *out_handled = false;
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    if (node != NULL && node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (node->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = node->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL ||
        node->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return 0;
    }

    parsed = parse_char_integer_text(node->span.text, node->span.length);
    effective_negative = negative || parsed.negative;
    if (!parsed.overflow &&
        !(effective_negative && parsed.magnitude > mylite_expression_int64_min_magnitude)) {
        return 0;
    }

    *out_handled = true;
    *out_value = effective_negative ? 0U : UINT32_MAX;
    return append_char_integer_warning(warnings, "DECIMAL", argument->span.text,
                                       argument->span.length);
}

static int char_text_value(const char *text, size_t text_length,
                           struct mylite_expression_warnings *warnings, uint32_t *out_value)
{
    struct char_integer_parse parsed = parse_char_integer_text(text, text_length);
    bool negative_overflow =
        parsed.negative && parsed.magnitude > mylite_expression_int64_min_magnitude;
    bool truncated =
        !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow || negative_overflow;
    uint64_t bits = 0U;

    if (truncated) {
        int status = append_char_integer_warning(warnings, "INTEGER", text, text_length);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_value = 0U;
        return 0;
    }

    if ((parsed.overflow && parsed.negative) || negative_overflow) {
        bits = 0U;
    } else {
        bits = parsed.negative ? unsigned_complement_from_magnitude(parsed.magnitude)
                               : parsed.magnitude;
    }
    *out_value = (uint32_t)bits;
    return 0;
}

static struct char_integer_parse parse_char_integer_text(const char *text, size_t text_length)
{
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text);
    size_t index = 0U;
    struct char_integer_parse parsed = {0};

    if (text == NULL) {
        text_length = 0U;
    }
    while (index < text_length && isspace(source[index])) {
        ++index;
    }
    if (index < text_length && (source[index] == '+' || source[index] == '-')) {
        parsed.negative = source[index] == '-';
        ++index;
    }
    while (index < text_length && isdigit(source[index])) {
        uint64_t digit = (uint64_t)(source[index] - '0');

        parsed.saw_digit = true;
        if (parsed.magnitude > (UINT64_MAX - digit) / MYLITE_EXPRESSION_DECIMAL_BASE) {
            parsed.magnitude = UINT64_MAX;
            parsed.overflow = true;
        } else if (!parsed.overflow) {
            parsed.magnitude = (parsed.magnitude * MYLITE_EXPRESSION_DECIMAL_BASE) + digit;
        }
        ++index;
    }
    while (index < text_length && isspace(source[index])) {
        ++index;
    }
    parsed.trailing_garbage = index != text_length;
    return parsed;
}

static int append_char_integer_warning(struct mylite_expression_warnings *warnings,
                                       const char *type_name, const char *text, size_t text_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      : (int)text_length;
    int length = snprintf(message, sizeof(message), "Truncated incorrect %s value: '%.*s'",
                          type_name == NULL ? "" : type_name, preview, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_char_bytes(char **result, size_t *result_length, uint32_t value)
{
    char bytes[MYLITE_EXPRESSION_CHAR_VALUE_BYTES];
    size_t offset = 0U;

    bytes[0] = (char)((value >> MYLITE_EXPRESSION_CHAR_SHIFT_24) & UINT32_C(0xFF));
    bytes[1] = (char)((value >> MYLITE_EXPRESSION_CHAR_SHIFT_16) & UINT32_C(0xFF));
    bytes[2] = (char)((value >> MYLITE_EXPRESSION_CHAR_SHIFT_8) & UINT32_C(0xFF));
    bytes[3] = (char)(value & UINT32_C(0xFF));

    while (offset + 1U < sizeof(bytes) && bytes[offset] == '\0') {
        ++offset;
    }
    return append_text(result, result_length, bytes + offset, sizeof(bytes) - offset);
}

static int set_char_result(enum char_function_charset charset, const char *charset_name,
                           const char *text, size_t text_length,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    if (text == NULL) {
        text_length = 0U;
    }
    if (!char_text_is_valid_for_charset(charset, text, text_length)) {
        int status =
            append_invalid_char_string_warning(warnings, (struct char_invalid_string_warning){
                                                             .charset_name = charset_name,
                                                             .text = text,
                                                             .text_length = text_length,
                                                         });

        if (status != 0) {
            return status;
        }
        if (charset == CHAR_FUNCTION_CHARSET_UTF8MB4 || charset == CHAR_FUNCTION_CHARSET_UTF8MB3) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return 0;
        }
    }
    return set_text_value(text == NULL ? "" : text, text_length, out_value);
}

static bool char_text_is_valid_for_charset(enum char_function_charset charset, const char *text,
                                           size_t text_length)
{
    switch (charset) {
    case CHAR_FUNCTION_CHARSET_BINARY:
    case CHAR_FUNCTION_CHARSET_LATIN1:
        return true;
    case CHAR_FUNCTION_CHARSET_UTF8MB4:
        return char_text_is_utf8(text, text_length, true);
    case CHAR_FUNCTION_CHARSET_UTF8MB3:
        return char_text_is_utf8(text, text_length, false);
    case CHAR_FUNCTION_CHARSET_ASCII:
        return char_text_is_ascii(text, text_length);
    case CHAR_FUNCTION_CHARSET_UNKNOWN:
        break;
    }
    return false;
}

static bool char_text_is_ascii(const char *text, size_t text_length)
{
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text);

    if (text == NULL) {
        text_length = 0U;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (source[index] > MYLITE_ASCII_MAX) {
            return false;
        }
    }
    return true;
}

static bool char_text_is_utf8(const char *text, size_t text_length, bool allow_four_byte)
{
    const unsigned char *source = (const unsigned char *)(text == NULL ? "" : text);
    size_t index = 0U;

    if (text == NULL) {
        text_length = 0U;
    }
    while (index < text_length) {
        unsigned char first = source[index];
        struct utf8_sequence sequence = {0};

        if (first <= MYLITE_ASCII_MAX) {
            ++index;
            continue;
        }
        if (!utf8_sequence_from_first(first, allow_four_byte, &sequence) ||
            index + sequence.length > text_length ||
            source[index + MYLITE_UTF8_SECOND_BYTE_OFFSET] < sequence.second_min ||
            source[index + MYLITE_UTF8_SECOND_BYTE_OFFSET] > sequence.second_max) {
            return false;
        }
        for (size_t offset = MYLITE_UTF8_CONTINUATION_START_OFFSET; offset < sequence.length;
             ++offset) {
            if (!utf8_continuation_byte(source[index + offset])) {
                return false;
            }
        }
        index += sequence.length;
    }
    return true;
}

static bool utf8_sequence_from_first(unsigned char first, bool allow_four_byte,
                                     struct utf8_sequence *out_sequence)
{
    static const struct utf8_sequence_range ranges[] = {
        {.length = MYLITE_UTF8_TWO_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_TWO_BYTE_MIN,
         .first_max = MYLITE_UTF8_TWO_BYTE_MAX,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_E0,
         .first_max = MYLITE_UTF8_E0,
         .second_min = MYLITE_UTF8_E0_SECOND_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_E1_MIN,
         .first_max = MYLITE_UTF8_EC_MAX,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_ED,
         .first_max = MYLITE_UTF8_ED,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_ED_SECOND_MAX},
        {.length = MYLITE_UTF8_THREE_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_EE_MIN,
         .first_max = MYLITE_UTF8_EF_MAX,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX},
        {.length = MYLITE_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_F0,
         .first_max = MYLITE_UTF8_F0,
         .second_min = MYLITE_UTF8_F0_SECOND_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX,
         .requires_four_byte = true},
        {.length = MYLITE_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_F1_MIN,
         .first_max = MYLITE_UTF8_F3_MAX,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_CONTINUATION_MAX,
         .requires_four_byte = true},
        {.length = MYLITE_UTF8_FOUR_BYTE_LENGTH,
         .first_min = MYLITE_UTF8_F4,
         .first_max = MYLITE_UTF8_F4,
         .second_min = MYLITE_UTF8_CONTINUATION_MIN,
         .second_max = MYLITE_UTF8_F4_SECOND_MAX,
         .requires_four_byte = true},
    };

    for (size_t index = 0U; index < sizeof(ranges) / sizeof(ranges[0]); ++index) {
        const struct utf8_sequence_range *range = &ranges[index];

        if (first >= range->first_min && first <= range->first_max &&
            (allow_four_byte || !range->requires_four_byte)) {
            *out_sequence = (struct utf8_sequence){
                .length = range->length,
                .second_min = range->second_min,
                .second_max = range->second_max,
            };
            return true;
        }
    }
    return false;
}

static bool utf8_continuation_byte(unsigned char character)
{
    return (character & MYLITE_UTF8_CONTINUATION_MASK) == MYLITE_UTF8_CONTINUATION_MARKER;
}

static int append_invalid_char_string_warning(struct mylite_expression_warnings *warnings,
                                              struct char_invalid_string_warning warning)
{
    static const char digits[] = "0123456789ABCDEF";
    char hex_preview[(MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW * 2U) + 1U];
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    const unsigned char *source = (const unsigned char *)(warning.text == NULL ? "" : warning.text);
    size_t text_length = warning.text == NULL ? 0U : warning.text_length;
    size_t preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                         ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                         : text_length;
    size_t output = 0U;
    int length = 0;

    for (size_t index = 0U; index < preview; ++index) {
        hex_preview[output++] = digits[source[index] >> 4U];
        hex_preview[output++] = digits[source[index] & MYLITE_EXPRESSION_HEX_LOW_NIBBLE_MASK];
    }
    hex_preview[output] = '\0';
    length = snprintf(message, sizeof(message), "Invalid %s character string: '%s'",
                      warning.charset_name == NULL || warning.charset_name[0] == '\0'
                          ? "binary"
                          : warning.charset_name,
                      hex_preview);
    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_INVALID_CHARACTER_STRING, message);
}

static char *copy_charset_node_name(const struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return copy_span_text("binary", strlen("binary"));
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL) {
        return decode_string_literal(node);
    }
    return copy_unquoted_identifier_text(node->span);
}

static char *copy_unquoted_identifier_text(struct mylite_sql_source_span span)
{
    const char *text = span.text == NULL ? "" : span.text;
    size_t start = 0U;
    size_t end = span.text == NULL ? 0U : span.length;
    char *copy = NULL;
    size_t output = 0U;

    if (end >= 2U && text[0] == '`' && text[end - 1U] == '`') {
        start = 1U;
        --end;
    }
    copy = malloc(end >= start ? end - start + 1U : 1U);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t index = start; index < end; ++index) {
        if (text[index] == '`' && index + 1U < end && text[index + 1U] == '`') {
            copy[output++] = text[index++];
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

static enum char_function_charset char_function_charset_from_name(const char *name)
{
    const char *text = name == NULL ? "binary" : name;
    size_t length = strlen(text);

    if (char_charset_name_is(text, length, "binary")) {
        return CHAR_FUNCTION_CHARSET_BINARY;
    }
    if (char_charset_name_is(text, length, "latin1")) {
        return CHAR_FUNCTION_CHARSET_LATIN1;
    }
    if (char_charset_name_is(text, length, "utf8mb4")) {
        return CHAR_FUNCTION_CHARSET_UTF8MB4;
    }
    if (char_charset_name_is(text, length, "utf8mb3") ||
        char_charset_name_is(text, length, "utf8")) {
        return CHAR_FUNCTION_CHARSET_UTF8MB3;
    }
    if (char_charset_name_is(text, length, "ascii")) {
        return CHAR_FUNCTION_CHARSET_ASCII;
    }
    return CHAR_FUNCTION_CHARSET_UNKNOWN;
}

static bool char_charset_name_is(const char *text, size_t text_length, const char *expected)
{
    return ascii_text_equal_ci((struct text_compare_input){
        .left = text,
        .left_length = text_length,
        .right = expected,
        .right_length = strlen(expected == NULL ? "" : expected),
    });
}

static int eval_hex_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    struct mylite_expression_value value = {0};
    uint64_t number = 0U;
    int status = eval_node(argument, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        status = set_hex_bytes_value(value.text_value == NULL ? "" : value.text_value,
                                     value.text_value == NULL ? 0U : value.text_length, out_value);
    } else {
        status = hex_numeric_value(&value, argument, warnings, &number);
        if (status == 0) {
            status = set_hex_uint64_value(number, out_value);
        }
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int hex_numeric_value(const struct mylite_expression_value *value,
                             const struct mylite_sql_ast_node *argument,
                             struct mylite_expression_warnings *warnings, uint64_t *out_number)
{
    if (value == NULL || out_number == NULL) {
        return -1;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        *out_number = (uint64_t)value->int64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        *out_number = value->uint64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        *out_number = (uint64_t)hex_real_to_signed_integer(value->real_value, argument);
        return 0;
    case MYLITE_EXPRESSION_VALUE_TEXT:
    case MYLITE_EXPRESSION_VALUE_NULL:
        break;
    }
    (void)warnings;
    return -1;
}

static int64_t hex_real_to_signed_integer(double value, const struct mylite_sql_ast_node *argument)
{
    if (numeric_argument_uses_exact_rounding(argument)) {
        return cast_real_to_signed_integer(value);
    }
    return cast_real_to_signed_integer_half_even(value);
}

static bool numeric_argument_uses_exact_rounding(const struct mylite_sql_ast_node *argument)
{
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = child_at(argument, 0U);
    }
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        return argument->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        return numeric_argument_uses_exact_rounding(child_at(argument, 0U));
    }
    return false;
}

static int64_t cast_real_to_signed_integer_half_even(double value)
{
    int64_t truncated = 0;
    double fraction = 0.0;

    if (value >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (value <= (double)INT64_MIN) {
        return INT64_MIN;
    }

    truncated = (int64_t)value;
    fraction = value - (double)truncated;
    if (fraction > mylite_expression_round_half ||
        (fraction == mylite_expression_round_half && (truncated & INT64_C(1)) != 0)) {
        ++truncated;
    } else if (fraction < -mylite_expression_round_half ||
               (fraction == -mylite_expression_round_half && (truncated & INT64_C(1)) != 0)) {
        --truncated;
    }
    return truncated;
}

static int set_hex_uint64_value(uint64_t number, struct mylite_expression_value *out_value)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    int length = snprintf(buffer, sizeof(buffer), "%llX", (unsigned long long)number);

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    return set_text_value(buffer, (size_t)length, out_value);
}

static int set_hex_bytes_value(const char *text, size_t text_length,
                               struct mylite_expression_value *out_value)
{
    static const char digits[] = "0123456789ABCDEF";
    const unsigned char *bytes = (const unsigned char *)(text == NULL ? "" : text);
    char *result = NULL;
    size_t result_length = 0U;

    if (text_length > (SIZE_MAX - 1U) / 2U) {
        return -1;
    }
    result_length = text_length * 2U;
    result = malloc(result_length + 1U);
    if (result == NULL) {
        return -1;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        result[index * 2U] = digits[bytes[index] >> 4U];
        result[(index * 2U) + 1U] = digits[bytes[index] & MYLITE_EXPRESSION_HEX_LOW_NIBBLE_MASK];
    }
    result[result_length] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = result_length;
    return 0;
}

static int eval_unhex_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = unhex_argument_to_text(&value, child_at(arguments, 0U), &text, &text_length);
    if (status == 0) {
        status = unhex_text_value(text, text_length, warnings, out_value);
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int unhex_argument_to_text(const struct mylite_expression_value *value,
                                  const struct mylite_sql_ast_node *argument, char **out_text,
                                  size_t *out_length)
{
    if (value == NULL || out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = NULL;
    *out_length = 0U;
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL &&
        !unhex_argument_uses_exact_decimal_text(argument)) {
        char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE] = {0};
        int length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);

        if (length < 0 || (size_t)length >= sizeof(buffer)) {
            return -1;
        }
        *out_text = copy_span_text(buffer, (size_t)length);
        if (*out_text == NULL) {
            return -1;
        }
        *out_length = (size_t)length;
        return 0;
    }
    return value_to_string_with_length(value, out_text, out_length);
}

static bool unhex_argument_uses_exact_decimal_text(const struct mylite_sql_ast_node *argument)
{
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = child_at(argument, 0U);
    }
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        return argument->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        return unhex_argument_uses_exact_decimal_text(child_at(argument, 0U));
    }
    return false;
}

static int unhex_text_value(const char *text, size_t text_length,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    const unsigned char *source = (const unsigned char *)text;
    size_t result_length = (text_length / 2U) + (text_length % 2U);
    size_t input = 0U;
    size_t output = 0U;
    char *result = NULL;

    if (text == NULL && text_length != 0U) {
        return -1;
    }
    if (text == NULL) {
        source = (const unsigned char *)"";
    }
    result = malloc(result_length + 1U);
    if (result == NULL) {
        return -1;
    }
    if ((text_length % 2U) != 0U) {
        int digit = hex_digit_value(source[input++]);

        if (digit < 0) {
            free(result);
            return append_unhex_warning(warnings, text, text_length);
        }
        result[output++] = (char)digit;
    }
    while (input < text_length) {
        if (input + 1U >= text_length) {
            free(result);
            return -1;
        }
        int high = hex_digit_value(source[input]);
        int low = hex_digit_value(source[input + 1U]);

        if (high < 0 || low < 0) {
            free(result);
            return append_unhex_warning(warnings, text, text_length);
        }
        result[output++] = (char)((high << 4U) | low);
        input += 2U;
    }
    result[result_length] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = result_length;
    return 0;
}

static int append_unhex_warning(struct mylite_expression_warnings *warnings, const char *text,
                                size_t text_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      : (int)text_length;
    int length = snprintf(message, sizeof(message),
                          "Incorrect string value: ''%.*s'' for "
                          "function unhex",
                          preview, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_INCORRECT_STRING_VALUE, message);
}

static int hex_digit_value(unsigned char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'F') {
        return (character - 'A') + MYLITE_EXPRESSION_HEX_ALPHA_OFFSET;
    }
    if (character >= 'a' && character <= 'f') {
        return (character - 'a') + MYLITE_EXPRESSION_HEX_ALPHA_OFFSET;
    }
    return -1;
}

static int eval_to_base64_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = base64_argument_to_text(&value, child_at(arguments, 0U), &text, &text_length);
    if (status == 0) {
        status = to_base64_text_value(text, text_length, out_value);
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int to_base64_text_value(const char *text, size_t text_length,
                                struct mylite_expression_value *out_value)
{
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw"
                                 "xyz0123456789+/";
    const unsigned char *source = (const unsigned char *)text;
    size_t result_length = base64_encoded_length(text_length);
    char *result = NULL;
    size_t input = 0U;
    size_t output = 0U;
    size_t line_length = 0U;

    if (text == NULL) {
        return text_length == 0U ? set_text_value("", 0U, out_value) : -1;
    }
    if (text_length == 0U) {
        return set_text_value("", 0U, out_value);
    }
    if (result_length == SIZE_MAX) {
        return -1;
    }
    result = malloc(result_length + 1U);
    if (result == NULL) {
        return -1;
    }
    while (input < text_length) {
        size_t source_count = text_length - input;
        unsigned int first = source[input];
        unsigned int second = 0U;
        unsigned int third = 0U;

        if (source_count > MYLITE_EXPRESSION_BASE64_INPUT_GROUP) {
            source_count = MYLITE_EXPRESSION_BASE64_INPUT_GROUP;
        }
        if (source_count > 1U) {
            second = source[input + 1U];
        }
        if (source_count > 2U) {
            third = source[input + 2U];
        }
        input += source_count;

        result[output++] = digits[first >> MYLITE_EXPRESSION_BASE64_SHIFT_TWO];
        result[output++] = digits[((first & MYLITE_EXPRESSION_BASE64_TWO_BIT_MASK)
                                   << MYLITE_EXPRESSION_BASE64_SHIFT_FOUR) |
                                  (second >> MYLITE_EXPRESSION_BASE64_SHIFT_FOUR)];
        result[output++] = source_count > 1U
                               ? digits[((second & MYLITE_EXPRESSION_BASE64_FOUR_BIT_MASK)
                                         << MYLITE_EXPRESSION_BASE64_SHIFT_TWO) |
                                        (third >> MYLITE_EXPRESSION_BASE64_SHIFT_SIX)]
                               : '=';
        result[output++] =
            source_count > 2U ? digits[third & MYLITE_EXPRESSION_BASE64_SIX_BIT_MASK] : '=';
        line_length += MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP;
        if (line_length == MYLITE_EXPRESSION_BASE64_LINE_LENGTH && input < text_length) {
            result[output++] = '\n';
            line_length = 0U;
        }
    }
    result[output] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = output;
    return 0;
}

static int base64_argument_to_text(const struct mylite_expression_value *value,
                                   const struct mylite_sql_ast_node *argument, char **out_text,
                                   size_t *out_length)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    bool matched_literal = false;
    int length = 0;
    int status = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = NULL;
    *out_length = 0U;
    status = base_conversion_exact_numeric_literal_to_text(argument, out_text, out_length,
                                                           &matched_literal);
    if (status != 0 || matched_literal) {
        if (status == 0 && *out_length > 0U && (*out_text)[0] == '+') {
            memmove(*out_text, *out_text + 1, *out_length);
            --*out_length;
        }
        return status;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return value_to_string_with_length(value, out_text, out_length);
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);
        break;
    case MYLITE_EXPRESSION_VALUE_TEXT:
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }
    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_text = copy_span_text(buffer, (size_t)length);
    if (*out_text == NULL) {
        return -1;
    }
    *out_length = (size_t)length;
    return 0;
}

static size_t base64_encoded_length(size_t text_length)
{
    size_t groups = 0U;
    size_t encoded = 0U;
    size_t newlines = 0U;

    if (text_length == 0U) {
        return 0U;
    }
    if (text_length > SIZE_MAX - (MYLITE_EXPRESSION_BASE64_INPUT_GROUP - 1U)) {
        return SIZE_MAX;
    }
    groups = (text_length + (MYLITE_EXPRESSION_BASE64_INPUT_GROUP - 1U)) /
             MYLITE_EXPRESSION_BASE64_INPUT_GROUP;
    if (groups > SIZE_MAX / MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP) {
        return SIZE_MAX;
    }
    encoded = groups * MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP;
    newlines = (encoded - 1U) / MYLITE_EXPRESSION_BASE64_LINE_LENGTH;
    if (encoded > SIZE_MAX - newlines) {
        return SIZE_MAX;
    }
    return encoded + newlines;
}

static int eval_from_base64_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    (void)warnings;
    status = base64_argument_to_text(&value, child_at(arguments, 0U), &text, &text_length);
    if (status == 0) {
        status = from_base64_text_value(text, text_length, out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int from_base64_text_value(const char *text, size_t text_length,
                                  struct mylite_expression_value *out_value)
{
    size_t clean_length = 0U;
    char *clean = copy_base64_clean_text(text, text_length, &clean_length);
    char *result = NULL;
    size_t result_length = 0U;
    size_t output = 0U;

    if (clean == NULL) {
        return -1;
    }
    if (clean_length == 0U) {
        free(clean);
        return set_text_value("", 0U, out_value);
    }
    if ((clean_length % MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP) != 0U) {
        free(clean);
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    result_length = (clean_length / MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP) *
                    MYLITE_EXPRESSION_BASE64_INPUT_GROUP;
    result = malloc(result_length + 1U);
    if (result == NULL) {
        free(clean);
        return -1;
    }
    for (size_t input = 0U; input < clean_length; input += MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP) {
        if (!base64_decode_group((const unsigned char *)clean + input,
                                 input + MYLITE_EXPRESSION_BASE64_OUTPUT_GROUP == clean_length,
                                 result, &output)) {
            free(result);
            free(clean);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return 0;
        }
    }
    free(clean);
    result[output] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = output;
    return 0;
}

static char *copy_base64_clean_text(const char *text, size_t text_length, size_t *out_length)
{
    const unsigned char *source = (const unsigned char *)text;
    char *clean = NULL;
    size_t output = 0U;

    if (text == NULL) {
        if (text_length != 0U) {
            return NULL;
        }
        clean = malloc(1U);
        if (clean == NULL) {
            return NULL;
        }
        clean[0] = '\0';
        *out_length = 0U;
        return clean;
    }
    source = (const unsigned char *)(text == NULL ? "" : text);
    if (text_length == SIZE_MAX) {
        return NULL;
    }
    clean = malloc(text_length + 1U);
    if (clean == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if (!base64_ignored_whitespace(source[index])) {
            clean[output++] = (char)source[index];
        }
    }
    clean[output] = '\0';
    *out_length = output;
    return clean;
}

static bool base64_decode_group(const unsigned char *source, bool is_last_group, char *result,
                                size_t *output)
{
    int first = base64_digit_value(source[0]);
    int second = base64_digit_value(source[1]);
    int third = source[2] == '=' ? 0 : base64_digit_value(source[2]);
    int fourth = source[3] == '=' ? 0 : base64_digit_value(source[3]);
    size_t pad = 0U;

    if (first < 0 || second < 0 || (source[2] != '=' && third < 0) ||
        (source[3] != '=' && fourth < 0)) {
        return false;
    }
    if (source[2] == '=') {
        if (source[3] != '=') {
            return false;
        }
        pad = 2U;
    } else if (source[3] == '=') {
        pad = 1U;
    }
    if (pad > 0U && !is_last_group) {
        return false;
    }

    unsigned int first_value = (unsigned int)first;
    unsigned int second_value = (unsigned int)second;
    unsigned int third_value = (unsigned int)third;
    unsigned int fourth_value = (unsigned int)fourth;

    result[(*output)++] = (char)((first_value << MYLITE_EXPRESSION_BASE64_SHIFT_TWO) |
                                 (second_value >> MYLITE_EXPRESSION_BASE64_SHIFT_FOUR));
    if (pad < 2U) {
        result[(*output)++] = (char)(((second_value & MYLITE_EXPRESSION_BASE64_FOUR_BIT_MASK)
                                      << MYLITE_EXPRESSION_BASE64_SHIFT_FOUR) |
                                     (third_value >> MYLITE_EXPRESSION_BASE64_SHIFT_TWO));
    }
    if (pad == 0U) {
        result[(*output)++] = (char)(((third_value & MYLITE_EXPRESSION_BASE64_TWO_BIT_MASK)
                                      << MYLITE_EXPRESSION_BASE64_SHIFT_SIX) |
                                     fourth_value);
    }
    return true;
}

static int base64_digit_value(unsigned char character)
{
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return (character - 'a') + MYLITE_EXPRESSION_BASE64_LOWER_ALPHA_OFFSET;
    }
    if (character >= '0' && character <= '9') {
        return (character - '0') + MYLITE_EXPRESSION_BASE64_DIGIT_OFFSET;
    }
    if (character == '+') {
        return MYLITE_EXPRESSION_BASE64_PLUS_VALUE;
    }
    if (character == '/') {
        return MYLITE_EXPRESSION_BASE64_SLASH_VALUE;
    }
    return -1;
}

static bool base64_ignored_whitespace(unsigned char character)
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\v' ||
           character == '\f' || character == '\r';
}

static int eval_base_conversion_function(enum mylite_scalar_function_id function_id,
                                         const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value)
{
    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_BIN:
        return eval_bin_oct_function(child_at(arguments, 0U), MYLITE_EXPRESSION_BINARY_BASE,
                                     context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_OCT:
        return eval_bin_oct_function(child_at(arguments, 0U), MYLITE_EXPRESSION_OCTAL_BASE, context,
                                     warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_CONV:
        return eval_conv_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return -1;
    }
    return -1;
}

static int eval_bit_count_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    uint64_t bits = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        status = bit_count_value_bits(&value, warnings, &bits);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = uint64_bit_count(bits)};
        }
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int bit_count_value_bits(const struct mylite_expression_value *value,
                                struct mylite_expression_warnings *warnings, uint64_t *out_bits)
{
    if (value == NULL || out_bits == NULL) {
        return -1;
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        *out_bits = (uint64_t)value->int64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        *out_bits = value->uint64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        *out_bits = (uint64_t)cast_real_to_signed_integer(value->real_value);
        return 0;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return bit_count_string_bits(value->text_value == NULL ? "" : value->text_value, warnings,
                                     out_bits);
    case MYLITE_EXPRESSION_VALUE_NULL:
        break;
    }
    return -1;
}

static int bit_count_string_bits(const char *text, struct mylite_expression_warnings *warnings,
                                 uint64_t *out_bits)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (out_bits == NULL) {
        return -1;
    }
    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_bits = 0U;
        return 0;
    }
    *out_bits =
        parsed.negative ? unsigned_complement_from_magnitude(parsed.magnitude) : parsed.magnitude;
    return 0;
}

static unsigned int uint64_bit_count(uint64_t value)
{
    unsigned int count = 0U;

    while (value != 0U) {
        value &= value - 1U;
        ++count;
    }
    return count;
}

static int eval_bit_length_function(const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    if (value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        status = value_to_string_with_length(&value, &text, &text_length);
    } else {
        status = cast_value_to_string(&value, &text);
        if (status == 0) {
            text_length = strlen(text);
        }
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    if (text_length > (size_t)(INT64_MAX / MYLITE_EXPRESSION_BITS_PER_BYTE)) {
        free(text);
        return -1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = (int64_t)text_length *
                                                                 MYLITE_EXPRESSION_BITS_PER_BYTE};
    free(text);
    return 0;
}

static int eval_crc32_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = eval_node(argument, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = crc32_argument_to_text(&value, argument, &text, &text_length);
    if (status == 0) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_UINT64,
            .uint64_value = crc32_bytes(text == NULL ? "" : text, text_length),
        };
    }

    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int crc32_argument_to_text(const struct mylite_expression_value *value,
                                  const struct mylite_sql_ast_node *argument, char **out_text,
                                  size_t *out_length)
{
    bool matched_literal = false;
    int status = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = NULL;
    *out_length = 0U;
    status = base_conversion_exact_numeric_literal_to_text(argument, out_text, out_length,
                                                           &matched_literal);
    if (status != 0 || matched_literal) {
        if (status == 0 && memchr(*out_text, '.', *out_length) == NULL) {
            free(*out_text);
            *out_text = NULL;
            *out_length = 0U;
        } else if (status == 0) {
            if (*out_length > 0U && (*out_text)[0] == '+') {
                memmove(*out_text, *out_text + 1U, *out_length);
                --*out_length;
            }
            normalize_crc32_exact_decimal_text(*out_text, out_length);
        }
        if (status != 0 || *out_text != NULL) {
            return status;
        }
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        return crc32_real_to_text(value->real_value, out_text, out_length);
    }
    return value_to_string_with_length(value, out_text, out_length);
}

static void normalize_crc32_exact_decimal_text(char *text, size_t *length)
{
    char *decimal = NULL;
    size_t sign_length = 0U;
    size_t decimal_index = 0U;
    size_t first_kept = 0U;

    if (text == NULL || length == NULL || *length == 0U) {
        return;
    }
    sign_length = text[0] == '-' ? 1U : 0U;
    decimal = memchr(text + sign_length, '.', *length - sign_length);
    if (decimal == NULL) {
        return;
    }
    decimal_index = (size_t)(decimal - text);
    first_kept = sign_length;
    while (first_kept + 1U < decimal_index && text[first_kept] == '0') {
        ++first_kept;
    }
    if (first_kept > sign_length) {
        size_t removed = first_kept - sign_length;

        memmove(text + sign_length, text + first_kept, *length - first_kept + 1U);
        *length -= removed;
    }
}

static int crc32_real_to_text(double value, char **out_text, size_t *out_length)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE] = {0};
    int length = 0;

    if (out_text == NULL || out_length == NULL) {
        return -1;
    }
    length = snprintf(buffer, sizeof(buffer), "%.15g", value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_length = (size_t)length;
    remove_positive_exponent_marker(buffer, out_length);
    *out_text = copy_span_text(buffer, *out_length);
    return *out_text == NULL ? -1 : 0;
}

static void remove_positive_exponent_marker(char *text, size_t *length)
{
    if (text == NULL || length == NULL || *length < 3U) {
        return;
    }
    for (size_t index = 1U; index + 1U < *length; ++index) {
        if ((text[index] == 'e' || text[index] == 'E') && text[index + 1U] == '+') {
            memmove(text + index + 1U, text + index + 2U, *length - index - 1U);
            --*length;
            return;
        }
    }
}

static uint32_t crc32_bytes(const char *text, size_t text_length)
{
    uint32_t crc = mylite_expression_crc32_initial;

    for (size_t index = 0U; index < text_length; ++index) {
        crc ^= (unsigned char)text[index];
        for (unsigned int bit = 0U; bit < CHAR_BIT; ++bit) {
            uint32_t mask = 0U - (crc & UINT32_C(1));

            crc = (crc >> 1U) ^ (mylite_expression_crc32_polynomial & mask);
        }
    }
    return crc ^ mylite_expression_crc32_initial;
}

static int eval_inet_aton_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    bool was_text = false;
    uint64_t address = 0U;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = inet_aton_input_to_text(&value, &text, &text_length, &was_text);
    if (status == 0 && parse_inet_aton_text(text, text_length, &address)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = address};
    } else if (status == 0) {
        status = append_inet_aton_warning(warnings, text, text_length, was_text);
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    }

    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_inet_ntoa_function(const struct mylite_sql_ast_node *arguments,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    struct mylite_expression_value value = {0};
    uint32_t address = 0U;
    int status = eval_node(argument, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = inet_ntoa_value_to_address(&value, argument, warnings, &address);
    if (status == 0) {
        status = set_inet_ntoa_result(address, out_value);
    } else if (status > 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        status = 0;
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int inet_aton_input_to_text(const struct mylite_expression_value *value, char **out_text,
                                   size_t *out_length, bool *out_was_text)
{
    if (value == NULL || out_text == NULL || out_length == NULL || out_was_text == NULL) {
        return -1;
    }
    *out_was_text = value->kind == MYLITE_EXPRESSION_VALUE_TEXT;
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return value_to_string_with_length(value, out_text, out_length);
    }
    if (cast_value_to_string(value, out_text) != 0) {
        return -1;
    }
    *out_length = strlen(*out_text);
    return 0;
}

static bool parse_inet_aton_text(const char *text, size_t text_length, uint64_t *out_address)
{
    struct inet_aton_parse parsed = {0};
    uint64_t address = 0U;

    if (out_address == NULL || !parse_inet_aton_parts(text, text_length, &parsed)) {
        return false;
    }
    for (size_t index = 0U; index < parsed.part_count; ++index) {
        uint64_t limit = 0U;

        if (!inet_aton_part_limit(parsed.part_count, index, &limit) ||
            parsed.parts[index] > limit) {
            return false;
        }
    }

    switch (parsed.part_count) {
    case 1U:
        address = parsed.parts[0];
        break;
    case 2U:
        address = (parsed.parts[0] << MYLITE_EXPRESSION_IPV4_FIRST_OCTET_SHIFT) | parsed.parts[1];
        break;
    case 3U:
        address = (parsed.parts[0] << MYLITE_EXPRESSION_IPV4_FIRST_OCTET_SHIFT) |
                  (parsed.parts[1] << MYLITE_EXPRESSION_IPV4_SECOND_OCTET_SHIFT) | parsed.parts[2];
        break;
    case 4U:
        address = (parsed.parts[0] << MYLITE_EXPRESSION_IPV4_FIRST_OCTET_SHIFT) |
                  (parsed.parts[1] << MYLITE_EXPRESSION_IPV4_SECOND_OCTET_SHIFT) |
                  (parsed.parts[2] << MYLITE_EXPRESSION_IPV4_THIRD_OCTET_SHIFT) | parsed.parts[3];
        break;
    default:
        return false;
    }
    *out_address = address;
    return true;
}

static bool parse_inet_aton_parts(const char *text, size_t text_length,
                                  struct inet_aton_parse *out_parse)
{
    uint64_t value = 0U;
    bool saw_any_digit = false;
    bool saw_digit = false;

    if (text == NULL || out_parse == NULL || text_length == 0U) {
        return false;
    }
    *out_parse = (struct inet_aton_parse){0};
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char character = (unsigned char)text[index];

        if (isdigit(character)) {
            uint64_t digit = (uint64_t)(character - '0');

            if (value > (UINT64_MAX - digit) / (uint64_t)MYLITE_EXPRESSION_DECIMAL_BASE) {
                return false;
            }
            value = (value * (uint64_t)MYLITE_EXPRESSION_DECIMAL_BASE) + digit;
            saw_any_digit = true;
            saw_digit = true;
        } else if (character == '.') {
            if (out_parse->part_count >= MYLITE_EXPRESSION_IPV4_PART_COUNT) {
                return false;
            }
            out_parse->parts[out_parse->part_count++] = value;
            value = 0U;
            saw_digit = false;
        } else {
            return false;
        }
    }
    if (!saw_any_digit || !saw_digit ||
        out_parse->part_count >= MYLITE_EXPRESSION_IPV4_PART_COUNT) {
        return false;
    }
    out_parse->parts[out_parse->part_count++] = value;
    return true;
}

static bool inet_aton_part_limit(size_t part_count, size_t part_index, uint64_t *out_limit)
{
    if (out_limit == NULL || part_count == 0U || part_count > MYLITE_EXPRESSION_IPV4_PART_COUNT ||
        part_index >= part_count) {
        return false;
    }
    *out_limit = MYLITE_EXPRESSION_IPV4_OCTET_MAX;
    return true;
}

static int append_inet_aton_warning(struct mylite_expression_warnings *warnings, const char *text,
                                    size_t text_length, bool was_text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      : (int)text_length;
    int length = was_text ? snprintf(message, sizeof(message),
                                     "Incorrect string value: ''%.*s'' "
                                     "for function inet_aton",
                                     preview, text == NULL ? "" : text)
                          : snprintf(message, sizeof(message),
                                     "Incorrect string value: '%.*s' "
                                     "for function inet_aton",
                                     preview, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_INCORRECT_STRING_VALUE, message);
}

static int inet_ntoa_value_to_address(const struct mylite_expression_value *value,
                                      const struct mylite_sql_ast_node *argument,
                                      struct mylite_expression_warnings *warnings,
                                      uint32_t *out_address)
{
    if (value == NULL || out_address == NULL) {
        return -1;
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        if (value->int64_value < 0 ||
            value->int64_value > (int64_t)mylite_expression_ipv4_u32_max) {
            return append_inet_ntoa_range_warning(warnings, value, argument) == 0 ? 1 : -1;
        }
        *out_address = (uint32_t)value->int64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > mylite_expression_ipv4_u32_max) {
            return append_inet_ntoa_range_warning(warnings, value, argument) == 0 ? 1 : -1;
        }
        *out_address = (uint32_t)value->uint64_value;
        return 0;
    case MYLITE_EXPRESSION_VALUE_REAL: {
        int64_t rounded = cast_real_to_signed_integer(value->real_value);

        if (rounded < 0 || rounded > (int64_t)mylite_expression_ipv4_u32_max) {
            return append_inet_ntoa_range_warning(warnings, value, argument) == 0 ? 1 : -1;
        }
        *out_address = (uint32_t)rounded;
        return 0;
    }
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return inet_ntoa_text_to_address(value->text_value == NULL ? "" : value->text_value,
                                         warnings, out_address);
    case MYLITE_EXPRESSION_VALUE_NULL:
        break;
    }
    return -1;
}

static int inet_ntoa_text_to_address(const char *text, struct mylite_expression_warnings *warnings,
                                     uint32_t *out_address)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (out_address == NULL) {
        return -1;
    }
    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_address = 0U;
        return 0;
    }
    if (parsed.negative || parsed.magnitude > mylite_expression_ipv4_u32_max) {
        return append_inet_ntoa_range_text_warning(warnings, text) == 0 ? 1 : -1;
    }
    *out_address = (uint32_t)parsed.magnitude;
    return 0;
}

static int append_inet_ntoa_range_warning(struct mylite_expression_warnings *warnings,
                                          const struct mylite_expression_value *value,
                                          const struct mylite_sql_ast_node *argument)
{
    char *text = NULL;
    int status = 0;

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_INT64 && value->int64_value < 0) {
        uint64_t magnitude = (uint64_t)(-(value->int64_value + 1)) + 1U;
        return append_inet_ntoa_negative_magnitude_warning(warnings, magnitude);
    }

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_REAL && argument != NULL) {
        bool handled = false;

        text = copy_span_text(argument->span.text, argument->span.length);
        status = text == NULL ? -1 : 0;
        if (status == 0 && value->real_value < 0.0) {
            status = append_inet_ntoa_negative_integer_span_warning(warnings, text, &handled);
        }
        if (status != 0 || handled) {
            free(text);
            return status;
        }
    } else if (value != NULL) {
        status = cast_value_to_string(value, &text);
    }
    if (status != 0) {
        return status;
    }
    status = append_inet_ntoa_range_text_warning(warnings, text == NULL ? "" : text);
    free(text);
    return status;
}

static int append_inet_ntoa_negative_magnitude_warning(struct mylite_expression_warnings *warnings,
                                                       uint64_t magnitude)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    int length = snprintf(buffer, sizeof(buffer), "-(%llu)", (unsigned long long)magnitude);

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    return append_inet_ntoa_range_text_warning(warnings, buffer);
}

static int
append_inet_ntoa_negative_integer_span_warning(struct mylite_expression_warnings *warnings,
                                               const char *text, bool *out_handled)
{
    if (out_handled == NULL) {
        return -1;
    }
    *out_handled = false;
    if (text == NULL || text[0] != '-' || text[1] == '\0') {
        return 0;
    }
    for (const char *scan = text + 1; *scan != '\0'; ++scan) {
        if (!isdigit((unsigned char)*scan)) {
            return 0;
        }
    }
    *out_handled = true;
    {
        char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
        int length = snprintf(buffer, sizeof(buffer), "-(%s)", text + 1);

        if (length < 0 || (size_t)length >= sizeof(buffer)) {
            return -1;
        }
        return append_inet_ntoa_range_text_warning(warnings, buffer);
    }
}

static int append_inet_ntoa_range_text_warning(struct mylite_expression_warnings *warnings,
                                               const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message),
                          "Incorrect integer value: '%.*s' for "
                          "function inet_ntoa",
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_INCORRECT_STRING_VALUE, message);
}

static int set_inet_ntoa_result(uint32_t address, struct mylite_expression_value *out_value)
{
    char text[MYLITE_EXPRESSION_IPV4_NTOA_BUFFER_SIZE];
    int length =
        snprintf(text, sizeof(text), "%u.%u.%u.%u", (unsigned int)((address >> 24U) & 0xFFU),
                 (unsigned int)((address >> 16U) & 0xFFU), (unsigned int)((address >> 8U) & 0xFFU),
                 (unsigned int)(address & 0xFFU));

    if (length < 0 || (size_t)length >= sizeof(text)) {
        return -1;
    }
    return set_text_value(text, (size_t)length, out_value);
}

static int eval_is_uuid_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};
    int status = eval_uuid_first_argument(child_at(arguments, 0U), context, warnings, &value, &text,
                                          &text_length);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = parse_uuid_text(text, text_length, bytes) ? 1 : 0,
        };
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return 0;
}

static int eval_uuid_to_bin_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};
    bool swap = false;
    int status = eval_uuid_first_argument(child_at(arguments, 0U), context, warnings, &value, &text,
                                          &text_length);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (!parse_uuid_text(text, text_length, bytes)) {
        status = append_uuid_incorrect_string_error(warnings, "uuid_to_bin", text, text_length);
        status = status == 0 ? MYLITE_EXEC_ERROR : status;
        goto cleanup;
    }
    status = eval_uuid_swap_flag(arguments, context, warnings, &swap);
    if (status == 0) {
        if (swap) {
            swap_uuid_time_parts(bytes);
        }
        status = set_uuid_binary_value(bytes, out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_bin_to_uuid_function(const struct mylite_sql_ast_node *arguments,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};
    bool swap = false;
    int status = eval_uuid_first_argument(child_at(arguments, 0U), context, warnings, &value, &text,
                                          &text_length);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (text_length != MYLITE_EXPRESSION_UUID_BINARY_LENGTH) {
        status = append_uuid_incorrect_string_error(warnings, "bin_to_uuid", text, text_length);
        status = status == 0 ? MYLITE_EXEC_ERROR : status;
        goto cleanup;
    }
    memcpy(bytes, text, MYLITE_EXPRESSION_UUID_BINARY_LENGTH);
    status = eval_uuid_swap_flag(arguments, context, warnings, &swap);
    if (status == 0) {
        if (swap) {
            unswap_uuid_time_parts(bytes);
        }
        status = set_uuid_text_value(bytes, out_value);
    }

cleanup:
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_uuid_first_argument(const struct mylite_sql_ast_node *argument,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value, char **out_text,
                                    size_t *out_length)
{
    int status = eval_node(argument, context, warnings, out_value);

    if (status != 0 || is_null(out_value)) {
        return status;
    }
    return value_to_string_with_length(out_value, out_text, out_length);
}

static int eval_uuid_swap_flag(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings, bool *out_swap)
{
    const struct mylite_sql_ast_node *flag_argument = child_at(arguments, 1U);
    struct mylite_expression_value value = {0};
    int truth = 0;
    int status = 0;

    if (out_swap == NULL) {
        return -1;
    }
    *out_swap = false;
    if (flag_argument == NULL) {
        return 0;
    }

    status = eval_node(flag_argument, context, warnings, &value);
    if (status == 0) {
        status = truth_value(&value, warnings, &truth);
    }
    if (status == 0) {
        *out_swap = truth > 0;
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static bool parse_uuid_text(const char *text, size_t text_length,
                            unsigned char out_bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH])
{
    if (text == NULL) {
        return false;
    }
    if (text_length == MYLITE_EXPRESSION_UUID_BRACED_TEXT_LENGTH && text[0] == '{' &&
        text[MYLITE_EXPRESSION_UUID_BRACED_TEXT_LENGTH - 1U] == '}') {
        return parse_uuid_unbraced_text(text + 1U, MYLITE_EXPRESSION_UUID_CANONICAL_TEXT_LENGTH,
                                        out_bytes);
    }
    return parse_uuid_unbraced_text(text, text_length, out_bytes);
}

static bool parse_uuid_unbraced_text(const char *text, size_t text_length,
                                     unsigned char out_bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH])
{
    unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};
    size_t hex_index = 0U;

    if (text == NULL || (text_length != MYLITE_EXPRESSION_UUID_TEXT_HEX_LENGTH &&
                         text_length != MYLITE_EXPRESSION_UUID_CANONICAL_TEXT_LENGTH)) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        int digit = 0;

        if (text_length == MYLITE_EXPRESSION_UUID_CANONICAL_TEXT_LENGTH &&
            uuid_canonical_dash_position(index)) {
            if (text[index] != '-') {
                return false;
            }
            continue;
        }
        digit = hex_digit_value((unsigned char)text[index]);
        if (digit < 0 || hex_index >= MYLITE_EXPRESSION_UUID_TEXT_HEX_LENGTH) {
            return false;
        }
        bytes[hex_index / 2U] =
            (unsigned char)((bytes[hex_index / 2U] << 4U) | (unsigned char)digit);
        ++hex_index;
    }
    if (hex_index != MYLITE_EXPRESSION_UUID_TEXT_HEX_LENGTH) {
        return false;
    }
    if (out_bytes != NULL) {
        memcpy(out_bytes, bytes, sizeof(bytes));
    }
    return true;
}

static bool uuid_canonical_dash_position(size_t index)
{
    return index == MYLITE_EXPRESSION_UUID_FIRST_DASH ||
           index == MYLITE_EXPRESSION_UUID_SECOND_DASH ||
           index == MYLITE_EXPRESSION_UUID_THIRD_DASH ||
           index == MYLITE_EXPRESSION_UUID_FOURTH_DASH;
}

static void swap_uuid_time_parts(unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH])
{
    unsigned char copy[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};

    memcpy(copy, bytes, sizeof(copy));
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_LOW_OFFSET,
           copy + MYLITE_EXPRESSION_UUID_TIME_HIGH_OFFSET,
           MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH,
           copy + MYLITE_EXPRESSION_UUID_TIME_MID_OFFSET, MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_MID_OFFSET,
           copy + MYLITE_EXPRESSION_UUID_TIME_LOW_OFFSET, MYLITE_EXPRESSION_UUID_TIME_LOW_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_REST_OFFSET, copy + MYLITE_EXPRESSION_UUID_REST_OFFSET,
           MYLITE_EXPRESSION_UUID_BINARY_LENGTH - MYLITE_EXPRESSION_UUID_REST_OFFSET);
}

static void unswap_uuid_time_parts(unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH])
{
    unsigned char copy[MYLITE_EXPRESSION_UUID_BINARY_LENGTH] = {0};

    memcpy(copy, bytes, sizeof(copy));
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_LOW_OFFSET,
           copy + MYLITE_EXPRESSION_UUID_TIME_MID_OFFSET, MYLITE_EXPRESSION_UUID_TIME_LOW_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_MID_OFFSET,
           copy + MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH,
           MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_TIME_HIGH_OFFSET,
           copy + MYLITE_EXPRESSION_UUID_TIME_LOW_OFFSET, MYLITE_EXPRESSION_UUID_TIME_FIELD_LENGTH);
    memcpy(bytes + MYLITE_EXPRESSION_UUID_REST_OFFSET, copy + MYLITE_EXPRESSION_UUID_REST_OFFSET,
           MYLITE_EXPRESSION_UUID_BINARY_LENGTH - MYLITE_EXPRESSION_UUID_REST_OFFSET);
}

static int set_uuid_binary_value(const unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH],
                                 struct mylite_expression_value *out_value)
{
    return set_text_value((const char *)bytes, MYLITE_EXPRESSION_UUID_BINARY_LENGTH, out_value);
}

static int set_uuid_text_value(const unsigned char bytes[MYLITE_EXPRESSION_UUID_BINARY_LENGTH],
                               struct mylite_expression_value *out_value)
{
    static const char digits[] = "0123456789abcdef";
    char text[MYLITE_EXPRESSION_UUID_CANONICAL_TEXT_LENGTH + 1U];
    size_t output = 0U;

    for (size_t index = 0U; index < MYLITE_EXPRESSION_UUID_BINARY_LENGTH; ++index) {
        if (output == MYLITE_EXPRESSION_UUID_FIRST_DASH ||
            output == MYLITE_EXPRESSION_UUID_SECOND_DASH ||
            output == MYLITE_EXPRESSION_UUID_THIRD_DASH ||
            output == MYLITE_EXPRESSION_UUID_FOURTH_DASH) {
            text[output++] = '-';
        }
        text[output++] = digits[bytes[index] >> 4U];
        text[output++] = digits[bytes[index] & MYLITE_EXPRESSION_HEX_LOW_NIBBLE_MASK];
    }
    text[output] = '\0';
    return set_text_value(text, output, out_value);
}

static int append_uuid_incorrect_string_error(struct mylite_expression_warnings *warnings,
                                              const char *function_name, const char *text,
                                              size_t text_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      : (int)text_length;
    int length =
        snprintf(message, sizeof(message), "Incorrect string value: '%.*s' for function %s",
                 preview, text == NULL ? "" : text, function_name == NULL ? "" : function_name);

    if (length < 0) {
        return -1;
    }
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_INCORRECT_STRING_VALUE,
        message);
}

static int eval_bin_oct_function(const struct mylite_sql_ast_node *argument, uint64_t to_base,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    uint64_t bits = 0U;
    int status = eval_node(argument, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }
    status = base_conversion_input_to_text(&value, argument, &text, &text_length);
    if (status == 0 && text_length == 0U) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        status = parse_base_conversion_input(text, text_length, MYLITE_EXPRESSION_DECIMAL_BASE,
                                             false, warnings, &bits);
        if (status == 0) {
            status = set_base_conversion_value(
                (struct base_conversion_format_input){
                    .bits = bits,
                    .to_base = to_base,
                    .signed_output = false,
                },
                out_value);
        }
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_conv_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value number = {0};
    struct mylite_expression_value from = {0};
    struct mylite_expression_value to_value = {0};
    int64_t from_base = 0;
    int64_t to_base = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &number);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &from);
    }
    if (status == 0) {
        status = eval_node(child_at(arguments, 2U), context, warnings, &to_value);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&number) || is_null(&from) || is_null(&to_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = base_argument_to_signed_integer(&from, child_at(arguments, 1U), warnings, &from_base);
    if (status == 0) {
        status =
            base_argument_to_signed_integer(&to_value, child_at(arguments, 2U), warnings, &to_base);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (!base_conversion_abs_base(from_base, &(uint64_t){0}) ||
        !base_conversion_abs_base(to_base, &(uint64_t){0})) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    status = eval_conv_conversion(&number, from_base, child_at(arguments, 0U), to_base, warnings,
                                  out_value);

cleanup:
    mylite_expression_value_deinit(&number);
    mylite_expression_value_deinit(&from);
    mylite_expression_value_deinit(&to_value);
    return status;
}

static int eval_conv_conversion(const struct mylite_expression_value *number, int64_t from_base,
                                const struct mylite_sql_ast_node *number_argument, int64_t to_base,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    char *text = NULL;
    size_t text_length = 0U;
    uint64_t from_base_abs = 0U;
    uint64_t to_base_abs = 0U;
    uint64_t bits = 0U;
    int status = 0;

    if (!base_conversion_abs_base(from_base, &from_base_abs) ||
        !base_conversion_abs_base(to_base, &to_base_abs)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }

    status = base_conversion_input_to_text(number, number_argument, &text, &text_length);
    if (status == 0 && text_length == 0U) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        status = parse_base_conversion_input(text, text_length, from_base_abs, from_base < 0,
                                             warnings, &bits);
        if (status == 0) {
            status = set_base_conversion_value(
                (struct base_conversion_format_input){
                    .bits = bits,
                    .to_base = to_base_abs,
                    .signed_output = to_base < 0,
                },
                out_value);
        }
    }
    free(text);
    return status;
}

static bool base_conversion_abs_base(int64_t base, uint64_t *out_abs_base)
{
    if (base >= MYLITE_EXPRESSION_MIN_BASE && base <= MYLITE_EXPRESSION_MAX_BASE) {
        *out_abs_base = (uint64_t)base;
        return true;
    }
    if (base <= -MYLITE_EXPRESSION_MIN_BASE && base >= -MYLITE_EXPRESSION_MAX_BASE) {
        *out_abs_base = (uint64_t)-base;
        return true;
    }
    return false;
}

static int base_argument_to_signed_integer(const struct mylite_expression_value *value,
                                           const struct mylite_sql_ast_node *argument,
                                           struct mylite_expression_warnings *warnings,
                                           int64_t *out_base)
{
    if (value == NULL || out_base == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_base = base_argument_uses_exact_rounding(argument)
                        ? cast_real_to_signed_integer(value->real_value)
                        : cast_real_to_signed_integer_half_even(value->real_value);
        return 0;
    }
    return cast_value_to_signed_integer(value, warnings, out_base);
}

static bool base_argument_uses_exact_rounding(const struct mylite_sql_ast_node *argument)
{
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = child_at(argument, 0U);
    }
    if (argument == NULL) {
        return false;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        return argument->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        return base_argument_uses_exact_rounding(child_at(argument, 0U));
    }
    return false;
}

static int base_conversion_input_to_text(const struct mylite_expression_value *value,
                                         const struct mylite_sql_ast_node *argument,
                                         char **out_text, size_t *out_length)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    bool matched_literal = false;
    int length = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = NULL;
    *out_length = 0U;

    {
        int status = base_conversion_exact_numeric_literal_to_text(argument, out_text, out_length,
                                                                   &matched_literal);

        if (status != 0 || matched_literal) {
            return status;
        }
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return base_conversion_real_to_text(value->real_value, out_text, out_length);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_length = value->text_value == NULL ? 0U : value->text_length;
        *out_text = copy_span_text(value->text_value == NULL ? "" : value->text_value, *out_length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_text = copy_span_text(buffer, (size_t)length);
    if (*out_text == NULL) {
        return -1;
    }
    *out_length = (size_t)length;
    return 0;
}

static int base_conversion_real_to_text(double value, char **out_text, size_t *out_length)
{
    int status = cast_real_to_string(value, out_text);

    if (status == 0) {
        *out_length = strlen(*out_text);
    }
    return status;
}

static int base_conversion_exact_numeric_literal_to_text(const struct mylite_sql_ast_node *argument,
                                                         char **out_text, size_t *out_length,
                                                         bool *out_matched)
{
    char sign = '\0';

    if (out_matched == NULL) {
        return -1;
    }
    *out_matched = false;
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = child_at(argument, 0U);
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        sign = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? '-' : '+';
        argument = child_at(argument, 0U);
        while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            argument = child_at(argument, 0U);
        }
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        (argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL)) {
        return 0;
    }

    *out_matched = true;
    return copy_base_conversion_literal_text(sign, argument, out_text, out_length);
}

static int copy_base_conversion_literal_text(char sign, const struct mylite_sql_ast_node *literal,
                                             char **out_text, size_t *out_length)
{
    const char *text = literal->span.text == NULL ? "" : literal->span.text;
    size_t text_length = literal->span.length;
    size_t sign_length = sign == '\0' ? 0U : 1U;
    bool prepend_zero = literal->literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL &&
                        text_length > 0U && text[0] == '.';
    size_t output_length = sign_length + (prepend_zero ? 1U : 0U) + text_length;
    char *copy = malloc(output_length + 1U);
    size_t offset = 0U;

    if (copy == NULL) {
        return -1;
    }
    if (sign != '\0') {
        copy[offset++] = sign;
    }
    if (prepend_zero) {
        copy[offset++] = '0';
    }
    memcpy(copy + offset, text, text_length);
    offset += text_length;
    copy[offset] = '\0';
    *out_text = copy;
    *out_length = output_length;
    return 0;
}

static int parse_base_conversion_input(const char *text, size_t text_length, uint64_t from_base,
                                       bool signed_input,
                                       struct mylite_expression_warnings *warnings,
                                       uint64_t *out_bits)
{
    struct base_conversion_parse parsed =
        parse_base_conversion_digits((struct base_conversion_parse_input){
            .text = text,
            .text_length = text_length,
            .from_base = from_base,
            .signed_input = signed_input,
        });

    if (out_bits == NULL) {
        return -1;
    }
    *out_bits = parsed.bits;
    if (!parsed.saw_digit || parsed.overflow) {
        return append_base_conversion_warning(warnings, text, text_length);
    }
    return 0;
}

static struct base_conversion_parse
parse_base_conversion_digits(struct base_conversion_parse_input input)
{
    const unsigned char *source = (const unsigned char *)(input.text == NULL ? "" : input.text);
    size_t text_length = input.text == NULL ? 0U : input.text_length;
    size_t index = 0U;
    uint64_t magnitude = 0U;
    bool negative = false;
    struct base_conversion_parse parsed = {0};

    while (index < text_length && isspace(source[index])) {
        ++index;
    }
    if (index < text_length && (source[index] == '+' || source[index] == '-')) {
        negative = source[index] == '-';
        ++index;
    }

    while (index < text_length) {
        int digit = base_digit_value(source[index]);
        uint64_t limit = UINT64_MAX;

        if (input.signed_input) {
            limit = negative ? mylite_expression_int64_min_magnitude : (uint64_t)INT64_MAX;
        }

        if (digit < 0 || (uint64_t)digit >= input.from_base) {
            break;
        }
        parsed.saw_digit = true;
        if (magnitude > (limit - (uint64_t)digit) / input.from_base) {
            magnitude = limit;
            parsed.overflow = true;
        } else if (!parsed.overflow) {
            magnitude = (magnitude * input.from_base) + (uint64_t)digit;
        }
        ++index;
    }

    if (!parsed.saw_digit || (parsed.overflow && negative && !input.signed_input)) {
        parsed.bits = 0U;
    } else {
        parsed.bits = negative ? unsigned_complement_from_magnitude(magnitude) : magnitude;
    }
    return parsed;
}

static int append_base_conversion_warning(struct mylite_expression_warnings *warnings,
                                          const char *text, size_t text_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int preview = text_length > MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      ? MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW
                      : (int)text_length;
    int length = snprintf(message, sizeof(message), "Truncated incorrect DECIMAL value: '%.*s'",
                          preview, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int base_digit_value(unsigned char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'Z') {
        return (character - 'A') + MYLITE_EXPRESSION_HEX_ALPHA_OFFSET;
    }
    if (character >= 'a' && character <= 'z') {
        return (character - 'a') + MYLITE_EXPRESSION_HEX_ALPHA_OFFSET;
    }
    return -1;
}

static int set_base_conversion_value(struct base_conversion_format_input input,
                                     struct mylite_expression_value *out_value)
{
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char reversed[MYLITE_EXPRESSION_BASE_CONVERSION_BUFFER_SIZE];
    char result[MYLITE_EXPRESSION_BASE_CONVERSION_BUFFER_SIZE];
    uint64_t magnitude = input.bits;
    size_t reversed_length = 0U;
    size_t result_length = 0U;
    bool negative = false;

    if (input.signed_output) {
        int64_t signed_value = signed_integer_from_uint64(input.bits);

        if (signed_value < 0) {
            negative = true;
            magnitude = signed_value == INT64_MIN ? mylite_expression_int64_min_magnitude
                                                  : (uint64_t)-signed_value;
        } else {
            magnitude = (uint64_t)signed_value;
        }
    }

    do {
        reversed[reversed_length++] = digits[magnitude % input.to_base];
        magnitude /= input.to_base;
    } while (magnitude != 0U && reversed_length < sizeof(reversed));

    if (negative) {
        result[result_length++] = '-';
    }
    while (reversed_length != 0U && result_length + 1U < sizeof(result)) {
        result[result_length++] = reversed[--reversed_length];
    }
    if (reversed_length != 0U) {
        return -1;
    }
    result[result_length] = '\0';
    return set_text_value(result, result_length, out_value);
}

static int eval_mod_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = eval_node(child_at(arguments, 0U), context, warnings, &left);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &right);
    }
    if (status == 0) {
        status =
            eval_arithmetic(MYLITE_SQL_AST_OPERATOR_MODULO, &left, &right, warnings, out_value);
    }
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_power_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value base = {0};
    struct mylite_expression_value exponent = {0};
    struct numeric_value base_number = {0};
    struct numeric_value exponent_number = {0};
    double result = 0.0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &base);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &exponent);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&base) || is_null(&exponent)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&base, warnings, &base_number);
    if (status == 0) {
        status = value_to_numeric(&exponent, warnings, &exponent_number);
    }
    if (status != 0) {
        goto cleanup;
    }

    errno = 0;
    result = pow(base_number.real_value, exponent_number.real_value);
    if (isnan(result) || isinf(result)) {
        status = append_power_out_of_range_error(warnings);
        if (status == 0) {
            status = MYLITE_EXEC_ERROR;
        }
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&base);
    mylite_expression_value_deinit(&exponent);
    return status;
}

static int eval_exp_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_value argument = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &argument);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&argument)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&argument, warnings, &number);
    if (status != 0) {
        goto cleanup;
    }

    errno = 0;
    result = exp(number.real_value);
    if (isnan(result) || isinf(result)) {
        status = append_exp_out_of_range_error(warnings);
        if (status == 0) {
            status = MYLITE_EXEC_ERROR;
        }
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int eval_log_function(enum mylite_scalar_function_id function_id,
                             const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    if (function_id == MYLITE_SCALAR_FUNCTION_LOG && child_count(arguments) == 2U) {
        return eval_binary_log_function(arguments, context, warnings, out_value);
    }
    return eval_unary_log_function(function_id, child_at(arguments, 0U), context, warnings,
                                   out_value);
}

static int eval_unary_log_function(enum mylite_scalar_function_id function_id,
                                   const struct mylite_sql_ast_node *argument,
                                   const struct mylite_expression_eval_context *context,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    int status = eval_node(argument, context, warnings, &value);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&value, warnings, &number);
    if (status != 0) {
        goto cleanup;
    }
    if (number.real_value <= 0.0) {
        status = append_invalid_logarithm_warning(warnings);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        goto cleanup;
    }

    result = log(number.real_value);
    if (function_id == MYLITE_SCALAR_FUNCTION_LOG2) {
        result = log2(number.real_value);
    } else if (function_id == MYLITE_SCALAR_FUNCTION_LOG10) {
        result = log10(number.real_value);
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_binary_log_function(const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value)
{
    struct mylite_expression_value base = {0};
    struct mylite_expression_value value = {0};
    struct numeric_value base_number = {0};
    struct numeric_value number = {0};
    int status = eval_node(child_at(arguments, 0U), context, warnings, &base);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&base)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&base, warnings, &base_number);
    if (status != 0) {
        goto cleanup;
    }
    if (base_number.real_value <= 0.0) {
        status = append_invalid_logarithm_warning(warnings);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        goto cleanup;
    }

    status = eval_node(child_at(arguments, 1U), context, warnings, &value);
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&value, warnings, &number);
    if (status != 0) {
        goto cleanup;
    }
    if (number.real_value <= 0.0 || base_number.real_value == 1.0) {
        status = append_invalid_logarithm_warning(warnings);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = log(number.real_value) / log(base_number.real_value),
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&base);
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_sqrt_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value argument = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &argument);

    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&argument)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_numeric(&argument, warnings, &number);
    if (status != 0) {
        goto cleanup;
    }
    if (number.real_value < 0.0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    if (number.real_value == 0.0 && argument.kind != MYLITE_EXPRESSION_VALUE_TEXT) {
        number.real_value = 0.0;
    }
    result = sqrt(number.real_value);
    if (isnan(result) || isinf(result)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int eval_trigonometric_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument_node = child_at(arguments, 0U);
    struct mylite_expression_value argument = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    bool used_pi_expression = trigonometric_pi_expression_value(argument_node, &number.real_value);
    int status = used_pi_expression ? 0 : eval_node(argument_node, context, warnings, &argument);

    if (status != 0) {
        goto cleanup;
    }
    if (!used_pi_expression && is_null(&argument)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    if (!used_pi_expression) {
        status = value_to_numeric(&argument, warnings, &number);
        if (status != 0) {
            goto cleanup;
        }
    }

    status = trigonometric_function_result(
        (struct trigonometric_input){.function_id = function_id, .input = number.real_value},
        warnings, &result);
    if (status != 0) {
        goto cleanup;
    }
    if (result == 0.0) {
        result = 0.0;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int trigonometric_function_result(struct trigonometric_input input,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_result)
{
    double result = 0.0;
    int status = 0;

    if (out_result == NULL) {
        return -1;
    }

    switch (input.function_id) {
    case MYLITE_SCALAR_FUNCTION_SIN:
        result = sin(input.input);
        break;
    case MYLITE_SCALAR_FUNCTION_COS:
        result = cos(input.input);
        break;
    case MYLITE_SCALAR_FUNCTION_TAN:
        result = tan(input.input);
        break;
    case MYLITE_SCALAR_FUNCTION_COT:
        result = 1.0 / tan(input.input);
        break;
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return -1;
    }

    if (trigonometric_result_is_out_of_range(input.function_id, result)) {
        status = append_cot_out_of_range_error(warnings);
        return status == 0 ? MYLITE_EXEC_ERROR : status;
    }

    *out_result = result;
    return 0;
}

static bool trigonometric_result_is_out_of_range(enum mylite_scalar_function_id function_id,
                                                 double result)
{
    return function_id == MYLITE_SCALAR_FUNCTION_COT && (isnan(result) || isinf(result));
}

static int eval_inverse_trigonometric_function(enum mylite_scalar_function_id function_id,
                                               const struct mylite_sql_ast_node *arguments,
                                               const struct mylite_expression_eval_context *context,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument_node = child_at(arguments, 0U);
    struct mylite_expression_value argument = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    bool null_result = false;
    int status = 0;

    if (function_id == MYLITE_SCALAR_FUNCTION_ATAN || function_id == MYLITE_SCALAR_FUNCTION_ATAN2) {
        return eval_atan_function(arguments, context, warnings, out_value);
    }

    status = eval_inverse_trigonometric_argument(argument_node, context, warnings, &argument,
                                                 &number, &null_result);

    if (status != 0) {
        goto cleanup;
    }
    if (null_result || inverse_trigonometric_input_is_out_of_domain(number.real_value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = inverse_trigonometric_function_result(
        (struct inverse_trigonometric_input){.function_id = function_id,
                                             .input = number.real_value},
        &result);
    if (status != 0) {
        goto cleanup;
    }
    if (inverse_trigonometric_result_is_null(result)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (result == 0.0) {
        result = 0.0;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int eval_atan_function(const struct mylite_sql_ast_node *arguments,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value y_argument = {0};
    struct mylite_expression_value x_argument = {0};
    struct numeric_value y_number = {0};
    struct numeric_value x_number = {0};
    size_t arity = child_count(arguments);
    double result = 0.0;
    bool null_result = false;
    int status = eval_atan_argument(child_at(arguments, 0U), context, warnings, &y_argument,
                                    &y_number, &null_result);

    if (status != 0) {
        goto cleanup;
    }
    if (null_result) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    if (arity == 2U) {
        status = eval_atan_argument(child_at(arguments, 1U), context, warnings, &x_argument,
                                    &x_number, &null_result);
        if (status != 0) {
            goto cleanup;
        }
        if (null_result) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            goto cleanup;
        }
    }

    if (y_number.real_value == 0.0) {
        y_number.real_value = 0.0;
    }
    if (x_number.real_value == 0.0) {
        x_number.real_value = 0.0;
    }
    result = atan_function_result(
        (struct atan_input){.y = y_number.real_value, .x = x_number.real_value}, arity);
    if (!isfinite(result)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (result == 0.0) {
        result = 0.0;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&x_argument);
    mylite_expression_value_deinit(&y_argument);
    return status;
}

static int eval_atan_argument(const struct mylite_sql_ast_node *argument_node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *argument,
                              struct numeric_value *number, bool *out_null_result)
{
    return eval_inverse_trigonometric_argument(argument_node, context, warnings, argument, number,
                                               out_null_result);
}

static double atan_function_result(struct atan_input input, size_t arity)
{
    if (arity == 1U) {
        return atan(input.y);
    }
    return atan2(input.y, input.x);
}

static int eval_inverse_trigonometric_argument(const struct mylite_sql_ast_node *argument_node,
                                               const struct mylite_expression_eval_context *context,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *argument,
                                               struct numeric_value *number, bool *out_null_result)
{
    int status = 0;

    *out_null_result = false;
    if (trigonometric_pi_expression_value(argument_node, &number->real_value)) {
        return 0;
    }

    status = eval_node(argument_node, context, warnings, argument);
    if (status != 0) {
        return status;
    }
    if (is_null(argument)) {
        *out_null_result = true;
        return 0;
    }
    return value_to_numeric(argument, warnings, number);
}

static int inverse_trigonometric_function_result(struct inverse_trigonometric_input input,
                                                 double *out_result)
{
    if (out_result == NULL) {
        return -1;
    }

    switch (input.function_id) {
    case MYLITE_SCALAR_FUNCTION_ACOS:
        *out_result = acos(input.input);
        return 0;
    case MYLITE_SCALAR_FUNCTION_ASIN:
        *out_result = asin(input.input);
        return 0;
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return -1;
    }
    return -1;
}

static bool inverse_trigonometric_input_is_out_of_domain(double value)
{
    return value < -1.0 || value > 1.0 || !isfinite(value);
}

static bool inverse_trigonometric_result_is_null(double result)
{
    return !isfinite(result);
}

static int eval_angle_conversion_function(enum mylite_scalar_function_id function_id,
                                          const struct mylite_sql_ast_node *arguments,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_warnings *warnings,
                                          struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *argument_node = child_at(arguments, 0U);
    struct mylite_expression_value argument = {0};
    struct numeric_value number = {0};
    double result = 0.0;
    bool null_result = false;
    int status = eval_angle_conversion_argument(argument_node, context, warnings, &argument,
                                                &number, &null_result);

    if (status != 0) {
        goto cleanup;
    }
    if (null_result) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = angle_conversion_result(
        (struct angle_conversion_input){.function_id = function_id, .input = number.real_value},
        &result);
    if (status != 0) {
        goto cleanup;
    }
    if (angle_conversion_result_is_out_of_range(result)) {
        status = append_angle_conversion_out_of_range_error(
            warnings, angle_conversion_function_name(function_id));
        if (status == 0) {
            status = MYLITE_EXEC_ERROR;
        }
        goto cleanup;
    }
    if (result == 0.0) {
        result = 0.0;
    }

    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = result,
        .compact_real_text = true,
    };

cleanup:
    mylite_expression_value_deinit(&argument);
    return status;
}

static int eval_angle_conversion_argument(const struct mylite_sql_ast_node *argument_node,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_warnings *warnings,
                                          struct mylite_expression_value *argument,
                                          struct numeric_value *number, bool *out_null_result)
{
    int status = 0;

    *out_null_result = false;
    if (trigonometric_pi_expression_value(argument_node, &number->real_value)) {
        return 0;
    }

    status = eval_node(argument_node, context, warnings, argument);
    if (status != 0) {
        return status;
    }
    if (is_null(argument)) {
        *out_null_result = true;
        return 0;
    }
    return value_to_numeric(argument, warnings, number);
}

static int angle_conversion_result(struct angle_conversion_input conversion, double *out_result)
{
    switch (conversion.function_id) {
    case MYLITE_SCALAR_FUNCTION_DEGREES:
        *out_result = (conversion.input * mylite_angle_straight_degrees) / mylite_pi_double_value;
        return 0;
    case MYLITE_SCALAR_FUNCTION_RADIANS:
        *out_result = (conversion.input / mylite_angle_straight_degrees) * mylite_pi_double_value;
        return 0;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
        return -1;
    }
    return -1;
}

static bool angle_conversion_result_is_out_of_range(double result)
{
    return isnan(result) || isinf(result);
}

static const char *angle_conversion_function_name(enum mylite_scalar_function_id function_id)
{
    if (function_id == MYLITE_SCALAR_FUNCTION_DEGREES) {
        return "degrees";
    }
    return "radians";
}

static bool trigonometric_pi_expression_value(const struct mylite_sql_ast_node *node,
                                              double *out_value)
{
    bool contains_pi = false;

    return trigonometric_pi_expression_value_impl(node, out_value, &contains_pi) && contains_pi;
}

static bool trigonometric_pi_expression_value_impl(const struct mylite_sql_ast_node *node,
                                                   double *out_value, bool *out_contains_pi)
{
    double left = 0.0;
    double right = 0.0;
    bool left_contains_pi = false;
    bool right_contains_pi = false;

    node = unwrap_parenthesized_node(node);
    if (node == NULL || out_value == NULL || out_contains_pi == NULL) {
        return false;
    }
    if (trigonometric_expression_is_pi_call(node)) {
        *out_value = mylite_pi_double_value;
        *out_contains_pi = true;
        return true;
    }
    if (trigonometric_pi_literal_value(node, out_value)) {
        *out_contains_pi = false;
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        trigonometric_pi_expression_value_impl(child_at(node, 0U), out_value, out_contains_pi)) {
        if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_value = -*out_value;
            return true;
        }
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE;
    }
    if (node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        !trigonometric_pi_expression_value_impl(child_at(node, 0U), &left, &left_contains_pi) ||
        !trigonometric_pi_expression_value_impl(child_at(node, 1U), &right, &right_contains_pi)) {
        return false;
    }
    *out_contains_pi = left_contains_pi || right_contains_pi;

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        *out_value = left + right;
        return true;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        *out_value = left - right;
        return true;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        *out_value = left * right;
        return true;
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        if (right == 0.0) {
            return false;
        }
        *out_value = left / right;
        return true;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_NONE:
        return false;
    }
    return false;
}

static bool trigonometric_pi_literal_value(const struct mylite_sql_ast_node *node,
                                           double *out_value)
{
    char *text = NULL;
    char *end = NULL;
    bool matched = false;

    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL ||
        (node->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         node->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         node->literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT)) {
        return false;
    }
    text = copy_span_text(node->span.text, node->span.length);
    if (text == NULL) {
        return false;
    }
    errno = 0;
    *out_value = strtod(text, &end);
    matched = errno != ERANGE && end != text && end != NULL && *end == '\0';
    free(text);
    return matched;
}

static bool trigonometric_expression_is_pi_call(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *arguments = NULL;

    if (node == NULL || node->kind != MYLITE_SQL_AST_FUNCTION_CALL ||
        scalar_function_id(node) != MYLITE_SCALAR_FUNCTION_PI) {
        return false;
    }
    arguments = child_at(node, 1U);
    return arguments != NULL && arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST &&
           child_count(arguments) == 0U;
}

static int eval_round_function(const struct mylite_sql_ast_node *arguments,
                               const struct mylite_expression_eval_context *context,
                               struct mylite_expression_warnings *warnings,
                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    const struct mylite_sql_ast_node *value_argument = child_at(arguments, 0U);
    int scale = 0;
    bool handled = false;
    int status = eval_node(value_argument, context, warnings, &value);

    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    status = eval_round_scale(arguments, context, warnings, &scale);
    if (status == 1) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }
    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return status;
    }
    status =
        round_exact_argument_value(value_argument, &value, scale, warnings, out_value, &handled);
    if (status == 0 && !handled) {
        status = round_approximate_value(&value, scale, warnings, out_value);
    }

    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_round_scale(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings, int *out_scale)
{
    const struct mylite_sql_ast_node *scale_argument = child_at(arguments, 1U);
    struct mylite_expression_value value = {0};
    int status = 0;

    if (out_scale == NULL) {
        return -1;
    }
    *out_scale = 0;
    if (scale_argument == NULL) {
        return 0;
    }
    status = eval_node(scale_argument, context, warnings, &value);
    if (status == 0 && is_null(&value)) {
        *out_scale = 0;
        mylite_expression_value_deinit(&value);
        return 1;
    }
    if (status == 0) {
        status = round_scale_from_value(&value, warnings, out_scale);
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_format_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *value_argument = child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = child_at(arguments, 1U);
    const struct mylite_sql_ast_node *locale_argument = child_at(arguments, 2U);
    struct mylite_expression_value scale_value = {0};
    struct mylite_expression_value value = {0};
    struct format_numeric_input input = {0};
    const struct format_locale *locale = format_default_locale();
    char *rounded_text = NULL;
    size_t rounded_length = 0U;
    bool locale_resolved = false;
    int scale = 0;
    int status = 0;

    if (locale_argument != NULL && format_locale_argument_is_literal(locale_argument)) {
        status = format_locale_from_argument(locale_argument, context, warnings, &locale);
        locale_resolved = true;
    }
    if (status == 0) {
        status = eval_node(scale_argument, context, warnings, &scale_value);
    }
    if (status != 0) {
        return status;
    }
    if (is_null(&scale_value)) {
        if (locale_argument != NULL && !locale_resolved) {
            status = format_locale_from_argument(locale_argument, context, warnings, &locale);
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&scale_value);
        return status;
    }

    if (status == 0) {
        status = format_scale_from_value(&scale_value, warnings, &scale);
    }
    if (status == 0 && locale_argument != NULL && !locale_resolved) {
        status = format_locale_from_argument(locale_argument, context, warnings, &locale);
    }
    if (status == 0) {
        status = eval_node(value_argument, context, warnings, &value);
    }
    if (status == 0 && !is_null(&value)) {
        status = format_input_from_value(value_argument, &value, warnings, &input);
    }
    if (status == 0 && is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        status = format_rounded_text_from_input(&input, scale, &rounded_text, &rounded_length);
        if (status == 0) {
            status = format_apply_locale(rounded_text, rounded_length, locale, out_value);
        }
    }

    free(rounded_text);
    format_numeric_input_deinit(&input);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&scale_value);
    return status;
}

static int format_locale_from_argument(const struct mylite_sql_ast_node *locale_argument,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       const struct format_locale **out_locale)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    size_t text_length = 0U;
    int status = 0;

    if (out_locale == NULL) {
        return -1;
    }
    *out_locale = format_default_locale();
    if (locale_argument == NULL) {
        return 0;
    }

    status = eval_node(locale_argument, context, warnings, &value);
    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        status = append_format_unknown_locale_warning(warnings, "NULL", 4U);
        mylite_expression_value_deinit(&value);
        return status;
    }

    status = value_to_string_with_length(&value, &text, &text_length);
    if (status == 0) {
        const struct format_locale *locale = format_locale_by_name(text, text_length);

        if (locale != NULL) {
            *out_locale = locale;
        } else {
            status = append_format_unknown_locale_warning(warnings, text, text_length);
        }
    }
    free(text);
    mylite_expression_value_deinit(&value);
    return status;
}

static bool format_locale_argument_is_literal(const struct mylite_sql_ast_node *locale_argument)
{
    while (locale_argument != NULL &&
           locale_argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        locale_argument = child_at(locale_argument, 0U);
    }
    return locale_argument != NULL && locale_argument->kind == MYLITE_SQL_AST_LITERAL;
}

static int format_input_from_value(const struct mylite_sql_ast_node *argument,
                                   const struct mylite_expression_value *value,
                                   struct mylite_expression_warnings *warnings,
                                   struct format_numeric_input *out_input)
{
    struct round_exact_argument_text exact = {0};
    int status = 0;

    if (out_input == NULL || value == NULL || is_null(value)) {
        return -1;
    }
    *out_input = (struct format_numeric_input){0};
    status = round_exact_argument_text(argument, value, &exact);
    if (status != 0) {
        return status;
    }
    if (exact.text != NULL) {
        out_input->exact_text = exact.text;
        out_input->exact_text_length = exact.text_length;
        out_input->has_exact_text = true;
        return 0;
    }
    return value_to_numeric(value, warnings, &out_input->number);
}

static int format_scale_from_value(const struct mylite_expression_value *value,
                                   struct mylite_expression_warnings *warnings, int *out_scale)
{
    int scale = 0;
    int status = round_scale_from_value(value, warnings, &scale);

    if (status != 0) {
        return status;
    }
    if (scale < 0) {
        scale = 0;
    }
    *out_scale = scale;
    return 0;
}

static int format_rounded_text_from_input(const struct format_numeric_input *input, int scale,
                                          char **out_text, size_t *out_length)
{
    if (input == NULL || out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = NULL;
    *out_length = 0U;
    if (input->has_exact_text) {
        return format_round_exact_decimal_text(
            (struct format_exact_round_input){
                .text = input->exact_text,
                .text_length = input->exact_text_length,
                .scale = scale,
            },
            out_text, out_length);
    }
    return format_round_approximate_text(
        (struct format_approximate_round_input){
            .value = input->number.real_value,
            .scale = scale,
        },
        out_text, out_length);
}

static int format_round_exact_decimal_text(struct format_exact_round_input input, char **out_text,
                                           size_t *out_length)
{
    char *copy = copy_span_text(input.text == NULL ? "" : input.text, input.text_length);
    struct decimal_text_parts parts = {0};
    size_t fraction_length = (size_t)input.scale;
    size_t copied_fraction = 0U;
    size_t digits_length = 0U;
    char *digits = NULL;
    struct mylite_expression_value value = {0};
    int status = 0;

    if (copy == NULL || out_text == NULL || out_length == NULL) {
        free(copy);
        return -1;
    }
    if (!parse_decimal_text_parts(copy, &parts)) {
        free(copy);
        return -1;
    }
    copied_fraction =
        fraction_length < parts.fraction_length ? fraction_length : parts.fraction_length;
    digits_length = parts.integer_length + fraction_length;
    digits = malloc(digits_length + 1U);
    if (digits == NULL) {
        free(copy);
        return -1;
    }
    memcpy(digits, parts.integer, parts.integer_length);
    if (copied_fraction != 0U) {
        memcpy(digits + parts.integer_length, parts.fraction, copied_fraction);
    }
    if (fraction_length > copied_fraction) {
        memset(digits + parts.integer_length + copied_fraction, '0',
               fraction_length - copied_fraction);
    }
    digits[digits_length] = '\0';

    if (fraction_length < parts.fraction_length && parts.fraction[fraction_length] >= '5') {
        status = increment_decimal_digits(&digits, &digits_length);
    }
    if (status == 0) {
        status = round_append_signed_decimal_result(&parts, digits, digits_length, fraction_length,
                                                    &value);
    }
    if (status == 0) {
        *out_text = value.text_value;
        *out_length = value.text_length;
        value.text_value = NULL;
        value.text_length = 0U;
        value.kind = MYLITE_EXPRESSION_VALUE_NULL;
    }

    mylite_expression_value_deinit(&value);
    free(digits);
    free(copy);
    return status;
}

static int format_round_approximate_text(struct format_approximate_round_input input,
                                         char **out_text, size_t *out_length)
{
    char buffer[MYLITE_EXPRESSION_FORMAT_TEXT_BUFFER_SIZE];
    long double factor = 1.0L;
    long double scaled = 0.0L;
    long double rounded = 0.0L;
    long double formatted = 0.0L;
    int length = 0;

    if (out_text == NULL || out_length == NULL) {
        return -1;
    }
    for (int index = 0; index < input.scale; ++index) {
        factor *= (long double)MYLITE_EXPRESSION_DECIMAL_BASE;
    }
    scaled = (long double)input.value * factor;
    rounded = round_half_even_long_double(scaled);
    formatted = rounded / factor;
    if (rounded == 0.0L && signbit((double)input.value)) {
        formatted = -0.0L;
    } else if (rounded == 0.0L) {
        formatted = 0.0L;
    }
    length = snprintf(buffer, sizeof(buffer), "%.*Lf", input.scale, formatted);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_text = copy_span_text(buffer, (size_t)length);
    if (*out_text == NULL) {
        return -1;
    }
    *out_length = (size_t)length;
    return 0;
}

static int format_apply_locale(const char *rounded_text, size_t rounded_length,
                               const struct format_locale *locale,
                               struct mylite_expression_value *out_value)
{
    const char *text = rounded_text == NULL ? "" : rounded_text;
    size_t text_length = rounded_text == NULL ? 0U : rounded_length;
    const char *integer = text;
    size_t integer_length = 0U;
    const char *fraction = NULL;
    size_t fraction_length = 0U;
    char *result = copy_span_text("", 0U);
    size_t result_length = 0U;
    int status = 0;

    if (result == NULL || locale == NULL) {
        free(result);
        return -1;
    }
    if (text_length != 0U && text[0] == '-') {
        status = append_text(&result, &result_length, "-", 1U);
        integer = text + 1U;
        --text_length;
    }
    fraction = memchr(integer, '.', text_length);
    if (fraction != NULL) {
        integer_length = (size_t)(fraction - integer);
        ++fraction;
        fraction_length = text_length - integer_length - 1U;
    } else {
        integer_length = text_length;
    }
    if (status == 0) {
        status =
            format_append_grouped_integer(&result, &result_length, integer, integer_length, locale);
    }
    if (status == 0 && fraction_length != 0U) {
        status = append_text(&result, &result_length, locale->decimal_separator,
                             strlen(locale->decimal_separator));
    }
    if (status == 0 && fraction_length != 0U) {
        status = append_text(&result, &result_length, fraction, fraction_length);
    }
    if (status == 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = result;
        out_value->text_length = result_length;
        result = NULL;
    }
    free(result);
    return status;
}

static int format_append_grouped_integer(char **result, size_t *result_length, const char *integer,
                                         size_t integer_length, const struct format_locale *locale)
{
    const char *separator = locale->group_separator == NULL ? "" : locale->group_separator;
    size_t separator_length = strlen(separator);

    if (locale->grouping == FORMAT_GROUPING_NONE || separator_length == 0U) {
        return append_text(result, result_length, integer, integer_length);
    }
    if (locale->grouping == FORMAT_GROUPING_INDIAN) {
        return format_append_indian_grouped_integer(result, result_length, integer, integer_length,
                                                    separator, separator_length);
    }
    return format_append_western_grouped_integer(result, result_length, integer, integer_length,
                                                 separator, separator_length);
}

static int format_append_western_grouped_integer(char **result, size_t *result_length,
                                                 const char *integer, size_t integer_length,
                                                 const char *separator, size_t separator_length)
{
    size_t first_group = integer_length % 3U;
    size_t offset = 0U;
    int status = 0;

    if (integer_length <= 3U) {
        return append_text(result, result_length, integer, integer_length);
    }
    if (first_group == 0U) {
        first_group = 3U;
    }
    status = append_text(result, result_length, integer, first_group);
    offset = first_group;
    while (status == 0 && offset < integer_length) {
        status = append_text(result, result_length, separator, separator_length);
        if (status == 0) {
            status = append_text(result, result_length, integer + offset, 3U);
            offset += 3U;
        }
    }
    return status;
}

static int format_append_indian_grouped_integer(char **result, size_t *result_length,
                                                const char *integer, size_t integer_length,
                                                const char *separator, size_t separator_length)
{
    size_t prefix_length = 0U;
    size_t offset = 0U;
    int status = 0;

    if (integer_length <= 3U) {
        return append_text(result, result_length, integer, integer_length);
    }
    prefix_length = (integer_length - 3U) % 2U;
    if (prefix_length == 0U) {
        prefix_length = 2U;
    }
    status = append_text(result, result_length, integer, prefix_length);
    offset = prefix_length;
    while (status == 0 && integer_length - offset > 3U) {
        status = append_text(result, result_length, separator, separator_length);
        if (status == 0) {
            status = append_text(result, result_length, integer + offset, 2U);
            offset += 2U;
        }
    }
    if (status == 0) {
        status = append_text(result, result_length, separator, separator_length);
    }
    if (status == 0) {
        status = append_text(result, result_length, integer + offset, integer_length - offset);
    }
    return status;
}

static const struct format_locale *format_locale_by_name(const char *name, size_t name_length)
{
    static const struct format_locale locales[] = {
        {"en_US", ".", ",", FORMAT_GROUPING_WESTERN}, {"de_DE", ",", ".", FORMAT_GROUPING_WESTERN},
        {"en_IN", ".", ",", FORMAT_GROUPING_INDIAN},  {"ru_RU", ",", " ", FORMAT_GROUPING_WESTERN},
        {"fr_FR", ",", "", FORMAT_GROUPING_NONE},     {"nl_NL", ",", "", FORMAT_GROUPING_NONE},
    };

    for (size_t index = 0U; index < sizeof(locales) / sizeof(locales[0]); ++index) {
        if (strlen(locales[index].name) == name_length &&
            ascii_text_equal_ci((struct text_compare_input){.left = locales[index].name,
                                                            .left_length = name_length,
                                                            .right = name,
                                                            .right_length = name_length})) {
            return &locales[index];
        }
    }
    return NULL;
}

static const struct format_locale *format_default_locale(void)
{
    static const struct format_locale locale = {"en_US", ".", ",", FORMAT_GROUPING_WESTERN};

    return &locale;
}

static int append_format_unknown_locale_warning(struct mylite_expression_warnings *warnings,
                                                const char *locale, size_t locale_length)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Unknown locale: '%.*s'", (int)locale_length,
                          locale == NULL ? "" : locale);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_UNKNOWN_LOCALE, message);
}

static void format_numeric_input_deinit(struct format_numeric_input *input)
{
    if (input == NULL) {
        return;
    }
    free(input->exact_text);
    *input = (struct format_numeric_input){0};
}

static int round_scale_from_value(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_scale)
{
    int64_t scale = 0;

    if (value == NULL || out_scale == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_scale = value->uint64_value > MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT
                         ? MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT
                         : (int)value->uint64_value;
        return 0;
    }
    if (cast_value_to_signed_integer(value, warnings, &scale) != 0) {
        return -1;
    }
    if (scale > MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT) {
        scale = MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT;
    } else if (scale < -MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT) {
        scale = -MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT;
    }
    *out_scale = (int)scale;
    return 0;
}

static int round_exact_argument_value(const struct mylite_sql_ast_node *argument,
                                      const struct mylite_expression_value *value, int scale,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value, bool *out_handled)
{
    struct round_exact_argument_text exact = {0};
    int status = 0;

    if (out_handled == NULL) {
        return -1;
    }
    *out_handled = false;
    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return 0;
    }

    status = round_exact_argument_text(argument, value, &exact);
    if (status != 0 || exact.text == NULL) {
        return status;
    }

    status = round_exact_decimal_text(exact.text, exact.text_length, out_value, scale);
    if (status == 0) {
        status = round_check_integer_result_bound(exact, warnings, out_value);
    }

    free(exact.text);
    *out_handled = status == 0;
    return status;
}

static int round_exact_argument_text(const struct mylite_sql_ast_node *argument,
                                     const struct mylite_expression_value *value,
                                     struct round_exact_argument_text *out_text)
{
    char buffer[MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE];
    bool integer_literal = false;
    int length = 0;

    if (out_text == NULL) {
        return -1;
    }
    *out_text = (struct round_exact_argument_text){0};
    if (round_argument_exact_literal_text(argument, &out_text->text, &out_text->text_length,
                                          &integer_literal)) {
        out_text->bound_signed = integer_literal && value->kind != MYLITE_EXPRESSION_VALUE_UINT64;
        out_text->bound_unsigned = integer_literal && value->kind == MYLITE_EXPRESSION_VALUE_UINT64;
        return out_text->text == NULL ? -1 : 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        out_text->bound_signed = true;
    } else if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        out_text->bound_unsigned = true;
    } else {
        return 0;
    }
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    out_text->text = copy_span_text(buffer, (size_t)length);
    out_text->text_length = (size_t)length;
    return out_text->text == NULL ? -1 : 0;
}

static int round_check_integer_result_bound(struct round_exact_argument_text input,
                                            struct mylite_expression_warnings *warnings,
                                            struct mylite_expression_value *out_value)
{
    const char *result = NULL;
    bool negative = false;
    const char *magnitude = NULL;
    const char *bound = "18446744073709551615";
    int status = 0;

    if ((!input.bound_signed && !input.bound_unsigned) ||
        out_value->kind != MYLITE_EXPRESSION_VALUE_TEXT || out_value->text_value == NULL) {
        return 0;
    }
    result = out_value->text_value;
    negative = result[0] == '-';
    magnitude = negative ? result + 1 : result;
    if (!input.bound_unsigned) {
        bound = negative ? "9223372036854775808" : "9223372036854775807";
    }
    trim_leading_decimal_zeros(&magnitude, &(size_t){strlen(magnitude)});
    if (!decimal_text_exceeds_bound(magnitude, bound)) {
        return 0;
    }
    mylite_expression_value_deinit(out_value);
    status = append_round_out_of_range_error(warnings, input.bound_unsigned);
    return status == 0 ? MYLITE_EXEC_ERROR : status;
}

static int round_exact_decimal_text(const char *text, size_t text_length,
                                    struct mylite_expression_value *out_value, int scale)
{
    char *copy = copy_span_text(text == NULL ? "" : text, text_length);
    struct decimal_text_parts parts = {0};
    int status = 0;

    if (copy == NULL) {
        return -1;
    }
    if (!parse_decimal_text_parts(copy, &parts)) {
        free(copy);
        return -1;
    }
    if (scale >= 0) {
        status = round_exact_decimal_positive_scale(&parts, scale, out_value);
    } else {
        status = round_exact_decimal_negative_scale(&parts, scale, out_value);
    }
    free(copy);
    return status;
}

static int round_exact_decimal_positive_scale(const struct decimal_text_parts *parts, int scale,
                                              struct mylite_expression_value *out_value)
{
    size_t requested_fraction = (size_t)scale;
    size_t kept_fraction =
        requested_fraction < parts->fraction_length ? requested_fraction : parts->fraction_length;
    size_t digits_length = parts->integer_length + kept_fraction;
    char *digits = malloc(digits_length + 1U);
    int status = 0;

    if (digits == NULL) {
        return -1;
    }
    memcpy(digits, parts->integer, parts->integer_length);
    if (kept_fraction != 0U) {
        memcpy(digits + parts->integer_length, parts->fraction, kept_fraction);
    }
    digits[digits_length] = '\0';
    if (kept_fraction < parts->fraction_length && parts->fraction[kept_fraction] >= '5') {
        status = increment_decimal_digits(&digits, &digits_length);
    }
    if (status == 0) {
        status = round_append_signed_decimal_result(parts, digits, digits_length, kept_fraction,
                                                    out_value);
    }
    free(digits);
    return status;
}

static int round_exact_decimal_negative_scale(const struct decimal_text_parts *parts, int scale,
                                              struct mylite_expression_value *out_value)
{
    size_t places = (size_t)(-scale);
    size_t kept_integer = parts->integer_length > places ? parts->integer_length - places : 0U;
    size_t digits_length = kept_integer;
    char *digits = malloc(digits_length + 1U);
    int status = 0;
    char round_digit = '0';

    if (digits == NULL) {
        return -1;
    }
    if (kept_integer != 0U) {
        memcpy(digits, parts->integer, kept_integer);
    }
    digits[digits_length] = '\0';
    if (parts->integer_length >= places && places != 0U) {
        round_digit = parts->integer[parts->integer_length - places];
    }
    if (round_digit >= '5') {
        status = increment_decimal_digits(&digits, &digits_length);
    }
    if (status == 0 && !(digits_length == 1U && digits[0] == '0')) {
        size_t old_length = digits_length;
        char *grown = realloc(digits, digits_length + places + 1U);

        if (grown == NULL) {
            free(digits);
            return -1;
        }
        digits = grown;
        memset(digits + old_length, '0', places);
        digits_length += places;
        digits[digits_length] = '\0';
    }
    if (status == 0) {
        status = round_append_signed_decimal_result(parts, digits, digits_length, 0U, out_value);
    }
    free(digits);
    return status;
}

static int round_append_signed_decimal_result(const struct decimal_text_parts *parts,
                                              const char *digits, size_t digits_length,
                                              size_t fraction_length,
                                              struct mylite_expression_value *out_value)
{
    const char *integer = digits;
    size_t integer_length = 0U;
    bool has_nonzero_digit = false;
    size_t output_length = 0U;
    char *result = NULL;
    size_t offset = 0U;

    if (digits == NULL) {
        digits = "0";
        digits_length = 1U;
        fraction_length = 0U;
    }
    if (fraction_length > digits_length) {
        return -1;
    }
    integer = digits;
    integer_length = digits_length - fraction_length;
    has_nonzero_digit = !decimal_digits_all_zero(digits, digits_length);

    while (integer_length > 1U && *integer == '0') {
        ++integer;
        --integer_length;
    }
    if (integer_length == 0U) {
        integer = "0";
        integer_length = 1U;
    }
    output_length = (parts->negative && has_nonzero_digit ? 1U : 0U) + integer_length +
                    (fraction_length == 0U ? 0U : 1U + fraction_length);
    result = malloc(output_length + 1U);
    if (result == NULL) {
        return -1;
    }
    if (parts->negative && has_nonzero_digit) {
        result[offset++] = '-';
    }
    memcpy(result + offset, integer, integer_length);
    offset += integer_length;
    if (fraction_length != 0U) {
        size_t fraction_offset = digits_length - fraction_length;

        result[offset++] = '.';
        memcpy(result + offset, digits + fraction_offset, fraction_length);
        offset += fraction_length;
    }
    result[offset] = '\0';
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    out_value->text_length = output_length;
    return 0;
}

static int round_approximate_value(const struct mylite_expression_value *value, int scale,
                                   struct mylite_expression_warnings *warnings,
                                   struct mylite_expression_value *out_value)
{
    struct numeric_value number = {0};
    long double factor = 1.0L;
    long double rounded = 0.0L;
    int places = scale < 0 ? -scale : scale;
    int status = value_to_numeric(value, warnings, &number);

    if (status != 0) {
        return status;
    }
    for (int index = 0; index < places; ++index) {
        factor *= (long double)MYLITE_EXPRESSION_DECIMAL_BASE;
    }
    if (scale >= 0) {
        rounded = round_half_even_long_double((long double)number.real_value * factor) / factor;
    } else {
        rounded = round_half_even_long_double((long double)number.real_value / factor) * factor;
    }
    return set_round_approximate_text(rounded, out_value, scale);
}

static long double round_half_even_long_double(long double value)
{
    long double truncated = 0.0L;
    long double fraction = 0.0L;
    int64_t integer = 0;

    if (value >= (long double)INT64_MAX || value <= (long double)INT64_MIN) {
        return value;
    }
    integer = (int64_t)value;
    truncated = (long double)integer;
    fraction = value - truncated;
    if (fraction > (long double)mylite_expression_round_half ||
        (fraction == (long double)mylite_expression_round_half && (integer & INT64_C(1)) != 0)) {
        return truncated + 1.0L;
    }
    if (fraction < -(long double)mylite_expression_round_half ||
        (fraction == -(long double)mylite_expression_round_half && (integer & INT64_C(1)) != 0)) {
        return truncated - 1.0L;
    }
    return truncated;
}

static int set_round_approximate_text(long double value, struct mylite_expression_value *out_value,
                                      int scale)
{
    char buffer[MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE];
    int decimals = scale > 0 ? scale : 0;
    int length = 0;

    if (decimals > MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT) {
        decimals = MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT;
    }
    length = snprintf(buffer, sizeof(buffer), "%.*Lf", decimals, value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    while (length > 0 && buffer[length - 1] == '0') {
        buffer[--length] = '\0';
    }
    if (length > 0 && buffer[length - 1] == '.') {
        buffer[--length] = '\0';
    }
    if (strcmp(buffer, "-0") == 0 || buffer[0] == '\0') {
        buffer[0] = '0';
        buffer[1] = '\0';
        length = 1;
    }
    return set_text_value(buffer, (size_t)length, out_value);
}

static int eval_truncate_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *value_argument = child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = child_at(arguments, 1U);
    struct mylite_expression_value value = {0};
    struct mylite_expression_value scale_value = {0};
    struct round_exact_argument_text exact = {0};
    struct numeric_value number = {0};
    bool value_is_null = false;
    bool scale_is_null = false;
    int scale = 0;
    int status = eval_node(value_argument, context, warnings, &value);

    if (status != 0) {
        goto cleanup;
    }
    status = eval_node(scale_argument, context, warnings, &scale_value);
    if (status != 0) {
        goto cleanup;
    }
    value_is_null = is_null(&value);
    scale_is_null = is_null(&scale_value);

    if (!value_is_null) {
        status = round_exact_argument_text(value_argument, &value, &exact);
        if (status != 0) {
            goto cleanup;
        }
        if (exact.text == NULL) {
            status = value_to_numeric(&value, warnings, &number);
            if (status != 0) {
                goto cleanup;
            }
        }
    }
    if (!scale_is_null) {
        status = round_scale_from_value(&scale_value, warnings, &scale);
        if (status != 0) {
            goto cleanup;
        }
    }

    if (value_is_null || scale_is_null) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }
    if (exact.text != NULL) {
        status = truncate_exact_decimal_text(exact.text, exact.text_length, out_value, scale);
    } else {
        status = truncate_approximate_numeric(&number, scale, out_value);
    }

cleanup:
    free(exact.text);
    mylite_expression_value_deinit(&scale_value);
    mylite_expression_value_deinit(&value);
    return status;
}

static int truncate_exact_decimal_text(const char *text, size_t text_length,
                                       struct mylite_expression_value *out_value, int scale)
{
    char *copy = copy_span_text(text == NULL ? "" : text, text_length);
    struct decimal_text_parts parts = {0};
    int status = 0;

    if (copy == NULL) {
        return -1;
    }
    if (!parse_decimal_text_parts(copy, &parts)) {
        free(copy);
        return -1;
    }
    if (scale >= 0) {
        status = truncate_exact_decimal_positive_scale(&parts, scale, out_value);
    } else {
        status = truncate_exact_decimal_negative_scale(&parts, scale, out_value);
    }
    free(copy);
    return status;
}

static int truncate_exact_decimal_positive_scale(const struct decimal_text_parts *parts, int scale,
                                                 struct mylite_expression_value *out_value)
{
    size_t requested_fraction = (size_t)scale;
    size_t kept_fraction =
        requested_fraction < parts->fraction_length ? requested_fraction : parts->fraction_length;
    size_t digits_length = parts->integer_length + kept_fraction;
    char *digits = malloc(digits_length + 1U);
    int status = 0;

    if (digits == NULL) {
        return -1;
    }
    memcpy(digits, parts->integer, parts->integer_length);
    if (kept_fraction != 0U) {
        memcpy(digits + parts->integer_length, parts->fraction, kept_fraction);
    }
    digits[digits_length] = '\0';
    status =
        round_append_signed_decimal_result(parts, digits, digits_length, kept_fraction, out_value);
    free(digits);
    return status;
}

static int truncate_exact_decimal_negative_scale(const struct decimal_text_parts *parts, int scale,
                                                 struct mylite_expression_value *out_value)
{
    size_t places = (size_t)(-scale);
    size_t kept_integer = parts->integer_length > places ? parts->integer_length - places : 0U;
    size_t digits_length = kept_integer;
    char *digits = malloc(digits_length + 1U);
    int status = 0;

    if (digits == NULL) {
        return -1;
    }
    if (kept_integer != 0U) {
        memcpy(digits, parts->integer, kept_integer);
    }
    digits[digits_length] = '\0';
    if (!(digits_length == 0U || (digits_length == 1U && digits[0] == '0'))) {
        size_t old_length = digits_length;
        char *grown = realloc(digits, digits_length + places + 1U);

        if (grown == NULL) {
            free(digits);
            return -1;
        }
        digits = grown;
        memset(digits + old_length, '0', places);
        digits_length += places;
        digits[digits_length] = '\0';
    }
    status = round_append_signed_decimal_result(parts, digits, digits_length, 0U, out_value);
    free(digits);
    return status;
}

static int truncate_approximate_numeric(const struct numeric_value *number, int scale,
                                        struct mylite_expression_value *out_value)
{
    long double factor = 1.0L;
    long double truncated = 0.0L;
    int places = scale < 0 ? -scale : scale;

    if (number == NULL) {
        return -1;
    }
    for (int index = 0; index < places; ++index) {
        factor *= (long double)MYLITE_EXPRESSION_DECIMAL_BASE;
    }
    if (scale >= 0) {
        truncated = truncl((long double)number->real_value * factor) / factor;
    } else {
        truncated = truncl((long double)number->real_value / factor) * factor;
    }
    return set_truncate_approximate_text(truncated, out_value, scale);
}

static int set_truncate_approximate_text(long double value,
                                         struct mylite_expression_value *out_value, int scale)
{
    char buffer[MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE];
    int decimals = scale > 0 ? scale : 0;
    int length = 0;

    if (decimals > MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT) {
        decimals = MYLITE_EXPRESSION_DECIMAL_ROUND_SCALE_LIMIT;
    }
    if (decimals == 0 && isfinite(value) && !(value == 0.0L && signbit(value)) &&
        value > (long double)INT64_MIN && value < (long double)INT64_MAX) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = (int64_t)value,
        };
        return 0;
    }
    length = snprintf(buffer, sizeof(buffer), "%.*Lf", decimals, value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    while (length > 0 && buffer[length - 1] == '0') {
        buffer[--length] = '\0';
    }
    if (length > 0 && buffer[length - 1] == '.') {
        buffer[--length] = '\0';
    }
    if (buffer[0] == '\0') {
        buffer[0] = '0';
        buffer[1] = '\0';
        length = 1;
    }
    return set_text_value(buffer, (size_t)length, out_value);
}

static bool round_argument_exact_literal_text(const struct mylite_sql_ast_node *argument,
                                              char **out_text, size_t *out_length,
                                              bool *out_integer_literal)
{
    char sign = '\0';

    if (out_text == NULL || out_length == NULL || out_integer_literal == NULL) {
        return false;
    }
    *out_text = NULL;
    *out_length = 0U;
    *out_integer_literal = false;
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = child_at(argument, 0U);
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        sign = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? '-' : '+';
        argument = child_at(argument, 0U);
        while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            argument = child_at(argument, 0U);
        }
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        (argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL)) {
        return false;
    }
    *out_integer_literal = argument->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER;
    return copy_base_conversion_literal_text(sign, argument, out_text, out_length) == 0;
}

static bool parse_decimal_text_parts(char *text, struct decimal_text_parts *out_parts)
{
    char *scan = text == NULL ? NULL : text;
    char *dot = NULL;

    if (scan == NULL || out_parts == NULL) {
        return false;
    }
    *out_parts = (struct decimal_text_parts){0};
    if (*scan == '+' || *scan == '-') {
        out_parts->negative = *scan == '-';
        ++scan;
    }
    dot = strchr(scan, '.');
    if (dot != NULL) {
        *dot = '\0';
        out_parts->fraction = dot + 1;
        out_parts->fraction_length = strlen(out_parts->fraction);
    } else {
        out_parts->fraction = "";
    }
    out_parts->integer = scan;
    out_parts->integer_length = strlen(scan);
    if (out_parts->integer_length == 0U) {
        out_parts->integer = "0";
        out_parts->integer_length = 1U;
    }
    trim_leading_decimal_zeros(&out_parts->integer, &out_parts->integer_length);
    return true;
}

static void trim_leading_decimal_zeros(const char **digits, size_t *length)
{
    if (digits == NULL || length == NULL || *digits == NULL) {
        return;
    }
    while (*length > 1U && **digits == '0') {
        ++*digits;
        --*length;
    }
}

static bool decimal_digits_all_zero(const char *digits, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (digits[index] != '0') {
            return false;
        }
    }
    return true;
}

static bool decimal_text_exceeds_bound(const char *text, const char *bound)
{
    size_t text_length = strlen(text == NULL ? "" : text);
    size_t bound_length = strlen(bound == NULL ? "" : bound);

    if (text_length != bound_length) {
        return text_length > bound_length;
    }
    return strcmp(text == NULL ? "" : text, bound == NULL ? "" : bound) > 0;
}

static int increment_decimal_digits(char **digits, size_t *length)
{
    size_t index = 0U;
    char *grown = NULL;

    if (digits == NULL || *digits == NULL || length == NULL) {
        return -1;
    }
    index = *length;
    while (index > 0U) {
        --index;
        if ((*digits)[index] < '9') {
            ++(*digits)[index];
            return 0;
        }
        (*digits)[index] = '0';
    }
    grown = realloc(*digits, *length + 2U);
    if (grown == NULL) {
        return -1;
    }
    memmove(grown + 1U, grown, *length + 1U);
    grown[0] = '1';
    ++*length;
    *digits = grown;
    return 0;
}

static int append_round_out_of_range_error(struct mylite_expression_warnings *warnings,
                                           bool unsigned_value)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_OUT_OF_RANGE,
        unsigned_value ? "BIGINT UNSIGNED value is out of range in "
                         "'round()'"
                       : "BIGINT value is out of range in 'round()'");
}

static int eval_numeric_unary_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct numeric_value number = {0};
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        mylite_expression_value_deinit(&value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_numeric(&value, warnings, &number);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_ABS:
        eval_abs_function(&number, out_value);
        return 0;
    case MYLITE_SCALAR_FUNCTION_SIGN:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (number.real_value > 0.0) -
                                                                     (number.real_value < 0.0)};
        return 0;
    case MYLITE_SCALAR_FUNCTION_FLOOR:
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                             .int64_value = floor_real_value(number.real_value)};
        return 0;
    case MYLITE_SCALAR_FUNCTION_CEIL:
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                             .int64_value = ceil_real_value(number.real_value)};
        return 0;
    default:
        return -1;
    }
}

static void eval_abs_function(const struct numeric_value *number,
                              struct mylite_expression_value *out_value)
{
    if (number->is_integer && !number->is_unsigned && number->int64_value >= 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = number->int64_value};
    } else if (number->is_integer && !number->is_unsigned && number->int64_value < 0 &&
               number->int64_value != INT64_MIN) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = -number->int64_value};
    } else if (number->is_integer && number->is_unsigned) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = number->uint64_value};
    } else {
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_REAL,
                                             .real_value = absolute_real_value(number->real_value)};
    }
}

static int eval_if_function(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    struct mylite_expression_value condition = {0};
    size_t branch_index = 2U;
    int truth = -1;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &condition);

    if (status == 0) {
        status = truth_value(&condition, warnings, &truth);
    }
    mylite_expression_value_deinit(&condition);
    if (status != 0) {
        return status;
    }
    if (truth == 1) {
        branch_index = 1U;
    }
    return eval_node(child_at(arguments, branch_index), context, warnings, out_value);
}

static int eval_ifnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (is_null(&value)) {
        mylite_expression_value_deinit(&value);
        return eval_node(child_at(arguments, 1U), context, warnings, out_value);
    }
    *out_value = value;
    return 0;
}

static int eval_nullif_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    bool moved_left = false;
    int comparison = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &left);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &right);
    }
    if (status == 0 && !is_null(&left) && !is_null(&right)) {
        status = compare_values(&left, &right, warnings, &comparison);
    }
    if (status == 0 && comparison == 0 && !is_null(&left) && !is_null(&right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        *out_value = left;
        moved_left = true;
    }
    if (!moved_left) {
        mylite_expression_value_deinit(&left);
    }
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_coalesce_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        int status = eval_node(argument, context, warnings, &value);

        if (status != 0) {
            mylite_expression_value_deinit(&value);
            return status;
        }
        if (!is_null(&value)) {
            *out_value = value;
            return 0;
        }
        mylite_expression_value_deinit(&value);
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    return 0;
}

static int eval_greatest_least_function(enum mylite_scalar_function_id function_id,
                                        const struct mylite_sql_ast_node *arguments,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value)
{
    size_t value_count = child_count(arguments);
    struct greatest_least_argument *values = NULL;
    struct greatest_least_eval_state state = {0};
    int status = 0;

    if (value_count < 2U) {
        return -1;
    }
    values = calloc(value_count, sizeof(values[0]));
    if (values == NULL) {
        return -1;
    }

    status = eval_greatest_least_arguments(arguments, context, warnings, values, &state);
    if (status == 0 && state.null_result) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        status = set_greatest_least_result(function_id, values, value_count, state.string_domain,
                                           warnings, out_value);
    }

    greatest_least_arguments_deinit(values, value_count);
    free(values);
    return status;
}

static int eval_greatest_least_arguments(const struct mylite_sql_ast_node *arguments,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct greatest_least_argument *values,
                                         struct greatest_least_eval_state *out_state)
{
    size_t index = 0U;

    *out_state = (struct greatest_least_eval_state){0};
    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL; argument = argument->next_sibling, ++index) {
        int status = eval_node(argument, context, warnings, &values[index].value);

        if (status != 0) {
            return status;
        }
        if (is_null(&values[index].value)) {
            out_state->null_result = true;
            return 0;
        }
        if (values[index].value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
            out_state->string_domain = true;
        }
    }
    return 0;
}

static int set_greatest_least_result(enum mylite_scalar_function_id function_id,
                                     struct greatest_least_argument *values, size_t value_count,
                                     bool string_domain,
                                     struct mylite_expression_warnings *warnings,
                                     struct mylite_expression_value *out_value)
{
    if (string_domain) {
        return set_greatest_least_string_result(function_id, values, value_count, out_value);
    }
    return set_greatest_least_numeric_result(function_id, values, value_count, warnings, out_value);
}

static int set_greatest_least_string_result(enum mylite_scalar_function_id function_id,
                                            struct greatest_least_argument *values,
                                            size_t value_count,
                                            struct mylite_expression_value *out_value)
{
    size_t selected = 0U;

    for (size_t index = 0U; index < value_count; ++index) {
        int status = greatest_least_argument_to_text(&values[index]);

        if (status != 0) {
            return status;
        }
    }

    for (size_t index = 1U; index < value_count; ++index) {
        int comparison = compare_greatest_least_text(&values[selected], &values[index]);

        if (greatest_least_candidate_replaces_selected(function_id, comparison)) {
            selected = index;
        }
    }

    int status = set_text_value(values[selected].text, values[selected].text_length, out_value);

    if (status == 0) {
        out_value->suppress_text_numeric_warnings = true;
    }
    return status;
}

static int set_greatest_least_numeric_result(enum mylite_scalar_function_id function_id,
                                             struct greatest_least_argument *values,
                                             size_t value_count,
                                             struct mylite_expression_warnings *warnings,
                                             struct mylite_expression_value *out_value)
{
    size_t selected = 0U;

    for (size_t index = 1U; index < value_count; ++index) {
        int comparison = 0;
        int status = compare_greatest_least_numeric_values(
            &values[selected].value, &values[index].value, warnings, &comparison);

        if (status != 0) {
            return status;
        }
        if (greatest_least_candidate_replaces_selected(function_id, comparison)) {
            selected = index;
        }
    }

    return mylite_expression_value_copy(&values[selected].value, out_value);
}

static int greatest_least_argument_to_text(struct greatest_least_argument *argument)
{
    if (argument->value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        const char *text = argument->value.text_value == NULL ? "" : argument->value.text_value;
        size_t length = argument->value.text_value == NULL ? 0U : argument->value.text_length;

        argument->text = copy_span_text(text, length);
        argument->text_length = length;
        return argument->text == NULL
                   ? -1
                   : greatest_least_argument_prepare_compare_text(argument, length);
    }

    int status = cast_value_to_string(&argument->value, &argument->text);

    if (status == 0) {
        size_t length = strlen(argument->text);

        argument->text_length = length;
        status = greatest_least_argument_prepare_compare_text(argument, length);
    }
    return status;
}

static int greatest_least_argument_prepare_compare_text(struct greatest_least_argument *argument,
                                                        size_t length)
{
    const char *text = argument->text == NULL ? "" : argument->text;

    while (length > 0U && text[length - 1U] == ' ') {
        --length;
    }

    argument->compare_text = malloc(length + 1U);
    if (argument->compare_text == NULL) {
        return -1;
    }
    if (length != 0U) {
        memcpy(argument->compare_text, text, length);
    }
    argument->compare_text[length] = '\0';
    for (size_t index = 0U; index < length; ++index) {
        argument->compare_text[index] =
            (char)ascii_case_fold((unsigned char)argument->compare_text[index]);
    }
    return 0;
}

static int compare_greatest_least_text(const struct greatest_least_argument *left,
                                       const struct greatest_least_argument *right)
{
    int comparison = strcmp(left->compare_text == NULL ? "" : left->compare_text,
                            right->compare_text == NULL ? "" : right->compare_text);

    return (comparison > 0) - (comparison < 0);
}

static int compare_greatest_least_numeric_values(const struct mylite_expression_value *left,
                                                 const struct mylite_expression_value *right,
                                                 struct mylite_expression_warnings *warnings,
                                                 int *out_compare)
{
    if (left->kind == MYLITE_EXPRESSION_VALUE_INT64 &&
        right->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_compare =
            (left->int64_value > right->int64_value) - (left->int64_value < right->int64_value);
        return 0;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_UINT64 &&
        right->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_compare =
            (left->uint64_value > right->uint64_value) - (left->uint64_value < right->uint64_value);
        return 0;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_INT64 &&
        right->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_compare = left->int64_value < 0
                           ? -1
                           : ((uint64_t)left->int64_value > right->uint64_value) -
                                 ((uint64_t)left->int64_value < right->uint64_value);
        return 0;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_UINT64 &&
        right->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_compare = right->int64_value < 0
                           ? 1
                           : (left->uint64_value > (uint64_t)right->int64_value) -
                                 (left->uint64_value < (uint64_t)right->int64_value);
        return 0;
    }
    return compare_values(left, right, warnings, out_compare);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static bool greatest_least_candidate_replaces_selected(enum mylite_scalar_function_id function_id,
                                                       int selected_vs_candidate)
{
    if (function_id == MYLITE_SCALAR_FUNCTION_GREATEST) {
        return selected_vs_candidate <= 0;
    }
    return selected_vs_candidate > 0;
}

static void greatest_least_arguments_deinit(struct greatest_least_argument *values,
                                            size_t value_count)
{
    for (size_t index = 0U; index < value_count; ++index) {
        mylite_expression_value_deinit(&values[index].value);
        free(values[index].text);
        free(values[index].compare_text);
    }
}

static int eval_isnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status == 0) {
        int null_result = is_null(&value) ? 1 : 0;

        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = null_result};
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value)
{
    char *text = NULL;
    char *end = NULL;
    errno = 0;

    switch (node->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        if (text[0] != '-' && strlen(text) >= MYLITE_EXPRESSION_UINT64_DIGITS) {
            unsigned long long unsigned_value =
                strtoull(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
            if (errno == 0 && end != text && *end == '\0' &&
                unsigned_value > (unsigned long long)INT64_MAX) {
                out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
                out_value->uint64_value = (uint64_t)unsigned_value;
                free(text);
                return 0;
            }
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = strtoll(text, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
        out_value->real_value = strtod(text, NULL);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = decode_string_literal(node);
        out_value->text_length = out_value->text_value == NULL ? 0U : strlen(out_value->text_value);
        return out_value->text_value == NULL ? -1 : 0;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return -1;
    }
    return -1;
}

static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int truth = -1;
    int result = 0;
    int status = eval_node(operand, context, warnings, &value);

    if (status != 0) {
        return status;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        result = value.kind == MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        result = value.kind != MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        status = truth_value(&value, warnings, &truth);
        if (status != 0) {
            break;
        }
        result = operator_kind == MYLITE_SQL_AST_OPERATOR_IS_TRUE ||
                         operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE
                     ? truth == 1
                     : truth == 0;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE) {
            result = !result;
        }
        break;
    default:
        status = -1;
        break;
    }

    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value low = {0};
    struct mylite_expression_value high = {0};
    struct between_truth truth = {.low = -1, .high = -1};
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &low);
    }
    if (status == 0) {
        status = eval_node(child_at(node, 2U), context, warnings, &high);
    }
    if (status != 0) {
        goto cleanup;
    }
    status = eval_between_bound_truth(&value, &low, true, warnings, &truth.low);
    if (status == 0) {
        status = eval_between_bound_truth(&value, &high, false, warnings, &truth.high);
    }
    if (status == 0) {
        set_between_result(operator_kind, truth, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&low);
    mylite_expression_value_deinit(&high);
    return status;
}

static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth)
{
    int comparison = 0;
    int status = 0;

    *out_truth = -1;
    if (is_null(value) || is_null(bound)) {
        return 0;
    }

    status = compare_values(value, bound, warnings, &comparison);
    if (status != 0) {
        return status;
    }
    *out_truth = lower_bound ? comparison >= 0 : comparison <= 0;
    return 0;
}

static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value)
{
    bool between = operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN;

    if (truth.low == 0 || truth.high == 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 0 : 1};
    } else if (truth.low < 0 || truth.high < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 1 : 0};
    }
}

static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value pattern = {0};
    struct mylite_expression_value escape_value = {0};
    char *value_text = NULL;
    char *pattern_text = NULL;
    char *escape_text = NULL;
    char escape = '\\';
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &pattern);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = eval_node(child_at(node, 2U), context, warnings, &escape_value);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value) || is_null(&pattern) ||
        (child_at(node, 2U) != NULL && is_null(&escape_value))) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&value, &value_text);
    if (status == 0) {
        status = value_to_string(&pattern, &pattern_text);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = value_to_string(&escape_value, &escape_text);
        if (status == 0 && strlen(escape_text) != 1U) {
            status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                    "Incorrect arguments to ESCAPE");
            if (status == 0) {
                status = -1;
            }
        }
        if (status == 0) {
            escape = escape_text[0];
        }
    }
    if (status == 0) {
        bool result = like_match(value_text, pattern_text, escape);
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
            result = !result;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = result ? 1 : 0};
    }

cleanup:
    free(value_text);
    free(pattern_text);
    free(escape_text);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&pattern);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    const struct mylite_sql_ast_node *list = child_at(node, 1U);
    bool saw_null = false;
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (list != NULL && list->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        status =
            context == NULL || context->eval_in_subquery == NULL
                ? -1
                : context->eval_in_subquery(context->user_data, node, &value, warnings, out_value);
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    for (const struct mylite_sql_ast_node *item = list == NULL ? NULL : list->first_child;
         item != NULL; item = item->next_sibling) {
        struct mylite_expression_value candidate = {0};
        int comparison = 0;

        status = eval_node(item, context, warnings, &candidate);
        if (status != 0) {
            mylite_expression_value_deinit(&candidate);
            break;
        }
        if (is_null(&candidate)) {
            saw_null = true;
            mylite_expression_value_deinit(&candidate);
            continue;
        }
        status = compare_values(&value, &candidate, warnings, &comparison);
        mylite_expression_value_deinit(&candidate);
        if (status != 0) {
            break;
        }
        if (comparison == 0) {
            bool result = operator_kind == MYLITE_SQL_AST_OPERATOR_IN;
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            mylite_expression_value_deinit(&value);
            return 0;
        }
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    if (saw_null) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
    return 0;
}

static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));
    const struct mylite_sql_ast_node *right = unwrap_parenthesized_node(child_at(node, 1U));

    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION || left == NULL ||
        left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL) {
        return false;
    }
    if ((node->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) &&
        right->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        return true;
    }
    return binary_expression_is_row_scalar_subquery(node);
}

static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));
    const struct mylite_sql_ast_node *right = unwrap_parenthesized_node(child_at(node, 1U));

    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION || left == NULL ||
        left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION) {
        return false;
    }
    return row_subquery_comparison_operator_is_supported(node->operator_kind);
}

static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

static int eval_quantified_comparison(const struct mylite_sql_ast_node *node,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = 0;

    if (quantified_comparison_has_row_left(node)) {
        return context == NULL || context->eval_row_subquery == NULL
                   ? -1
                   : context->eval_row_subquery(context->user_data, node, context, warnings,
                                                out_value);
    }

    status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    status = context == NULL || context->eval_quantified_subquery == NULL
                 ? -1
                 : context->eval_quantified_subquery(context->user_data, node, &value, warnings,
                                                     out_value);
    mylite_expression_value_deinit(&value);
    return status;
}

static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));

    return node != NULL && node->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON && left != NULL &&
           left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR;
}

static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct numeric_value number = {0};
    int truth = -1;
    int status = 0;

    if (is_null(operand)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        status = truth_value(operand, warnings, &truth);
        if (status != 0) {
            return status;
        }
        if (truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = truth == 0 ? 1 : 0};
        }
        return 0;
    }

    status = value_to_numeric(operand, warnings, &number);
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_BITWISE_NOT) {
        uint64_t value = number.is_unsigned ? number.uint64_value : (uint64_t)number.int64_value;
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = ~value};
        return 0;
    }
    if (number.is_integer && !number.is_unsigned) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.int64_value
                                                                             : number.int64_value};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.real_value
                                                                        : number.real_value};
    return 0;
}

static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    if ((operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO) &&
        right_number.real_value == 0.0) {
        status = append_warning(warnings, MYLITE_WARNING_DIVISION_BY_ZERO, "Division by 0");
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_REAL,
                                                      .real_value = left_number.real_value /
                                                                    right_number.real_value};
        return 0;
    }

    if (left_number.is_integer && right_number.is_integer) {
        int64_t left_int =
            left_number.is_unsigned ? (int64_t)left_number.uint64_value : left_number.int64_value;
        int64_t right_int = right_number.is_unsigned ? (int64_t)right_number.uint64_value
                                                     : right_number.int64_value;
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        switch (operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->int64_value = left_int + right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->int64_value = left_int - right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->int64_value = left_int * right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
            out_value->int64_value = left_int / right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MODULO:
            out_value->int64_value = left_int % right_int;
            return 0;
        default:
            break;
        }
    }

    out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        out_value->real_value = left_number.real_value + right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        out_value->real_value = left_number.real_value - right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        out_value->real_value = left_number.real_value * right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = (int64_t)(left_number.real_value / right_number.real_value);
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        out_value->real_value =
            left_number.real_value -
            ((double)((int64_t)(left_number.real_value / right_number.real_value)) *
             right_number.real_value);
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    uint64_t left_int = 0U;
    uint64_t right_int = 0U;
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    left_int =
        left_number.is_unsigned ? left_number.uint64_value : (uint64_t)left_number.int64_value;
    right_int =
        right_number.is_unsigned ? right_number.uint64_value : (uint64_t)right_number.int64_value;
    out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        out_value->uint64_value = left_int & right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        out_value->uint64_value = left_int ^ right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        out_value->uint64_value = left_int | right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int << right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int >> right_int;
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    int comparison = 0;
    bool result = false;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        if (is_null(left) || is_null(right)) {
            result = is_null(left) && is_null(right);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            return 0;
        }
    } else if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (compare_values(left, right, warnings, &comparison) != 0) {
        return -1;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        result = comparison == 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        result = comparison != 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        result = comparison < 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        result = comparison <= 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        result = comparison > 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        result = comparison >= 0;
        break;
    default:
        return -1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    int left_truth = -1;
    int right_truth = -1;
    int status = truth_value(left, warnings, &left_truth);

    if (status == 0) {
        status = truth_value(right, warnings, &right_truth);
    }
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        if (left_truth == 0 || right_truth == 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        }
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        if (left_truth == 1 || right_truth == 1) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        }
        return 0;
    }
    if (left_truth < 0 || right_truth < 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
    } else {
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = left_truth != right_truth ? 1 : 0;
    }
    return 0;
}

static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth)
{
    struct numeric_value number = {0};
    int status = 0;

    if (is_null(value)) {
        *out_truth = -1;
        return 0;
    }
    status = value_to_numeric(value, warnings, &number);
    if (status != 0) {
        return status;
    }
    *out_truth = number.real_value == 0.0 ? 0 : 1;
    return 0;
}

static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare)
{
    if (is_numeric_kind(left->kind) || is_numeric_kind(right->kind)) {
        struct numeric_value left_number = {0};
        struct numeric_value right_number = {0};
        int status = value_to_numeric(left, warnings, &left_number);

        if (status == 0) {
            status = value_to_numeric(right, warnings, &right_number);
        }
        if (status != 0) {
            return status;
        }
        *out_compare = (left_number.real_value > right_number.real_value) -
                       (left_number.real_value < right_number.real_value);
        return 0;
    }

    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int status = value_to_string_with_length(left, &left_text, &left_length);

    if (status == 0) {
        status = value_to_string_with_length(right, &right_text, &right_length);
    }
    if (status == 0) {
        size_t compare_length = left_length < right_length ? left_length : right_length;
        int comparison = compare_length == 0U ? 0 : memcmp(left_text, right_text, compare_length);

        if (comparison == 0) {
            comparison = (left_length > right_length) - (left_length < right_length);
        }
        *out_compare = (comparison > 0) - (comparison < 0);
    }
    free(left_text);
    free(right_text);
    return status;
}

static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric)
{
    *out_numeric = (struct numeric_value){0};
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        out_numeric->int64_value = value->int64_value;
        out_numeric->uint64_value = (uint64_t)value->int64_value;
        out_numeric->real_value = (double)value->int64_value;
        out_numeric->is_integer = true;
        return 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        out_numeric->uint64_value = value->uint64_value;
        out_numeric->int64_value = (int64_t)value->uint64_value;
        out_numeric->real_value = (double)value->uint64_value;
        out_numeric->is_integer = true;
        out_numeric->is_unsigned = true;
        return 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        out_numeric->real_value = value->real_value;
        out_numeric->int64_value = numeric_real_to_truncated_int64(value->real_value);
        return 0;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return text_value_to_numeric(value, warnings, out_numeric);
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }
    return -1;
}

static int text_value_to_numeric(const struct mylite_expression_value *value,
                                 struct mylite_expression_warnings *warnings,
                                 struct numeric_value *out_numeric)
{
    if (value->suppress_text_numeric_warnings) {
        return text_value_to_numeric_without_warnings(value, out_numeric);
    }

    char *text = value->text_value == NULL
                     ? copy_span_text("", 0U)
                     : copy_span_text(value->text_value, strlen(value->text_value));
    char *start = text;
    int status = 0;

    if (text == NULL) {
        return -1;
    }
    while (isspace((unsigned char)*start)) {
        ++start;
    }

    if (!numeric_text_has_digit(start)) {
        status = append_numeric_text_without_digits_warning(
            warnings, (struct numeric_text_input){.start = start, .text = text});
    } else if (numeric_text_is_hex_like(start)) {
        status = append_truncation_warning(warnings, text);
    } else {
        status = parse_numeric_text_double(
            (struct numeric_text_parse_input){.text = text, .start = start}, warnings, out_numeric);
    }
    free(text);
    return status;
}

static int text_value_to_numeric_without_warnings(const struct mylite_expression_value *value,
                                                  struct numeric_value *out_numeric)
{
    char *text = value->text_value == NULL
                     ? copy_span_text("", 0U)
                     : copy_span_text(value->text_value, strlen(value->text_value));
    char *start = text;
    char *end = NULL;
    bool overflow = false;

    if (text == NULL) {
        return -1;
    }
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (!numeric_text_has_digit(start) || numeric_text_is_hex_like(start)) {
        out_numeric->is_integer = true;
        free(text);
        return 0;
    }

    errno = 0;
    out_numeric->real_value = strtod(start, &end);
    overflow = errno == ERANGE && isinf(out_numeric->real_value);
    if (overflow) {
        clamp_numeric_text_range(out_numeric);
    } else {
        out_numeric->int64_value = numeric_real_to_truncated_int64(out_numeric->real_value);
    }
    out_numeric->uint64_value = (uint64_t)out_numeric->int64_value;
    out_numeric->is_integer = !overflow && numeric_text_prefix_is_integer(start, end);
    free(text);
    return 0;
}

static bool numeric_text_prefix_is_integer(const char *start, const char *end)
{
    const char *scan = start;

    if (scan == NULL || end == NULL || end <= scan) {
        return true;
    }
    if (*scan == '+' || *scan == '-') {
        ++scan;
    }
    for (; scan < end; ++scan) {
        if (*scan == '.' || *scan == 'e' || *scan == 'E') {
            return false;
        }
    }
    return true;
}

static bool numeric_text_has_digit(const char *start)
{
    const char *scan = start;

    if (*scan == '+' || *scan == '-') {
        ++scan;
    }
    for (; *scan != '\0'; ++scan) {
        if (isdigit((unsigned char)*scan)) {
            return true;
        }
        if (*scan != '.') {
            break;
        }
    }
    return false;
}

static bool numeric_text_is_hex_like(const char *start)
{
    const char *number_start = start;

    if (*number_start == '+' || *number_start == '-') {
        ++number_start;
    }
    return number_start[0] == '0' && (number_start[1] == 'x' || number_start[1] == 'X');
}

static int parse_numeric_text_double(struct numeric_text_parse_input input,
                                     struct mylite_expression_warnings *warnings,
                                     struct numeric_value *out_numeric)
{
    char *end = NULL;
    bool overflow = false;

    errno = 0;
    out_numeric->real_value = strtod(input.start, &end);
    overflow = errno == ERANGE && isinf(out_numeric->real_value);
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (overflow) {
        clamp_numeric_text_range(out_numeric);
    } else {
        out_numeric->int64_value = numeric_real_to_truncated_int64(out_numeric->real_value);
    }
    if (end == input.start || (end != NULL && *end != '\0') || overflow) {
        return append_truncation_warning(warnings, input.text);
    }
    return 0;
}

static void clamp_numeric_text_range(struct numeric_value *numeric)
{
    numeric->real_value = signbit(numeric->real_value) ? -DBL_MAX : DBL_MAX;
    numeric->int64_value = numeric_real_to_truncated_int64(numeric->real_value);
}

static int64_t numeric_real_to_truncated_int64(double value)
{
    if (isnan(value)) {
        return 0;
    }
    if (value >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (value <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    return (int64_t)value;
}

static int append_numeric_text_without_digits_warning(struct mylite_expression_warnings *warnings,
                                                      struct numeric_text_input input)
{
    if (*input.start == '\0') {
        return 0;
    }
    return append_truncation_warning(warnings, input.text);
}

static int cast_value_to_signed_integer(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_integer)
{
    if (out_integer == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_integer = value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_integer = signed_integer_from_uint64(value->uint64_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_integer = cast_real_to_signed_integer(value->real_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_signed_integer(value->text_value == NULL ? "" : value->text_value,
                                             warnings, out_integer);
    }
    return -1;
}

static int cast_value_to_unsigned_integer(const struct mylite_expression_value *value,
                                          struct mylite_expression_warnings *warnings,
                                          uint64_t *out_integer)
{
    if (out_integer == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_integer = (uint64_t)value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_integer = value->uint64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_integer = (uint64_t)cast_real_to_signed_integer(value->real_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_unsigned_integer(value->text_value == NULL ? "" : value->text_value,
                                               warnings, out_integer);
    }
    return -1;
}

static int cast_string_to_signed_integer(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         int64_t *out_integer)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_integer = 0;
        return 0;
    }
    if (parsed.negative) {
        *out_integer = parsed.magnitude == mylite_expression_int64_min_magnitude
                           ? INT64_MIN
                           : -(int64_t)parsed.magnitude;
        return 0;
    }
    *out_integer = signed_integer_from_uint64(parsed.magnitude);
    if (!parsed.overflow && parsed.magnitude > (uint64_t)INT64_MAX) {
        return append_signed_complement_warning(warnings);
    }
    return 0;
}

static int cast_string_to_unsigned_integer(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_integer)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_integer = 0;
        return 0;
    }
    if (parsed.negative) {
        *out_integer = unsigned_complement_from_magnitude(parsed.magnitude);
        if (!parsed.overflow && parsed.magnitude != 0U) {
            return append_unsigned_complement_warning(warnings);
        }
        return 0;
    }
    *out_integer = parsed.magnitude;
    return 0;
}

static struct cast_integer_parse parse_cast_integer_text(const char *text)
{
    const char *start = text == NULL ? "" : text;
    const char *scan = NULL;
    const uint64_t radix = (uint64_t)MYLITE_EXPRESSION_DECIMAL_BASE;
    struct cast_integer_parse parsed = {0};

    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (*start == '+' || *start == '-') {
        parsed.negative = *start == '-';
        ++start;
    }
    scan = start;
    while (isdigit((unsigned char)*scan)) {
        uint64_t digit = (uint64_t)(*scan - '0');
        uint64_t limit = parsed.negative ? mylite_expression_int64_min_magnitude : UINT64_MAX;

        parsed.saw_digit = true;
        if (parsed.magnitude > (limit - digit) / radix) {
            parsed.magnitude = limit;
            parsed.overflow = true;
        } else if (!parsed.overflow) {
            parsed.magnitude = (parsed.magnitude * radix) + digit;
        }
        ++scan;
    }
    while (isspace((unsigned char)*scan)) {
        ++scan;
    }
    parsed.trailing_garbage = *scan != '\0';
    return parsed;
}

static int64_t signed_integer_from_uint64(uint64_t value)
{
    if (value <= (uint64_t)INT64_MAX) {
        return (int64_t)value;
    }
    return INT64_MIN + (int64_t)(value - mylite_expression_int64_min_magnitude);
}

static uint64_t unsigned_complement_from_magnitude(uint64_t magnitude)
{
    if (magnitude == 0U) {
        return 0U;
    }
    return (UINT64_MAX - magnitude) + 1U;
}

static int cast_value_to_decimal_double(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        double *out_number)
{
    if (out_number == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_number = (double)value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_number = (double)value->uint64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_number = value->real_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_decimal_double(value->text_value == NULL ? "" : value->text_value,
                                             warnings, out_number);
    }
    return -1;
}

static int cast_string_to_decimal_double(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_number)
{
    char *copy = copy_span_text(text == NULL ? "" : text, strlen(text == NULL ? "" : text));
    char *start = NULL;
    char *end = NULL;

    if (copy == NULL) {
        return -1;
    }
    start = copy;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    *out_number = strtod(start, &end);
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        int status = append_cast_truncation_warning(warnings, "DECIMAL", text);

        if (end == start) {
            *out_number = 0.0;
        }
        free(copy);
        return status;
    }
    free(copy);
    return 0;
}

static int cast_value_to_string(const struct mylite_expression_value *value, char **out_text)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    int length = 0;

    if (out_text == NULL || value == NULL) {
        return -1;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        *out_text = length <= 0 || (size_t)length >= sizeof(buffer)
                        ? NULL
                        : copy_span_text(buffer, (size_t)length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        *out_text = length <= 0 || (size_t)length >= sizeof(buffer)
                        ? NULL
                        : copy_span_text(buffer, (size_t)length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return cast_real_to_string(value->real_value, out_text);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_text = copy_span_text(value->text_value == NULL ? "" : value->text_value,
                                   value->text_value == NULL ? 0U : value->text_length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }
    return -1;
}

static int cast_real_to_string(double value, char **out_text)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE] = {0};
    int length = snprintf(buffer, sizeof(buffer), "%.15g", value);

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_text = copy_span_text(buffer, (size_t)length);
    return *out_text == NULL ? -1 : 0;
}

static int64_t cast_real_to_signed_integer(double value)
{
    double rounded = 0.0;

    if (value >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (value <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    if (value >= 0.0) {
        rounded = value + mylite_expression_round_half;
    } else {
        rounded = value - mylite_expression_round_half;
    }
    if (rounded >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (rounded <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    return (int64_t)rounded;
}

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_scale) {
        return (unsigned int)target->column_scale;
    }
    return 0U;
}

static double absolute_real_value(double value)
{
    return value < 0.0 ? -value : value;
}

static int64_t floor_real_value(double value)
{
    int64_t truncated = (int64_t)value;

    return (double)truncated > value ? truncated - 1 : truncated;
}

static int64_t ceil_real_value(double value)
{
    int64_t truncated = (int64_t)value;

    return (double)truncated < value ? truncated + 1 : truncated;
}

static int value_to_string(const struct mylite_expression_value *value, char **out_text)
{
    size_t length = 0U;

    return value_to_string_with_length(value, out_text, &length);
}

static int value_to_string_with_length(const struct mylite_expression_value *value, char **out_text,
                                       size_t *out_length)
{
    if (out_text == NULL || out_length == NULL) {
        return -1;
    }
    *out_text = mylite_expression_value_to_text(value);
    if (*out_text == NULL && !is_null(value)) {
        return -1;
    }
    *out_length = 0U;
    if (*out_text != NULL && !is_null(value)) {
        *out_length = strlen(*out_text);
    }
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT && *out_text != NULL) {
        *out_length = value->text_length;
    }
    return 0;
}

static int format_compact_real_text(double value, char *buffer, size_t buffer_size)
{
    enum {
        min_double_precision = 15,
        max_double_precision = 17,
    };

    if (value == DBL_TRUE_MIN || value == -DBL_TRUE_MIN) {
        int length = snprintf(buffer, buffer_size, "%s5e-324", value < 0.0 ? "-" : "");

        return (length <= 0 || (size_t)length >= buffer_size) ? length : (int)strlen(buffer);
    }

    for (int precision = min_double_precision; precision <= max_double_precision; ++precision) {
        int length = snprintf(buffer, buffer_size, "%.*g", precision, value);

        if (length <= 0 || (size_t)length >= buffer_size) {
            return length;
        }
        normalize_real_exponent_text(buffer);
        if (compact_real_text_round_trips(value, buffer)) {
            return (int)strlen(buffer);
        }
    }

    int length = snprintf(buffer, buffer_size, "%.17g", value);
    if (length <= 0 || (size_t)length >= buffer_size) {
        return length;
    }
    normalize_real_exponent_text(buffer);
    return (int)strlen(buffer);
}

static bool compact_real_text_round_trips(double value, const char *text)
{
    char *end = NULL;
    double parsed = strtod(text, &end);

    return end != text && *end == '\0' && parsed == value &&
           (value != 0.0 || signbit(parsed) == signbit(value));
}

static void normalize_real_exponent_text(char *text)
{
    char *exponent = strchr(text, 'e');
    char *read = NULL;
    char *write = NULL;

    if (exponent == NULL) {
        return;
    }

    write = exponent + 1;
    read = write;
    if (*read == '+') {
        ++read;
    } else if (*read == '-') {
        *write++ = *read++;
    }
    while (read[0] == '0' && isdigit((unsigned char)read[1])) {
        ++read;
    }
    memmove(write, read, strlen(read) + 1U);
}

static int set_text_value(const char *text, size_t length,
                          struct mylite_expression_value *out_value)
{
    out_value->text_value = copy_span_text(text, length);
    if (out_value->text_value == NULL) {
        return -1;
    }
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->suppress_text_numeric_warnings = false;
    out_value->preserve_temporal_fraction_digits = false;
    out_value->text_length = length;
    return 0;
}

static int append_text(char **text, size_t *length, const char *addition, size_t addition_length)
{
    char *updated = NULL;

    if (*length >= (size_t)PTRDIFF_MAX || addition_length > (size_t)PTRDIFF_MAX - *length - 1U) {
        return -1;
    }

    updated = realloc(*text, *length + addition_length + 1U);
    if (updated == NULL) {
        return -1;
    }
    if (addition_length != 0U) {
        memcpy(updated + *length, addition, addition_length);
    }
    *length += addition_length;
    updated[*length] = '\0';
    *text = updated;
    return 0;
}

static bool ascii_text_equal_ci(struct text_compare_input input)
{
    if (input.left == NULL || input.right == NULL) {
        return false;
    }
    if (input.left_length != input.right_length) {
        return false;
    }
    if (strlen(input.left) < input.left_length) {
        return false;
    }
    for (size_t index = 0U; index < input.left_length; ++index) {
        if (ascii_case_fold((unsigned char)input.left[index]) !=
            ascii_case_fold((unsigned char)input.right[index])) {
            return false;
        }
    }
    return true;
}

static int utf8_char_count(const char *text, int64_t *out_count)
{
    int64_t count = 0;

    if (text == NULL) {
        *out_count = 0;
        return 0;
    }

    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        if ((*cursor & MYLITE_UTF8_CONTINUATION_MASK) != MYLITE_UTF8_CONTINUATION_MARKER) {
            ++count;
        }
    }
    *out_count = count;
    return 0;
}

static size_t utf8_offset_for_chars(const char *text, int64_t char_count)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int64_t count = 0;

    if (text == NULL || char_count <= 0) {
        return 0U;
    }
    while (*cursor != '\0' && count < char_count) {
        ++cursor;
        while (*cursor != '\0' &&
               (*cursor & MYLITE_UTF8_CONTINUATION_MASK) == MYLITE_UTF8_CONTINUATION_MARKER) {
            ++cursor;
        }
        ++count;
    }
    return (size_t)((const char *)cursor - text);
}

static size_t utf8_first_character_length(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)(text == NULL ? "" : text);
    size_t length = 0U;

    if (*cursor == '\0') {
        return 0U;
    }
    ++cursor;
    length = 1U;
    while (*cursor != '\0' &&
           (*cursor & MYLITE_UTF8_CONTINUATION_MASK) == MYLITE_UTF8_CONTINUATION_MARKER) {
        ++cursor;
        ++length;
    }
    return length;
}

static int64_t find_text_match_position(struct locate_search search)
{
    const char *source = search.text == NULL ? "" : search.text;
    const char *target = search.needle == NULL ? "" : search.needle;
    size_t source_length = strlen(source);
    size_t target_length = strlen(target);
    size_t offset = search.start_offset;
    int64_t position = search.start_position;

    if (target_length == 0U) {
        return search.start_offset <= source_length ? position : 0;
    }
    if (search.start_offset > source_length ||
        target_length > source_length - search.start_offset) {
        return 0;
    }

    while (offset + target_length <= source_length) {
        if (memcmp(source + offset, target, target_length) == 0) {
            return position;
        }
        ++position;
        offset = utf8_offset_for_chars(source, position - 1);
    }
    return 0;
}

static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message)
{
    return mylite_expression_warnings_append(warnings, code, message);
}

static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Truncated incorrect DOUBLE value: '%.*s'",
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_cast_truncation_warning(struct mylite_expression_warnings *warnings,
                                          const char *type_name, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Truncated incorrect %s value: '%.*s'",
                          type_name == NULL ? "" : type_name,
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_power_out_of_range_error(struct mylite_expression_warnings *warnings)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_OUT_OF_RANGE,
        "DOUBLE value is out of range in 'pow()'");
}

static int append_exp_out_of_range_error(struct mylite_expression_warnings *warnings)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_OUT_OF_RANGE,
        "DOUBLE value is out of range in 'exp()'");
}

static int append_cot_out_of_range_error(struct mylite_expression_warnings *warnings)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_OUT_OF_RANGE,
        "DOUBLE value is out of range in 'cot()'");
}

static int append_angle_conversion_out_of_range_error(struct mylite_expression_warnings *warnings,
                                                      const char *function_name)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "DOUBLE value is out of range in '%s()'",
                          function_name == NULL ? "" : function_name);

    if (length < 0) {
        return -1;
    }
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_ERROR, MYLITE_WARNING_OUT_OF_RANGE, message);
}

static int append_invalid_logarithm_warning(struct mylite_expression_warnings *warnings)
{
    return append_warning(warnings, MYLITE_WARNING_INVALID_ARGUMENT_FOR_LOGARITHM,
                          "Invalid argument for logarithm");
}

static int append_char_truncation_warning(struct mylite_expression_warnings *warnings,
                                          uint64_t length, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int written = snprintf(message, sizeof(message), "Truncated incorrect CHAR(%llu) value: '%.*s'",
                           (unsigned long long)length, MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW,
                           text == NULL ? "" : text);

    if (written < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_signed_complement_warning(struct mylite_expression_warnings *warnings)
{
    return append_warning(warnings, MYLITE_WARNING_UNKNOWN,
                          "Cast to signed converted positive out-of-range "
                          "integer to its negative "
                          "complement");
}

static int append_unsigned_complement_warning(struct mylite_expression_warnings *warnings)
{
    return append_warning(warnings, MYLITE_WARNING_UNKNOWN,
                          "Cast to unsigned converted negative integer to "
                          "its positive "
                          "complement");
}

static char *copy_span_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    if (length != 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *decode_string_literal(const struct mylite_sql_ast_node *node)
{
    const char *text = node->span.text;
    size_t length = node->span.length;
    size_t start = 0U;
    size_t end = length;
    char *decoded = NULL;
    size_t output = 0U;

    if (length >= 2U && (text[0] == '\'' || text[0] == '"')) {
        start = 1U;
        end = length - 1U;
    } else if (length >= 3U && (text[0] == 'N' || text[0] == 'n') &&
               (text[1] == '\'' || text[1] == '"')) {
        start = 2U;
        end = length - 1U;
    }

    decoded = malloc(end >= start ? end - start + 1U : 1U);
    if (decoded == NULL) {
        return NULL;
    }
    for (size_t index = start; index < end; ++index) {
        if (text[index] == '\\' && index + 1U < end) {
            char escaped = '\0';

            if (decode_string_escape(text[index + 1U], &escaped)) {
                decoded[output++] = escaped;
                ++index;
            } else {
                decoded[output++] = text[index];
            }
        } else if ((text[index] == '\'' || text[index] == '"') && index + 1U < end &&
                   text[index + 1U] == text[index]) {
            decoded[output++] = text[index++];
        } else {
            decoded[output++] = text[index];
        }
    }
    decoded[output] = '\0';
    return decoded;
}

static bool decode_string_escape(char escaped, char *out_character)
{
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case 'Z':
        *out_character = '\x1A';
        return true;
    default:
        return false;
    }
}

static const struct mylite_sql_ast_node *
unwrap_parenthesized_node(const struct mylite_sql_ast_node *node)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    return node;
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
    const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;

    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static size_t child_count(const struct mylite_sql_ast_node *node)
{
    size_t count = 0U;

    for (const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;
         child != NULL; child = child->next_sibling) {
        ++count;
    }
    return count;
}

static enum mylite_scalar_function_id scalar_function_id(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *name = child_at(node, 0U);

    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_SCALAR_FUNCTION_UNKNOWN;
    }
    return scalar_function_id_from_span(name->span);
}

static enum mylite_scalar_function_id
scalar_function_id_from_span(struct mylite_sql_source_span span)
{
    static const struct {
        const char *name;
        enum mylite_scalar_function_id id;
    } functions[] = {
        {"CONCAT", MYLITE_SCALAR_FUNCTION_CONCAT},
        {"CONCAT_WS", MYLITE_SCALAR_FUNCTION_CONCAT_WS},
        {"LENGTH", MYLITE_SCALAR_FUNCTION_LENGTH},
        {"OCTET_LENGTH", MYLITE_SCALAR_FUNCTION_LENGTH},
        {"CHAR_LENGTH", MYLITE_SCALAR_FUNCTION_CHAR_LENGTH},
        {"CHARACTER_LENGTH", MYLITE_SCALAR_FUNCTION_CHAR_LENGTH},
        {"ASCII", MYLITE_SCALAR_FUNCTION_ASCII},
        {"ORD", MYLITE_SCALAR_FUNCTION_ORD},
        {"LOWER", MYLITE_SCALAR_FUNCTION_LOWER},
        {"LCASE", MYLITE_SCALAR_FUNCTION_LOWER},
        {"UPPER", MYLITE_SCALAR_FUNCTION_UPPER},
        {"UCASE", MYLITE_SCALAR_FUNCTION_UPPER},
        {"LEFT", MYLITE_SCALAR_FUNCTION_LEFT},
        {"RIGHT", MYLITE_SCALAR_FUNCTION_RIGHT},
        {"SUBSTRING", MYLITE_SCALAR_FUNCTION_SUBSTRING},
        {"SUBSTR", MYLITE_SCALAR_FUNCTION_SUBSTRING},
        {"MID", MYLITE_SCALAR_FUNCTION_SUBSTRING},
        {"SUBSTRING_INDEX", MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX},
        {"TRIM", MYLITE_SCALAR_FUNCTION_TRIM},
        {"LTRIM", MYLITE_SCALAR_FUNCTION_LTRIM},
        {"RTRIM", MYLITE_SCALAR_FUNCTION_RTRIM},
        {"REPLACE", MYLITE_SCALAR_FUNCTION_REPLACE},
        {"REPEAT", MYLITE_SCALAR_FUNCTION_REPEAT},
        {"SPACE", MYLITE_SCALAR_FUNCTION_SPACE},
        {"REVERSE", MYLITE_SCALAR_FUNCTION_REVERSE},
        {"LPAD", MYLITE_SCALAR_FUNCTION_LPAD},
        {"RPAD", MYLITE_SCALAR_FUNCTION_RPAD},
        {"INSERT", MYLITE_SCALAR_FUNCTION_INSERT},
        {"QUOTE", MYLITE_SCALAR_FUNCTION_QUOTE},
        {"ELT", MYLITE_SCALAR_FUNCTION_ELT},
        {"FIELD", MYLITE_SCALAR_FUNCTION_FIELD},
        {"FIND_IN_SET", MYLITE_SCALAR_FUNCTION_FIND_IN_SET},
        {"MAKE_SET", MYLITE_SCALAR_FUNCTION_MAKE_SET},
        {"CHAR", MYLITE_SCALAR_FUNCTION_CHAR},
        {"CHARSET", MYLITE_SCALAR_FUNCTION_CHARSET},
        {"COLLATION", MYLITE_SCALAR_FUNCTION_COLLATION},
        {"COERCIBILITY", MYLITE_SCALAR_FUNCTION_COERCIBILITY},
        {"HEX", MYLITE_SCALAR_FUNCTION_HEX},
        {"UNHEX", MYLITE_SCALAR_FUNCTION_UNHEX},
        {"TO_BASE64", MYLITE_SCALAR_FUNCTION_TO_BASE64},
        {"FROM_BASE64", MYLITE_SCALAR_FUNCTION_FROM_BASE64},
        {"BIN", MYLITE_SCALAR_FUNCTION_BIN},
        {"OCT", MYLITE_SCALAR_FUNCTION_OCT},
        {"CONV", MYLITE_SCALAR_FUNCTION_CONV},
        {"LOCATE", MYLITE_SCALAR_FUNCTION_LOCATE},
        {"POSITION", MYLITE_SCALAR_FUNCTION_LOCATE},
        {"INSTR", MYLITE_SCALAR_FUNCTION_INSTR},
        {"ABS", MYLITE_SCALAR_FUNCTION_ABS},
        {"SIGN", MYLITE_SCALAR_FUNCTION_SIGN},
        {"FLOOR", MYLITE_SCALAR_FUNCTION_FLOOR},
        {"CEIL", MYLITE_SCALAR_FUNCTION_CEIL},
        {"CEILING", MYLITE_SCALAR_FUNCTION_CEIL},
        {"ROUND", MYLITE_SCALAR_FUNCTION_ROUND},
        {"FORMAT", MYLITE_SCALAR_FUNCTION_FORMAT},
        {"TRUNCATE", MYLITE_SCALAR_FUNCTION_TRUNCATE},
        {"EXP", MYLITE_SCALAR_FUNCTION_EXP},
        {"LN", MYLITE_SCALAR_FUNCTION_LN},
        {"LOG", MYLITE_SCALAR_FUNCTION_LOG},
        {"LOG2", MYLITE_SCALAR_FUNCTION_LOG2},
        {"LOG10", MYLITE_SCALAR_FUNCTION_LOG10},
        {"POW", MYLITE_SCALAR_FUNCTION_POWER},
        {"POWER", MYLITE_SCALAR_FUNCTION_POWER},
        {"SQRT", MYLITE_SCALAR_FUNCTION_SQRT},
        {"SIN", MYLITE_SCALAR_FUNCTION_SIN},
        {"COS", MYLITE_SCALAR_FUNCTION_COS},
        {"TAN", MYLITE_SCALAR_FUNCTION_TAN},
        {"COT", MYLITE_SCALAR_FUNCTION_COT},
        {"DEGREES", MYLITE_SCALAR_FUNCTION_DEGREES},
        {"RADIANS", MYLITE_SCALAR_FUNCTION_RADIANS},
        {"ACOS", MYLITE_SCALAR_FUNCTION_ACOS},
        {"ASIN", MYLITE_SCALAR_FUNCTION_ASIN},
        {"ATAN", MYLITE_SCALAR_FUNCTION_ATAN},
        {"ATAN2", MYLITE_SCALAR_FUNCTION_ATAN2},
        {"MOD", MYLITE_SCALAR_FUNCTION_MOD},
        {"PI", MYLITE_SCALAR_FUNCTION_PI},
        {"IF", MYLITE_SCALAR_FUNCTION_IF},
        {"IFNULL", MYLITE_SCALAR_FUNCTION_IFNULL},
        {"NULLIF", MYLITE_SCALAR_FUNCTION_NULLIF},
        {"COALESCE", MYLITE_SCALAR_FUNCTION_COALESCE},
        {"GREATEST", MYLITE_SCALAR_FUNCTION_GREATEST},
        {"LEAST", MYLITE_SCALAR_FUNCTION_LEAST},
        {"STRCMP", MYLITE_SCALAR_FUNCTION_STRCMP},
        {"ISNULL", MYLITE_SCALAR_FUNCTION_ISNULL},
        {"DATABASE", MYLITE_SCALAR_FUNCTION_DATABASE},
        {"SCHEMA", MYLITE_SCALAR_FUNCTION_SCHEMA},
        {"VERSION", MYLITE_SCALAR_FUNCTION_VERSION},
        {"LAST_INSERT_ID", MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID},
        {"ROW_COUNT", MYLITE_SCALAR_FUNCTION_ROW_COUNT},
        {"CONNECTION_ID", MYLITE_SCALAR_FUNCTION_CONNECTION_ID},
        {"USER", MYLITE_SCALAR_FUNCTION_USER},
        {"SESSION_USER", MYLITE_SCALAR_FUNCTION_USER},
        {"SYSTEM_USER", MYLITE_SCALAR_FUNCTION_USER},
        {"CURRENT_USER", MYLITE_SCALAR_FUNCTION_CURRENT_USER},
        {"NOW", MYLITE_SCALAR_FUNCTION_NOW},
        {"CURDATE", MYLITE_SCALAR_FUNCTION_CURDATE},
        {"CURRENT_DATE", MYLITE_SCALAR_FUNCTION_CURRENT_DATE},
        {"CURTIME", MYLITE_SCALAR_FUNCTION_CURTIME},
        {"CURRENT_TIME", MYLITE_SCALAR_FUNCTION_CURRENT_TIME},
        {"LOCALTIME", MYLITE_SCALAR_FUNCTION_LOCALTIME},
        {"LOCALTIMESTAMP", MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP},
        {"DATE", MYLITE_SCALAR_FUNCTION_DATE},
        {"DATEDIFF", MYLITE_SCALAR_FUNCTION_DATEDIFF},
        {"TIMESTAMPDIFF", MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF},
        {"DATE_ADD", MYLITE_SCALAR_FUNCTION_DATE_ADD},
        {"DATE_SUB", MYLITE_SCALAR_FUNCTION_DATE_SUB},
        {"ADDDATE", MYLITE_SCALAR_FUNCTION_ADDDATE},
        {"SUBDATE", MYLITE_SCALAR_FUNCTION_SUBDATE},
        {"YEAR", MYLITE_SCALAR_FUNCTION_YEAR},
        {"MONTH", MYLITE_SCALAR_FUNCTION_MONTH},
        {"DAY", MYLITE_SCALAR_FUNCTION_DAY},
        {"DAYOFMONTH", MYLITE_SCALAR_FUNCTION_DAY},
        {"HOUR", MYLITE_SCALAR_FUNCTION_HOUR},
        {"MINUTE", MYLITE_SCALAR_FUNCTION_MINUTE},
        {"SECOND", MYLITE_SCALAR_FUNCTION_SECOND},
        {"EXTRACT", MYLITE_SCALAR_FUNCTION_EXTRACT},
        {"BIT_COUNT", MYLITE_SCALAR_FUNCTION_BIT_COUNT},
        {"BIT_LENGTH", MYLITE_SCALAR_FUNCTION_BIT_LENGTH},
        {"CRC32", MYLITE_SCALAR_FUNCTION_CRC32},
        {"INET_ATON", MYLITE_SCALAR_FUNCTION_INET_ATON},
        {"INET_NTOA", MYLITE_SCALAR_FUNCTION_INET_NTOA},
        {"IS_UUID", MYLITE_SCALAR_FUNCTION_IS_UUID},
        {"UUID_TO_BIN", MYLITE_SCALAR_FUNCTION_UUID_TO_BIN},
        {"BIN_TO_UUID", MYLITE_SCALAR_FUNCTION_BIN_TO_UUID},
    };

    for (size_t index = 0U; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        if (ascii_span_equals(span, functions[index].name)) {
            return functions[index].id;
        }
    }
    return MYLITE_SCALAR_FUNCTION_UNKNOWN;
}

static bool scalar_function_depends_on_session(enum mylite_scalar_function_id function_id)
{
    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
    case MYLITE_SCALAR_FUNCTION_CONNECTION_ID:
    case MYLITE_SCALAR_FUNCTION_USER:
    case MYLITE_SCALAR_FUNCTION_CURRENT_USER:
    case MYLITE_SCALAR_FUNCTION_CHARSET:
    case MYLITE_SCALAR_FUNCTION_COLLATION:
    case MYLITE_SCALAR_FUNCTION_COERCIBILITY:
    case MYLITE_SCALAR_FUNCTION_STRCMP:
    case MYLITE_SCALAR_FUNCTION_NOW:
    case MYLITE_SCALAR_FUNCTION_CURDATE:
    case MYLITE_SCALAR_FUNCTION_CURRENT_DATE:
    case MYLITE_SCALAR_FUNCTION_CURTIME:
    case MYLITE_SCALAR_FUNCTION_CURRENT_TIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIME:
    case MYLITE_SCALAR_FUNCTION_LOCALTIMESTAMP:
        return true;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_CONCAT_WS:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_ASCII:
    case MYLITE_SCALAR_FUNCTION_ORD:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING:
    case MYLITE_SCALAR_FUNCTION_SUBSTRING_INDEX:
    case MYLITE_SCALAR_FUNCTION_TRIM:
    case MYLITE_SCALAR_FUNCTION_LTRIM:
    case MYLITE_SCALAR_FUNCTION_RTRIM:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_INSERT:
    case MYLITE_SCALAR_FUNCTION_REPEAT:
    case MYLITE_SCALAR_FUNCTION_SPACE:
    case MYLITE_SCALAR_FUNCTION_REVERSE:
    case MYLITE_SCALAR_FUNCTION_LPAD:
    case MYLITE_SCALAR_FUNCTION_RPAD:
    case MYLITE_SCALAR_FUNCTION_QUOTE:
    case MYLITE_SCALAR_FUNCTION_ELT:
    case MYLITE_SCALAR_FUNCTION_FIELD:
    case MYLITE_SCALAR_FUNCTION_FIND_IN_SET:
    case MYLITE_SCALAR_FUNCTION_MAKE_SET:
    case MYLITE_SCALAR_FUNCTION_CHAR:
    case MYLITE_SCALAR_FUNCTION_HEX:
    case MYLITE_SCALAR_FUNCTION_UNHEX:
    case MYLITE_SCALAR_FUNCTION_TO_BASE64:
    case MYLITE_SCALAR_FUNCTION_FROM_BASE64:
    case MYLITE_SCALAR_FUNCTION_BIN:
    case MYLITE_SCALAR_FUNCTION_OCT:
    case MYLITE_SCALAR_FUNCTION_CONV:
    case MYLITE_SCALAR_FUNCTION_BIT_COUNT:
    case MYLITE_SCALAR_FUNCTION_BIT_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CRC32:
    case MYLITE_SCALAR_FUNCTION_INET_ATON:
    case MYLITE_SCALAR_FUNCTION_INET_NTOA:
    case MYLITE_SCALAR_FUNCTION_IS_UUID:
    case MYLITE_SCALAR_FUNCTION_UUID_TO_BIN:
    case MYLITE_SCALAR_FUNCTION_BIN_TO_UUID:
    case MYLITE_SCALAR_FUNCTION_LOCATE:
    case MYLITE_SCALAR_FUNCTION_INSTR:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ROUND:
    case MYLITE_SCALAR_FUNCTION_FORMAT:
    case MYLITE_SCALAR_FUNCTION_TRUNCATE:
    case MYLITE_SCALAR_FUNCTION_EXP:
    case MYLITE_SCALAR_FUNCTION_POWER:
    case MYLITE_SCALAR_FUNCTION_SQRT:
    case MYLITE_SCALAR_FUNCTION_LN:
    case MYLITE_SCALAR_FUNCTION_LOG:
    case MYLITE_SCALAR_FUNCTION_LOG2:
    case MYLITE_SCALAR_FUNCTION_LOG10:
    case MYLITE_SCALAR_FUNCTION_SIN:
    case MYLITE_SCALAR_FUNCTION_COS:
    case MYLITE_SCALAR_FUNCTION_TAN:
    case MYLITE_SCALAR_FUNCTION_COT:
    case MYLITE_SCALAR_FUNCTION_DEGREES:
    case MYLITE_SCALAR_FUNCTION_RADIANS:
    case MYLITE_SCALAR_FUNCTION_ACOS:
    case MYLITE_SCALAR_FUNCTION_ASIN:
    case MYLITE_SCALAR_FUNCTION_ATAN:
    case MYLITE_SCALAR_FUNCTION_ATAN2:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_GREATEST:
    case MYLITE_SCALAR_FUNCTION_LEAST:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
    case MYLITE_SCALAR_FUNCTION_DATE:
    case MYLITE_SCALAR_FUNCTION_DATEDIFF:
    case MYLITE_SCALAR_FUNCTION_TIMESTAMPDIFF:
    case MYLITE_SCALAR_FUNCTION_DATE_ADD:
    case MYLITE_SCALAR_FUNCTION_DATE_SUB:
    case MYLITE_SCALAR_FUNCTION_ADDDATE:
    case MYLITE_SCALAR_FUNCTION_SUBDATE:
    case MYLITE_SCALAR_FUNCTION_YEAR:
    case MYLITE_SCALAR_FUNCTION_MONTH:
    case MYLITE_SCALAR_FUNCTION_DAY:
    case MYLITE_SCALAR_FUNCTION_HOUR:
    case MYLITE_SCALAR_FUNCTION_MINUTE:
    case MYLITE_SCALAR_FUNCTION_SECOND:
    case MYLITE_SCALAR_FUNCTION_EXTRACT:
        return false;
    }
    return false;
}

static bool ascii_span_equals(struct mylite_sql_source_span span, const char *text)
{
    size_t text_length = text == NULL ? 0U : strlen(text);

    if (span.length != text_length || span.text == NULL || text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        if (ascii_case_fold((unsigned char)span.text[index]) !=
            ascii_case_fold((unsigned char)text[index])) {
            return false;
        }
    }
    return true;
}

static bool is_null(const struct mylite_expression_value *value)
{
    return value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL;
}

static bool is_numeric_kind(enum mylite_expression_value_kind kind)
{
    return kind == MYLITE_EXPRESSION_VALUE_INT64 || kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
           kind == MYLITE_EXPRESSION_VALUE_REAL;
}

static bool like_match(const char *value, const char *pattern, char escape)
{
    return like_match_here(value == NULL ? "" : value, pattern == NULL ? "" : pattern, escape);
}

static bool like_match_here(const char *value, const char *pattern, char escape)
{
    if (*pattern == '\0') {
        return *value == '\0';
    }
    if (*pattern == '%') {
        do {
            if (like_match_here(value, pattern + 1, escape)) {
                return true;
            }
        } while (*value++ != '\0');
        return false;
    }
    if (*pattern == escape && pattern[1] != '\0') {
        return *value != '\0' &&
               ascii_case_fold((unsigned char)*value) ==
                   ascii_case_fold((unsigned char)pattern[1]) &&
               like_match_here(value + 1, pattern + 2, escape);
    }
    if (*pattern == '_') {
        return *value != '\0' && like_match_here(value + 1, pattern + 1, escape);
    }
    return *value != '\0' &&
           ascii_case_fold((unsigned char)*value) == ascii_case_fold((unsigned char)*pattern) &&
           like_match_here(value + 1, pattern + 1, escape);
}

static int ascii_case_fold(int character)
{
    return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

// NOLINTEND(misc-no-recursion, readability-implicit-bool-conversion)
