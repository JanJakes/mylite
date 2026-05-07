#ifndef MYLITE_RUNTIME_MYLITE_PREPARED_STATEMENTS_TYPES_H
#define MYLITE_RUNTIME_MYLITE_PREPARED_STATEMENTS_TYPES_H

#include "sql/mylite_ast.h"

#include <stddef.h>

struct mylite_prepared_statement_entry {
    char *name;
    char *sql_text;
    size_t parameter_count;
    unsigned int parse_modes;
};

struct mylite_prepared_statement_store {
    struct mylite_prepared_statement_entry *items;
    size_t count;
};

struct mylite_prepare_statement_plan {
    char *name;
    char *source_sql_text;
    struct mylite_sql_ast source_ast;
    const struct mylite_sql_ast_node *source;
};

struct mylite_execute_prepared_plan {
    char *name;
    char **using_names;
    size_t using_count;
};

struct mylite_deallocate_prepare_plan {
    char *name;
};

#endif
