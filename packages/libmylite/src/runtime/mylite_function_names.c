#include "mylite_function_names.h"

#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stddef.h>

static bool function_name_matches_any(const struct mylite_sql_ast_node *name,
                                      const char *const *candidates, size_t candidate_count);

bool mylite_function_name_has_text_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CONCAT", "LOWER",  "LCASE",  "UPPER",
                                        "UCASE",  "LEFT",   "RIGHT",  "REPLACE",
                                        "IF",     "IFNULL", "NULLIF", "COALESCE"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_slice_string_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {
        "CONCAT_WS", "SUBSTRING", "SUBSTR", "MID",   "SUBSTRING_INDEX", "TRIM",
        "LTRIM",     "RTRIM",     "INSERT", "QUOTE", "REPEAT",          "SPACE",
        "REVERSE",   "LPAD",      "RPAD",   "ELT",   "MAKE_SET"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_make_set(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MAKE_SET"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_elt(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ELT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_quote(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"QUOTE"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_insert(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INSERT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_char(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CHAR"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_hex(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"HEX"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_unhex(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UNHEX"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_base64(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_BASE64"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_from_base64(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FROM_BASE64"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_format(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FORMAT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_binary_string_result(const struct mylite_sql_ast_node *name)
{
    if (mylite_function_name_is_unhex(name) || mylite_function_name_is_uuid_to_bin(name)) {
        return true;
    }
    return mylite_function_name_is_from_base64(name);
}

bool mylite_function_name_has_connection_string_result(const struct mylite_sql_ast_node *name)
{
    if (mylite_function_name_is_hex(name) || mylite_function_name_is_to_base64(name) ||
        mylite_function_name_is_bin_to_uuid(name) || mylite_function_name_is_format(name)) {
        return true;
    }
    return mylite_function_name_has_base_conversion_result(name);
}

bool mylite_function_name_has_base_conversion_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIN", "OCT", "CONV"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_field(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FIELD"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_find_in_set(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FIND_IN_SET"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_greatest_least(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"GREATEST", "LEAST"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_strcmp(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"STRCMP"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_date_extraction(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DATE"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_datediff(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DATEDIFF"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_timestampdiff(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TIMESTAMPDIFF"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_days(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_DAYS"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_seconds(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_SECONDS"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_from_days(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FROM_DAYS"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_time_extraction(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TIME"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_year_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"YEAR"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_month_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MONTH"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_day_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DAY", "DAYOFMONTH"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_hour_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"HOUR"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_minute_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MINUTE"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_second_part(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SECOND"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_extract(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"EXTRACT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_date_interval_arithmetic(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {
        "TIMESTAMPADD", "DATE_ADD", "DATE_SUB", "ADDDATE", "SUBDATE",
    };

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_concat_ws(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CONCAT_WS"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_uses_source_length(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TRIM", "LTRIM", "RTRIM", "SUBSTRING_INDEX"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_charset(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CHARSET"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_collation(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"COLLATION"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_coercibility(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"COERCIBILITY"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_charset_collation_introspection(const struct mylite_sql_ast_node *name)
{
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        return true;
    }
    return mylite_function_name_is_coercibility(name);
}

bool mylite_function_name_has_length_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LENGTH", "OCTET_LENGTH", "CHAR_LENGTH", "CHARACTER_LENGTH",
                                        "BIT_LENGTH"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_bit_count(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIT_COUNT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_crc32(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CRC32"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inet_aton(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INET_ATON"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inet_ntoa(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INET_NTOA"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_is_uuid(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"IS_UUID"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_uuid_to_bin(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UUID_TO_BIN"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_bin_to_uuid(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIN_TO_UUID"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_ascii(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ASCII"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_ord(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ORD"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_search_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LOCATE", "POSITION", "INSTR"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_integer_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SIGN", "FLOOR", "CEIL", "CEILING"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_exp(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"EXP"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_logarithm(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LN", "LOG", "LOG2", "LOG10"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_power(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"POW", "POWER"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_sqrt(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SQRT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_trigonometric(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SIN", "COS", "TAN", "COT"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inverse_trigonometric(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ACOS", "ASIN", "ATAN", "ATAN2"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_angle_conversion(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DEGREES", "RADIANS"};

    return function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

static bool function_name_matches_any(const struct mylite_sql_ast_node *name,
                                      const char *const *candidates, size_t candidate_count)
{
    if (name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < candidate_count; ++index) {
        if (mylite_span_equal_ci(name->span, candidates[index])) {
            return true;
        }
    }
    return false;
}
