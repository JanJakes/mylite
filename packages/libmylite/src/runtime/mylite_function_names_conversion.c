#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

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

bool mylite_function_name_is_hex(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"HEX"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_unhex(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UNHEX"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_to_base64(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TO_BASE64"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_from_base64(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FROM_BASE64"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_format(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FORMAT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_base_conversion_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIN", "OCT", "CONV"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inet_aton(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INET_ATON"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_inet_ntoa(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INET_NTOA"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_is_uuid(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"IS_UUID"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_uuid_to_bin(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UUID_TO_BIN"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_bin_to_uuid(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"BIN_TO_UUID"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_uuid(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UUID"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_uuid_short(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"UUID_SHORT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
