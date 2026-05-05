#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

bool mylite_function_name_has_length_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LENGTH", "OCTET_LENGTH", "CHAR_LENGTH", "CHARACTER_LENGTH",
                                        "BIT_LENGTH"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_integer_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SIGN", "FLOOR", "CEIL", "CEILING"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_bit_count(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIT_COUNT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_crc32(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CRC32"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_exp(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"EXP"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_logarithm(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LN", "LOG", "LOG2", "LOG10"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_power(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"POW", "POWER"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_sqrt(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SQRT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_trigonometric(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"SIN", "COS", "TAN", "COT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inverse_trigonometric(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ACOS", "ASIN", "ATAN", "ATAN2"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_angle_conversion(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"DEGREES", "RADIANS"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
