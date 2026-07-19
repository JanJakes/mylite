#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_H

#include <stdbool.h>

struct information_schema_predicate_plan;
struct information_schema_query;
struct mylite_db;

int mylite_execution_information_schema_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_predicate_plan *predicate,
    const char *const *row,
    bool *out_matches
);

#endif
