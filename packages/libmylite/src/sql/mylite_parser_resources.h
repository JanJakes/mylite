#ifndef MYLITE_SQL_MYLITE_PARSER_RESOURCES_H
#define MYLITE_SQL_MYLITE_PARSER_RESOURCES_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_sql_parse_result;

enum {
    mylite_sql_parser_retry_token_limit = 65536,
    mylite_sql_parser_retry_parenthesis_depth_limit = 512,
    mylite_sql_parser_retry_callback_limit = 8,
    mylite_sql_parser_lexer_pass_limit = 4,
    mylite_sql_parser_retry_workspace_minimum = 256 * 1024,
    mylite_sql_parser_retry_workspace_limit = 8 * 1024 * 1024,
    mylite_sql_parser_retry_workspace_input_multiplier = 96,
};

struct mylite_sql_parser_resource_tracker {
    size_t lexer_pass_count;
    size_t retry_token_count;
    size_t retry_allocation_count;
    size_t retry_allocation_bytes;
    size_t retry_workspace_bytes;
    size_t retry_workspace_peak_bytes;
    size_t retry_workspace_limit;
    bool retry_budget_exhausted;
};

void mylite_sql_parser_resource_tracker_init(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t input_length
);
bool mylite_sql_parser_resource_record_lexer_pass(struct mylite_sql_parser_resource_tracker *tracker
);
bool mylite_sql_parser_resource_record_retry_token(
    struct mylite_sql_parser_resource_tracker *tracker
);
bool mylite_sql_parser_resource_workspace_fits(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t old_bytes,
    size_t new_bytes
);
void mylite_sql_parser_resource_record_workspace(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t old_bytes,
    size_t new_bytes
);
void mylite_sql_parser_resource_release_workspace(
    struct mylite_sql_parser_resource_tracker *tracker,
    size_t bytes
);
void mylite_sql_parser_resource_exhaust(struct mylite_sql_parser_resource_tracker *tracker);
void mylite_sql_parser_resource_publish(
    const struct mylite_sql_parser_resource_tracker *tracker,
    struct mylite_sql_parse_result *result
);

#endif
