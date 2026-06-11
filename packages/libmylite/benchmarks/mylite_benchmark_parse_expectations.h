#ifndef MYLITE_BENCHMARK_PARSE_EXPECTATIONS_H
#define MYLITE_BENCHMARK_PARSE_EXPECTATIONS_H

#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_benchmark_expected_parse_failure {
    size_t query_index;
    char *status_name;
    char *token_kind_name;
    char *reason;
};

struct mylite_benchmark_expected_parse_failure_list {
    struct mylite_benchmark_expected_parse_failure *items;
    size_t count;
    size_t capacity;
};

int mylite_benchmark_load_expected_parse_failures(
    const char *path,
    struct mylite_benchmark_expected_parse_failure_list *out_expectations
);
int mylite_benchmark_parse_expected_parse_failures(
    const char *data,
    size_t length,
    const char *source_name,
    struct mylite_benchmark_expected_parse_failure_list *out_expectations
);
void mylite_benchmark_expected_parse_failure_list_deinit(
    struct mylite_benchmark_expected_parse_failure_list *expectations
);
const struct mylite_benchmark_expected_parse_failure *mylite_benchmark_expected_parse_failure_find(
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    size_t query_index
);
bool mylite_benchmark_expected_parse_failure_matches(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
);

#endif
