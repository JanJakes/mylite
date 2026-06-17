#ifndef MYLITE_SQL_MYLITE_PARSER_PLACEHOLDERS_H
#define MYLITE_SQL_MYLITE_PARSER_PLACEHOLDERS_H

#include "mylite_parser.h"

#include <stdbool.h>

enum mylite_sql_parse_status mylite_sql_parser_try_parse_select_result_option_before_duplicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_constructor_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_tableless_select_limit_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_repeated_select_locking_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_legacy_create_index_type_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);
enum mylite_sql_parse_status mylite_sql_parser_try_parse_placeholder_statement(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);

#endif
