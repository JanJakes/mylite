#ifndef MYLITE_RUNTIME_MYLITE_FUNCTION_NAMES_H
#define MYLITE_RUNTIME_MYLITE_FUNCTION_NAMES_H

#include <stdbool.h>

struct mylite_sql_ast_node;

bool mylite_function_name_has_text_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_slice_string_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_make_set(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_elt(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_quote(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_insert(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_char(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_hex(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_unhex(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_to_base64(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_from_base64(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_format(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_binary_string_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_connection_string_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_base_conversion_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_field(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_find_in_set(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_greatest_least(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_strcmp(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_regexp_like(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_date_extraction(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_datediff(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_last_day(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_timestampdiff(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_to_days(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_to_seconds(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_from_days(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_from_unixtime(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_str_to_date(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_time_extraction(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_time_to_sec(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_sec_to_time(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_timediff(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_timestamp(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_year_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_month_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_day_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_dayofweek_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_dayofyear_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_quarter_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_hour_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_minute_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_second_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_microsecond_part(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_extract(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_date_interval_arithmetic(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_concat_ws(const struct mylite_sql_ast_node *name);
bool mylite_function_name_uses_source_length(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_charset(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_collation(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_coercibility(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_charset_collation_introspection(
    const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_length_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_bit_count(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_crc32(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_inet_aton(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_inet_ntoa(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_is_uuid(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_uuid_to_bin(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_bin_to_uuid(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_uuid(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_uuid_short(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_ascii(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_ord(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_search_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_has_integer_result(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_exp(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_logarithm(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_power(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_sqrt(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_trigonometric(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_inverse_trigonometric(const struct mylite_sql_ast_node *name);
bool mylite_function_name_is_angle_conversion(const struct mylite_sql_ast_node *name);

#endif
