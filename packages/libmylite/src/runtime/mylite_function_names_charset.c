#include "mylite_function_names.h"

#include "mylite_function_name_match.h"

bool mylite_function_name_is_charset_collation_introspection(
    const struct mylite_sql_ast_node *name
) {
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        return true;
    }
    return mylite_function_name_is_coercibility(name);
}

bool mylite_function_name_is_charset(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"CHARSET"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_collation(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"COLLATION"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}

bool mylite_function_name_is_coercibility(const struct mylite_sql_ast_node *name) {
    static const char *const names[] = {"COERCIBILITY"};

    return mylite_function_name_matches_any(name, names, sizeof(names) / sizeof(names[0]));
}
