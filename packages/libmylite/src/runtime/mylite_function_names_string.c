#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

bool mylite_function_name_has_text_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CONCAT", "LOWER",  "LCASE",  "UPPER",
                                        "UCASE",  "LEFT",   "RIGHT",  "REPLACE",
                                        "IF",     "IFNULL", "NULLIF", "COALESCE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_has_slice_string_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {
        "CONCAT_WS", "SUBSTRING", "SUBSTR", "MID",   "SUBSTRING_INDEX", "TRIM",
        "LTRIM",     "RTRIM",     "INSERT", "QUOTE", "REPEAT",          "SPACE",
        "REVERSE",   "LPAD",      "RPAD",   "ELT",   "MAKE_SET"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_uses_source_length(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"TRIM", "LTRIM", "RTRIM", "SUBSTRING_INDEX"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_make_set(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"MAKE_SET"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_elt(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ELT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_quote(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"QUOTE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_insert(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"INSERT"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_char(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CHAR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_concat_ws(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"CONCAT_WS"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_ascii(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ASCII"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_ord(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"ORD"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
