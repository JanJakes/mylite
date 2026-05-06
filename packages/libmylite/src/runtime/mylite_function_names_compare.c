#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

bool mylite_function_name_has_search_result(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"LOCATE", "POSITION", "INSTR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_field(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FIELD"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_find_in_set(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"FIND_IN_SET"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_greatest_least(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"GREATEST", "LEAST"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_strcmp(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"STRCMP"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_regexp_like(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"REGEXP_LIKE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_regexp_instr(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"REGEXP_INSTR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_regexp_substr(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"REGEXP_SUBSTR"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_regexp_replace(const struct mylite_sql_ast_node *name)
{
    static const char *const names[] = {"REGEXP_REPLACE"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
