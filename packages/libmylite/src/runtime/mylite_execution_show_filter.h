#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SHOW_FILTER_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SHOW_FILTER_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;

struct show_like_filter {
    bool has_pattern;
    char *pattern;
    size_t pattern_length;
};

int mylite_execution_make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
);
void mylite_execution_show_like_filter_deinit(struct show_like_filter *filter);
bool mylite_execution_show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
);
bool mylite_execution_show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
);
bool mylite_execution_show_like_pattern_matches_with_escape(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    char escape_character
);
int mylite_execution_build_show_databases_column_name(
    const struct show_like_filter *filter,
    char **out_name
);
int mylite_execution_build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
);

static inline int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
) {
    return mylite_execution_make_show_like_filter(database, pattern_node, out_filter);
}

static inline void show_like_filter_deinit(struct show_like_filter *filter) {
    mylite_execution_show_like_filter_deinit(filter);
}

static inline bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
) {
    return mylite_execution_show_like_filter_matches(filter, value, case_sensitive);
}

static inline bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
) {
    return mylite_execution_show_like_pattern_matches(
        pattern,
        pattern_length,
        value,
        value_length,
        case_sensitive
    );
}

static inline bool show_like_pattern_matches_with_escape(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive,
    char escape_character
) {
    return mylite_execution_show_like_pattern_matches_with_escape(
        pattern,
        pattern_length,
        value,
        value_length,
        case_sensitive,
        escape_character
    );
}

static inline int build_show_databases_column_name(
    const struct show_like_filter *filter,
    char **out_name
) {
    return mylite_execution_build_show_databases_column_name(filter, out_name);
}

static inline int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
) {
    return mylite_execution_build_show_tables_column_name(schema_name, filter, out_name);
}

#endif
