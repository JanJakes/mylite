#ifndef MYLITE_SQL_MYLITE_PARSER_PLACEHOLDERS_H
#define MYLITE_SQL_MYLITE_PARSER_PLACEHOLDERS_H

#include "mylite_parser.h"

#include <stdbool.h>

struct mylite_sql_parser_retry_context {
    struct mylite_sql_token *tokens;
    size_t *matching_parenthesis_indexes;
    unsigned char *parenthesis_flags;
    size_t token_count;
    enum mylite_sql_parse_status status;
    bool has_non_trailing_semicolon;
    bool parentheses_balanced;
};

enum mylite_sql_parse_status mylite_sql_parser_retry_context_init(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parser_retry_context *out_context
);
void mylite_sql_parser_retry_context_deinit(struct mylite_sql_parser_retry_context *context);
bool mylite_sql_parser_retry_context_parenthesis_has_top_level_comma(
    const struct mylite_sql_parser_retry_context *context,
    const struct mylite_sql_token *left_parenthesis,
    size_t *inout_token_index
);

enum mylite_sql_parse_status mylite_sql_parser_try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_constructor_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_row_constructor_predicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_tableless_select_limit_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_repeated_select_locking_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_legacy_create_index_type_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_placeholder_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    const struct mylite_sql_parser_retry_context *retry_context,
    bool *out_handled
);

#endif
