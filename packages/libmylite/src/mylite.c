#include <mylite/mylite.h>

#include "mylite_charset.h"
#include "mylite_expression.h"
#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "runtime/mylite_connection_statement.h"
#include "runtime/mylite_diagnostics.h"
#include "runtime/mylite_dml.h"
#include "runtime/mylite_dml_statement.h"
#include "runtime/mylite_dml_types.h"
#include "runtime/mylite_error_codes.h"
#include "runtime/mylite_expression_descriptor.h"
#include "runtime/mylite_expression_validation.h"
#include "runtime/mylite_field_descriptor.h"
#include "runtime/mylite_function_names.h"
#include "runtime/mylite_information_schema.h"
#include "runtime/mylite_metadata.h"
#include "runtime/mylite_metadata_constants.h"
#include "runtime/mylite_metadata_types.h"
#include "runtime/mylite_runtime.h"
#include "runtime/mylite_schema.h"
#include "runtime/mylite_schema_types.h"
#include "runtime/mylite_select.h"
#include "runtime/mylite_select_aggregate.h"
#include "runtime/mylite_select_eval.h"
#include "runtime/mylite_select_from.h"
#include "runtime/mylite_select_join_cache.h"
#include "runtime/mylite_select_projection.h"
#include "runtime/mylite_select_resolve.h"
#include "runtime/mylite_select_row_loader.h"
#include "runtime/mylite_select_rowset.h"
#include "runtime/mylite_select_sql.h"
#include "runtime/mylite_select_types.h"
#include "runtime/mylite_session_functions.h"
#include "runtime/mylite_show.h"
#include "runtime/mylite_show_types.h"
#include "runtime/mylite_span.h"
#include "runtime/mylite_sqlite_value.h"
#include "runtime/mylite_statement.h"
#include "runtime/mylite_statement_prepare.h"
#include "runtime/mylite_table_ddl.h"
#include "runtime/mylite_table_ddl_statement.h"
#include "runtime/mylite_table_ddl_types.h"
#include "runtime/mylite_temporal_functions.h"
#include "runtime/mylite_transaction_types.h"
#include "runtime/mylite_transactions.h"
#include "sql/mylite_lexer.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt);
static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt);
static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_rename_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_truncate_table_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
static int prepare_alter_table_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int prepare_create_index_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_drop_index_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_replace_values_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
static int prepare_replace_set_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int prepare_transaction_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement);
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt);
static int prepare_table_select_sqlite_statement(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt);
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
static int prepare_select_subquery_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             mylite_stmt **out_stmt);
static int validate_scalar_select_order_by_clause(mylite_db *database,
                                                  const struct mylite_sql_ast_node *order_by_clause,
                                                  const struct mylite_result_metadata *metadata);
static int bind_scalar_select_limit_clause(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *limit_clause);
static int copy_scalar_select_item(mylite_stmt *stmt, const struct mylite_sql_ast_node *item,
                                   size_t index, const char *source_sql, size_t source_sql_length);
static int copy_scalar_select_item_expression(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              size_t index, const char *source_sql,
                                              size_t source_sql_length);
static int validate_scalar_select_order_item(mylite_db *database,
                                             const struct mylite_sql_ast_node *order_item,
                                             const struct mylite_result_metadata *metadata);
static int validate_scalar_select_order_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_result_metadata *metadata);
static int
validate_scalar_select_order_function_call(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_result_metadata *metadata);
static int resolve_scalar_select_order_reference(mylite_db *database,
                                                 const struct mylite_result_metadata *metadata,
                                                 const struct mylite_sql_ast_node *expression);
static int prepare_union_query_expression_statement(mylite_db *database,
                                                    const struct mylite_sql_ast_node *statement,
                                                    const char *sql, size_t sql_length,
                                                    mylite_stmt **out_stmt);
static int validate_union_operand_column_counts(mylite_db *database,
                                                const struct mylite_union_plan *plan);
static int bind_union_query_clauses(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt *stmt);
static int collect_union_query_operands(mylite_db *database, const struct mylite_sql_ast_node *node,
                                        struct mylite_union_plan *plan);
static int append_union_query_operand(mylite_db *database, struct mylite_union_plan *plan,
                                      mylite_stmt *operand,
                                      enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                                      bool has_operator);
static int prepare_union_query_operand(mylite_db *database, const struct mylite_sql_ast_node *node,
                                       mylite_stmt **out_operand);
static const struct mylite_sql_ast_node *
unwrap_union_query_primary(const struct mylite_sql_ast_node *node);
static int attach_union_result_metadata(mylite_stmt *stmt);
static int initialize_union_output_plan(mylite_stmt *stmt);
static int add_union_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *label);
static int aggregate_union_result_metadata(mylite_stmt *stmt);
static int bind_union_global_order_by_clause(mylite_db *database,
                                             const struct mylite_sql_ast_node *order_by_clause,
                                             struct mylite_select_plan *plan);
static int bind_union_global_order_item(mylite_db *database,
                                        const struct mylite_sql_ast_node *order_item,
                                        struct mylite_select_plan *plan);
static int bind_union_global_order_expression(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_select_plan *plan);
static int bind_union_global_order_function_call(mylite_db *database,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_select_plan *plan);
static int resolve_union_order_reference(mylite_db *database, const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression,
                                         enum mylite_select_order_key_kind *out_kind,
                                         size_t *out_index);
static int attach_select_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan);
static int copy_select_result_column_metadata(mylite_db *database,
                                              struct mylite_result_column_metadata *metadata,
                                              const struct mylite_select_plan *plan,
                                              size_t output_index);
static int infer_select_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_scalar_expression_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_expression_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
static int infer_identifier_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor);
static int infer_literal_descriptor(mylite_db *database,
                                    const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_value *value,
                                    struct mylite_field_descriptor *out_descriptor);
static int infer_unary_expression_descriptor(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression,
                                             const struct mylite_expression_value *value,
                                             struct mylite_field_descriptor *out_descriptor);
static int infer_binary_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_ternary_expression_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_expression_value *value,
                                               struct mylite_field_descriptor *out_descriptor);
static int infer_function_expression_descriptor(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression,
                                                const struct mylite_expression_value *value,
                                                struct mylite_field_descriptor *out_descriptor);
static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static bool function_result_nullable(bool arguments_nullable,
                                     const struct mylite_expression_value *value);
static uint64_t text_function_result_length(mylite_db *database,
                                            const struct mylite_expression_value *value);
static bool infer_session_function_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *name,
                                              struct mylite_field_descriptor *out_descriptor);
static bool
infer_current_temporal_function_descriptor(const struct mylite_sql_ast_node *expression,
                                           struct mylite_field_descriptor *out_descriptor);
static int infer_temporal_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static struct mylite_field_descriptor current_datetime_function_descriptor(unsigned int fsp);
static struct mylite_field_descriptor current_date_function_descriptor(void);
static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp);
static bool
infer_temporal_scalar_function_descriptor(const struct mylite_sql_ast_node *name,
                                          bool arguments_nullable,
                                          struct mylite_field_descriptor *out_descriptor);
static bool infer_temporal_part_function_descriptor(const struct mylite_sql_ast_node *expression,
                                                    struct mylite_field_descriptor *out_descriptor);
static int infer_time_function_descriptor(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          const struct mylite_expression_value *value,
                                          struct mylite_field_descriptor *out_descriptor);
static unsigned int
time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor,
                                const struct mylite_expression_value *value);
static bool time_function_argument_is_approximate(const struct mylite_sql_ast_node *argument,
                                                  const struct mylite_field_descriptor *descriptor);
static struct mylite_field_descriptor time_function_descriptor(unsigned int decimals);
static unsigned int time_function_value_decimals(const struct mylite_expression_value *value);
static int infer_date_interval_function_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *expression,
                                                   struct mylite_field_descriptor *out_descriptor);
static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database);
static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals);

static bool interval_unit_has_time_part(enum mylite_sql_ast_interval_unit unit);
static bool infer_fixed_integer_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static bool infer_exp_function_descriptor(const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor);
static bool infer_power_function_descriptor(const struct mylite_sql_ast_node *name,
                                            struct mylite_field_descriptor *out_descriptor);
static bool infer_sqrt_function_descriptor(const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor);
static bool infer_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    struct mylite_field_descriptor *out_descriptor);
static bool
infer_inverse_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor);
static bool
infer_angle_conversion_function_descriptor(const struct mylite_sql_ast_node *name,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor);
static bool infer_list_index_function_descriptor(const struct mylite_sql_ast_node *name,
                                                 bool nullable,
                                                 struct mylite_field_descriptor *out_descriptor);
static bool infer_uuid_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_make_set_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_char_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *expression,
                                          struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_string_encoding_function_descriptor(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     struct mylite_field_descriptor *out_descriptor,
                                                     bool *out_matched);
static bool
infer_base_conversion_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_hex_function_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression,
                                         struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_unhex_function_descriptor(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_to_base64_function_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               struct mylite_field_descriptor *out_descriptor);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_from_base64_function_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
static uint64_t base64_encoded_descriptor_length(uint64_t source_length);
static uint64_t base64_decoded_descriptor_length(uint64_t source_length);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t quote_function_result_length(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t quote_function_source_display_length(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *source,
                                                     uint64_t max_bytes_per_character,
                                                     bool *out_source_is_null);
static uint64_t
quote_function_descriptor_display_length(const struct mylite_field_descriptor *descriptor,
                                         uint64_t max_bytes_per_character);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t insert_function_result_length(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t make_set_function_result_length(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static int make_set_function_members_are_all_null(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *expression,
                                                  bool *out_all_null);
static uint64_t make_set_all_null_result_length(const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t elt_function_result_length(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t elt_argument_result_length(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t expression_text_display_length(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_slice_string_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool nullable, struct mylite_field_descriptor *out_descriptor, bool *out_matched);
// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t slice_string_function_result_length(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    const struct mylite_expression_value *value);
static bool
infer_session_or_inet_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor);
static bool infer_strcmp_function_descriptor(const struct mylite_sql_ast_node *name,
                                             bool result_nullable,
                                             struct mylite_field_descriptor *out_descriptor);
static bool infer_inet_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor);
static bool
function_name_has_binary_numeric_collation_result(const struct mylite_sql_ast_node *name);
static int infer_aggregate_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
static int infer_case_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor);
static int infer_cast_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_expression_value *value,
                                            struct mylite_field_descriptor *out_descriptor);
static int
infer_scalar_subquery_expression_descriptor(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor);
static int infer_in_subquery_expression_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *expression,
                                                   struct mylite_field_descriptor *out_descriptor);
static int infer_row_subquery_expression_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    struct mylite_field_descriptor *out_descriptor);
static int infer_quantified_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor);
static int infer_case_result_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_case_descriptor_aggregate *aggregate);
static void aggregate_case_result_descriptor(const struct mylite_field_descriptor *descriptor,
                                             struct mylite_case_descriptor_aggregate *aggregate);
static struct mylite_field_descriptor
finalize_case_descriptor(mylite_db *database,
                         const struct mylite_case_descriptor_aggregate *aggregate);
static struct mylite_field_descriptor
cast_signed_descriptor(const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_unsigned_descriptor(const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_decimal_descriptor(const struct mylite_sql_ast_node *target,
                        const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_character_descriptor(mylite_db *database, const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          const struct mylite_field_descriptor *source);
static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target);
static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target);
static uint64_t cast_character_length(mylite_db *database, const struct mylite_sql_ast_node *target,
                                      const struct mylite_expression_value *value,
                                      const struct mylite_field_descriptor *source);
static unsigned int cast_target_charset_id(mylite_db *database,
                                           const struct mylite_sql_ast_node *target);
static int cast_target_charset_max_length(mylite_db *database,
                                          const struct mylite_sql_ast_node *target);
static int infer_function_arguments_nullable(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *arguments,
                                             bool *out_nullable);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor);
static int infer_greatest_least_function_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static int greatest_least_function_uses_string_domain(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      const struct mylite_sql_ast_node *arguments,
                                                      bool *out_string_domain);
static int infer_greatest_least_string_descriptor(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *arguments,
                                                  bool result_nullable,
                                                  struct mylite_field_descriptor *out_descriptor);
static uint64_t greatest_least_string_result_length(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *arguments);
static uint64_t
greatest_least_argument_string_length(mylite_db *database,
                                      const struct mylite_field_descriptor *descriptor);
static int infer_greatest_least_numeric_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *arguments,
                                                   bool result_nullable,
                                                   struct mylite_field_descriptor *out_descriptor);
static void
aggregate_greatest_least_numeric_descriptor(const struct mylite_field_descriptor *argument,
                                            struct mylite_field_descriptor *aggregate,
                                            bool *out_saw_nonnull);
static int infer_round_function_descriptor(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor);
static int infer_format_function_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor);
static uint64_t
format_function_result_character_length(const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *argument,
                                        const struct mylite_field_descriptor *argument_descriptor);
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument);
static uint64_t format_literal_fraction_length(const struct mylite_sql_ast_node *argument);
static int infer_truncate_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              bool result_nullable,
                                              struct mylite_field_descriptor *out_descriptor);
static bool infer_logarithm_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor);
static bool
round_function_argument_is_approximate_literal(const struct mylite_sql_ast_node *argument);
static bool round_function_constant_scale(const struct mylite_sql_ast_node *argument,
                                          int *out_scale);
static int round_function_descriptor_scale(int scale);
static void
truncate_decimal_descriptor_for_constant_scale(struct mylite_field_descriptor *descriptor,
                                               const struct mylite_field_descriptor *source,
                                               int scale);
static bool infer_code_search_function_descriptor(const struct mylite_sql_ast_node *name,
                                                  bool nullable,
                                                  struct mylite_field_descriptor *out_descriptor);
static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit);
static int build_select_outputs(mylite_db *database, const struct mylite_sql_ast_node *select_list,
                                bool allow_expression_outputs, struct mylite_select_plan *plan);
static int prepare_table_select_custom_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *where_clause,
                                                 const char *sql, size_t sql_length,
                                                 struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt);
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan);
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan);
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan);
static int bind_select_predicate_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *plan);
static int bind_select_predicate_expression_in_clause(mylite_db *database,
                                                      const struct mylite_sql_ast_node *expression,
                                                      const struct mylite_select_plan *plan,
                                                      const char *clause_context,
                                                      size_t first_table, size_t table_count);
static int bind_select_predicate_binary_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_select_plan *plan,
                                                   const char *clause_context, size_t first_table,
                                                   size_t table_count);
static int bind_select_predicate_in_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count);
static int bind_select_predicate_row_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count);
static int bind_select_predicate_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count);
static int bind_select_subquery_expression(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           bool scalar_context);
static int bind_select_in_subquery_expression(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_select_plan *outer_plan);
static int bind_select_quantified_subquery_expression(mylite_db *database,
                                                      const struct mylite_sql_ast_node *expression,
                                                      const struct mylite_select_plan *outer_plan);
static int bind_select_row_subquery_expression(mylite_db *database,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_select_plan *outer_plan);
static int validate_in_subquery_expression(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_select_plan *outer_plan);
static int validate_row_subquery_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *outer_plan);
static int validate_quantified_subquery_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_select_plan *outer_plan);
static bool row_subquery_expression_is_supported(const struct mylite_sql_ast_node *expression);
static bool row_subquery_expression_is_membership(const struct mylite_sql_ast_node *expression);
static bool
row_subquery_expression_is_positive_membership(const struct mylite_sql_ast_node *expression);
static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *expression);
static bool binary_expression_is_row_in_subquery(const struct mylite_sql_ast_node *expression);
static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *expression);
static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind);
static const struct mylite_sql_ast_node *
row_subquery_select_statement(const struct mylite_sql_ast_node *expression);
static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *expression);
static bool
quantified_comparison_is_row_subquery_alias(const struct mylite_sql_ast_node *expression);
static size_t row_constructor_width(const struct mylite_sql_ast_node *row);
static int bind_select_predicate_row_constructor(mylite_db *database,
                                                 const struct mylite_sql_ast_node *row,
                                                 const struct mylite_select_plan *plan,
                                                 const char *clause_context, size_t first_table,
                                                 size_t table_count);
static int bind_select_aggregate_aware_row_constructor(mylite_db *database,
                                                       const struct mylite_sql_ast_node *row,
                                                       struct mylite_select_plan *plan,
                                                       const char *clause_context);
static int bind_select_order_row_constructor(mylite_db *database,
                                             const struct mylite_sql_ast_node *row,
                                             struct mylite_select_plan *plan);
static bool in_subquery_references_outer_plan(const struct mylite_sql_ast_node *node,
                                              const struct mylite_select_plan *outer_plan,
                                              const struct mylite_sql_ast_node *select_statement);
static bool
in_subquery_has_unqualified_outer_column_reference(const struct mylite_sql_ast_node *node,
                                                   const struct mylite_select_plan *outer_plan);
static bool select_statement_has_visible_table_span(const struct mylite_sql_ast_node *node,
                                                    struct mylite_sql_source_span name);
static const struct mylite_sql_ast_node *
qualified_identifier_first_part(const struct mylite_sql_ast_node *identifier);
static const struct mylite_sql_ast_node *
qualified_identifier_last_part(const struct mylite_sql_ast_node *identifier);
static int bind_select_function_arguments(mylite_db *database,
                                          const struct mylite_sql_ast_node *expression,
                                          const struct mylite_select_plan *plan,
                                          const char *clause_context, size_t first_table,
                                          size_t table_count);
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan);
static int bind_select_aggregate_aware_expression(mylite_db *database,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_select_plan *plan,
                                                  const char *clause_context);
static int bind_select_aggregate_aware_binary_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context);
static int bind_select_aggregate_aware_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context);
static int bind_select_aggregate_aware_children(mylite_db *database,
                                                const struct mylite_sql_ast_node *expression,
                                                struct mylite_select_plan *plan,
                                                const char *clause_context);
static int bind_select_aggregate_aware_function(mylite_db *database,
                                                const struct mylite_sql_ast_node *expression,
                                                struct mylite_select_plan *plan,
                                                const char *clause_context);
static int bind_select_aggregate_call(mylite_db *database,
                                      const struct mylite_sql_ast_node *expression,
                                      struct mylite_select_plan *plan);
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan);
static int bind_select_group_item(mylite_db *database, const struct mylite_sql_ast_node *group_item,
                                  struct mylite_select_plan *plan);
static int bind_select_group_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan);
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan);
static bool select_literal_is_supported(const struct mylite_sql_ast_node *expression);
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan);
static int bind_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  struct mylite_select_plan *plan);
static int bind_select_order_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan);
static int bind_select_order_binary_expression(mylite_db *database,
                                               const struct mylite_sql_ast_node *expression,
                                               struct mylite_select_plan *plan);
static int bind_select_order_in_subquery_expression(mylite_db *database,
                                                    const struct mylite_sql_ast_node *expression,
                                                    struct mylite_select_plan *plan);
static int
bind_select_order_quantified_subquery_expression(mylite_db *database,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_select_plan *plan);
static int validate_select_expression_outputs(mylite_db *database,
                                              const struct mylite_select_plan *plan);
static int validate_select_distinct_order_key(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_select_order_key *order_key,
                                              size_t order_position);
static int validate_select_distinct_order_expression(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     size_t order_position);
static int validate_select_distinct_order_expression_node(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, size_t order_position, bool alias_first);
static int push_select_distinct_order_expression_child(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first);
static int push_select_distinct_order_expression_children(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first);
static bool
pop_select_distinct_order_expression(struct mylite_select_distinct_order_validation_stack *stack,
                                     const struct mylite_sql_ast_node **out_expression,
                                     bool *out_alias_first);
static void select_distinct_order_validation_stack_deinit(
    struct mylite_select_distinct_order_validation_stack *stack);
static int validate_select_distinct_order_identifier(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *identifier,
                                                     size_t order_position, bool alias_first);
static int validate_select_distinct_order_identifier_column_first(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *identifier, size_t order_position, bool *out_resolved);
static bool
select_distinct_order_expression_matches_output(const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression);
static const struct mylite_sql_ast_node *
unwrap_parenthesized_expression(const struct mylite_sql_ast_node *expression);
static bool select_distinct_column_index_is_output(const struct mylite_select_plan *plan,
                                                   size_t column_index);
static int validate_select_grouping(mylite_db *database, const struct mylite_select_plan *plan);
static int validate_select_grouping_clause_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy);
static bool select_output_contains_aggregate(const struct mylite_select_plan *plan,
                                             size_t output_index);
static bool select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression);
static bool select_output_is_group_invariant(const struct mylite_select_plan *plan,
                                             size_t output_index);
static bool
select_expression_is_group_invariant(const struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *expression,
                                     enum mylite_select_grouping_reference_policy reference_policy);
static bool select_expression_children_are_group_invariant(
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *first_child,
    enum mylite_select_grouping_reference_policy reference_policy);
static bool
select_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                     const struct mylite_sql_ast_node *identifier,
                                     enum mylite_select_grouping_reference_policy reference_policy);
static bool
select_having_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *identifier);
static bool
select_order_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *identifier);
static bool select_column_reference_is_grouped(const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *identifier);
static bool select_column_index_is_grouped(const struct mylite_select_plan *plan,
                                           size_t column_index);
static bool select_output_is_grouped_by_key(const struct mylite_select_plan *plan,
                                            size_t output_index);
static bool select_expression_is_grouped(const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression);
static bool select_group_key_matches_column_output(const struct mylite_select_plan *plan,
                                                   const struct mylite_select_group_key *group_key,
                                                   size_t output_index);
static bool ast_span_text_equal_ci(struct mylite_sql_source_span left,
                                   struct mylite_sql_source_span right);
static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan);
static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan);
static int append_select_expression_output(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_sql_ast_node *alias,
                                           struct mylite_select_plan *plan);
static int collect_select_aggregate_bindings(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan);
static int bind_select_count_distinct_arguments(mylite_db *database,
                                                const struct mylite_sql_ast_node *arguments,
                                                struct mylite_select_plan *plan);
static int infer_count_distinct_argument_descriptors(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments, struct mylite_select_aggregate_binding *binding);
static int resolve_select_order_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_order_key_kind *out_kind,
                                          size_t *out_index);
static int resolve_select_group_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_group_key_kind *out_kind,
                                          size_t *out_index);
static int maybe_resolve_select_group_table_reference(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      char **parts, size_t part_count,
                                                      enum mylite_select_group_key_kind *out_kind,
                                                      size_t *out_index, bool *out_resolved);
static int maybe_resolve_select_group_output_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_group_key_kind *out_kind,
                                                       size_t *out_index, bool *out_resolved);
static int resolve_select_having_reference(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           enum mylite_select_order_key_kind *out_kind,
                                           size_t *out_index);
static int resolve_select_having_reference_internal(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    enum mylite_select_order_key_kind *out_kind,
                                                    size_t *out_index, bool emit_warnings);
static int maybe_resolve_select_having_table_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_order_key_kind *out_kind,
                                                       size_t *out_index, bool emit_warnings,
                                                       bool *out_resolved);
static int maybe_resolve_select_having_output_reference(mylite_db *database,
                                                        const struct mylite_select_plan *plan,
                                                        char **parts, size_t part_count,
                                                        enum mylite_select_order_key_kind *out_kind,
                                                        size_t *out_index, bool *out_resolved);
static size_t select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index);
static size_t select_output_label_span_count(const struct mylite_select_plan *plan,
                                             struct mylite_sql_source_span label,
                                             size_t *out_index);
static bool parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value);
static char *copy_select_alias(const struct mylite_sql_ast_node *alias);
static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier);
static int set_select_ambiguous_order_column_error(mylite_db *database, const char *column_name);
static int set_select_unknown_group_column_error(mylite_db *database, const char *column_name);
static int set_select_ambiguous_group_column_warning(mylite_db *database, const char *column_name,
                                                     const char *clause_context);
static int set_select_invalid_group_function_error(mylite_db *database);
static int set_select_duplicate_mode_error(mylite_db *database);
static int set_select_only_full_group_by_error(mylite_db *database, const char *expression_text,
                                               bool implicit_group);
static int set_select_distinct_order_column_error(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_column_error_context context);
static int set_select_unsupported_projection_error(mylite_db *database);
static int set_select_unsupported_where_error(mylite_db *database);
static int set_select_unsupported_order_error(mylite_db *database);
static int set_select_unsupported_join_grouping_error(mylite_db *database);
static int set_union_column_count_error(mylite_db *database);
static int set_union_global_order_table_error(mylite_db *database, const char *table_name);
static int clone_table_select_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *where_clause,
                                          const char *sql, size_t sql_length);
static int clone_table_select_join_expressions(mylite_stmt *stmt, const char *sql,
                                               size_t sql_length);
static int clone_table_select_output_expressions(mylite_stmt *stmt, const char *sql,
                                                 size_t sql_length);
static int clone_table_select_group_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length);
static int clone_table_select_having_expression(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length);
static int clone_table_select_order_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length);
static int collect_table_select_aggregate_bindings(mylite_stmt *stmt);
static int collect_table_select_expression_aggregate_bindings(mylite_stmt *stmt);
static int collect_table_select_order_aggregate_bindings(mylite_stmt *stmt);
static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node);
static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt);
static int execute_union_query_statement(mylite_stmt *stmt);
static int materialize_union_query_result(mylite_stmt *stmt);
static int scan_union_operand(mylite_stmt *stmt, mylite_stmt *operand,
                              enum mylite_sql_ast_set_duplicate_mode duplicate_mode);
static int append_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand, bool distinct);
static int execute_union_operand_statement(mylite_stmt *operand);
static int copy_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand,
                                          struct mylite_table_select_row *out_row);
static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row);
static int deduplicate_union_result_rows(mylite_stmt *stmt);
static int evaluate_union_order_values(mylite_stmt *stmt, struct mylite_table_select_row *row);
static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value);
static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value);
static int append_and_clear_union_database_warnings(mylite_db *database,
                                                    struct mylite_expression_warnings *warnings);
static int execute_scalar_select_statement(mylite_stmt *stmt);
static int evaluate_scalar_select_result(mylite_stmt *stmt);
static int evaluate_scalar_select_result_item(mylite_stmt *stmt, size_t index);
static int evaluate_scalar_select_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int evaluate_dml_materialize_session_function(
    void *user_data, const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int set_dml_materialize_where_predicate_eval_error(void *user_data);
static int evaluate_statement_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value);
static int evaluate_strcmp_function(mylite_stmt *stmt,
                                    const struct mylite_sql_ast_node *function_call,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    const struct mylite_select_table *table,
                                    struct mylite_expression_value *out_value);
static int infer_strcmp_collation_info(mylite_stmt *stmt,
                                       const struct mylite_sql_ast_node *function_call,
                                       const struct mylite_select_table *table,
                                       struct mylite_charset_collation_info *out_info);
static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value);
static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length);
static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative);
static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length);
static bool decimal_literal_span_is_zero(const char *text, size_t length);
static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length, struct mylite_strcmp_compare_options options);
static void trim_strcmp_trailing_spaces(const char *text, size_t *length);
static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options);
static struct mylite_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info);
static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name);
static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info);
static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value);
static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value);
static int infer_expression_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, struct mylite_charset_collation_info *out_info);
static int infer_literal_collation_info(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_charset_collation_info *out_info);
static int infer_identifier_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, struct mylite_charset_collation_info *out_info);
static int infer_function_collation_info(mylite_db *database,
                                         const struct mylite_expression_collation_context *context,
                                         const struct mylite_sql_ast_node *expression,
                                         struct mylite_charset_collation_info *out_info);
static int infer_char_function_collation_info(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_charset_collation_info *out_info);
static int infer_quote_function_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, struct mylite_charset_collation_info *out_info);
static int infer_function_arguments_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, size_t first_argument, bool numeric_as_connection,
    struct mylite_charset_collation_info *out_info);
static int infer_cast_collation_info(mylite_db *database,
                                     const struct mylite_sql_ast_node *expression,
                                     struct mylite_charset_collation_info *out_info);
static int
infer_descriptor_collation_info(mylite_db *database,
                                const struct mylite_expression_collation_context *context,
                                const struct mylite_sql_ast_node *expression, int text_coercibility,
                                struct mylite_charset_collation_info *out_info);
static int infer_table_identifier_descriptor(mylite_db *database,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_field_descriptor *out_descriptor);
static struct mylite_charset_collation_info binary_collation_info(int coercibility);
static struct mylite_charset_collation_info connection_collation_info(const mylite_db *database,
                                                                      int coercibility);
static struct mylite_charset_collation_info latin1_swedish_collation_info(int coercibility);
static struct mylite_charset_collation_info utf8mb3_general_collation_info(int coercibility);
static struct mylite_charset_collation_info char_function_collation_info(const char *charset_name);
static struct mylite_charset_collation_info
descriptor_collation_info(const struct mylite_field_descriptor *descriptor, int text_coercibility);
static int execute_table_select_statement(mylite_stmt *stmt);
static int materialize_table_select_result(mylite_stmt *stmt);
static int materialize_ordered_table_select_result(mylite_stmt *stmt);
static int materialize_unordered_table_select_result(mylite_stmt *stmt);
static int
append_unordered_table_select_matched_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state,
                                          bool distinct);
static int append_unordered_table_select_distinct_row(mylite_stmt *stmt,
                                                      struct mylite_table_select_row *row);
static int
append_unordered_table_select_limited_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state);
static int materialize_joined_table_select_result(mylite_stmt *stmt);
static int materialize_outer_joined_table_select_result(mylite_stmt *stmt);
static int scan_joined_table_select_rows(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         struct mylite_table_select_row *row);
static int
scan_outer_joined_table_select_rows(mylite_stmt *stmt,
                                    struct mylite_table_select_join_materialize_state *state);
static int materialize_select_from_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset);
static int materialize_select_base_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset);
static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, struct mylite_table_select_table_rowset *rowset);
static int append_select_join_step_matches(mylite_stmt *stmt,
                                           struct mylite_table_select_join_materialize_state *state,
                                           const struct mylite_select_join_step *step,
                                           const struct mylite_table_select_table_rowset *left,
                                           bool *right_matched,
                                           struct mylite_table_select_table_rowset *out_rowset);
static int append_select_join_step_left_matches(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset);
static int append_select_join_step_match(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         const struct mylite_select_join_step *step,
                                         const struct mylite_select_join_row_pair *rows,
                                         bool *out_matches,
                                         struct mylite_table_select_table_rowset *out_rowset);
static int
append_select_null_extended_left_row(mylite_stmt *stmt,
                                     const struct mylite_table_select_row *left_row,
                                     struct mylite_table_select_table_rowset *out_rowset);
static int append_select_null_extended_right_rows(
    mylite_stmt *stmt, const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right, const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset);
static int
append_select_null_extended_right_row(mylite_stmt *stmt, const struct mylite_select_join_step *step,
                                      const struct mylite_table_select_row *right_row,
                                      size_t right_row_index,
                                      struct mylite_table_select_table_rowset *out_rowset);
static int append_empty_joined_table_select_row(mylite_stmt *stmt,
                                                struct mylite_table_select_table_rowset *rowset,
                                                struct mylite_table_select_row **out_row);
static int copy_select_base_table_row_values(mylite_db *database,
                                             struct mylite_table_select_row *row,
                                             const struct mylite_select_table *table,
                                             size_t table_index,
                                             const struct mylite_table_select_row *source,
                                             size_t source_row_index);
static int copy_select_row_range_values(struct mylite_table_select_row *target,
                                        const struct mylite_table_select_row *source,
                                        struct mylite_select_table_range range,
                                        const struct mylite_select_plan *plan);
static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, size_t range_count);
static int
process_outer_joined_table_range_row(mylite_stmt *stmt,
                                     struct mylite_table_select_join_materialize_state *state,
                                     const struct mylite_table_select_table_rowset *range_rowsets,
                                     const size_t *row_indexes, size_t range_count);
static int evaluate_table_select_join_step_conditions(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row, const struct mylite_select_join_step *step,
    bool *out_matches);
static int advance_joined_table_select_scan(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, bool *out_finished);
static int backtrack_joined_table_select_scan(mylite_stmt *stmt,
                                              struct mylite_table_select_join_scan_state *scan,
                                              bool *out_finished);
static int process_joined_table_select_scan_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, const struct mylite_select_table *table,
    const struct mylite_table_select_table_rowset *rowset);
static void clear_joined_table_select_scan_frame(struct mylite_table_select_join_scan_state *scan,
                                                 const struct mylite_select_table *table,
                                                 size_t table_index);
static void
clear_joined_table_select_scan_copies(mylite_stmt *stmt,
                                      const struct mylite_table_select_join_scan_state *scan);
static int copy_joined_table_select_row_values(struct mylite_table_select_row *row,
                                               const struct mylite_select_table *table,
                                               const struct mylite_table_select_row *source);
static void clear_joined_table_select_row_values(struct mylite_table_select_row *row,
                                                 const struct mylite_select_table *table);
static int evaluate_table_select_join_stage_conditions(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_join_scan_state *scan, bool *out_matches);
static int evaluate_table_select_join_stage_conditions_uncached(
    mylite_stmt *stmt, const struct mylite_table_select_row *row, size_t available_table_count,
    bool *out_matches);
static int evaluate_table_select_using_stage_conditions(mylite_stmt *stmt,
                                                        const struct mylite_table_select_row *row,
                                                        size_t available_table_count,
                                                        bool *out_matches);
static int evaluate_table_select_join_stage_predicates(mylite_stmt *stmt,
                                                       const struct mylite_table_select_row *row,
                                                       size_t available_table_count,
                                                       bool *out_matches);
static int evaluate_table_select_using_column(mylite_stmt *stmt,
                                              const struct mylite_table_select_row *row,
                                              const struct mylite_select_join_using_column *column,
                                              bool *out_matches);
static int
process_joined_table_select_full_row(mylite_stmt *stmt,
                                     struct mylite_table_select_join_materialize_state *state,
                                     const struct mylite_table_select_row *row);
static int process_joined_table_select_nonaggregate_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row);
static int materialize_aggregate_table_select_result(mylite_stmt *stmt);
static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count);
static int append_empty_implicit_table_select_group(mylite_stmt *stmt,
                                                    struct mylite_table_select_group **groups,
                                                    size_t *group_count);
static int append_finalized_table_select_groups(mylite_stmt *stmt,
                                                struct mylite_table_select_group *groups,
                                                size_t group_count);
static int append_finalized_table_select_group(mylite_stmt *stmt,
                                               struct mylite_table_select_group *group);
static int evaluate_table_select_row_matches(mylite_stmt *stmt,
                                             const struct mylite_table_select_row *row,
                                             bool *out_matches);
static int check_table_select_distinct_duplicate(mylite_stmt *stmt,
                                                 struct mylite_table_select_row *row,
                                                 bool *out_duplicate);
static int append_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group **groups,
                                     size_t *group_count, const struct mylite_table_select_row *row,
                                     struct mylite_table_select_group **out_group);
static int find_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *groups,
                                   size_t group_count, const struct mylite_table_select_row *row,
                                   struct mylite_table_select_group **out_group);
static int initialize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                         const struct mylite_table_select_row *row);
static int update_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                     const struct mylite_table_select_row *row);
static int finalize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                       struct mylite_table_select_row *out_row);
static int evaluate_table_select_join_conditions(mylite_stmt *stmt,
                                                 const struct mylite_table_select_row *row,
                                                 bool *out_matches);
static int evaluate_table_select_using_conditions(mylite_stmt *stmt,
                                                  const struct mylite_table_select_row *row,
                                                  bool *out_matches);
static int evaluate_table_select_join_predicates(mylite_stmt *stmt,
                                                 const struct mylite_table_select_row *row,
                                                 bool *out_matches);
static int set_where_predicate_eval_error(mylite_stmt *stmt);
static void table_select_group_deinit(struct mylite_table_select_group *group);
static int copy_scalar_select_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt);
static int append_scalar_select_warnings_to_database(mylite_stmt *stmt);
static int evaluate_scalar_select_expression(mylite_stmt *stmt,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_expression_value *out_value);
static int evaluate_scalar_aggregate_expression(mylite_stmt *stmt,
                                                const struct mylite_sql_ast_node *expression,
                                                struct mylite_expression_value *out_value);
static int
evaluate_scalar_count_distinct_expression(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *arguments,
                                          const struct mylite_expression_eval_context *context,
                                          struct mylite_expression_value *out_value);
static int evaluate_scalar_numeric_aggregate_expression(
    mylite_stmt *stmt, enum mylite_sql_ast_aggregate_kind aggregate_kind,
    const struct mylite_expression_value *argument, struct mylite_expression_value *out_value);
static int evaluate_scalar_select_subquery_expression(void *user_data,
                                                      const struct mylite_sql_ast_node *subquery,
                                                      struct mylite_expression_warnings *warnings,
                                                      struct mylite_expression_value *out_value);
static int evaluate_scalar_select_in_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
static int evaluate_scalar_select_quantified_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
static int evaluate_scalar_select_row_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int evaluate_select_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value);
static int evaluate_in_subquery_expression(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value);
static int evaluate_in_subquery_expression_inner(mylite_stmt *stmt,
                                                 const struct mylite_sql_ast_node *expression,
                                                 const struct mylite_expression_value *left,
                                                 struct mylite_expression_warnings *warnings,
                                                 struct mylite_expression_value *out_value);
static int evaluate_quantified_subquery_expression(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value);
static int
evaluate_row_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int evaluate_quantified_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value);
static int evaluate_row_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int evaluate_row_in_subquery_statement(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              mylite_stmt *subquery_stmt,
                                              const struct mylite_row_expression_values *left,
                                              struct mylite_expression_warnings *warnings,
                                              struct mylite_expression_value *out_value);
static int evaluate_row_scalar_subquery_statement(mylite_stmt *stmt,
                                                  const struct mylite_sql_ast_node *expression,
                                                  mylite_stmt *subquery_stmt,
                                                  const struct mylite_row_expression_values *left,
                                                  struct mylite_expression_value *out_value);
static int prepare_in_subquery_statement(mylite_stmt *stmt,
                                         const struct mylite_sql_ast_node *expression,
                                         mylite_stmt **out_subquery_stmt,
                                         size_t *out_order_key_count, bool *out_restore_order_keys);
static int prepare_row_subquery_statement(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *expression,
                                          size_t expected_width, mylite_stmt **out_subquery_stmt,
                                          size_t *out_order_key_count,
                                          bool *out_restore_order_keys);
static int prepare_quantified_subquery_statement(mylite_stmt *stmt,
                                                 const struct mylite_sql_ast_node *expression,
                                                 mylite_stmt **out_subquery_stmt,
                                                 size_t *out_order_key_count,
                                                 bool *out_restore_order_keys);
static int scan_in_subquery_statement(const struct mylite_in_subquery_scan_context *context,
                                      struct mylite_in_subquery_scan_state *state);
static int scan_in_subquery_statement_row(const struct mylite_in_subquery_scan_context *context,
                                          struct mylite_in_subquery_scan_state *state);
static int scan_row_in_subquery_statement(const struct mylite_row_in_subquery_scan_context *context,
                                          struct mylite_row_in_subquery_scan_state *state);
static int
scan_row_in_subquery_statement_row(const struct mylite_row_in_subquery_scan_context *context,
                                   struct mylite_row_in_subquery_scan_state *state);
static bool row_expression_values_has_null(const struct mylite_row_expression_values *values);
static int
scan_quantified_subquery_statement(const struct mylite_quantified_subquery_scan_context *context,
                                   struct mylite_quantified_subquery_scan_state *state);
static int scan_quantified_subquery_statement_row(
    const struct mylite_quantified_subquery_scan_context *context,
    struct mylite_quantified_subquery_scan_state *state);
static int finish_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                         const struct mylite_expression_value *left, bool has_row,
                                         bool matched, bool saw_unknown,
                                         struct mylite_expression_value *out_value);
static int finish_row_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                             bool has_row, bool matched, bool saw_unknown,
                                             struct mylite_expression_value *out_value);
static int
finish_quantified_subquery_expression(enum mylite_sql_ast_subquery_quantifier quantifier,
                                      const struct mylite_quantified_subquery_scan_state *scan,
                                      struct mylite_expression_value *out_value);
static bool
quantified_comparison_result(const struct mylite_quantified_subquery_scan_context *context,
                             int comparison);
static bool quantified_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind);
static int
evaluate_row_constructor_values(const struct mylite_sql_ast_node *row,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_row_expression_values *out_values);
static int copy_subquery_statement_row_values(mylite_stmt *stmt, size_t width,
                                              struct mylite_row_expression_values *out_values);
static int copy_subquery_statement_row_value(mylite_stmt *stmt, size_t index,
                                             struct mylite_expression_value *out_value);
static int compare_row_values(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_row_expression_values *left,
                              const struct mylite_row_expression_values *right,
                              struct mylite_expression_warnings *warnings, int *out_truth);
static int compare_row_values_for_equality(const struct mylite_row_expression_values *left,
                                           const struct mylite_row_expression_values *right,
                                           struct mylite_expression_warnings *warnings,
                                           int *out_truth);
static int
compare_row_values_for_null_safe_equality(const struct mylite_row_expression_values *left,
                                          const struct mylite_row_expression_values *right,
                                          struct mylite_expression_warnings *warnings,
                                          int *out_truth);
static int compare_row_values_for_order(enum mylite_sql_ast_operator operator_kind,
                                        const struct mylite_row_expression_values *left,
                                        const struct mylite_row_expression_values *right,
                                        struct mylite_expression_warnings *warnings,
                                        int *out_truth);
static int row_order_comparison_truth(struct mylite_row_order_comparison comparison,
                                      int *out_truth);
static void row_expression_values_deinit(struct mylite_row_expression_values *values);
static int evaluate_scalar_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_value *out_value);
static int evaluate_exists_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_value *out_value);
static int subquery_statement_has_row(mylite_stmt *stmt, bool *out_has_row);
static int copy_subquery_statement_column_value(mylite_stmt *stmt,
                                                struct mylite_expression_value *out_value);
static int validate_scalar_subquery_select_list(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement);
static int validate_in_subquery_select(mylite_db *database,
                                       const struct mylite_sql_ast_node *statement);
static int validate_in_subquery_prepared_columns(mylite_db *database, const mylite_stmt *stmt);
static int validate_row_subquery_select_columns(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                size_t expected_width);
static int validate_row_subquery_prepared_columns(mylite_db *database, const mylite_stmt *stmt,
                                                  size_t expected_width);
static int append_subquery_warnings(struct mylite_expression_warnings *destination,
                                    const struct mylite_expression_warnings *source);
static int set_subquery_operand_columns_error(mylite_db *database);
static int set_subquery_operand_column_count_error(mylite_db *database, size_t expected_width);
static int set_in_subquery_limit_error(mylite_db *database);
static int set_row_quantified_non_alias_error(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression);
static int set_scalar_subquery_cardinality_error(mylite_db *database);
static bool binary_expression_is_in_subquery(const struct mylite_sql_ast_node *expression);

static const struct mylite_select_eval_callbacks table_select_eval_callbacks = {
    .resolve_order_reference = resolve_select_order_reference,
    .resolve_having_reference = resolve_select_having_reference_internal,
    .eval_session_function = evaluate_statement_session_function,
    .eval_subquery = evaluate_select_subquery_expression,
    .eval_in_subquery = evaluate_in_subquery_expression,
    .eval_quantified_subquery = evaluate_quantified_subquery_expression,
    .eval_row_subquery = evaluate_row_subquery_expression,
    .copy_column_value = mylite_select_copy_current_sqlite_column_value,
    .set_expression_eval_error = set_where_predicate_eval_error,
};

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    struct mylite_sql_parse_result parse_result;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    const struct mylite_sql_ast_node *statement = NULL;
    int status = MYLITE_OK;

    if (out_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;

    if (database == NULL || sql == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_diagnostics_clear_error_message(database);
    if (database->transaction_released) {
        return mylite_connection_set_released_error(database);
    }
    parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = length,
            .modes = 0U,
        },
        &parse_result);
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        mylite_diagnostics_clear_warnings(database);
        status = mylite_statement_map_parse_status(database, parse_status);
        if (status != MYLITE_NOMEM) {
            (void)mylite_diagnostics_append_current_error_condition(database,
                                                                    MYLITE_MYSQL_ER_PARSE_ERROR);
        }
        mylite_sql_parse_result_deinit(&parse_result);
        return status;
    }

    statement = mylite_ast_single_statement(parse_result.root);
    if (!mylite_statement_ast_preserves_diagnostics(statement)) {
        mylite_diagnostics_clear_warnings(database);
    }

    status = prepare_parsed_statement(database, parse_result.root, sql, length, out_stmt);
    if (status != MYLITE_OK && status != MYLITE_NOMEM) {
        (void)mylite_diagnostics_ensure_current_error_condition(database,
                                                                MYLITE_MYSQL_ER_UNKNOWN_ERROR);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    return status;
}

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt)
{
    struct mylite_sqlite_translate_result translate_result;
    enum mylite_sqlite_translate_status translate_status = MYLITE_SQLITE_TRANSLATE_OK;
    const struct mylite_sql_ast_node *statement = mylite_ast_single_statement(root);
    int status = MYLITE_OK;

    if (statement != NULL) {
        switch (statement->kind) {
        case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_USE_STATEMENT:
            return prepare_schema_lifecycle_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
            return mylite_connection_prepare_charset_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
            return prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
            return prepare_drop_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
            return prepare_rename_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
            return prepare_truncate_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
            return prepare_alter_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
            return prepare_create_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
            return prepare_drop_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
            return prepare_insert_values_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
            return prepare_insert_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
            return prepare_replace_values_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
            return prepare_replace_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_UPDATE_STATEMENT:
            return mylite_dml_prepare_update_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_DELETE_STATEMENT:
            return mylite_dml_prepare_delete_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_COMMIT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
            return prepare_transaction_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return mylite_show_prepare_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
            return mylite_show_prepare_variables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
            return mylite_show_prepare_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
            return mylite_show_prepare_engines_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
            return mylite_show_prepare_character_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
            return mylite_show_prepare_collation_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
            return mylite_show_prepare_tables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
            return mylite_show_prepare_table_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
            return mylite_show_prepare_columns_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
            return mylite_show_prepare_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
            return mylite_show_prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
            return mylite_show_prepare_create_schema_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
            return mylite_show_prepare_diagnostics_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
            return mylite_show_prepare_diagnostics_count_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
            return mylite_show_prepare_describe_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_QUERY_EXPRESSION:
            return prepare_union_query_expression_statement(database, statement, sql, sql_length,
                                                            out_stmt);
        case MYLITE_SQL_AST_SELECT_STATEMENT:
            status = validate_select_duplicate_mode(database, statement);
            if (status != MYLITE_OK) {
                return status;
            }
            status =
                mylite_information_schema_prepare_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED) {
                return status;
            }
            status = prepare_table_select_statement(database, statement, sql, sql_length, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            status = prepare_scalar_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            break;
        case MYLITE_SQL_AST_SCRIPT:
        case MYLITE_SQL_AST_SELECT_LIST:
        case MYLITE_SQL_AST_SELECT_ITEM:
        case MYLITE_SQL_AST_FROM_DUAL:
        case MYLITE_SQL_AST_FROM_TABLE:
        case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
        case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
        case MYLITE_SQL_AST_JOIN_EXPRESSION:
        case MYLITE_SQL_AST_JOIN_CONDITION:
        case MYLITE_SQL_AST_USING_COLUMN_LIST:
        case MYLITE_SQL_AST_USING_COLUMN:
        case MYLITE_SQL_AST_IDENTIFIER:
        case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        case MYLITE_SQL_AST_WILDCARD:
        case MYLITE_SQL_AST_LITERAL:
        case MYLITE_SQL_AST_UNARY_EXPRESSION:
        case MYLITE_SQL_AST_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        case MYLITE_SQL_AST_EXPRESSION_LIST:
        case MYLITE_SQL_AST_FUNCTION_CALL:
        case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        case MYLITE_SQL_AST_CASE_EXPRESSION:
        case MYLITE_SQL_AST_CASE_WHEN_LIST:
        case MYLITE_SQL_AST_CASE_WHEN:
        case MYLITE_SQL_AST_CAST_EXPRESSION:
        case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
        case MYLITE_SQL_AST_GROUP_ITEM_LIST:
        case MYLITE_SQL_AST_GROUP_ITEM:
        case MYLITE_SQL_AST_HAVING_CLAUSE:
        case MYLITE_SQL_AST_AGGREGATE_CALL:
        case MYLITE_SQL_AST_WHERE_CLAUSE:
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        case MYLITE_SQL_AST_ORDER_ITEM_LIST:
        case MYLITE_SQL_AST_ORDER_ITEM:
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_LIMIT_BOUND:
        case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        case MYLITE_SQL_AST_IF_EXISTS:
        case MYLITE_SQL_AST_IF_NOT_EXISTS:
        case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        case MYLITE_SQL_AST_SCHEMA_OPTION:
        case MYLITE_SQL_AST_DEFAULT:
        case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        case MYLITE_SQL_AST_COLUMN_DEFINITION:
        case MYLITE_SQL_AST_COLUMN_TYPE:
        case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
        case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
        case MYLITE_SQL_AST_KEY_PART_LIST:
        case MYLITE_SQL_AST_KEY_PART:
        case MYLITE_SQL_AST_INDEX_TYPE:
        case MYLITE_SQL_AST_INDEX_OPTION_LIST:
        case MYLITE_SQL_AST_INDEX_OPTION:
        case MYLITE_SQL_AST_SECONDARY_INDEX:
        case MYLITE_SQL_AST_UNIQUE_INDEX:
        case MYLITE_SQL_AST_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_TABLE_OPTION:
        case MYLITE_SQL_AST_TABLE_NAME_LIST:
        case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
        case MYLITE_SQL_AST_INSERT_ROW:
        case MYLITE_SQL_AST_INSERT_ROW_LIST:
        case MYLITE_SQL_AST_INSERT_VALUE_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_TARGET:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_DELETE_TARGET:
        case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
        case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        case MYLITE_SQL_AST_UNION_EXPRESSION:
        case MYLITE_SQL_AST_QUERY_PRIMARY:
        case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
        case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION:
        case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
        case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
        case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
            break;
        }
    }

    translate_status = mylite_sqlite_translate(root, &translate_result);
    if (translate_status != MYLITE_SQLITE_TRANSLATE_OK) {
        return mylite_statement_map_translate_status(database, translate_status);
    }

    status = mylite_statement_prepare_sqlite(database, translate_result.sql, out_stmt);
    mylite_sqlite_translate_result_deinit(&translate_result);
    return status;
}

static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_CREATE_SCHEMA;
        break;
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_ALTER_SCHEMA;
        break;
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_DROP_SCHEMA;
        break;
    case MYLITE_SQL_AST_USE_STATEMENT:
        kind = MYLITE_STMT_USE_SCHEMA;
        break;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_CALL:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_CREATE_TABLE, statement, out_stmt);
}

static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_DROP_TABLE, statement, out_stmt);
}

static int prepare_rename_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_RENAME_TABLE, statement, out_stmt);
}

static int prepare_truncate_table_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_TRUNCATE_TABLE, statement, out_stmt);
}

static int prepare_alter_table_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_ALTER_TABLE, statement, out_stmt);
}

static int prepare_create_index_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_CREATE_INDEX, statement, out_stmt);
}

static int prepare_drop_index_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_DROP_INDEX, statement, out_stmt);
}

static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_VALUES, statement, out_stmt);
}

static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_SET, statement, out_stmt);
}

static int prepare_replace_values_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_REPLACE_VALUES, statement, out_stmt);
}

static int prepare_replace_set_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_REPLACE_SET, statement, out_stmt);
}

static int prepare_transaction_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        kind = MYLITE_STMT_START_TRANSACTION;
        break;
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        kind = MYLITE_STMT_BEGIN_TRANSACTION;
        break;
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        kind = MYLITE_STMT_COMMIT;
        break;
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        kind = MYLITE_STMT_ROLLBACK;
        break;
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_SAVEPOINT;
        break;
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_ROLLBACK_TO_SAVEPOINT;
        break;
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_RELEASE_SAVEPOINT;
        break;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_CALL:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement)
{
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }
    if (statement->select_duplicate_mode_conflict) {
        return set_select_duplicate_mode_error(database);
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    const struct mylite_sql_ast_node *group_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    const struct mylite_sql_ast_node *having_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    const struct mylite_select_clause_nodes clauses = {
        .where = where_clause,
        .group_by = group_by_clause,
        .having = having_clause,
        .order_by = order_by_clause,
        .limit = limit_clause,
    };
    bool custom_runtime = false;
    struct mylite_select_plan plan = {0};
    int status = MYLITE_OK;

    if (from_clause == NULL || (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_TABLE_REFERENCES)) {
        return MYLITE_UNSUPPORTED;
    }

    plan.duplicate_mode = statement->select_duplicate_mode;
    status = mylite_select_bind_from_clause(database, from_clause, &plan);
    if (status == MYLITE_OK) {
        status = bind_select_join_predicates(database, &plan);
    }
    if (status == MYLITE_OK) {
        status = build_select_outputs(database, select_list, true, &plan);
    }
    if (status == MYLITE_OK && mylite_select_plan_table_count(&plan) > 1U &&
        (group_by_clause != NULL || having_clause != NULL)) {
        status = set_select_unsupported_join_grouping_error(database);
    }
    if (status == MYLITE_OK) {
        custom_runtime = mylite_select_plan_requires_custom_runtime(&plan, &clauses);
    }
    if (status == MYLITE_OK) {
        status = bind_table_select_clauses(database, &clauses, &plan);
    }
    if (status == MYLITE_OK && custom_runtime) {
        status = prepare_table_select_custom_statement(database, where_clause, sql, sql_length,
                                                       &plan, out_stmt);
    }
    if (status == MYLITE_OK && !custom_runtime) {
        status = prepare_table_select_sqlite_statement(database, &plan, out_stmt);
    }

    mylite_select_plan_deinit(&plan);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_sqlite_statement(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt)
{
    char *sqlite_sql = mylite_select_build_physical_sql(database, plan);
    int status = MYLITE_OK;

    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        status = attach_select_result_metadata(*out_stmt, plan);
        if (status != MYLITE_OK) {
            mylite_finalize(*out_stmt);
            *out_stmt = NULL;
        }
    }
    sqlite3_free(sqlite_sql);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE) != NULL ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE_REFERENCES) != NULL ||
        (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL &&
         from_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE &&
         from_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE)) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM || mylite_ast_child_at(item, 0U) == NULL) {
            return MYLITE_UNSUPPORTED;
        }
    }
    return prepare_custom_statement(database, MYLITE_STMT_SCALAR_SELECT, statement, out_stmt);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_select_subquery_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             mylite_stmt **out_stmt)
{
    const char *sql = statement == NULL ? NULL : statement->span.text;
    size_t sql_length = statement == NULL ? 0U : statement->span.length;
    int status = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = validate_select_duplicate_mode(database, statement);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_information_schema_prepare_select_statement(database, statement, out_stmt);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = prepare_table_select_statement(database, statement, sql, sql_length, out_stmt);
    if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
        return status;
    }
    return prepare_scalar_select_statement(database, statement, out_stmt);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_union_query_expression_statement(mylite_db *database,
                                                    const struct mylite_sql_ast_node *statement,
                                                    const char *sql, size_t sql_length,
                                                    mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *body = mylite_ast_child_at(statement, 0U);
    mylite_stmt *stmt = NULL;
    int status = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_QUERY_EXPRESSION || body == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_UNION_QUERY,
        .affected_rows = -1,
    };

    status = collect_union_query_operands(database, body, &stmt->union_plan);
    if (status == MYLITE_OK && stmt->union_plan.operand_count < 2U) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = validate_union_operand_column_counts(database, &stmt->union_plan);
    }
    if (status == MYLITE_OK) {
        status = attach_union_result_metadata(stmt);
    }
    if (status == MYLITE_OK) {
        status = initialize_union_output_plan(stmt);
    }
    if (status == MYLITE_OK) {
        status = bind_union_query_clauses(database, statement, sql, sql_length, stmt);
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    stmt->preserve_prepare_warnings = database->warnings.count > 0U;
    *out_stmt = stmt;
    return MYLITE_OK;
}

static int validate_union_operand_column_counts(mylite_db *database,
                                                const struct mylite_union_plan *plan)
{
    int column_count = 0;

    if (plan == NULL || plan->operand_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_count = mylite_column_count(plan->operands[0]);
    if (column_count <= 0) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 1U; index < plan->operand_count; ++index) {
        if (mylite_column_count(plan->operands[index]) != column_count) {
            return set_union_column_count_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_union_query_clauses(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    int status = MYLITE_OK;

    if (limit_clause != NULL) {
        status = mylite_select_bind_limit_clause(limit_clause, &stmt->select_plan);
    }
    if (status == MYLITE_OK && order_by_clause != NULL) {
        status = bind_union_global_order_by_clause(database, order_by_clause, &stmt->select_plan);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        stmt->select_sql_text = mylite_copy_span_text(sql, sql_length);
        if (stmt->select_sql_text == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_order_expressions(stmt, sql, sql_length);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int collect_union_query_operands(mylite_db *database, const struct mylite_sql_ast_node *node,
                                        struct mylite_union_plan *plan)
{
    const struct mylite_sql_ast_node *unwrapped = unwrap_union_query_primary(node);

    if (unwrapped == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNION_EXPRESSION) {
        const struct mylite_sql_ast_node *left = mylite_ast_child_at(unwrapped, 0U);
        const struct mylite_sql_ast_node *right = mylite_ast_child_at(unwrapped, 1U);
        mylite_stmt *right_operand = NULL;
        int status = collect_union_query_operands(database, left, plan);

        if (status != MYLITE_OK) {
            return status;
        }
        status = prepare_union_query_operand(database, right, &right_operand);
        if (status != MYLITE_OK) {
            return status;
        }
        status = append_union_query_operand(database, plan, right_operand,
                                            unwrapped->set_duplicate_mode, true);
        if (status != MYLITE_OK) {
            mylite_finalize(right_operand);
        }
        return status;
    }

    {
        mylite_stmt *operand = NULL;
        int status = prepare_union_query_operand(database, unwrapped, &operand);

        if (status != MYLITE_OK) {
            return status;
        }
        status = append_union_query_operand(database, plan, operand,
                                            MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT, false);
        if (status != MYLITE_OK) {
            mylite_finalize(operand);
        }
        return status;
    }
}

static int append_union_query_operand(mylite_db *database, struct mylite_union_plan *plan,
                                      mylite_stmt *operand,
                                      enum mylite_sql_ast_set_duplicate_mode duplicate_mode,
                                      bool has_operator)
{
    mylite_stmt **operands = NULL;

    if (plan == NULL || operand == NULL || (has_operator && plan->operand_count == 0U)) {
        return MYLITE_UNSUPPORTED;
    }

    operands = (mylite_stmt **)realloc((void *)plan->operands,
                                       (plan->operand_count + 1U) * sizeof(*plan->operands));
    if (operands == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    plan->operands = operands;

    if (has_operator) {
        enum mylite_sql_ast_set_duplicate_mode *operators =
            (enum mylite_sql_ast_set_duplicate_mode *)realloc(
                (void *)plan->operators, plan->operand_count * sizeof(*plan->operators));

        if (operators == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        plan->operators = operators;
        plan->operators[plan->operand_count - 1U] = duplicate_mode;
    }

    plan->operands[plan->operand_count++] = operand;
    return MYLITE_OK;
}

static int prepare_union_query_operand(mylite_db *database, const struct mylite_sql_ast_node *node,
                                       mylite_stmt **out_operand)
{
    const struct mylite_sql_ast_node *operand = unwrap_union_query_primary(node);

    *out_operand = NULL;
    if (operand == NULL || operand->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }
    return prepare_select_subquery_statement(database, operand, out_operand);
}

static const struct mylite_sql_ast_node *
unwrap_union_query_primary(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *current = node;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUERY_PRIMARY) {
        current = mylite_ast_child_at(current, 0U);
    }
    return current;
}

static int attach_union_result_metadata(mylite_stmt *stmt)
{
    const mylite_stmt *first_operand = NULL;
    struct mylite_result_metadata metadata = {0};
    int column_count = 0;

    if (stmt == NULL || stmt->union_plan.operand_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    first_operand = stmt->union_plan.operands[0];
    column_count = mylite_column_count(first_operand);
    if (column_count <= 0) {
        return MYLITE_UNSUPPORTED;
    }

    metadata.columns = calloc((size_t)column_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = (size_t)column_count;

    for (size_t index = 0U; index < metadata.column_count; ++index) {
        const struct mylite_result_column_metadata *source =
            mylite_result_metadata_column(first_operand, (int)index);
        const char *label = mylite_column_name(first_operand, (int)index);
        int status =
            mylite_result_metadata_copy_text(stmt->database, &metadata.columns[index].name, label);

        if (status == MYLITE_OK) {
            metadata.columns[index].descriptor =
                source == NULL ? mylite_expression_descriptor_defaults() : source->descriptor;
        }
        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return aggregate_union_result_metadata(stmt);
}

static int initialize_union_output_plan(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 0U; index < stmt->result_metadata.column_count; ++index) {
        int status = add_union_output_column(stmt->database, &stmt->select_plan,
                                             stmt->result_metadata.columns[index].name);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return stmt->select_plan.output_count == stmt->result_metadata.column_count ? MYLITE_OK
                                                                                : MYLITE_NOMEM;
}

static int add_union_output_column(mylite_db *database, struct mylite_select_plan *plan,
                                   const char *label)
{
    struct mylite_select_output_column output = {
        .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
        .column_index = plan->output_count,
    };

    output.label = label == NULL ? NULL : mylite_copy_span_text(label, strlen(label));
    if (label != NULL && output.label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    {
        int status = mylite_select_plan_add_output_column(plan, &output);

        if (status != MYLITE_OK) {
            mylite_select_output_column_deinit(&output);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
}

static int aggregate_union_result_metadata(mylite_stmt *stmt)
{
    for (size_t operand_index = 1U; operand_index < stmt->union_plan.operand_count;
         ++operand_index) {
        const mylite_stmt *operand = stmt->union_plan.operands[operand_index];

        for (size_t column_index = 0U; column_index < stmt->result_metadata.column_count;
             ++column_index) {
            const struct mylite_result_column_metadata *source =
                mylite_result_metadata_column(operand, (int)column_index);
            struct mylite_field_descriptor descriptor =
                source == NULL ? mylite_expression_descriptor_defaults() : source->descriptor;

            mylite_expression_descriptor_merge_union_operand(
                stmt->database, &stmt->result_metadata.columns[column_index].descriptor,
                &descriptor);
        }
    }
    return MYLITE_OK;
}

static int bind_union_global_order_by_clause(mylite_db *database,
                                             const struct mylite_sql_ast_node *order_by_clause,
                                             struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return set_select_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_union_global_order_item(database, item, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->order_key_count == 0U ? set_select_unsupported_order_error(database) : MYLITE_OK;
}

static int bind_union_global_order_item(mylite_db *database,
                                        const struct mylite_sql_ast_node *order_item,
                                        struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);
    struct mylite_select_order_key order_key = {
        .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return set_select_unsupported_order_error(database);
    }
    if (order_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > plan->output_count) {
            char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = mylite_select_set_unknown_order_column_error(database, reference);
            free(reference);
            return status;
        }
        order_key.kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        order_key.output_index = (size_t)(ordinal - 1U);
        order_key.expression = NULL;
        mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
        return mylite_select_plan_add_order_key(plan, &order_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_union_order_reference(database, plan, expression, &kind, &index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            order_key.kind = kind;
            order_key.output_index = index;
            order_key.expression = NULL;
            mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
            return mylite_select_plan_add_order_key(plan, &order_key);
        }
    }

    {
        int status = bind_union_global_order_expression(database, expression, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_order_key(plan, &order_key);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_union_global_order_expression(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_select_plan *plan)
{
    if (expression == NULL) {
        return set_select_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_union_order_reference(database, plan, expression, &kind, &index);

        if (status == MYLITE_OK && kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            mylite_select_plan_mark_output_order_reference(plan, index);
        }
        return status;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        if (expression->kind == MYLITE_SQL_AST_CAST_EXPRESSION) {
            int status = mylite_expression_validate_cast_target_charset(database, expression);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_union_global_order_expression(database, child, plan);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return bind_union_global_order_function_call(database, expression, plan);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    default:
        return set_select_unsupported_order_error(database);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_union_global_order_function_call(mylite_db *database,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (!mylite_expression_is_supported_function_call(expression)) {
        return set_select_unsupported_order_error(database);
    }
    {
        int status = mylite_expression_validate_char_function_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        int status = bind_union_global_order_expression(database, child, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_union_order_reference(mylite_db *database, const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression,
                                         enum mylite_select_order_key_kind *out_kind,
                                         size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count != 1U) {
        const char *table_name = part_count == 2U ? parts[0] : parts[1];

        status = set_union_global_order_table_error(database, table_name);
        goto cleanup;
    }

    {
        size_t output_index = 0U;
        size_t output_matches = select_output_label_count(plan, parts[0], &output_index);

        if (output_matches > 1U) {
            status = set_select_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 1U) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
            *out_index = output_index;
            goto cleanup;
        }
    }

    status = mylite_select_set_unknown_order_column_error(database, parts[0]);

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int attach_select_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan)
{
    struct mylite_result_metadata metadata = {0};

    if (plan->output_count == 0U) {
        return MYLITE_OK;
    }

    metadata.columns = calloc(plan->output_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = plan->output_count;

    for (size_t index = 0U; index < plan->output_count; ++index) {
        int status = copy_select_result_column_metadata(stmt->database, &metadata.columns[index],
                                                        plan, index);

        if (status != MYLITE_OK) {
            mylite_result_metadata_deinit(&metadata);
            return status;
        }
    }

    mylite_result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int copy_select_result_column_metadata(mylite_db *database,
                                              struct mylite_result_column_metadata *metadata,
                                              const struct mylite_select_plan *plan,
                                              size_t output_index)
{
    const struct mylite_select_output_column *output = &plan->outputs[output_index];
    int status = mylite_result_metadata_copy_text(database, &metadata->name, output->label);

    if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
        if (status == MYLITE_OK) {
            status = infer_select_expression_descriptor(database, plan, output->expression,
                                                        &metadata->descriptor);
        }
        return status;
    }

    const struct mylite_select_table *table = NULL;
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, output->column_index, &table);
    const char *visible_table_name;

    if (column == NULL || table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    visible_table_name = table->alias == NULL ? table->table_name : table->alias;
    metadata->descriptor = column->descriptor;
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->schema_name, table->schema_name);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->table_name, visible_table_name);
    }
    if (status == MYLITE_OK) {
        status = mylite_result_metadata_copy_text(database, &metadata->origin_schema_name,
                                                  table->schema_name);
    }
    if (status == MYLITE_OK) {
        status = mylite_result_metadata_copy_text(database, &metadata->origin_table_name,
                                                  table->table_name);
    }
    if (status == MYLITE_OK) {
        status =
            mylite_result_metadata_copy_text(database, &metadata->origin_column_name, column->name);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_select_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return infer_expression_descriptor(database, plan, expression, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_scalar_expression_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return infer_expression_descriptor(database, NULL, expression, value, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_expression_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *node = expression;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = mylite_ast_child_at(node, 0U);
    }
    if (node == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return infer_literal_descriptor(database, node, value, out_descriptor);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return infer_identifier_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        break;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return infer_unary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return infer_binary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return infer_ternary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return infer_case_expression_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return infer_function_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return infer_aggregate_expression_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return infer_cast_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP: {
        unsigned int fsp = 0U;

        if (node->has_column_precision) {
            fsp = (unsigned int)node->column_precision;
        }
        *out_descriptor = current_datetime_function_descriptor(fsp);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        return infer_scalar_subquery_expression_descriptor(database, node, out_descriptor);
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return infer_quantified_subquery_expression_descriptor(database, plan, node,
                                                               out_descriptor);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return MYLITE_UNSUPPORTED;
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

static int infer_identifier_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor)
{
    size_t column_index = plan == NULL ? 0U : mylite_select_plan_column_count(plan);
    int status = plan == NULL ? MYLITE_UNSUPPORTED
                              : mylite_select_resolve_plan_column_reference(
                                    database, plan, expression, "field list", &column_index);

    if (status != MYLITE_OK || column_index >= mylite_select_plan_column_count(plan)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status == MYLITE_OK ? MYLITE_UNSUPPORTED : status;
    }

    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, column_index, NULL);

    if (column == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    *out_descriptor = column->descriptor;
    return MYLITE_OK;
}

static int infer_literal_descriptor(mylite_db *database,
                                    const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_value *value,
                                    struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        descriptor.flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        break;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_expression_descriptor_literal_integer_length(expression, value);
        if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
            descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        break;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        descriptor = mylite_expression_descriptor_decimal(false);
        descriptor.decimals = mylite_expression_descriptor_literal_decimal_scale(expression);
        descriptor.length = expression->span.length + 1U;
        break;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        descriptor.type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor.length = mylite_expression_descriptor_string_length(database, value, expression);
        descriptor.decimals = mylite_mysql_not_fixed_decimals;
        descriptor.charset_id = mylite_expression_descriptor_connection_charset_id(database);
        mylite_field_descriptor_set_not_null(&descriptor, true);
        break;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        descriptor = mylite_expression_descriptor_from_value(value);
        break;
    }

    *out_descriptor = descriptor;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_unary_expression_descriptor(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression,
                                             const struct mylite_expression_value *value,
                                             struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor operand = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = MYLITE_OK;

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_boolean(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             value, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        nullable = mylite_expression_descriptor_is_nullable(&operand);
        operand.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
        operand.length = mylite_expression_descriptor_max_u64(operand.length, 2U);
        mylite_field_descriptor_set_nullable(&operand, nullable);
        *out_descriptor = operand;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_binary_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    if (binary_expression_is_row_subquery(expression)) {
        return infer_row_subquery_expression_descriptor(database, plan, expression, out_descriptor);
    }
    if (binary_expression_is_in_subquery(expression)) {
        return infer_in_subquery_expression_descriptor(database, plan, expression, out_descriptor);
    }

    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor right = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &left);

    if (status == MYLITE_OK) {
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 1U),
                                             NULL, &right);
    }
    if (status != MYLITE_OK) {
        return status;
    }

    if (mylite_expression_descriptor_is_nullable(&left) ||
        mylite_expression_descriptor_is_nullable(&right)) {
        nullable = true;
    } else {
        nullable = false;
    }
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length =
            mylite_expression_descriptor_max_u64(left.length, right.length) + 1U;
        if ((left.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U ||
            (right.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_expression_descriptor_max_u64(left.length, right.length);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        *out_descriptor = mylite_expression_descriptor_decimal(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE: {
        bool descriptor_nullable = false;

        if (nullable) {
            bool forces_not_null =
                mylite_expression_descriptor_operator_forces_not_null(expression->operator_kind);

            if (!forces_not_null) {
                descriptor_nullable = true;
            }
        }
        *out_descriptor = mylite_expression_descriptor_boolean(descriptor_nullable);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_ternary_expression_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_expression_value *value,
                                               struct mylite_field_descriptor *out_descriptor)
{
    bool nullable = false;

    (void)value;
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        *out_descriptor = mylite_expression_descriptor_boolean(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_expression_descriptor(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression,
                                                const struct mylite_expression_value *value,
                                                struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool nullable = false;
    bool result_nullable = false;
    bool matched_string_encoding = false;
    bool matched_slice_string = false;
    int status = MYLITE_OK;

    if (!mylite_expression_is_supported_function_call(expression)) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_function_arguments_nullable(database, plan, arguments, &nullable);
    if (status != MYLITE_OK) {
        return status;
    }
    result_nullable = function_result_nullable(nullable, value);

    if (infer_common_scalar_function_descriptor(database, name, nullable, result_nullable,
                                                out_descriptor)) {
        return MYLITE_OK;
    }
    status = infer_temporal_function_descriptor(database, plan, expression, value, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_variadic_scalar_function_descriptor(database, plan, expression, value,
                                                       result_nullable, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_string_encoding_function_descriptor(database, plan, expression, out_descriptor,
                                                       &matched_string_encoding);
    if (status != MYLITE_OK || matched_string_encoding) {
        return status;
    }
    status = infer_slice_string_function_descriptor(database, plan, expression, value, nullable,
                                                    out_descriptor, &matched_slice_string);
    if (status != MYLITE_OK || matched_slice_string) {
        return status;
    }
    if (mylite_function_name_has_text_result(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = text_function_result_length(database, value),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = result_nullable,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }
    if (infer_fixed_integer_function_descriptor(name, result_nullable, out_descriptor)) {
        return MYLITE_OK;
    }
    if (infer_code_search_function_descriptor(name, result_nullable, out_descriptor)) {
        return MYLITE_OK;
    }
    if (infer_list_index_function_descriptor(name, result_nullable, out_descriptor)) {
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "ISNULL")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(false);
        out_descriptor->length = 1U;
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "ABS")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        if (value != NULL && value->kind != MYLITE_EXPRESSION_VALUE_NULL) {
            *out_descriptor = mylite_expression_descriptor_from_value(value);
            mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        }
        return MYLITE_OK;
    }
    if (mylite_function_name_has_integer_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_integer_function_display_length;
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "MOD")) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "PI")) {
        (void)value;
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_pi_function_display_length,
            .decimals = mylite_mysql_pi_function_scale,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_temporal_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    if (infer_current_temporal_function_descriptor(expression, out_descriptor)) {
        return MYLITE_OK;
    }
    if (infer_temporal_part_function_descriptor(expression, out_descriptor)) {
        return MYLITE_OK;
    }
    int status = infer_time_function_descriptor(database, plan, expression, value, out_descriptor);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_date_interval_function_descriptor(database, plan, expression, out_descriptor);
}

static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (infer_session_or_inet_function_descriptor(database, name, out_descriptor)) {
        return true;
    }
    if (infer_strcmp_function_descriptor(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (infer_uuid_function_descriptor(database, name, out_descriptor)) {
        return true;
    }
    if (infer_exp_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_logarithm_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_power_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_sqrt_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_trigonometric_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_inverse_trigonometric_function_descriptor(name, out_descriptor)) {
        return true;
    }
    if (infer_angle_conversion_function_descriptor(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (infer_temporal_scalar_function_descriptor(name, arguments_nullable, out_descriptor)) {
        return true;
    }
    return infer_base_conversion_function_descriptor(database, name, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     const struct mylite_expression_value *value,
                                                     bool result_nullable,
                                                     struct mylite_field_descriptor *out_descriptor)
{
    int status = infer_round_function_descriptor(database, plan, expression, value, result_nullable,
                                                 out_descriptor);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_format_function_descriptor(database, plan, expression, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_truncate_function_descriptor(database, plan, expression, value, result_nullable,
                                                out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_greatest_least_function_descriptor(database, plan, expression, result_nullable,
                                                      out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_char_function_descriptor(database, expression, out_descriptor);
}

static bool infer_exp_function_descriptor(const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_exp(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool infer_logarithm_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_logarithm(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool infer_power_function_descriptor(const struct mylite_sql_ast_node *name,
                                            struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_power(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool infer_sqrt_function_descriptor(const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_sqrt(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool infer_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_trigonometric(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool
infer_inverse_trigonometric_function_descriptor(const struct mylite_sql_ast_node *name,
                                                struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_inverse_trigonometric(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static bool
infer_angle_conversion_function_descriptor(const struct mylite_sql_ast_node *name,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_angle_conversion(name)) {
        return false;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return true;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_round_function_descriptor(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *value,
                                           bool result_nullable,
                                           struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    int scale = 0;
    int status = MYLITE_OK;

    if (name == NULL || !mylite_span_equal_ci(name->span, "ROUND")) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_double_display_length + 1U,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = result_nullable,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            int rounded_scale = round_function_descriptor_scale(scale);

            if (rounded_scale < 0) {
                out_descriptor->decimals = 0U;
                if (out_descriptor->length > value_descriptor.decimals) {
                    out_descriptor->length -= value_descriptor.decimals;
                }
            } else if ((unsigned int)rounded_scale < out_descriptor->decimals) {
                out_descriptor->decimals = (unsigned int)rounded_scale;
            }
        }
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_TINY ||
        value_descriptor.type == MYLITE_FIELD_TYPE_SHORT ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONGLONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_INT24 ||
        value_descriptor.type == MYLITE_FIELD_TYPE_YEAR) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        if ((value_descriptor.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        *out_descriptor = mylite_expression_descriptor_from_value(value);
        if (out_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
            *out_descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length + 1U,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
        }
        return MYLITE_OK;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_format_function_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t character_length = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_format(name)) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    character_length =
        format_function_result_character_length(plan, value_argument, &value_descriptor);
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = character_length * max_bytes_per_character,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t
format_function_result_character_length(const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *argument,
                                        const struct mylite_field_descriptor *argument_descriptor)
{
    uint64_t literal_length = format_literal_result_character_length(argument);

    (void)plan;
    if (literal_length != 0U) {
        return literal_length;
    }
    if (argument_descriptor == NULL || argument_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return mylite_format_null_character_length;
    }
    switch (argument_descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        return argument_descriptor->length + mylite_format_numeric_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
        return mylite_format_float_character_length;
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
        return argument_descriptor->length + mylite_format_string_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return mylite_format_string_descriptor_extra_length;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument)
{
    uint64_t fraction_length = 0U;

    argument = unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        uint64_t length = format_literal_result_character_length(mylite_ast_child_at(argument, 0U));

        return length == 0U ? 0U : length + 1U;
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL) {
        return 0U;
    }
    switch (argument->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        return mylite_format_null_character_length;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return argument->span.length + mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        fraction_length = format_literal_fraction_length(argument);
        return argument->span.length + mylite_format_decimal_literal_extra_length + fraction_length;
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return mylite_format_float_character_length;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        return argument->span.length >= 2U
                   ? argument->span.length - 2U + mylite_format_literal_extra_length
                   : mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        break;
    }
    return 0U;
}

static uint64_t format_literal_fraction_length(const struct mylite_sql_ast_node *argument)
{
    const char *text = argument == NULL ? NULL : argument->span.text;
    const char *dot = text == NULL ? NULL : memchr(text, '.', argument->span.length);

    if (dot == NULL) {
        return 0U;
    }
    return (uint64_t)(argument->span.length - (size_t)(dot - text) - 1U);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_truncate_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              bool result_nullable,
                                              struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *scale_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    int scale = 0;
    int status = MYLITE_OK;

    if (name == NULL || !mylite_span_equal_ci(name->span, "TRUNCATE")) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    if (round_function_argument_is_approximate_literal(value_argument)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_double_display_length + 1U,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = result_nullable,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }

    if (value_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        *out_descriptor = value_descriptor;
        out_descriptor->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if (round_function_constant_scale(scale_argument, &scale)) {
            truncate_decimal_descriptor_for_constant_scale(out_descriptor, &value_descriptor,
                                                           scale);
        }
        mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_TINY ||
        value_descriptor.type == MYLITE_FIELD_TYPE_SHORT ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_LONGLONG ||
        value_descriptor.type == MYLITE_FIELD_TYPE_INT24 ||
        value_descriptor.type == MYLITE_FIELD_TYPE_YEAR) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        if ((value_descriptor.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    }
    if (value_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        *out_descriptor = mylite_expression_descriptor_from_value(value);
        if (out_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
            *out_descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length + 1U,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
        }
        return MYLITE_OK;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_double_display_length + 1U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

static bool
round_function_argument_is_approximate_literal(const struct mylite_sql_ast_node *argument)
{
    argument = unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument = unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    if (argument->literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT) {
        return false;
    }
    return true;
}

static bool round_function_constant_scale(const struct mylite_sql_ast_node *argument,
                                          int *out_scale)
{
    bool negative = false;
    int64_t scale = 0;

    if (out_scale == NULL) {
        return false;
    }
    *out_scale = 0;
    if (argument == NULL) {
        return true;
    }
    while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        argument = mylite_ast_child_at(argument, 0U);
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        argument = mylite_ast_child_at(argument, 0U);
        while (argument != NULL && argument->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
            argument = mylite_ast_child_at(argument, 0U);
        }
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    enum { decimal_base = 10 };

    for (size_t index = 0U; index < argument->span.length; ++index) {
        char character = argument->span.text[index];

        if (!isdigit((unsigned char)character)) {
            return false;
        }
        if (scale < INT64_MAX / decimal_base) {
            scale = (scale * decimal_base) + (int64_t)(character - '0');
        } else {
            scale = INT64_MAX;
        }
    }
    if (negative) {
        scale = -scale;
    }
    if (scale > INT_MAX) {
        *out_scale = INT_MAX;
    } else if (scale < INT_MIN) {
        *out_scale = INT_MIN;
    } else {
        *out_scale = (int)scale;
    }
    return true;
}

static int round_function_descriptor_scale(int scale)
{
    enum { round_scale_limit = 30 };

    if (scale > round_scale_limit) {
        return round_scale_limit;
    }
    if (scale < -round_scale_limit) {
        return -round_scale_limit;
    }
    return scale;
}

static void
truncate_decimal_descriptor_for_constant_scale(struct mylite_field_descriptor *descriptor,
                                               const struct mylite_field_descriptor *source,
                                               int scale)
{
    int truncated_scale = round_function_descriptor_scale(scale);
    uint64_t remove_length = 0U;

    if (descriptor == NULL || source == NULL) {
        return;
    }
    if (truncated_scale < 0 || truncated_scale == 0) {
        descriptor->decimals = 0U;
        remove_length = source->decimals == 0U ? 0U : (uint64_t)source->decimals + 1U;
    } else if ((unsigned int)truncated_scale < descriptor->decimals) {
        remove_length = (uint64_t)(descriptor->decimals - (unsigned int)truncated_scale);
        descriptor->decimals = (unsigned int)truncated_scale;
    }

    if (remove_length != 0U) {
        descriptor->length =
            descriptor->length > remove_length ? descriptor->length - remove_length : 1U;
    }
}

static bool
infer_session_or_inet_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor)
{
    if (infer_session_function_descriptor(database, name, out_descriptor)) {
        return true;
    }
    return infer_inet_function_descriptor(database, name, out_descriptor);
}

static bool infer_strcmp_function_descriptor(const struct mylite_sql_ast_node *name,
                                             bool result_nullable,
                                             struct mylite_field_descriptor *out_descriptor)
{
    if (!mylite_function_name_is_strcmp(name)) {
        return false;
    }

    *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
    out_descriptor->length = 2U;
    return true;
}

static bool infer_inet_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_inet_aton(name)) {
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return true;
    }
    if (mylite_function_name_is_inet_ntoa(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length = max_bytes_per_character > UINT64_MAX / mylite_mysql_inet_ntoa_result_chars
                              ? mylite_mysql_long_text_length
                              : mylite_mysql_inet_ntoa_result_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    return false;
}

static bool infer_uuid_function_descriptor(mylite_db *database,
                                           const struct mylite_sql_ast_node *name,
                                           struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_is_uuid(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = 1U;
        return true;
    }
    if (mylite_function_name_is_uuid_to_bin(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_uuid_binary_result_bytes,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    if (mylite_function_name_is_bin_to_uuid(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length = max_bytes_per_character > UINT64_MAX / mylite_mysql_uuid_text_result_chars
                              ? mylite_mysql_long_text_length
                              : mylite_mysql_uuid_text_result_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(out_descriptor, true);
        return true;
    }
    return false;
}

static bool function_result_nullable(bool arguments_nullable,
                                     const struct mylite_expression_value *value)
{
    if (value != NULL) {
        return value->kind == MYLITE_EXPRESSION_VALUE_NULL;
    }
    return arguments_nullable;
}

static uint64_t text_function_result_length(mylite_db *database,
                                            const struct mylite_expression_value *value)
{
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return mylite_expression_descriptor_string_length(database, value, NULL);
    }
    return mylite_mysql_text_length;
}

static bool infer_fixed_integer_function_descriptor(const struct mylite_sql_ast_node *name,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_has_length_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_length_function_display_length;
        return true;
    }
    if (mylite_function_name_is_bit_count(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_bit_count_function_display_length;
        return true;
    }
    if (mylite_function_name_is_crc32(name)) {
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(result_nullable);
        out_descriptor->length = mylite_mysql_crc32_function_display_length;
        return true;
    }
    return false;
}

static bool infer_session_function_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *name,
                                              struct mylite_field_descriptor *out_descriptor)
{
    if (name == NULL) {
        return false;
    }
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length =
            max_bytes_per_character >
                    UINT64_MAX / mylite_mysql_charset_collation_function_display_chars
                ? mylite_mysql_long_text_length
                : mylite_mysql_charset_collation_function_display_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_collation_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_function_name_is_coercibility(name)) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_coercibility_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "DATABASE") ||
        mylite_span_equal_ci(name->span, "SCHEMA")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = mylite_mysql_schema_function_display_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "VERSION")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL,
            .length = mylite_mysql_version_function_display_length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "USER") ||
        mylite_span_equal_ci(name->span, "SESSION_USER") ||
        mylite_span_equal_ci(name->span, "SYSTEM_USER") ||
        mylite_span_equal_ci(name->span, "CURRENT_USER")) {
        uint64_t max_bytes_per_character =
            mylite_expression_descriptor_connection_character_max_length(database);
        uint64_t length =
            max_bytes_per_character > UINT64_MAX / mylite_mysql_identity_function_display_chars
                ? mylite_mysql_long_text_length
                : mylite_mysql_identity_function_display_chars * max_bytes_per_character;

        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = true,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "LAST_INSERT_ID")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "CONNECTION_ID")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED |
                     MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    if (mylite_span_equal_ci(name->span, "ROW_COUNT")) {
        *out_descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_LONGLONG,
            .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_session_integer_function_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = false,
        };
        return true;
    }
    return false;
}

static bool
infer_current_temporal_function_descriptor(const struct mylite_sql_ast_node *expression,
                                           struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    unsigned int fsp = 0U;

    if (!mylite_temporal_current_function_fsp(expression, &fsp)) {
        return false;
    }
    if (mylite_temporal_function_name_is_current_datetime(name)) {
        *out_descriptor = current_datetime_function_descriptor(fsp);
        return true;
    }
    if (mylite_temporal_function_name_is_current_date(name)) {
        *out_descriptor = current_date_function_descriptor();
        return true;
    }
    if (mylite_temporal_function_name_is_current_time(name)) {
        *out_descriptor = current_time_function_descriptor(fsp);
        return true;
    }
    return false;
}

static struct mylite_field_descriptor current_datetime_function_descriptor(unsigned int fsp)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = fsp == 0U ? mylite_mysql_datetime_display_length
                            : mylite_mysql_datetime_fraction_display_base + fsp,
        .decimals = fsp,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

static struct mylite_field_descriptor current_date_function_descriptor(void)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATE,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_date_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_TIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = fsp == 0U ? mylite_mysql_current_time_display_length
                            : mylite_mysql_current_time_fraction_display_base + fsp,
        .decimals = fsp,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

static bool
infer_temporal_scalar_function_descriptor(const struct mylite_sql_ast_node *name,
                                          bool arguments_nullable,
                                          struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_date_extraction(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_DATE,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_date_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_datediff(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_datediff_function_display_length;
        return true;
    }
    if (mylite_function_name_is_timestampdiff(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return true;
    }
    if (mylite_function_name_is_to_days(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_to_days_function_display_length;
        return true;
    }
    if (mylite_function_name_is_to_seconds(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_to_seconds_function_display_length;
        return true;
    }
    if (mylite_function_name_is_from_days(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_DATE,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_date_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = arguments_nullable,
        };

        mylite_field_descriptor_set_nullable(&descriptor, arguments_nullable);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_year_part(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_YEAR,
            .flags = MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_year_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_month_part(name) || mylite_function_name_is_day_part(name) ||
        mylite_function_name_is_minute_part(name) || mylite_function_name_is_second_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_short_display_length;
        return true;
    }
    if (mylite_function_name_is_hour_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_hour_display_length;
        return true;
    }
    return false;
}

static bool infer_temporal_part_function_descriptor(const struct mylite_sql_ast_node *expression,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (mylite_function_name_is_extract(name)) {
        if (!expression->interval_spec ||
            !extract_interval_unit_supported(expression->interval_unit)) {
            return false;
        }
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_YEAR
                                     ? mylite_mysql_extract_year_display_length
                                     : mylite_mysql_temporal_part_short_display_length;
        if (expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
            out_descriptor->length = mylite_mysql_temporal_part_hour_display_length;
        }
        return true;
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_time_function_descriptor(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          const struct mylite_expression_value *value,
                                          struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_expression_value evaluated_value = {0};
    struct mylite_expression_warnings warnings = {0};
    const struct mylite_expression_value *descriptor_value = value;
    unsigned int decimals = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_time_extraction(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (descriptor_value == NULL && mylite_expression_is_cacheable_no_table(expression) &&
        mylite_expression_eval(expression, &warnings, &evaluated_value) == 0) {
        descriptor_value = &evaluated_value;
    }
    status = infer_expression_descriptor(database, plan, argument, NULL, &argument_descriptor);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (argument_descriptor.type == MYLITE_FIELD_TYPE_TIME ||
        argument_descriptor.type == MYLITE_FIELD_TYPE_DATETIME ||
        argument_descriptor.type == MYLITE_FIELD_TYPE_TIMESTAMP) {
        decimals = argument_descriptor.decimals;
    } else if (argument_descriptor.type == MYLITE_FIELD_TYPE_DATE ||
               argument_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        decimals = 0U;
    } else {
        decimals =
            time_function_argument_decimals(argument, &argument_descriptor, descriptor_value);
    }
    *out_descriptor = time_function_descriptor(decimals);
    status = MYLITE_OK;

cleanup:
    mylite_expression_value_deinit(&evaluated_value);
    mylite_expression_warnings_deinit(&warnings);
    return status;
}

static unsigned int
time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor,
                                const struct mylite_expression_value *value)
{
    if (time_function_argument_is_approximate(argument, descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    if (descriptor != NULL && descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        return descriptor->decimals > mylite_mysql_temporal_max_fsp ? mylite_mysql_temporal_max_fsp
                                                                    : descriptor->decimals;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
            return time_function_value_decimals(value);
        }
        if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return mylite_mysql_temporal_max_fsp;
        }
    }
    return 0U;
}

static bool time_function_argument_is_approximate(const struct mylite_sql_ast_node *argument,
                                                  const struct mylite_field_descriptor *descriptor)
{
    argument = unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument = unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_LITERAL &&
        argument->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return true;
    }
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_FLOAT) {
        return true;
    }
    return descriptor->type == MYLITE_FIELD_TYPE_DOUBLE;
}

static struct mylite_field_descriptor time_function_descriptor(unsigned int decimals)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_TIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_time_display_length
                                 : mylite_mysql_time_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static unsigned int time_function_value_decimals(const struct mylite_expression_value *value)
{
    const char *dot = value == NULL ? NULL : strchr(value->text_value, '.');
    unsigned int decimals = 0U;

    if (dot == NULL) {
        return 0U;
    }
    for (++dot; *dot >= '0' && *dot <= '9' && decimals < mylite_mysql_temporal_max_fsp; ++dot) {
        ++decimals;
    }
    return decimals;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_date_interval_function_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *expression,
                                                   struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *temporal = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor temporal_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (!mylite_function_name_is_date_interval_arithmetic(name) || !expression->interval_spec) {
        return MYLITE_UNSUPPORTED;
    }

    status = infer_expression_descriptor(database, plan, temporal, NULL, &temporal_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    if (temporal_descriptor.type == MYLITE_FIELD_TYPE_DATE) {
        if (interval_unit_has_time_part(expression->interval_unit)) {
            *out_descriptor = date_interval_datetime_descriptor(0U);
        } else {
            *out_descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DATE,
                .flags = MYLITE_FIELD_FLAG_BINARY,
                .length = mylite_mysql_date_display_length,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(out_descriptor, true);
        }
        return MYLITE_OK;
    }
    if (temporal_descriptor.type == MYLITE_FIELD_TYPE_DATETIME ||
        temporal_descriptor.type == MYLITE_FIELD_TYPE_TIMESTAMP) {
        *out_descriptor = date_interval_datetime_descriptor(temporal_descriptor.decimals);
        return MYLITE_OK;
    }

    *out_descriptor = date_interval_string_descriptor(database);
    return MYLITE_OK;
}

static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length =
        max_bytes_per_character > UINT64_MAX / mylite_mysql_date_arithmetic_string_result_chars
            ? mylite_mysql_long_text_length
            : mylite_mysql_date_arithmetic_string_result_chars * max_bytes_per_character;
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_datetime_display_length
                                 : mylite_mysql_datetime_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static bool infer_list_index_function_descriptor(const struct mylite_sql_ast_node *name,
                                                 bool nullable,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_field(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(false);
        out_descriptor->length = mylite_mysql_list_index_function_display_length;
        return true;
    }
    if (mylite_function_name_is_find_in_set(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_list_index_function_display_length;
        return true;
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_make_set_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor)
{
    bool all_members_null = false;
    int status =
        make_set_function_members_are_all_null(database, plan, expression, &all_members_null);
    bool nullable = false;
    unsigned int flags = 0U;
    uint64_t length = 0U;
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);

    if (status != MYLITE_OK) {
        return status;
    }
    status = infer_function_arguments_nullable(database, plan, mylite_ast_child_at(expression, 1U),
                                               &nullable);
    if (status != MYLITE_OK) {
        return status;
    }

    if (all_members_null) {
        flags = MYLITE_FIELD_FLAG_BINARY;
        length = make_set_all_null_result_length(expression);
        charset_id = mylite_mysql_binary_charset_id;
    } else {
        length = make_set_function_result_length(database, plan, expression);
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, nullable);
    return MYLITE_OK;
}

static bool
infer_base_conversion_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *name,
                                          struct mylite_field_descriptor *out_descriptor)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length =
        max_bytes_per_character > UINT64_MAX / mylite_mysql_base_conversion_result_chars
            ? mylite_mysql_long_text_length
            : mylite_mysql_base_conversion_result_chars * max_bytes_per_character;

    if (!mylite_function_name_has_base_conversion_result(name)) {
        return false;
    }
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return true;
}

static int infer_char_function_descriptor(mylite_db *database,
                                          const struct mylite_sql_ast_node *expression,
                                          struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *charset_node = mylite_ast_child_at(expression, 2U);
    char *charset_name = NULL;
    bool binary_result = false;
    size_t arity = 0U;
    uint64_t length = 0U;
    unsigned int flags = 0U;
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);

    if (!mylite_function_name_is_char(name)) {
        return MYLITE_UNSUPPORTED;
    }

    charset_name = charset_node == NULL
                       ? mylite_copy_span_text(mylite_mysql_binary_charset_name,
                                               strlen(mylite_mysql_binary_charset_name))
                       : mylite_copy_schema_text_span(charset_node);
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }

    if (mylite_ascii_case_equal(charset_name, "binary")) {
        binary_result = true;
    } else if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
        int status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);

        free(charset_name);
        return status;
    }
    free(charset_name);
    if (binary_result) {
        flags = MYLITE_FIELD_FLAG_BINARY;
        charset_id = mylite_mysql_binary_charset_id;
    }

    arity = mylite_sql_ast_node_child_count(arguments);
    if (arity > UINT64_MAX / mylite_mysql_char_function_argument_bytes) {
        length = mylite_mysql_long_text_length;
    } else {
        length = (uint64_t)arity * mylite_mysql_char_function_argument_bytes;
        if (!binary_result) {
            uint64_t max_bytes_per_character =
                mylite_expression_descriptor_connection_character_max_length(database);

            if (max_bytes_per_character != 0U && length > UINT64_MAX / max_bytes_per_character) {
                length = mylite_mysql_long_text_length;
            } else {
                length *= max_bytes_per_character;
            }
        }
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_string_encoding_function_descriptor(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     struct mylite_field_descriptor *out_descriptor,
                                                     bool *out_matched)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    *out_matched = true;
    if (mylite_function_name_is_hex(name)) {
        return infer_hex_function_descriptor(database, plan, expression, out_descriptor);
    }
    if (mylite_function_name_is_unhex(name)) {
        return infer_unhex_function_descriptor(database, plan, expression, out_descriptor);
    }
    if (mylite_function_name_is_to_base64(name)) {
        return infer_to_base64_function_descriptor(database, plan, expression, out_descriptor);
    }
    if (mylite_function_name_is_from_base64(name)) {
        return infer_from_base64_function_descriptor(database, plan, expression, out_descriptor);
    }
    *out_matched = false;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_hex_function_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression,
                                         struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length = 0U;
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (source_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        length = 0U;
    } else if (mylite_expression_descriptor_has_numeric_result(&source_descriptor)) {
        length = max_bytes_per_character > UINT64_MAX / mylite_mysql_hex_numeric_result_chars
                     ? mylite_mysql_long_text_length
                     : mylite_mysql_hex_numeric_result_chars * max_bytes_per_character;
    } else if (source_descriptor.length > UINT64_MAX / 2U ||
               (source_descriptor.length * 2U) > UINT64_MAX / max_bytes_per_character) {
        length = mylite_mysql_long_text_length;
    } else {
        length = source_descriptor.length * 2U * max_bytes_per_character;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_unhex_function_descriptor(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t length = 0U;
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (source_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        length = 0U;
    } else if (source_descriptor.length == UINT64_MAX) {
        length = mylite_mysql_long_text_length;
    } else {
        length = (source_descriptor.length + 1U) / 2U;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_to_base64_function_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t encoded_chars = 0U;
    uint64_t length = 0U;
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    encoded_chars = source_descriptor.type == MYLITE_FIELD_TYPE_NULL
                        ? 0U
                        : base64_encoded_descriptor_length(source_descriptor.length);
    length = max_bytes_per_character != 0U && encoded_chars > UINT64_MAX / max_bytes_per_character
                 ? mylite_mysql_long_text_length
                 : encoded_chars * max_bytes_per_character;

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_from_base64_function_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t length = 0U;
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    length = source_descriptor.type == MYLITE_FIELD_TYPE_NULL
                 ? 0U
                 : base64_decoded_descriptor_length(source_descriptor.length);

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t base64_encoded_descriptor_length(uint64_t source_length)
{
    uint64_t groups = 0U;
    uint64_t encoded = 0U;
    uint64_t newlines = 0U;

    if (source_length == 0U) {
        return 0U;
    }
    if (source_length > UINT64_MAX - (mylite_mysql_base64_input_group - 1U)) {
        return mylite_mysql_long_text_length;
    }
    groups =
        (source_length + (mylite_mysql_base64_input_group - 1U)) / mylite_mysql_base64_input_group;
    if (groups > UINT64_MAX / mylite_mysql_base64_output_group) {
        return mylite_mysql_long_text_length;
    }
    encoded = groups * mylite_mysql_base64_output_group;
    newlines = (encoded - 1U) / mylite_mysql_base64_line_length;
    if (encoded > UINT64_MAX - newlines) {
        return mylite_mysql_long_text_length;
    }
    return encoded + newlines;
}

static uint64_t base64_decoded_descriptor_length(uint64_t source_length)
{
    return ((source_length / mylite_mysql_base64_output_group) * mylite_mysql_base64_input_group) +
           (((source_length % mylite_mysql_base64_output_group) * mylite_mysql_base64_input_group) /
            mylite_mysql_base64_output_group);
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t quote_function_result_length(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t quote_length = 0U;
    bool source_is_null = false;
    uint64_t source_length = quote_function_source_display_length(
        database, plan, source, max_bytes_per_character, &source_is_null);

    if (source_is_null) {
        return max_bytes_per_character > UINT64_MAX / 4U ? mylite_mysql_long_text_length
                                                         : 4U * max_bytes_per_character;
    }

    if (max_bytes_per_character > UINT64_MAX / 2U) {
        return mylite_mysql_long_text_length;
    }
    quote_length = 2U * max_bytes_per_character;
    if (source_length > (UINT64_MAX - quote_length) / 2U) {
        return mylite_mysql_long_text_length;
    }
    return (source_length * 2U) + quote_length;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t quote_function_source_display_length(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *source,
                                                     uint64_t max_bytes_per_character,
                                                     bool *out_source_is_null)
{
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    *out_source_is_null = false;
    if (source != NULL && mylite_expression_is_cacheable_no_table(source) &&
        mylite_expression_eval(source, &warnings, &value) == 0) {
        if (value.kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_source_is_null = true;
            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return 0U;
        }
        if (value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
            uint64_t result =
                mylite_expression_descriptor_utf8_display_character_count(value.text_value) *
                max_bytes_per_character;

            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return result;
        }
        if (source->kind == MYLITE_SQL_AST_LITERAL &&
            (source->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
             source->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE)) {
            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return max_bytes_per_character;
        }
    }
    mylite_expression_value_deinit(&value);
    mylite_expression_warnings_deinit(&warnings);

    if (infer_expression_descriptor(database, plan, source, NULL, &descriptor) == MYLITE_OK) {
        return quote_function_descriptor_display_length(&descriptor, max_bytes_per_character);
    }
    return mylite_mysql_text_length;
}

static uint64_t
quote_function_descriptor_display_length(const struct mylite_field_descriptor *descriptor,
                                         uint64_t max_bytes_per_character)
{
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        return descriptor->length;
    }
    if (descriptor == NULL || max_bytes_per_character == 0U ||
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t insert_function_result_length(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t source_length =
        expression_text_display_length(database, plan, mylite_ast_child_at(arguments, 0U));
    uint64_t replacement_length =
        expression_text_display_length(database, plan, mylite_ast_child_at(arguments, 3U));

    if (source_length > UINT64_MAX - replacement_length) {
        return mylite_mysql_long_text_length;
    }
    return source_length + replacement_length;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t make_set_function_result_length(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t separator_length =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t result = 0U;
    size_t member_count = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL; argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument);

        if (member_count != 0U) {
            if (result > UINT64_MAX - separator_length) {
                return mylite_mysql_long_text_length;
            }
            result += separator_length;
        }
        if (result > UINT64_MAX - length) {
            return mylite_mysql_long_text_length;
        }
        result += length;
        ++member_count;
    }
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int make_set_function_members_are_all_null(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *expression,
                                                  bool *out_all_null)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool saw_member = false;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL; argument = argument->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, argument, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        saw_member = true;
        if (descriptor.type != MYLITE_FIELD_TYPE_NULL) {
            *out_all_null = false;
            return MYLITE_OK;
        }
    }
    *out_all_null = saw_member;
    return MYLITE_OK;
}

static uint64_t make_set_all_null_result_length(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    size_t member_count = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL; argument = argument->next_sibling) {
        ++member_count;
    }
    if (member_count <= 1U) {
        return 0U;
    }
    return (uint64_t)(member_count - 1U);
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t elt_function_result_length(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t result = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL; argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument);

        result = mylite_expression_descriptor_max_u64(result, length);
    }
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t elt_argument_result_length(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (infer_expression_descriptor(database, plan, expression, NULL, &descriptor) == MYLITE_OK) {
        if (descriptor.type == MYLITE_FIELD_TYPE_NULL) {
            return 0U;
        }
        if (descriptor.charset_id == mylite_mysql_binary_charset_id) {
            uint64_t max_bytes_per_character =
                mylite_expression_descriptor_connection_character_max_length(database);

            return descriptor.length > UINT64_MAX / max_bytes_per_character
                       ? mylite_mysql_long_text_length
                       : descriptor.length * max_bytes_per_character;
        }
        return descriptor.length;
    }
    return expression_text_display_length(database, plan, expression);
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t expression_text_display_length(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression)
{
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    uint64_t result = mylite_mysql_text_length;

    if (expression != NULL && mylite_expression_is_cacheable_no_table(expression) &&
        mylite_expression_eval(expression, &warnings, &value) == 0) {
        char *text = mylite_expression_value_to_text(&value);

        if (text == NULL) {
            result = 0U;
        } else {
            result = mylite_expression_descriptor_utf8_display_character_count(text) *
                     mylite_expression_descriptor_connection_character_max_length(database);
        }
        free(text);
        mylite_expression_value_deinit(&value);
        mylite_expression_warnings_deinit(&warnings);
        return result;
    }

    mylite_expression_value_deinit(&value);
    mylite_expression_warnings_deinit(&warnings);
    if (infer_expression_descriptor(database, plan, expression, NULL, &descriptor) == MYLITE_OK) {
        return descriptor.length;
    }
    return result;
}

static int infer_slice_string_function_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool nullable, struct mylite_field_descriptor *out_descriptor, bool *out_matched)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    *out_matched = mylite_function_name_has_slice_string_result(name);
    if (!*out_matched) {
        return MYLITE_OK;
    }
    if (mylite_function_name_is_make_set(name)) {
        (void)value;
        (void)nullable;
        return infer_make_set_function_descriptor(database, plan, expression, out_descriptor);
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = slice_string_function_result_length(database, plan, expression, value),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t slice_string_function_result_length( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();

    if (mylite_function_name_is_elt(name)) {
        return elt_function_result_length(database, plan, expression);
    }
    if (mylite_function_name_is_quote(name)) {
        return quote_function_result_length(database, plan, expression);
    }
    if (mylite_function_name_is_insert(name)) {
        return insert_function_result_length(database, plan, expression);
    }
    if (!mylite_function_name_uses_source_length(name) && value != NULL &&
        value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return mylite_expression_descriptor_string_length(database, value, NULL);
    }
    if (mylite_function_name_is_concat_ws(name)) {
        return mylite_mysql_text_length;
    }
    if (source != NULL && infer_expression_descriptor(database, plan, source, NULL,
                                                      &source_descriptor) == MYLITE_OK) {
        return source_descriptor.length;
    }
    return mylite_mysql_text_length;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_aggregate_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        int status =
            infer_expression_descriptor(database, plan, argument, NULL, &argument_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    switch (expression->aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_mysql_signed_longlong_display_length;
        mylite_field_descriptor_set_not_null(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_SUM:
        if (argument_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_sum_decimal_display_length;
            descriptor.decimals = argument_descriptor.decimals;
        } else if ((argument_descriptor.flags & MYLITE_FIELD_FLAG_NUM) != 0U &&
                   argument_descriptor.type != MYLITE_FIELD_TYPE_FLOAT &&
                   argument_descriptor.type != MYLITE_FIELD_TYPE_DOUBLE) {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_sum_integer_display_length;
            descriptor.decimals = 0U;
        } else {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        if (argument_descriptor.type == MYLITE_FIELD_TYPE_FLOAT ||
            argument_descriptor.type == MYLITE_FIELD_TYPE_DOUBLE ||
            (argument_descriptor.flags & MYLITE_FIELD_FLAG_NUM) == 0U) {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        } else {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_avg_display_length;
            descriptor.decimals = argument_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL
                                      ? mylite_mysql_avg_decimal_scale
                                      : mylite_mysql_avg_integer_scale;
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        descriptor = argument_descriptor;
        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_case_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor)
{
    size_t when_list_index = 0U;
    size_t else_expression_index = 1U;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_expression = NULL;
    struct mylite_case_descriptor_aggregate aggregate = {0};

    if (expression->case_expression_simple) {
        when_list_index = 1U;
        else_expression_index = 2U;
    }
    when_list = mylite_ast_child_at(expression, when_list_index);
    else_expression = mylite_ast_child_at(expression, else_expression_index);

    aggregate.descriptor = mylite_expression_descriptor_defaults();
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        int status =
            infer_case_result_descriptor(database, plan, mylite_ast_child_at(arm, 1U), &aggregate);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (else_expression == NULL) {
        aggregate.has_result = true;
        aggregate.nullable = true;
    } else {
        int status = infer_case_result_descriptor(database, plan, else_expression, &aggregate);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    *out_descriptor = finalize_case_descriptor(database, &aggregate);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_case_result_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_case_descriptor_aggregate *aggregate)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status = infer_expression_descriptor(database, plan, expression, NULL, &descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    aggregate_case_result_descriptor(&descriptor, aggregate);
    return MYLITE_OK;
}

static void aggregate_case_result_descriptor(const struct mylite_field_descriptor *descriptor,
                                             struct mylite_case_descriptor_aggregate *aggregate)
{
    aggregate->has_result = true;
    if (mylite_expression_descriptor_is_nullable(descriptor)) {
        aggregate->nullable = true;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        aggregate->nullable = true;
        return;
    }

    if (!aggregate->has_non_null_result) {
        aggregate->descriptor = *descriptor;
        aggregate->has_non_null_result = true;
    } else {
        aggregate->descriptor.length =
            mylite_expression_descriptor_max_u64(aggregate->descriptor.length, descriptor->length);
        aggregate->descriptor.decimals = (unsigned int)mylite_expression_descriptor_max_u64(
            aggregate->descriptor.decimals, descriptor->decimals);
        aggregate->descriptor.flags |= descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED;
        aggregate->descriptor.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_NOT_NULL;
    }

    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        aggregate->has_text_result = true;
    }
    if (mylite_expression_descriptor_has_decimal_result(descriptor)) {
        aggregate->has_decimal_result = true;
    }
    if (mylite_expression_descriptor_has_double_result(descriptor)) {
        aggregate->has_double_result = true;
    }
}

static struct mylite_field_descriptor
finalize_case_descriptor(mylite_db *database,
                         const struct mylite_case_descriptor_aggregate *aggregate)
{
    struct mylite_field_descriptor descriptor = aggregate->descriptor;

    if (!aggregate->has_result || !aggregate->has_non_null_result) {
        return mylite_expression_descriptor_null();
    }
    if (aggregate->has_text_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = aggregate->descriptor.length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = aggregate->nullable,
        };
    } else if (aggregate->has_decimal_result) {
        descriptor = mylite_expression_descriptor_decimal(aggregate->nullable);
        descriptor.length = aggregate->descriptor.length;
        descriptor.decimals = aggregate->descriptor.decimals;
    } else if (aggregate->has_double_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->descriptor.length,
                                                           mylite_mysql_double_display_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = aggregate->nullable,
        };
    } else {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        descriptor.charset_id = mylite_mysql_binary_charset_id;
    }
    mylite_field_descriptor_set_nullable(&descriptor, aggregate->nullable);
    return descriptor;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_cast_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_expression_value *value,
                                            struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }
    status = mylite_expression_validate_cast_target_charset(database, expression);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    switch (target->column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        if (target->column_type_unsigned) {
            *out_descriptor = cast_unsigned_descriptor(&source_descriptor);
        } else {
            *out_descriptor = cast_signed_descriptor(&source_descriptor);
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        *out_descriptor = cast_decimal_descriptor(target, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        *out_descriptor = cast_character_descriptor(database, target, value, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

static int infer_scalar_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(select_statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *select_expression = NULL;
    mylite_stmt *subquery_stmt = NULL;
    int status = validate_scalar_subquery_select_list(database, select_statement);

    if (status != MYLITE_OK) {
        return status;
    }

    select_item = select_list == NULL ? NULL : select_list->first_child;
    select_expression = mylite_ast_child_at(select_item, 0U);
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return infer_expression_descriptor(database, NULL, select_expression, NULL, out_descriptor);
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (subquery_stmt->result_metadata.column_count == 1U) {
        *out_descriptor = subquery_stmt->result_metadata.columns[0].descriptor;
    } else if (mylite_column_count(subquery_stmt) == 1) {
        *out_descriptor = mylite_expression_descriptor_defaults();
    } else {
        status = set_subquery_operand_columns_error(database);
    }
    mylite_finalize(subquery_stmt);
    if (status == MYLITE_OK) {
        mylite_expression_descriptor_set_scalar_subquery_nullable(out_descriptor);
    }
    return status;
}

static int infer_in_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (!binary_expression_is_in_subquery(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (left_expression == NULL || left_expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = validate_in_subquery_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

static int infer_row_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    int status = MYLITE_OK;

    if (!binary_expression_is_row_subquery(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = validate_row_subquery_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(expression->operator_kind !=
                                                           MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL);
    return MYLITE_OK;
}

static int infer_quantified_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left =
        unwrap_parenthesized_expression(left_expression);
    int status = MYLITE_OK;

    if (quantified_comparison_is_row_subquery_alias(expression)) {
        status = validate_row_subquery_expression(database, expression, plan);
        if (status != MYLITE_OK) {
            *out_descriptor = mylite_expression_descriptor_defaults();
            return status;
        }

        *out_descriptor = mylite_expression_descriptor_boolean(true);
        return MYLITE_OK;
    }

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !quantified_comparison_operator_is_supported(expression->operator_kind)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return set_row_quantified_non_alias_error(database, expression);
    }

    status = infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = validate_quantified_subquery_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

static struct mylite_field_descriptor
cast_signed_descriptor(const struct mylite_field_descriptor *source)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_signed_longlong_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor
cast_unsigned_descriptor(const struct mylite_field_descriptor *source)
{
    struct mylite_field_descriptor descriptor = cast_signed_descriptor(source);

    descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    return descriptor;
}

static struct mylite_field_descriptor
cast_decimal_descriptor(const struct mylite_sql_ast_node *target,
                        const struct mylite_field_descriptor *source)
{
    unsigned int precision = cast_decimal_precision(target);
    unsigned int scale = cast_decimal_scale(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = (uint64_t)precision + (scale == 0U ? 1U : 2U),
        .decimals = scale,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor
cast_character_descriptor(mylite_db *database, const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          const struct mylite_field_descriptor *source)
{
    unsigned int charset_id = cast_target_charset_id(database, target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = cast_character_length(database, target, value, source),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    if (charset_id == mylite_mysql_binary_charset_id) {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY;
    }
    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_precision) {
        return (unsigned int)target->column_precision;
    }
    return mylite_mysql_cast_default_decimal_precision;
}

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_scale) {
        return (unsigned int)target->column_scale;
    }
    return 0U;
}

static uint64_t cast_character_length(mylite_db *database, const struct mylite_sql_ast_node *target,
                                      const struct mylite_expression_value *value,
                                      const struct mylite_field_descriptor *source)
{
    int max_length = cast_target_charset_max_length(database, target);

    if (target != NULL && target->has_column_length) {
        return target->column_length * (uint64_t)max_length;
    }
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return (uint64_t)(value->text_value == NULL ? 0U : value->text_length) *
               (uint64_t)max_length;
    }
    if (source != NULL && source->length != 0U) {
        return source->length;
    }
    return 0U;
}

static unsigned int cast_target_charset_id(mylite_db *database,
                                           const struct mylite_sql_ast_node *target)
{
    char *name = NULL;
    const struct mylite_charset *charset = NULL;
    const struct mylite_collation *collation = NULL;

    if (target == NULL || !target->has_column_character_set) {
        if (target != NULL && target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
            return mylite_mysql_binary_charset_id;
        }
        return mylite_expression_descriptor_connection_charset_id(database);
    }

    name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (name == NULL) {
        return mylite_expression_descriptor_connection_charset_id(database);
    }
    charset = mylite_charset_lookup(name);
    if (charset != NULL) {
        collation = mylite_collation_lookup(charset->default_collation);
    }
    free(name);
    return collation == NULL ? mylite_expression_descriptor_connection_charset_id(database)
                             : (unsigned int)collation->id;
}

static int cast_target_charset_max_length(mylite_db *database,
                                          const struct mylite_sql_ast_node *target)
{
    char *name = NULL;
    const struct mylite_charset *charset = NULL;

    if (target == NULL || !target->has_column_character_set) {
        charset =
            target != NULL && target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY
                ? mylite_charset_lookup("binary")
                : mylite_charset_lookup(database == NULL ? NULL : database->character_set_results);
        return charset == NULL ? 1 : charset->max_length;
    }

    name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (name == NULL) {
        return 1;
    }
    charset = mylite_charset_lookup(name);
    free(name);
    return charset == NULL ? 1 : charset->max_length;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_nullable(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *arguments,
                                             bool *out_nullable)
{
    bool nullable = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }
    *out_nullable = nullable;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_function_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool string_domain = false;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_greatest_least(name)) {
        return MYLITE_UNSUPPORTED;
    }

    status = greatest_least_function_uses_string_domain(database, plan, arguments, &string_domain);
    if (status != MYLITE_OK) {
        return status;
    }
    if (string_domain) {
        return infer_greatest_least_string_descriptor(database, plan, arguments, result_nullable,
                                                      out_descriptor);
    }
    return infer_greatest_least_numeric_descriptor(database, plan, arguments, result_nullable,
                                                   out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int greatest_least_function_uses_string_domain(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      const struct mylite_sql_ast_node *arguments,
                                                      bool *out_string_domain)
{
    *out_string_domain = false;
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_has_text_result(&descriptor) ||
            descriptor.type == MYLITE_FIELD_TYPE_BLOB) {
            *out_string_domain = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_string_descriptor(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *arguments,
                                                  bool result_nullable,
                                                  struct mylite_field_descriptor *out_descriptor)
{
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = greatest_least_string_result_length(database, plan, arguments),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t greatest_least_string_result_length(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *arguments)
{
    uint64_t length = 0U;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

        if (infer_expression_descriptor(database, plan, child, NULL, &descriptor) != MYLITE_OK) {
            return mylite_mysql_long_text_length;
        }
        length = mylite_expression_descriptor_max_u64(
            length, greatest_least_argument_string_length(database, &descriptor));
    }
    return length;
}

static uint64_t
greatest_least_argument_string_length(mylite_db *database,
                                      const struct mylite_field_descriptor *descriptor)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return 0U;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_BLOB) {
        return descriptor->length;
    }
    if (max_bytes_per_character != 0U &&
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_numeric_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *arguments,
                                                   bool result_nullable,
                                                   struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor aggregate = mylite_expression_descriptor_defaults();
    bool saw_nonnull = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        aggregate_greatest_least_numeric_descriptor(&descriptor, &aggregate, &saw_nonnull);
    }

    if (!saw_nonnull) {
        *out_descriptor = mylite_expression_descriptor_null();
    } else {
        *out_descriptor = aggregate;
    }
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

static void
aggregate_greatest_least_numeric_descriptor(const struct mylite_field_descriptor *argument,
                                            struct mylite_field_descriptor *aggregate,
                                            bool *out_saw_nonnull)
{
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return;
    }

    if (!*out_saw_nonnull) {
        *aggregate = *argument;
        aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        aggregate->charset_id = mylite_mysql_binary_charset_id;
        *out_saw_nonnull = true;
        return;
    }

    if (mylite_expression_descriptor_has_double_result(argument) ||
        argument->type == MYLITE_FIELD_TYPE_FLOAT ||
        mylite_expression_descriptor_has_double_result(aggregate) ||
        aggregate->type == MYLITE_FIELD_TYPE_FLOAT) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
                mylite_mysql_double_display_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    if (mylite_expression_descriptor_has_decimal_result(argument) ||
        mylite_expression_descriptor_has_decimal_result(aggregate)) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
            .decimals =
                aggregate->decimals > argument->decimals ? aggregate->decimals : argument->decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    aggregate->type = MYLITE_FIELD_TYPE_LONGLONG;
    aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
    if (((aggregate->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U) ||
        ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U)) {
        aggregate->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
    }
    aggregate->charset_id = mylite_mysql_binary_charset_id;
    aggregate->length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length);
    aggregate->decimals = 0U;
}

static bool infer_code_search_function_descriptor(const struct mylite_sql_ast_node *name,
                                                  bool nullable,
                                                  struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_ascii(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_ascii_function_display_length;
        return true;
    }
    if (mylite_function_name_is_ord(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_ord_function_display_length;
        return true;
    }
    if (mylite_function_name_has_search_result(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_mysql_search_function_display_length;
        return true;
    }
    return false;
}

static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit)
{
    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        return false;
    }
    return false;
}

static bool interval_unit_has_time_part(enum mylite_sql_ast_interval_unit unit)
{
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
        return true;
    }
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE) {
        return true;
    }
    return unit == MYLITE_SQL_AST_INTERVAL_UNIT_SECOND;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int build_select_outputs(mylite_db *database, const struct mylite_sql_ast_node *select_list,
                                bool allow_expression_outputs, struct mylite_select_plan *plan)
{
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        int status = append_select_item_outputs(database, item, allow_expression_outputs, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->output_count == 0U) {
        return set_select_unsupported_projection_error(database);
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_custom_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *where_clause,
                                                 const char *sql, size_t sql_length,
                                                 struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt)
{
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_stmt *stmt = NULL;
    char *scan_sql = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (mylite_select_plan_table_count(plan) <= 1U) {
        scan_sql = mylite_select_build_scan_sql(database, plan);
        if (scan_sql == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }

        rc = sqlite3_prepare_v3(database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT,
                                &sqlite_stmt, NULL);
        sqlite3_free(scan_sql);
        if (rc != SQLITE_OK) {
            return mylite_diagnostics_set_sqlite_error(database);
        }
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        sqlite3_finalize(sqlite_stmt);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_TABLE_SELECT,
        .sqlite_stmt = sqlite_stmt,
        .affected_rows = -1,
    };

    status = attach_select_result_metadata(stmt, plan);
    if (status == MYLITE_OK) {
        stmt->select_plan = *plan;
        *plan = (struct mylite_select_plan){0};
        status = clone_table_select_expressions(stmt, where_clause, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        stmt->preserve_prepare_warnings = database->warnings.count > 0U;
        *out_stmt = stmt;
        return MYLITE_OK;
    }

    mylite_finalize(stmt);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan)
{
    int status = MYLITE_OK;

    if (clauses->where != NULL) {
        status = bind_select_where_clause(database, clauses->where, plan);
    }
    if (status == MYLITE_OK && clauses->group_by != NULL) {
        status = bind_select_group_by_clause(database, clauses->group_by, plan);
    }
    if (status == MYLITE_OK && clauses->having != NULL) {
        status = bind_select_having_clause(database, clauses->having, plan);
    }
    if (status == MYLITE_OK && clauses->limit != NULL) {
        status = mylite_select_bind_limit_clause(clauses->limit, plan);
    }
    if (status == MYLITE_OK && clauses->order_by != NULL) {
        status = bind_select_order_by_clause(database, clauses->order_by, plan);
    }
    if (status == MYLITE_OK) {
        status = validate_select_grouping(database, plan);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(where_clause, 0U);

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        predicate == NULL) {
        return set_select_unsupported_where_error(database);
    }
    return bind_select_predicate_expression(database, predicate, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan)
{
    for (size_t index = 0U; index < plan->join_predicate_count; ++index) {
        const struct mylite_select_join_predicate *predicate = &plan->join_predicates[index];
        int status = bind_select_predicate_expression_in_clause(
            database, predicate->expression, plan, "on clause", predicate->first_table,
            predicate->table_count);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *plan)
{
    return bind_select_predicate_expression_in_clause(database, expression, plan, "where clause",
                                                      0U, mylite_select_plan_table_count(plan));
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_expression_in_clause(mylite_db *database,
                                                      const struct mylite_sql_ast_node *expression,
                                                      const struct mylite_select_plan *plan,
                                                      const char *clause_context,
                                                      size_t first_table, size_t table_count)
{
    if (expression == NULL) {
        return set_select_unsupported_where_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        if (select_literal_is_supported(expression)) {
            return MYLITE_OK;
        }
        return set_select_unsupported_where_error(database);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        size_t column_index = mylite_select_plan_column_count(plan);
        int status = mylite_select_resolve_plan_column_reference_in_scope(
            database, plan, expression, clause_context, first_table, table_count, &column_index);

        return status == MYLITE_OK && column_index >= mylite_select_plan_column_count(plan)
                   ? MYLITE_UNSUPPORTED
                   : status;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_select_predicate_expression_in_clause(
                database, child, plan, clause_context, first_table, table_count);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return bind_select_predicate_binary_expression(database, expression, plan, clause_context,
                                                       first_table, table_count);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return bind_select_subquery_expression(
            database, expression, expression->kind == MYLITE_SQL_AST_SUBQUERY_EXPRESSION);
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return bind_select_predicate_quantified_subquery_expression(
            database, expression, plan, clause_context, first_table, table_count);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return set_select_unsupported_where_error(database);
    case MYLITE_SQL_AST_CAST_EXPRESSION: {
        int status = mylite_expression_validate_cast_target_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_predicate_expression_in_clause(database,
                                                          mylite_ast_child_at(expression, 0U), plan,
                                                          clause_context, first_table, table_count);
    }
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return bind_select_function_arguments(database, expression, plan, clause_context,
                                              first_table, table_count);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return set_select_invalid_group_function_error(database);
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return set_select_unsupported_where_error(database);
    }

    return set_select_unsupported_where_error(database);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_binary_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_select_plan *plan,
                                                   const char *clause_context, size_t first_table,
                                                   size_t table_count)
{
    if (binary_expression_is_row_subquery(expression)) {
        return bind_select_predicate_row_subquery_expression(
            database, expression, plan, clause_context, first_table, table_count);
    }
    if (binary_expression_is_in_subquery(expression)) {
        return bind_select_predicate_in_subquery_expression(
            database, expression, plan, clause_context, first_table, table_count);
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_predicate_expression_in_clause(
            database, child, plan, clause_context, first_table, table_count);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_in_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (left == NULL || left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_where_error(database);
    }
    status = bind_select_predicate_expression_in_clause(database, left, plan, clause_context,
                                                        first_table, table_count);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_in_subquery_expression(database, expression, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_row_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count)
{
    const struct mylite_sql_ast_node *left =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    int status = MYLITE_OK;

    if (left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_where_error(database);
    }
    status = bind_select_predicate_row_constructor(database, left, plan, clause_context,
                                                   first_table, table_count);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_row_subquery_expression(database, expression, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_select_plan *plan, const char *clause_context, size_t first_table,
    size_t table_count)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left = unwrap_parenthesized_expression(left);
    int status = MYLITE_OK;

    if (quantified_comparison_is_row_subquery_alias(expression)) {
        status = bind_select_predicate_row_constructor(database, unwrapped_left, plan,
                                                       clause_context, first_table, table_count);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_row_subquery_expression(database, expression, plan);
    }
    if (unwrapped_left == NULL) {
        return set_select_unsupported_where_error(database);
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_row_quantified_non_alias_error(database, expression);
    }
    status = bind_select_predicate_expression_in_clause(database, left, plan, clause_context,
                                                        first_table, table_count);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_quantified_subquery_expression(database, expression, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_subquery_expression(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           bool scalar_context)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION &&
         expression->kind != MYLITE_SQL_AST_EXISTS_EXPRESSION) ||
        select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }

    if (scalar_context) {
        status = validate_scalar_subquery_select_list(database, select_statement);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (expression->kind == MYLITE_SQL_AST_EXISTS_EXPRESSION &&
        (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL)) {
        return MYLITE_OK;
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (subquery_stmt != NULL) {
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_in_subquery_expression(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_select_plan *outer_plan)
{
    return validate_in_subquery_expression(database, expression, outer_plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_quantified_subquery_expression(mylite_db *database,
                                                      const struct mylite_sql_ast_node *expression,
                                                      const struct mylite_select_plan *outer_plan)
{
    return validate_quantified_subquery_expression(database, expression, outer_plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_row_subquery_expression(mylite_db *database,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_select_plan *outer_plan)
{
    return validate_row_subquery_expression(database, expression, outer_plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_in_subquery_expression(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_select_plan *outer_plan)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (!binary_expression_is_in_subquery(expression)) {
        return MYLITE_UNSUPPORTED;
    }
    if (in_subquery_references_outer_plan(select_statement, outer_plan, select_statement)) {
        return set_select_unsupported_where_error(database);
    }

    status = validate_in_subquery_select(database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (in_subquery_has_unqualified_outer_column_reference(select_statement, outer_plan)) {
            return set_select_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = validate_in_subquery_prepared_columns(database, subquery_stmt);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_row_subquery_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *outer_plan)
{
    const struct mylite_sql_ast_node *left =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *select_statement = row_subquery_select_statement(expression);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    size_t expected_width = row_constructor_width(left);
    int status = MYLITE_OK;

    if (!row_subquery_expression_is_supported(expression) || expected_width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    if (in_subquery_references_outer_plan(select_statement, outer_plan, select_statement)) {
        return set_select_unsupported_where_error(database);
    }
    if (row_subquery_expression_is_membership(expression) &&
        mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return set_in_subquery_limit_error(database);
    }

    status = validate_row_subquery_select_columns(database, select_statement, expected_width);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (in_subquery_has_unqualified_outer_column_reference(select_statement, outer_plan)) {
            return set_select_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = validate_row_subquery_prepared_columns(database, subquery_stmt, expected_width);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_quantified_subquery_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_select_plan *outer_plan)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !quantified_comparison_operator_is_supported(expression->operator_kind) ||
        expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE) {
        return MYLITE_UNSUPPORTED;
    }
    if (quantified_comparison_has_row_left(expression)) {
        return set_row_quantified_non_alias_error(database, expression);
    }
    if (in_subquery_references_outer_plan(select_statement, outer_plan, select_statement)) {
        return set_select_unsupported_where_error(database);
    }

    status = validate_in_subquery_select(database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return MYLITE_OK;
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (in_subquery_has_unqualified_outer_column_reference(select_statement, outer_plan)) {
            return set_select_unsupported_where_error(database);
        }
        return status;
    }
    if (subquery_stmt != NULL) {
        status = validate_in_subquery_prepared_columns(database, subquery_stmt);
        mylite_finalize(subquery_stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_row_constructor(mylite_db *database,
                                                 const struct mylite_sql_ast_node *row,
                                                 const struct mylite_select_plan *plan,
                                                 const char *clause_context, size_t first_table,
                                                 size_t table_count)
{
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_where_error(database);
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_predicate_expression_in_clause(
            database, child, plan, clause_context, first_table, table_count);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_aware_row_constructor(mylite_db *database,
                                                       const struct mylite_sql_ast_node *row,
                                                       struct mylite_select_plan *plan,
                                                       const char *clause_context)
{
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_projection_error(database);
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_aggregate_aware_expression(database, child, plan, clause_context);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_row_constructor(mylite_db *database,
                                             const struct mylite_sql_ast_node *row,
                                             struct mylite_select_plan *plan)
{
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_order_error(database);
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_order_expression(database, child, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool in_subquery_references_outer_plan(const struct mylite_sql_ast_node *node,
                                              const struct mylite_select_plan *outer_plan,
                                              const struct mylite_sql_ast_node *select_statement)
{
    const struct mylite_sql_ast_node *first = NULL;

    if (node == NULL || outer_plan == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        first = qualified_identifier_first_part(node);
        if (first != NULL && mylite_select_plan_has_visible_table_span(outer_plan, first->span) &&
            !select_statement_has_visible_table_span(select_statement, first->span)) {
            return true;
        }
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (in_subquery_references_outer_plan(child, outer_plan, select_statement)) {
            return true;
        }
    }
    return false;
}

static bool in_subquery_has_unqualified_outer_column_reference( // NOLINT(misc-no-recursion)
    const struct mylite_sql_ast_node *node, const struct mylite_select_plan *outer_plan)
{
    if (node == NULL || outer_plan == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_FROM_TABLE) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_SELECT_ITEM) {
        return in_subquery_has_unqualified_outer_column_reference(mylite_ast_child_at(node, 0U),
                                                                  outer_plan);
    }
    if (node->kind == MYLITE_SQL_AST_IDENTIFIER &&
        mylite_select_plan_has_column_span(outer_plan, node->span)) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (in_subquery_has_unqualified_outer_column_reference(child, outer_plan)) {
            return true;
        }
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_statement_has_visible_table_span(const struct mylite_sql_ast_node *node,
                                                    struct mylite_sql_source_span name)
{
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_FROM_TABLE) {
        const struct mylite_sql_ast_node *table_name = mylite_ast_child_at(node, 0U);
        const struct mylite_sql_ast_node *alias = mylite_ast_child_at(node, 1U);
        const struct mylite_sql_ast_node *visible_name =
            alias == NULL ? qualified_identifier_last_part(table_name) : alias;

        if (visible_name == NULL) {
            return false;
        }
        return ast_span_text_equal_ci(visible_name->span, name);
    }
    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        if (select_statement_has_visible_table_span(child, name)) {
            return true;
        }
    }
    return false;
}

static const struct mylite_sql_ast_node *
qualified_identifier_first_part(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 0U);
    }
    return current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER ? current : NULL;
}

static const struct mylite_sql_ast_node *
qualified_identifier_last_part(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 1U);
    }
    return current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER ? current : NULL;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_function_arguments(mylite_db *database,
                                          const struct mylite_sql_ast_node *expression,
                                          const struct mylite_select_plan *plan,
                                          const char *clause_context, size_t first_table,
                                          size_t table_count)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (!mylite_expression_is_supported_function_call(expression)) {
        return set_select_unsupported_where_error(database);
    }
    {
        int status = mylite_expression_validate_char_function_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        int status = bind_select_predicate_expression_in_clause(
            database, child, plan, clause_context, first_table, table_count);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan)
{
    int status = bind_select_aggregate_aware_expression(database, expression, plan, "field list");

    if (status == MYLITE_UNSUPPORTED) {
        return set_select_unsupported_projection_error(database);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_aware_expression(mylite_db *database,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_select_plan *plan,
                                                  const char *clause_context)
{
    if (expression == NULL) {
        return set_select_unsupported_projection_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        if (select_literal_is_supported(expression)) {
            return MYLITE_OK;
        }
        return set_select_unsupported_projection_error(database);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (clause_context != NULL && strcmp(clause_context, "having clause") == 0) {
            enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
            size_t index = 0U;

            return resolve_select_having_reference(database, plan, expression, &kind, &index);
        }
        return bind_select_predicate_expression(database, expression, plan);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        return bind_select_aggregate_aware_children(database, expression, plan, clause_context);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return bind_select_aggregate_aware_binary_expression(database, expression, plan,
                                                             clause_context);
    case MYLITE_SQL_AST_CAST_EXPRESSION: {
        int status = mylite_expression_validate_cast_target_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_aggregate_aware_expression(database, mylite_ast_child_at(expression, 0U),
                                                      plan, clause_context);
    }
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return bind_select_aggregate_aware_function(database, expression, plan, clause_context);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return bind_select_aggregate_call(database, expression, plan);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return bind_select_subquery_expression(
            database, expression, expression->kind == MYLITE_SQL_AST_SUBQUERY_EXPRESSION);
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return bind_select_aggregate_aware_quantified_subquery_expression(database, expression,
                                                                          plan, clause_context);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return set_select_unsupported_projection_error(database);
    }

    return set_select_unsupported_projection_error(database);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_aware_binary_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context)
{
    if (binary_expression_is_row_subquery(expression)) {
        const struct mylite_sql_ast_node *left =
            unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
        int status = MYLITE_OK;

        if (left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
            return set_select_unsupported_projection_error(database);
        }
        status = bind_select_aggregate_aware_row_constructor(database, left, plan, clause_context);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_row_subquery_expression(database, expression, plan);
    }
    if (binary_expression_is_in_subquery(expression)) {
        const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
        int status = MYLITE_OK;

        if (left == NULL || left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
            return set_select_unsupported_projection_error(database);
        }
        status = bind_select_aggregate_aware_expression(database, left, plan, clause_context);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_in_subquery_expression(database, expression, plan);
    }
    return bind_select_aggregate_aware_children(database, expression, plan, clause_context);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_aware_quantified_subquery_expression(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left = unwrap_parenthesized_expression(left);
    int status = MYLITE_OK;

    if (quantified_comparison_is_row_subquery_alias(expression)) {
        status = bind_select_aggregate_aware_row_constructor(database, unwrapped_left, plan,
                                                             clause_context);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_row_subquery_expression(database, expression, plan);
    }
    if (unwrapped_left == NULL) {
        return set_select_unsupported_projection_error(database);
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_row_quantified_non_alias_error(database, expression);
    }
    status = bind_select_aggregate_aware_expression(database, left, plan, clause_context);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_quantified_subquery_expression(database, expression, plan);
}

static int bind_select_aggregate_aware_children( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context)
{
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_aggregate_aware_expression(database, child, plan, clause_context);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_select_aggregate_aware_function( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const char *clause_context)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (!mylite_expression_is_supported_function_call(expression)) {
        return set_select_unsupported_projection_error(database);
    }
    {
        int status = mylite_expression_validate_char_function_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        int status = bind_select_aggregate_aware_expression(database, child, plan, clause_context);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_call(mylite_db *database,
                                      const struct mylite_sql_ast_node *expression,
                                      struct mylite_select_plan *plan)
{
    struct mylite_select_aggregate_binding binding = {
        .call = expression,
        .argument = mylite_ast_child_at(expression, 1U),
        .kind = expression->aggregate_kind,
        .argument_kind = expression->aggregate_argument,
    };
    int status = MYLITE_OK;

    if (binding.kind == MYLITE_SQL_AST_AGGREGATE_NONE ||
        binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_NONE) {
        return set_select_invalid_group_function_error(database);
    }
    if (binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        status = bind_select_predicate_expression(database, binding.argument, plan);
        if (status != MYLITE_OK) {
            return status;
        }
    } else if (binding.argument_kind ==
               MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
        status = bind_select_count_distinct_arguments(database, binding.argument, plan);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    status = infer_aggregate_expression_descriptor(database, plan, expression, &binding.descriptor);
    if (status == MYLITE_OK &&
        binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
        status =
            infer_count_distinct_argument_descriptors(database, plan, binding.argument, &binding);
    }
    if (status != MYLITE_OK) {
        mylite_select_aggregate_binding_deinit(&binding);
        return status;
    }
    plan->has_aggregate = true;
    status = mylite_select_plan_add_aggregate_binding(plan, &binding);
    if (status != MYLITE_OK) {
        mylite_select_aggregate_binding_deinit(&binding);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_count_distinct_arguments(mylite_db *database,
                                                const struct mylite_sql_ast_node *arguments,
                                                struct mylite_select_plan *plan)
{
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_EXPRESSION_LIST ||
        arguments->first_child == NULL) {
        return set_select_invalid_group_function_error(database);
    }

    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int status = bind_select_predicate_expression(database, argument, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_count_distinct_argument_descriptors(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments, struct mylite_select_aggregate_binding *binding)
{
    size_t argument_count = mylite_sql_ast_node_child_count(arguments);

    if (argument_count == 0U) {
        return set_select_invalid_group_function_error(database);
    }

    binding->argument_descriptors = calloc(argument_count, sizeof(*binding->argument_descriptors));
    if (binding->argument_descriptors == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    binding->argument_descriptor_count = argument_count;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        int status = infer_expression_descriptor(database, plan, argument, NULL,
                                                 &binding->argument_descriptors[index]);

        if (status != MYLITE_OK) {
            return status;
        }
        ++index;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(group_by_clause, 0U);

    if (group_by_clause == NULL || group_by_clause->kind != MYLITE_SQL_AST_GROUP_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_GROUP_ITEM_LIST) {
        return set_select_unknown_group_column_error(database, "");
    }

    plan->has_group_by = true;
    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_select_group_item(database, item, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_group_item(mylite_db *database, const struct mylite_sql_ast_node *group_item,
                                  struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(group_item, 0U);
    struct mylite_select_group_key group_key = {
        .kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (group_item == NULL || group_item->kind != MYLITE_SQL_AST_GROUP_ITEM || expression == NULL) {
        return set_select_unknown_group_column_error(database, "");
    }
    if (group_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        group_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > plan->output_count) {
            char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_select_unknown_group_column_error(database, reference);
            free(reference);
            return status;
        }
        if (select_output_contains_aggregate(plan, (size_t)(ordinal - 1U))) {
            return set_select_invalid_group_function_error(database);
        }
        group_key.kind = MYLITE_SELECT_GROUP_KEY_OUTPUT;
        group_key.output_index = (size_t)(ordinal - 1U);
        group_key.expression = NULL;
        return mylite_select_plan_add_group_key(plan, &group_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        enum mylite_select_group_key_kind kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_select_group_reference(database, plan, expression, &kind, &index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_GROUP_KEY_OUTPUT) {
            if (select_output_contains_aggregate(plan, index)) {
                return set_select_invalid_group_function_error(database);
            }
            group_key.kind = kind;
            group_key.output_index = index;
            group_key.expression = NULL;
            return mylite_select_plan_add_group_key(plan, &group_key);
        }
    }

    {
        int status = bind_select_group_expression(database, expression, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_group_key(plan, &group_key);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_group_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan)
{
    int status = bind_select_predicate_expression(database, expression, plan);

    if (status == MYLITE_UNSUPPORTED) {
        return set_select_unknown_group_column_error(database, "");
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(having_clause, 0U);

    if (having_clause == NULL || having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE ||
        expression == NULL) {
        return set_select_unsupported_where_error(database);
    }

    plan->has_having = true;
    plan->having_expression = expression;
    return bind_select_aggregate_aware_expression(database, expression, plan, "having clause");
}

static bool select_literal_is_supported(const struct mylite_sql_ast_node *expression)
{
    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        return true;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return false;
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return set_select_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_select_order_item(database, item, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->order_key_count == 0U) {
        return set_select_unsupported_order_error(database);
    }
    return validate_select_expression_outputs(database, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);
    struct mylite_select_order_key order_key = {
        .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return set_select_unsupported_order_error(database);
    }
    if (order_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > plan->output_count) {
            char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = mylite_select_set_unknown_order_column_error(database, reference);
            free(reference);
            return status;
        }
        order_key.kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        order_key.output_index = (size_t)(ordinal - 1U);
        order_key.expression = NULL;
        mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
        return mylite_select_plan_add_order_key(plan, &order_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_select_order_reference(database, plan, expression, &kind, &index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            order_key.kind = kind;
            order_key.output_index = index;
            order_key.expression = NULL;
            mylite_select_plan_mark_output_order_reference(plan, order_key.output_index);
            return mylite_select_plan_add_order_key(plan, &order_key);
        }
    }

    {
        int status = bind_select_order_expression(database, expression, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return mylite_select_plan_add_order_key(plan, &order_key);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan)
{
    if (expression == NULL) {
        return set_select_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_select_order_reference(database, plan, expression, &kind, &index);

        if (status == MYLITE_OK && kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            mylite_select_plan_mark_output_order_reference(plan, index);
        }
        return status;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_select_order_expression(database, child, plan);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return bind_select_order_binary_expression(database, expression, plan);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return bind_select_subquery_expression(
            database, expression, expression->kind == MYLITE_SQL_AST_SUBQUERY_EXPRESSION);
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return bind_select_order_quantified_subquery_expression(database, expression, plan);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return set_select_unsupported_order_error(database);
    case MYLITE_SQL_AST_CAST_EXPRESSION: {
        int status = mylite_expression_validate_cast_target_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_order_expression(database, mylite_ast_child_at(expression, 0U), plan);
    }
    case MYLITE_SQL_AST_FUNCTION_CALL: {
        const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

        if (!mylite_expression_is_supported_function_call(expression)) {
            return set_select_unsupported_order_error(database);
        }
        {
            int status = mylite_expression_validate_char_function_charset(database, expression);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                         : arguments->first_child;
             child != NULL; child = child->next_sibling) {
            int status = bind_select_order_expression(database, child, plan);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return bind_select_aggregate_aware_expression(database, expression, plan, "order clause");
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return set_select_unsupported_order_error(database);
    }

    return set_select_unsupported_order_error(database);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_binary_expression(mylite_db *database,
                                               const struct mylite_sql_ast_node *expression,
                                               struct mylite_select_plan *plan)
{
    if (binary_expression_is_row_subquery(expression)) {
        const struct mylite_sql_ast_node *left =
            unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
        int status = MYLITE_OK;

        if (left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
            return set_select_unsupported_order_error(database);
        }
        status = bind_select_order_row_constructor(database, left, plan);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_row_subquery_expression(database, expression, plan);
    }
    if (binary_expression_is_in_subquery(expression)) {
        return bind_select_order_in_subquery_expression(database, expression, plan);
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = bind_select_order_expression(database, child, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_in_subquery_expression(mylite_db *database,
                                                    const struct mylite_sql_ast_node *expression,
                                                    struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (left == NULL || left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_select_unsupported_order_error(database);
    }
    status = bind_select_order_expression(database, left, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_in_subquery_expression(database, expression, plan);
}

static int bind_select_order_quantified_subquery_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left = unwrap_parenthesized_expression(left);
    int status = MYLITE_OK;

    if (quantified_comparison_is_row_subquery_alias(expression)) {
        status = bind_select_order_row_constructor(database, unwrapped_left, plan);
        if (status != MYLITE_OK) {
            return status;
        }
        return bind_select_row_subquery_expression(database, expression, plan);
    }
    if (unwrapped_left == NULL) {
        return set_select_unsupported_order_error(database);
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return set_row_quantified_non_alias_error(database, expression);
    }
    status = bind_select_order_expression(database, left, plan);
    if (status != MYLITE_OK) {
        return status;
    }
    return bind_select_quantified_subquery_expression(database, expression, plan);
}

static int validate_select_expression_outputs(mylite_db *database,
                                              const struct mylite_select_plan *plan)
{
    if (!mylite_select_duplicate_mode_is_distinct(plan->duplicate_mode)) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        int status = validate_select_distinct_order_key(database, plan, &plan->order_keys[index],
                                                        index + 1U);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_select_distinct_order_key(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_select_order_key *order_key,
                                              size_t order_position)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        return MYLITE_OK;
    }
    return validate_select_distinct_order_expression(database, plan, order_key->expression,
                                                     order_position);
}

static int validate_select_distinct_order_expression(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     size_t order_position)
{
    struct mylite_select_distinct_order_validation_stack stack = {0};
    const struct mylite_sql_ast_node *current = NULL;
    bool alias_first = false;
    int status = push_select_distinct_order_expression_child(database, &stack, expression, true);

    while (status == MYLITE_OK &&
           pop_select_distinct_order_expression(&stack, &current, &alias_first)) {
        status = validate_select_distinct_order_expression_node(database, plan, &stack, current,
                                                                order_position, alias_first);
    }

    select_distinct_order_validation_stack_deinit(&stack);
    return status;
}

static int validate_select_distinct_order_expression_node(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, size_t order_position, bool alias_first)
{
    if (expression == NULL || select_distinct_order_expression_matches_output(plan, expression)) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return validate_select_distinct_order_identifier(database, plan, expression, order_position,
                                                         alias_first);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        return push_select_distinct_order_expression_children(database, stack, expression, false);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return push_select_distinct_order_expression_child(
            database, stack, mylite_ast_child_at(expression, 0U), alias_first);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return push_select_distinct_order_expression_child(
            database, stack, mylite_ast_child_at(expression, 0U), false);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return push_select_distinct_order_expression_children(
            database, stack, mylite_ast_child_at(expression, 1U), false);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
            return push_select_distinct_order_expression_child(
                database, stack, mylite_ast_child_at(expression, 1U), false);
        }
        return MYLITE_OK;
    default:
        return MYLITE_OK;
    }
}

static int push_select_distinct_order_expression_child(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first)
{
    const size_t next_count = stack->count + 1U;
    struct mylite_select_distinct_order_validation_frame *frames = NULL;

    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (next_count <= stack->capacity) {
        stack->frames[stack->count++] = (struct mylite_select_distinct_order_validation_frame){
            .expression = expression,
            .alias_first = alias_first,
        };
        return MYLITE_OK;
    }

    frames = realloc(stack->frames, next_count * sizeof(*stack->frames));
    if (frames == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    stack->frames = frames;
    stack->capacity = next_count;
    stack->frames[stack->count++] = (struct mylite_select_distinct_order_validation_frame){
        .expression = expression,
        .alias_first = alias_first,
    };
    return MYLITE_OK;
}

static int push_select_distinct_order_expression_children(
    mylite_db *database, struct mylite_select_distinct_order_validation_stack *stack,
    const struct mylite_sql_ast_node *expression, bool alias_first)
{
    struct mylite_select_distinct_order_validation_frame *children = NULL;
    const struct mylite_sql_ast_node *child = expression == NULL ? NULL : expression->first_child;
    size_t child_count = 0U;
    int status = MYLITE_OK;

    for (; child != NULL; child = child->next_sibling) {
        ++child_count;
    }
    if (child_count == 0U) {
        return MYLITE_OK;
    }

    children = calloc(child_count, sizeof(*children));
    if (children == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    child = expression->first_child;
    for (size_t index = 0U; index < child_count; ++index) {
        children[index].expression = child;
        child = child->next_sibling;
    }
    for (size_t index = child_count; status == MYLITE_OK && index > 0U; --index) {
        status = push_select_distinct_order_expression_child(
            database, stack, children[index - 1U].expression, alias_first);
    }
    free(children);
    return status;
}

static bool
pop_select_distinct_order_expression(struct mylite_select_distinct_order_validation_stack *stack,
                                     const struct mylite_sql_ast_node **out_expression,
                                     bool *out_alias_first)
{
    if (stack->count == 0U) {
        *out_expression = NULL;
        *out_alias_first = false;
        return false;
    }
    --stack->count;
    *out_expression = stack->frames[stack->count].expression;
    *out_alias_first = stack->frames[stack->count].alias_first;
    return true;
}

static void select_distinct_order_validation_stack_deinit(
    struct mylite_select_distinct_order_validation_stack *stack)
{
    free(stack->frames);
    *stack = (struct mylite_select_distinct_order_validation_stack){0};
}

static int validate_select_distinct_order_identifier(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *identifier,
                                                     size_t order_position, bool alias_first)
{
    enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    size_t index = 0U;
    bool resolved = false;
    int status = MYLITE_OK;

    if (!alias_first) {
        status = validate_select_distinct_order_identifier_column_first(database, plan, identifier,
                                                                        order_position, &resolved);
    }

    if (status != MYLITE_OK || resolved) {
        return status;
    }

    status = resolve_select_order_reference(database, plan, identifier, &kind, &index);

    if (status != MYLITE_OK) {
        return status;
    }
    if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT ||
        select_distinct_column_index_is_output(plan, index)) {
        return MYLITE_OK;
    }
    return set_select_distinct_order_column_error(
        database, plan,
        (struct mylite_select_distinct_order_column_error_context){
            .order_position = order_position,
            .column_index = index,
        });
}

static int validate_select_distinct_order_identifier_column_first(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *identifier, size_t order_position, bool *out_resolved)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    size_t column_index = mylite_select_plan_column_count(plan);
    int status = mylite_copy_identifier_parts(identifier, parts, &part_count);

    *out_resolved = false;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    if (part_count == 1U) {
        size_t column_matches = mylite_select_count_plan_column_parts_matches(
            plan, parts, part_count, 0U, mylite_select_plan_table_count(plan), &column_index);

        if (column_matches > 1U) {
            status = set_select_ambiguous_order_column_error(database, parts[0]);
            *out_resolved = true;
            goto cleanup;
        }
        if (column_matches == 1U) {
            *out_resolved = true;
            if (select_distinct_column_index_is_output(plan, column_index)) {
                goto cleanup;
            }
            status = set_select_distinct_order_column_error(
                database, plan,
                (struct mylite_select_distinct_order_column_error_context){
                    .order_position = order_position,
                    .column_index = column_index,
                });
        }
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static bool
select_distinct_order_expression_matches_output(const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);

    if (unwrapped == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];
        const struct mylite_sql_ast_node *output_expression =
            unwrap_parenthesized_expression(output->expression);

        if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION && output_expression != NULL &&
            ast_span_text_equal_ci(output_expression->span, unwrapped->span)) {
            return true;
        }
    }
    return false;
}

static const struct mylite_sql_ast_node *
unwrap_parenthesized_expression(const struct mylite_sql_ast_node *expression)
{
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        expression = mylite_ast_child_at(expression, 0U);
    }
    return expression;
}

static bool select_distinct_column_index_is_output(const struct mylite_select_plan *plan,
                                                   size_t column_index)
{
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN && output->column_index == column_index) {
            return true;
        }
    }
    return false;
}

static int validate_select_grouping(mylite_db *database, const struct mylite_select_plan *plan)
{
    bool aggregate_query = (plan->has_aggregate || plan->has_group_by || plan->has_having) != 0;
    bool implicit_group = true;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (!aggregate_query) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (!select_output_is_group_invariant(plan, index)) {
            return set_select_only_full_group_by_error(database, output->label, implicit_group);
        }
    }
    if (plan->having_expression != NULL) {
        int status = validate_select_grouping_clause_expression(
            database, plan, plan->having_expression, MYLITE_SELECT_GROUPING_REFERENCE_HAVING);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        const struct mylite_select_order_key *order_key = &plan->order_keys[index];

        if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            if (!select_output_is_group_invariant(plan, order_key->output_index)) {
                const char *label = order_key->output_index < plan->output_count
                                        ? plan->outputs[order_key->output_index].label
                                        : "";

                return set_select_only_full_group_by_error(database, label, implicit_group);
            }
            continue;
        }

        {
            int status = validate_select_grouping_clause_expression(
                database, plan, order_key->expression, MYLITE_SELECT_GROUPING_REFERENCE_ORDER);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_select_grouping_clause_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    char *expression_text = NULL;
    bool implicit_group = true;
    int status = MYLITE_OK;

    if (plan->has_group_by) {
        implicit_group = false;
    }

    if (select_expression_is_group_invariant(plan, expression, reference_policy)) {
        return MYLITE_OK;
    }

    expression_text = expression == NULL
                          ? NULL
                          : mylite_copy_span_text(expression->span.text, expression->span.length);
    if (expression != NULL && expression_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_select_only_full_group_by_error(database, expression_text, implicit_group);
    free(expression_text);
    return status;
}

static bool select_output_contains_aggregate(const struct mylite_select_plan *plan,
                                             size_t output_index)
{
    if (output_index >= plan->output_count) {
        return false;
    }
    if (plan->outputs[output_index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
        return false;
    }
    return select_expression_contains_aggregate(plan->outputs[output_index].expression);
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_expression_contains_aggregate(const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON) {
        return select_expression_contains_aggregate(mylite_ast_child_at(expression, 0U));
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        if (select_expression_contains_aggregate(child)) {
            return true;
        }
    }
    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_output_is_group_invariant(const struct mylite_select_plan *plan,
                                             size_t output_index)
{
    const struct mylite_select_output_column *output = NULL;

    if (output_index >= plan->output_count) {
        return false;
    }
    if (select_output_is_grouped_by_key(plan, output_index)) {
        return true;
    }
    output = &plan->outputs[output_index];
    if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN) {
        return select_column_index_is_grouped(plan, output->column_index);
    }
    return select_expression_is_group_invariant(plan, output->expression,
                                                MYLITE_SELECT_GROUPING_REFERENCE_SELECT);
}

static bool select_expression_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *expression,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    if (expression == NULL) {
        return false;
    }
    if (select_expression_is_grouped(plan, expression)) {
        return true;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return true;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return select_identifier_is_group_invariant(plan, expression, reference_policy);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                    reference_policy);
    case MYLITE_SQL_AST_FUNCTION_CALL: {
        const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

        return select_expression_children_are_group_invariant(
            plan, arguments == NULL ? NULL : arguments->first_child, reference_policy);
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        if (binary_expression_is_in_subquery(expression)) {
            return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                        reference_policy);
        }
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return true;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return select_expression_is_group_invariant(plan, mylite_ast_child_at(expression, 0U),
                                                    reference_policy);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return select_expression_children_are_group_invariant(plan, expression->first_child,
                                                              reference_policy);
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return false;
    }

    return false;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_expression_children_are_group_invariant(
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *first_child,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    for (const struct mylite_sql_ast_node *child = first_child; child != NULL;
         child = child->next_sibling) {
        if (!select_expression_is_group_invariant(plan, child, reference_policy)) {
            return false;
        }
    }
    return true;
}

static bool select_identifier_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *identifier,
    enum mylite_select_grouping_reference_policy reference_policy)
{
    switch (reference_policy) {
    case MYLITE_SELECT_GROUPING_REFERENCE_SELECT:
        return select_column_reference_is_grouped(plan, identifier);
    case MYLITE_SELECT_GROUPING_REFERENCE_HAVING:
        return select_having_identifier_is_group_invariant(plan, identifier);
    case MYLITE_SELECT_GROUPING_REFERENCE_ORDER:
        return select_order_identifier_is_group_invariant(plan, identifier);
    }
    return false;
}

static bool select_having_identifier_is_group_invariant( // NOLINT(misc-no-recursion)
    const struct mylite_select_plan *plan, const struct mylite_sql_ast_node *identifier)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    if (select_column_reference_is_grouped(plan, identifier)) {
        return true;
    }
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return false;
    }

    output_matches = select_output_label_span_count(plan, identifier->span, &output_index);
    if (output_matches != 1U) {
        return false;
    }
    return select_output_is_group_invariant(plan, output_index);
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_order_identifier_is_group_invariant(const struct mylite_select_plan *plan,
                                                       const struct mylite_sql_ast_node *identifier)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    if (identifier != NULL && identifier->kind == MYLITE_SQL_AST_IDENTIFIER) {
        output_matches = select_output_label_span_count(plan, identifier->span, &output_index);
        if (output_matches == 1U) {
            return select_output_is_group_invariant(plan, output_index);
        }
        if (output_matches > 1U) {
            return false;
        }
    }
    return select_column_reference_is_grouped(plan, identifier);
}

static bool select_column_reference_is_grouped(const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *identifier)
{
    size_t column_index = plan->table.column_count;

    if (identifier == NULL || (identifier->kind != MYLITE_SQL_AST_IDENTIFIER &&
                               identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }
    if (mylite_select_resolve_column_reference(&plan->table, identifier, &column_index) !=
            MYLITE_OK ||
        column_index == plan->table.column_count) {
        return false;
    }
    return select_column_index_is_grouped(plan, column_index);
}

static bool select_column_index_is_grouped(const struct mylite_select_plan *plan,
                                           size_t column_index)
{
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];
        size_t group_column_index = plan->table.column_count;

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT &&
            group_key->output_index < plan->output_count &&
            plan->outputs[group_key->output_index].kind == MYLITE_SELECT_OUTPUT_COLUMN &&
            plan->outputs[group_key->output_index].column_index == column_index) {
            return true;
        }
        if (group_key->kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION ||
            group_key->expression == NULL ||
            (group_key->expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
             group_key->expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
            continue;
        }
        if (mylite_select_resolve_column_reference(&plan->table, group_key->expression,
                                                   &group_column_index) == MYLITE_OK &&
            group_column_index == column_index) {
            return true;
        }
    }
    return false;
}

static bool select_output_is_grouped_by_key(const struct mylite_select_plan *plan,
                                            size_t output_index)
{
    if (output_index >= plan->output_count) {
        return false;
    }
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_OUTPUT &&
            group_key->output_index == output_index) {
            return true;
        }
        if (select_group_key_matches_column_output(plan, group_key, output_index)) {
            return true;
        }
    }
    return false;
}

static bool select_expression_is_grouped(const struct mylite_select_plan *plan,
                                         const struct mylite_sql_ast_node *expression)
{
    for (size_t index = 0U; index < plan->group_key_count; ++index) {
        const struct mylite_select_group_key *group_key = &plan->group_keys[index];

        if (group_key->kind == MYLITE_SELECT_GROUP_KEY_EXPRESSION &&
            group_key->expression != NULL &&
            ast_span_text_equal_ci(group_key->expression->span, expression->span)) {
            return true;
        }
    }
    return false;
}

static bool select_group_key_matches_column_output(const struct mylite_select_plan *plan,
                                                   const struct mylite_select_group_key *group_key,
                                                   size_t output_index)
{
    size_t group_column_index = plan->table.column_count;
    const struct mylite_select_output_column *output = &plan->outputs[output_index];

    if (output->kind != MYLITE_SELECT_OUTPUT_COLUMN ||
        group_key->kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION || group_key->expression == NULL) {
        return false;
    }
    if (group_key->expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        group_key->expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return false;
    }
    if (mylite_select_resolve_column_reference(&plan->table, group_key->expression,
                                               &group_column_index) != MYLITE_OK) {
        return false;
    }
    return group_column_index == output->column_index;
}

static bool ast_span_text_equal_ci(struct mylite_sql_source_span left,
                                   struct mylite_sql_source_span right)
{
    if (left.length != right.length || left.text == NULL || right.text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < left.length; ++index) {
        unsigned char left_byte = (unsigned char)left.text[index];
        unsigned char right_byte = (unsigned char)right.text[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
    }
    return true;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(select_item, 0U);
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(select_item, 1U);

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
        expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression->kind == MYLITE_SQL_AST_WILDCARD) {
        if (alias != NULL) {
            return set_select_unsupported_projection_error(database);
        }
        return mylite_select_append_wildcard_outputs(database, expression, plan);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return append_select_column_output(database, expression, alias, plan);
    }
    if (allow_expression_outputs) {
        return append_select_expression_output(database, expression, alias, plan);
    }
    return set_select_unsupported_projection_error(database);
}

static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan)
{
    size_t column_index = mylite_select_plan_column_count(plan);
    char *label = NULL;
    int status = mylite_select_resolve_plan_column_reference(database, plan, expression,
                                                             "field list", &column_index);

    if (status != MYLITE_OK) {
        return status;
    }
    if (column_index == mylite_select_plan_column_count(plan)) {
        return MYLITE_UNSUPPORTED;
    }

    label =
        alias == NULL ? copy_select_final_identifier_label(expression) : copy_select_alias(alias);
    if (label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_plan_add_output_column(plan, &(const struct mylite_select_output_column){
                                                            .kind = MYLITE_SELECT_OUTPUT_COLUMN,
                                                            .column_index = column_index,
                                                            .label = label,
                                                        });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int append_select_expression_output(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_sql_ast_node *alias,
                                           struct mylite_select_plan *plan)
{
    char *label = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return set_select_unsupported_projection_error(database);
    }
    status = bind_select_projection_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    label = alias == NULL ? mylite_copy_span_text(expression->span.text, expression->span.length)
                          : copy_select_alias(alias);
    if (label == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_plan_add_output_column(plan, &(const struct mylite_select_output_column){
                                                            .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
                                                            .expression = expression,
                                                            .label = label,
                                                        });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int collect_select_aggregate_bindings(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan)
{
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        struct mylite_select_aggregate_binding binding = {
            .call = expression,
            .argument = mylite_ast_child_at(expression, 1U),
            .kind = expression->aggregate_kind,
            .argument_kind = expression->aggregate_argument,
        };
        int status =
            infer_aggregate_expression_descriptor(database, plan, expression, &binding.descriptor);

        if (status == MYLITE_OK &&
            binding.argument_kind == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
            status = infer_count_distinct_argument_descriptors(database, plan, binding.argument,
                                                               &binding);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_plan_add_aggregate_binding(plan, &binding);
        }
        if (status != MYLITE_OK) {
            mylite_select_aggregate_binding_deinit(&binding);
            return status;
        }
        plan->has_aggregate = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON) {
        return collect_select_aggregate_bindings(database, mylite_ast_child_at(expression, 0U),
                                                 plan);
    }

    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = collect_select_aggregate_bindings(database, child, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_select_order_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_order_key_kind *out_kind,
                                          size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = select_output_label_count(plan, parts[0], &output_index);

        if (output_matches > 1U) {
            status = set_select_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 1U) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
            *out_index = output_index;
            goto cleanup;
        }
    }

    status = mylite_select_resolve_plan_column_parts(database, plan, parts, part_count,
                                                     "order clause", out_index);
    if (status == MYLITE_OK) {
        *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static int resolve_select_group_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_group_key_kind *out_kind,
                                          size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    bool resolved = false;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    status = maybe_resolve_select_group_table_reference(database, plan, parts, part_count, out_kind,
                                                        out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }
    status = maybe_resolve_select_group_output_reference(database, plan, parts, part_count,
                                                         out_kind, out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }

    {
        char *reference = mylite_select_copy_reference_name(expression);

        if (reference == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        status = set_select_unknown_group_column_error(database, reference);
        free(reference);
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static int maybe_resolve_select_group_table_reference(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      char **parts, size_t part_count,
                                                      enum mylite_select_group_key_kind *out_kind,
                                                      size_t *out_index, bool *out_resolved)
{
    size_t column_index = 0U;

    *out_resolved = false;
    if (part_count < 1U || part_count > 3U ||
        !mylite_select_reference_qualifiers_match(&plan->table, parts, part_count)) {
        return MYLITE_OK;
    }

    column_index = mylite_select_column_index(&plan->table, parts[part_count - 1U]);
    if (column_index == plan->table.column_count) {
        return MYLITE_OK;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = select_output_label_count(plan, parts[0], &output_index);

        if (output_matches != 0U) {
            int status =
                set_select_ambiguous_group_column_warning(database, parts[0], "group statement");

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    *out_kind = MYLITE_SELECT_GROUP_KEY_EXPRESSION;
    *out_index = column_index;
    *out_resolved = true;
    return MYLITE_OK;
}

static int maybe_resolve_select_group_output_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_group_key_kind *out_kind,
                                                       size_t *out_index, bool *out_resolved)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    *out_resolved = false;
    if (part_count != 1U) {
        return MYLITE_OK;
    }

    output_matches = select_output_label_count(plan, parts[0], &output_index);
    if (output_matches > 1U) {
        return set_select_ambiguous_order_column_error(database, parts[0]);
    }
    if (output_matches == 1U) {
        *out_kind = MYLITE_SELECT_GROUP_KEY_OUTPUT;
        *out_index = output_index;
        *out_resolved = true;
    }
    return MYLITE_OK;
}

static int resolve_select_having_reference(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           enum mylite_select_order_key_kind *out_kind,
                                           size_t *out_index)
{
    return resolve_select_having_reference_internal(database, plan, expression, out_kind, out_index,
                                                    true);
}

static int resolve_select_having_reference_internal(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    enum mylite_select_order_key_kind *out_kind,
                                                    size_t *out_index, bool emit_warnings)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    bool resolved = false;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    status = maybe_resolve_select_having_table_reference(
        database, plan, parts, part_count, out_kind, out_index, emit_warnings, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }
    status = maybe_resolve_select_having_output_reference(database, plan, parts, part_count,
                                                          out_kind, out_index, &resolved);
    if (status != MYLITE_OK || resolved) {
        goto cleanup;
    }

    {
        char *reference = mylite_select_copy_reference_name(expression);

        if (reference == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '", reference,
                                                            "' in 'having clause'");
        if (status == MYLITE_OK) {
            status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                                     mylite_error_message(database));
            status = status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        free(reference);
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static int maybe_resolve_select_having_table_reference(mylite_db *database,
                                                       const struct mylite_select_plan *plan,
                                                       char **parts, size_t part_count,
                                                       enum mylite_select_order_key_kind *out_kind,
                                                       size_t *out_index, bool emit_warnings,
                                                       bool *out_resolved)
{
    size_t column_index = 0U;

    *out_resolved = false;
    if (part_count < 1U || part_count > 3U ||
        !mylite_select_reference_qualifiers_match(&plan->table, parts, part_count)) {
        return MYLITE_OK;
    }

    column_index = mylite_select_column_index(&plan->table, parts[part_count - 1U]);
    if (column_index == plan->table.column_count) {
        return MYLITE_OK;
    }
    if (!select_column_index_is_grouped(plan, column_index)) {
        return MYLITE_OK;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = select_output_label_count(plan, parts[0], &output_index);

        if (emit_warnings && output_matches != 0U) {
            int status =
                set_select_ambiguous_group_column_warning(database, parts[0], "having clause");

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = column_index;
    *out_resolved = true;
    return MYLITE_OK;
}

static int maybe_resolve_select_having_output_reference(mylite_db *database,
                                                        const struct mylite_select_plan *plan,
                                                        char **parts, size_t part_count,
                                                        enum mylite_select_order_key_kind *out_kind,
                                                        size_t *out_index, bool *out_resolved)
{
    size_t output_index = 0U;
    size_t output_matches = 0U;

    *out_resolved = false;
    if (part_count != 1U) {
        return MYLITE_OK;
    }

    output_matches = select_output_label_count(plan, parts[0], &output_index);
    if (output_matches > 1U) {
        return set_select_ambiguous_order_column_error(database, parts[0]);
    }
    if (output_matches == 1U) {
        *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        *out_index = output_index;
        *out_resolved = true;
    }
    return MYLITE_OK;
}

static size_t select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_ascii_case_equal(plan->outputs[index].label, label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

static size_t select_output_label_span_count(const struct mylite_select_plan *plan,
                                             struct mylite_sql_source_span label, size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            mylite_span_equal_ci(label, plan->outputs[index].label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

static bool parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value)
{
    enum { decimal_radix = 10U };
    uint64_t value = 0U;

    *out_value = 0U;
    if (span.text == NULL || span.length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char byte = (unsigned char)span.text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }
    *out_value = value;
    return true;
}

static char *copy_select_alias(const struct mylite_sql_ast_node *alias)
{
    if (alias == NULL) {
        return NULL;
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        alias->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(alias);
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return mylite_copy_identifier_span(alias);
    }
    return NULL;
}

static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = mylite_ast_child_at(current, 1U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return NULL;
    }
    return mylite_copy_identifier_span(current);
}

static int set_select_ambiguous_order_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Column '", column_name,
                                                            "' in order clause is ambiguous");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unknown_group_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '",
                                                            column_name, "' in 'group statement'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_ambiguous_group_column_warning(mylite_db *database, const char *column_name,
                                                     const char *clause_context)
{
    char *message = sqlite3_mprintf("Column '%q' in %s is ambiguous", column_name,
                                    clause_context == NULL ? "group statement" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_diagnostics_append_warning(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR, message);
    sqlite3_free(message);
    return status;
}

static int set_select_invalid_group_function_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(database, "Invalid use of group function");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_INVALID_GROUP_FUNC_USE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_duplicate_mode_error(mylite_db *database)
{
    int status =
        mylite_diagnostics_set_error_message(database, "Incorrect usage of ALL and DISTINCT");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_USAGE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_only_full_group_by_error(mylite_db *database, const char *expression_text,
                                               bool implicit_group)
{
    int status = MYLITE_OK;

    if (implicit_group) {
        status = mylite_diagnostics_set_error_message_parts(
            database,
            "In aggregated query without GROUP BY, expression contains nonaggregated "
            "column '",
            expression_text == NULL ? "" : expression_text,
            "'; this is incompatible with sql_mode=only_full_group_by");
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_MIX_OF_GROUP_FUNC_AND_FIELDS, mylite_error_message(database));
    } else {
        status = mylite_diagnostics_set_error_message_parts(
            database, "Expression contains nonaggregated column '",
            expression_text == NULL ? "" : expression_text,
            "' which is not functionally dependent on GROUP BY; this is incompatible with "
            "sql_mode=only_full_group_by");
        if (status == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_FIELD_WITH_GROUP,
                                                 mylite_error_message(database));
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_distinct_order_column_error(
    mylite_db *database, const struct mylite_select_plan *plan,
    struct mylite_select_distinct_order_column_error_context context)
{
    const struct mylite_select_table *table = NULL;
    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, context.column_index, &table);
    const char *schema_name = table == NULL || table->schema_name == NULL ? "" : table->schema_name;
    const char *table_name = table == NULL || table->table_name == NULL ? "" : table->table_name;
    const char *column_name = column == NULL || column->name == NULL ? "" : column->name;
    char *reference = sqlite3_mprintf("%q.%q.%q", schema_name, table_name, column_name);
    char *message = NULL;
    int status = MYLITE_OK;

    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    message = sqlite3_mprintf(
        "Expression #%llu of ORDER BY clause is not in SELECT list, references column '%q' "
        "which is not in SELECT list; this is incompatible with DISTINCT",
        (unsigned long long)context.order_position, reference);
    sqlite3_free(reference);
    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_FIELD_IN_ORDER_NOT_SELECT, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unsupported_projection_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported SELECT projection") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_where_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported WHERE predicate") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_order_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported ORDER BY expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_join_grouping_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(
            database, "Unsupported GROUP BY or HAVING over joined tables") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int set_union_column_count_error(mylite_db *database)
{
    static const char message[] = "The used SELECT statements have a different number of columns";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(
        database, MYLITE_MYSQL_ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_union_global_order_table_error(mylite_db *database, const char *table_name)
{
    char *message =
        sqlite3_mprintf("Table '%q' from one of the SELECTs cannot be used in global ORDER clause",
                        table_name == NULL ? "" : table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database, MYLITE_MYSQL_ER_TABLENAME_NOT_ALLOWED_HERE, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int clone_table_select_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *where_clause,
                                          const char *sql, size_t sql_length)
{
    int status = MYLITE_OK;

    stmt->select_sql_text = mylite_copy_span_text(sql, sql_length);
    if (stmt->select_sql_text == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (where_clause != NULL) {
        const struct mylite_sql_ast_node *predicate = mylite_ast_child_at(where_clause, 0U);
        struct mylite_sql_ast_node *clone = NULL;

        status = clone_table_select_expression_node(stmt, predicate, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_predicate = clone;
    }

    status = clone_table_select_join_expressions(stmt, sql, sql_length);
    if (status == MYLITE_OK) {
        status = clone_table_select_output_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_group_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_having_expression(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = clone_table_select_order_expressions(stmt, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        status = collect_table_select_aggregate_bindings(stmt);
    }
    return status;
}

static int clone_table_select_join_expressions(mylite_stmt *stmt, const char *sql,
                                               size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = clone_table_select_expression_node(
            stmt, stmt->select_plan.join_predicates[index].expression, sql, sql_length, &clone);

        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.join_predicates[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_output_expressions(mylite_stmt *stmt, const char *sql,
                                                 size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.outputs[index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.outputs[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.outputs[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_group_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.group_key_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.group_keys[index].kind != MYLITE_SELECT_GROUP_KEY_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.group_keys[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.group_keys[index].expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_having_expression(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    if (stmt->select_plan.having_expression != NULL) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = clone_table_select_expression_node(stmt, stmt->select_plan.having_expression,
                                                        sql, sql_length, &clone);

        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.having_expression = clone;
    }
    return MYLITE_OK;
}

static int clone_table_select_order_expressions(mylite_stmt *stmt, const char *sql,
                                                size_t sql_length)
{
    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;
        int status = MYLITE_OK;

        if (stmt->select_plan.order_keys[index].kind != MYLITE_SELECT_ORDER_KEY_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.order_keys[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.order_keys[index].expression = clone;
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int collect_table_select_aggregate_bindings(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    mylite_select_plan_clear_aggregate_bindings(&stmt->select_plan);
    status = collect_table_select_expression_aggregate_bindings(stmt);
    if (status == MYLITE_OK && stmt->select_plan.having_expression != NULL) {
        status = collect_select_aggregate_bindings(
            stmt->database, stmt->select_plan.having_expression, &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = collect_table_select_order_aggregate_bindings(stmt);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int collect_table_select_expression_aggregate_bindings(mylite_stmt *stmt)
{
    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = MYLITE_OK;

        if (stmt->select_plan.outputs[index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
            continue;
        }
        status = collect_select_aggregate_bindings(
            stmt->database, stmt->select_plan.outputs[index].expression, &stmt->select_plan);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int collect_table_select_order_aggregate_bindings(mylite_stmt *stmt)
{
    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        int status = MYLITE_OK;

        if (stmt->select_plan.order_keys[index].kind != MYLITE_SELECT_ORDER_KEY_EXPRESSION) {
            continue;
        }
        status = collect_select_aggregate_bindings(
            stmt->database, stmt->select_plan.order_keys[index].expression, &stmt->select_plan);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    return MYLITE_OK;
}

static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node)
{
    int status =
        mylite_statement_clone_sql_ast_subtree(&stmt->select_predicate_ast, expression, source_sql,
                                               stmt->select_sql_text, sql_length, out_node);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    switch (kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_copy_statement_name(statement, &stmt->schema_name);
        if (status == MYLITE_OK) {
            status = mylite_schema_copy_options(statement, &stmt->options);
        }
        break;
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_copy_create_table_statement(statement, &stmt->create_table);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_copy_drop_table_statement(statement, &stmt->drop_table);
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_copy_rename_table_statement(statement, &stmt->rename_table);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_copy_truncate_table_statement(statement, &stmt->truncate_table);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_copy_alter_table_statement(statement, &stmt->alter_table);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_copy_create_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_copy_drop_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_copy_insert_values_statement(statement, &stmt->insert_values,
                                                         &stmt->insert_update);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_copy_insert_set_statement(statement, &stmt->insert_values,
                                                      &stmt->insert_set, &stmt->insert_update);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_copy_replace_values_statement(statement, &stmt->insert_values);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_copy_replace_set_statement(statement, &stmt->insert_values,
                                                       &stmt->insert_set);
        break;
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
        status = mylite_transaction_copy_statement(statement, stmt);
        break;
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_copy_savepoint_statement(statement, stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        status = copy_scalar_select_statement(statement, stmt);
        break;
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_SQLITE:
        status = MYLITE_UNSUPPORTED;
        break;
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    if (kind == MYLITE_STMT_CREATE_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_CREATE_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_statement_execute_custom(mylite_stmt *stmt)
{
    const struct mylite_dml_expression_callbacks dml_expression_callbacks = {
        .user_data = stmt,
        .eval_session_function = evaluate_dml_materialize_session_function,
        .set_where_predicate_eval_error = set_dml_materialize_where_predicate_eval_error,
    };
    int status = MYLITE_OK;

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return execute_scalar_select_statement(stmt);
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        return execute_table_select_statement(stmt);
    }
    if (stmt->kind == MYLITE_STMT_UNION_QUERY) {
        return execute_union_query_statement(stmt);
    }
    if (stmt->executed) {
        return MYLITE_DONE;
    }
    if (stmt->kind == MYLITE_STMT_REPLACE_VALUES || stmt->kind == MYLITE_STMT_REPLACE_SET) {
        status = mylite_dml_append_replace_delayed_warning(stmt);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }
    if (mylite_statement_kind_writes(stmt->kind) && stmt->database->transaction_active &&
        stmt->database->transaction_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
        stmt->affected_rows = -1;
        return mylite_transaction_set_read_only_error(stmt->database);
    }
    stmt->executed = true;

    switch (stmt->kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
        status = mylite_schema_execute_create_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_SCHEMA:
        status = mylite_schema_execute_alter_statement(stmt);
        break;
    case MYLITE_STMT_DROP_SCHEMA:
        status = mylite_schema_execute_drop_statement(stmt);
        break;
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_execute_use_statement(stmt);
        break;
    case MYLITE_STMT_SET_NAMES:
        status = mylite_connection_execute_set_names_statement(stmt);
        break;
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = mylite_connection_execute_set_character_set_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_execute_create_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->create_table,
            stmt->if_not_exists);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_execute_drop_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->drop_table, stmt->if_exists);
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_execute_rename_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_execute_truncate_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_execute_alter_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_execute_create_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_execute_drop_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_execute_insert_values_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_execute_insert_set_statement(stmt);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_execute_replace_values_statement(stmt);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_execute_replace_set_statement(stmt);
        break;
    case MYLITE_STMT_UPDATE:
        status = mylite_dml_execute_update_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_DELETE:
        status = mylite_dml_execute_delete_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_execute_statement(stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        return execute_scalar_select_statement(stmt);
    case MYLITE_STMT_TABLE_SELECT:
        return execute_table_select_statement(stmt);
    case MYLITE_STMT_UNION_QUERY:
        return execute_union_query_statement(stmt);
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
}

static int execute_scalar_select_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (!stmt->scalar_result.row_available || stmt->scalar_result.has_row) {
        return MYLITE_DONE;
    }

    status = evaluate_scalar_select_result(stmt);
    if (status != MYLITE_OK) {
        return status;
    }

    stmt->executed = true;
    stmt->database->warnings = stmt->scalar_result.warnings;
    stmt->scalar_result.warnings = (struct mylite_expression_warnings){0};
    stmt->affected_rows = -1;
    stmt->scalar_result.has_row = true;
    return MYLITE_ROW;
}

static int evaluate_scalar_select_result(mylite_stmt *stmt)
{
    for (size_t index = 0U; index < stmt->scalar_result.value_count; ++index) {
        int status = evaluate_scalar_select_result_item(stmt, index);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_scalar_select_result_item(mylite_stmt *stmt, size_t index)
{
    int status = MYLITE_OK;

    if (stmt->scalar_result.expressions[index] == NULL) {
        return MYLITE_OK;
    }

    mylite_expression_value_deinit(&stmt->scalar_result.values[index]);
    free(stmt->scalar_result.texts[index]);
    stmt->scalar_result.texts[index] = NULL;

    status = evaluate_scalar_select_expression(stmt, stmt->scalar_result.expressions[index],
                                               &stmt->scalar_result.values[index]);
    if (status != MYLITE_OK) {
        int warning_status = append_scalar_select_warnings_to_database(stmt);

        return warning_status != MYLITE_OK ? warning_status : status;
    }
    stmt->scalar_result.texts[index] =
        mylite_expression_value_to_text(&stmt->scalar_result.values[index]);
    if (stmt->scalar_result.values[index].kind != MYLITE_EXPRESSION_VALUE_NULL &&
        stmt->scalar_result.texts[index] == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int evaluate_scalar_select_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    return evaluate_statement_session_function((mylite_stmt *)user_data, function_call,
                                               expression_context, warnings, NULL, out_value);
}

static int
evaluate_union_session_function(void *user_data, const struct mylite_sql_ast_node *function_call,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;

    return evaluate_statement_session_function(context == NULL ? NULL : context->stmt,
                                               function_call, expression_context, warnings, NULL,
                                               out_value);
}

static int evaluate_dml_materialize_session_function(
    void *user_data, const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    return evaluate_statement_session_function((mylite_stmt *)user_data, function_call,
                                               expression_context, warnings, table, out_value);
}

static int set_dml_materialize_where_predicate_eval_error(void *user_data)
{
    return set_where_predicate_eval_error((mylite_stmt *)user_data);
}

static int evaluate_statement_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = NULL;
    int status = mylite_session_evaluate_core_function(stmt, function_call, expression_context,
                                                       warnings, out_value);

    if (status != -1) {
        return status;
    }
    name = mylite_ast_child_at(function_call, 0U);
    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return -1;
    }
    if (mylite_function_name_is_strcmp(name)) {
        return evaluate_strcmp_function(stmt, function_call, expression_context, warnings, table,
                                        out_value);
    }
    if (mylite_function_name_is_charset_collation_introspection(name)) {
        return evaluate_charset_collation_function(stmt, function_call, expression_context,
                                                   warnings, table, out_value);
    }
    return -1;
}

static int evaluate_strcmp_function(mylite_stmt *stmt,
                                    const struct mylite_sql_ast_node *function_call,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    const struct mylite_select_table *table,
                                    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *left_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *right_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_charset_collation_info collation_info =
        binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL ||
        mylite_sql_ast_node_child_count(arguments) != 2U) {
        return -1;
    }

    status =
        mylite_expression_eval_with_context(left_argument, expression_context, warnings, &left);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (left.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status =
        mylite_expression_eval_with_context(right_argument, expression_context, warnings, &right);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = infer_strcmp_collation_info(stmt, function_call, table, &collation_info);
    if (status == MYLITE_OK) {
        status = set_strcmp_function_result(stmt->database, &left, left_argument, &right,
                                            right_argument, &collation_info, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&right);
    mylite_expression_value_deinit(&left);
    return status;
}

static int infer_strcmp_collation_info(mylite_stmt *stmt,
                                       const struct mylite_sql_ast_node *function_call,
                                       const struct mylite_select_table *table,
                                       struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    struct mylite_expression_collation_context context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }

    status = infer_function_arguments_collation_info(stmt->database, &context, arguments, 0U, true,
                                                     out_info);
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_info->coercibility == mylite_mysql_coercibility_ignorable ||
        out_info->collation == NULL) {
        *out_info = connection_collation_info(stmt->database, mylite_mysql_coercibility_coercible);
    }
    return MYLITE_OK;
}

static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_strcmp_compare_options options =
        strcmp_compare_options_for_collation(collation_info);
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int status = strcmp_value_to_text(database, left, left_argument, &left_text, &left_length);

    if (status == MYLITE_OK) {
        status = strcmp_value_to_text(database, right, right_argument, &right_text, &right_length);
    }
    if (status == MYLITE_OK) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value =
                compare_strcmp_texts(left_text, left_length, right_text, right_length, options),
        };
    }

    free(right_text);
    free(left_text);
    return status;
}

static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length)
{
    enum { strcmp_text_buffer_size = 64 };
    const struct mylite_sql_ast_node *decimal_literal = NULL;
    char buffer[strcmp_text_buffer_size];
    bool negative_decimal = false;
    int length = 0;

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return -1;
    }
    decimal_literal = strcmp_decimal_literal_argument(argument, &negative_decimal);
    if (decimal_literal != NULL) {
        return strcmp_decimal_literal_to_text(database, decimal_literal, negative_decimal, out_text,
                                              out_length);
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);
        break;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_length = value->text_value == NULL ? 0U : value->text_length;
        *out_text =
            mylite_copy_span_text(value->text_value == NULL ? "" : value->text_value, *out_length);
        if (*out_text != NULL) {
            return MYLITE_OK;
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_length = (size_t)length;
    *out_text = mylite_copy_span_text(buffer, *out_length);
    if (*out_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative)
{
    bool negative = false;

    argument = unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        argument = unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (out_negative != NULL) {
        *out_negative = negative;
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        return NULL;
    }
    return argument;
}

static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length)
{
    const char *text = literal == NULL ? NULL : literal->span.text;
    size_t length = literal == NULL ? 0U : literal->span.length;
    const char *dot = NULL;
    size_t integer_length = 0U;
    size_t fractional_length = 0U;
    size_t integer_offset = 0U;
    size_t sign_length = 0U;
    size_t normalized_integer_length = 0U;
    size_t fractional_output_length = 0U;
    size_t result_length = 0U;
    size_t output = 0U;
    char *result = NULL;
    bool zero = false;

    if (text == NULL) {
        return -1;
    }
    dot = memchr(text, '.', length);
    if (dot == NULL) {
        return -1;
    }
    integer_length = (size_t)(dot - text);
    fractional_length = length - integer_length - 1U;
    zero = decimal_literal_span_is_zero(text, length);

    while (integer_offset < integer_length && text[integer_offset] == '0') {
        ++integer_offset;
    }
    normalized_integer_length = integer_length - integer_offset;
    if (normalized_integer_length == 0U) {
        normalized_integer_length = 1U;
    }
    sign_length = negative && !zero ? 1U : 0U;
    if (fractional_length > 0U) {
        if (fractional_length == SIZE_MAX) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        fractional_output_length = 1U + fractional_length;
    }
    if (normalized_integer_length > SIZE_MAX - sign_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length = sign_length + normalized_integer_length;
    if (result_length == SIZE_MAX || fractional_output_length >= SIZE_MAX - result_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length += fractional_output_length;
    result = malloc(result_length + 1U);
    if (result == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (negative && !zero) {
        result[output++] = '-';
    }
    if (integer_length == integer_offset) {
        result[output++] = '0';
    } else {
        memcpy(result + output, text + integer_offset, normalized_integer_length);
        output += normalized_integer_length;
    }
    if (fractional_length > 0U) {
        result[output++] = '.';
        memcpy(result + output, dot + 1, fractional_length);
        output += fractional_length;
    }
    result[output] = '\0';
    *out_text = result;
    *out_length = output;
    return MYLITE_OK;
}

static bool decimal_literal_span_is_zero(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] >= '1' && text[index] <= '9') {
            return false;
        }
    }
    return true;
}

static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length, struct mylite_strcmp_compare_options options)
{
    size_t compare_length = 0U;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    if (options.ignore_trailing_spaces) {
        trim_strcmp_trailing_spaces(left, &left_length);
        trim_strcmp_trailing_spaces(right, &right_length);
    }

    compare_length = left_length < right_length ? left_length : right_length;
    for (size_t index = 0U; index < compare_length; ++index) {
        unsigned char left_byte = strcmp_compare_byte((unsigned char)left[index], options);
        unsigned char right_byte = strcmp_compare_byte((unsigned char)right[index], options);

        if (left_byte != right_byte) {
            return left_byte > right_byte ? 1 : -1;
        }
    }
    return (left_length > right_length) - (left_length < right_length);
}

static void trim_strcmp_trailing_spaces(const char *text, size_t *length)
{
    if (text == NULL || length == NULL) {
        return;
    }
    while (*length > 0U && text[*length - 1U] == ' ') {
        *length -= 1U;
    }
}

static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options)
{
    if (!options.case_sensitive && value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}

static struct mylite_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;

    return (struct mylite_strcmp_compare_options){
        .ignore_trailing_spaces = strcmp_collation_ignores_trailing_spaces(collation_name),
        .case_sensitive = strcmp_collation_is_case_sensitive(info),
    };
}

static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name)
{
    const struct mylite_collation *collation = mylite_collation_lookup(collation_name);

    if (collation == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(collation->pad_attribute, "PAD SPACE");
}

static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;
    size_t collation_length = strlen(collation_name);

    if (info != NULL &&
        mylite_ascii_case_equal(info->character_set, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (mylite_ascii_case_equal(collation_name, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (collation_length < 4U) {
        return false;
    }
    return mylite_ascii_case_equal(collation_name + collation_length - 4U, "_bin");
}

static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(function_call, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_expression_collation_context collation_context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    struct mylite_charset_collation_info info =
        binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || argument == NULL) {
        return -1;
    }

    status = infer_expression_collation_info(stmt->database, &collation_context, argument, &info);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_expression_eval_with_context(argument, expression_context, warnings, &value);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    return set_charset_collation_function_result(stmt->database, name, &info, out_value);
}

static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value)
{
    if (mylite_function_name_is_charset(name)) {
        return mylite_session_set_text_function_value(database, info->character_set, out_value);
    }
    if (mylite_function_name_is_collation(name)) {
        return mylite_session_set_text_function_value(database, info->collation, out_value);
    }
    if (mylite_function_name_is_coercibility(name)) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = info->coercibility,
        };
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_expression_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(expression);

    if (node == NULL || out_info == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return infer_literal_collation_info(database, node, out_info);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return infer_identifier_collation_info(database, context, node, out_info);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return infer_function_collation_info(database, context, node, out_info);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return infer_cast_collation_info(database, node, out_info);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return infer_descriptor_collation_info(database, context, node,
                                               mylite_mysql_coercibility_coercible, out_info);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

static int infer_literal_collation_info(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_charset_collation_info *out_info)
{
    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_info = binary_collation_info(mylite_mysql_coercibility_ignorable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_STRING:
        *out_info = connection_collation_info(database, mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        *out_info = utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_info = binary_collation_info(mylite_mysql_coercibility_numeric);
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return infer_descriptor_collation_info(database, NULL, expression,
                                               mylite_mysql_coercibility_coercible, out_info);
    }
    return MYLITE_UNSUPPORTED;
}

static int infer_identifier_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression, struct mylite_charset_collation_info *out_info)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_UNSUPPORTED;

    if (context != NULL && context->table != NULL) {
        status =
            infer_table_identifier_descriptor(database, context->table, expression, &descriptor);
    } else if (context != NULL && context->plan != NULL) {
        status = infer_identifier_descriptor(database, context->plan, expression, &descriptor);
    }
    if (status != MYLITE_OK) {
        return status;
    }
    *out_info = descriptor_collation_info(&descriptor, mylite_mysql_coercibility_implicit);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_collation_info(mylite_db *database,
                                         const struct mylite_expression_collation_context *context,
                                         const struct mylite_sql_ast_node *expression,
                                         struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (mylite_function_name_is_char(name)) {
        return infer_char_function_collation_info(database, expression, out_info);
    }
    if (mylite_function_name_has_binary_string_result(name)) {
        *out_info = binary_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (mylite_function_name_has_connection_string_result(name)) {
        *out_info = connection_collation_info(database, mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (mylite_function_name_is_inet_ntoa(name)) {
        *out_info = connection_collation_info(database, mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (mylite_function_name_is_charset(name) || mylite_function_name_is_collation(name)) {
        *out_info = utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (function_name_has_binary_numeric_collation_result(name)) {
        *out_info = binary_collation_info(mylite_mysql_coercibility_numeric);
        return MYLITE_OK;
    }
    if (name != NULL && (mylite_span_equal_ci(name->span, "DATABASE") ||
                         mylite_span_equal_ci(name->span, "SCHEMA") ||
                         mylite_span_equal_ci(name->span, "VERSION"))) {
        *out_info = utf8mb3_general_collation_info(mylite_mysql_coercibility_system_constant);
        return MYLITE_OK;
    }
    if (name != NULL && (mylite_span_equal_ci(name->span, "USER") ||
                         mylite_span_equal_ci(name->span, "SESSION_USER") ||
                         mylite_span_equal_ci(name->span, "SYSTEM_USER") ||
                         mylite_span_equal_ci(name->span, "CURRENT_USER"))) {
        *out_info = utf8mb3_general_collation_info(mylite_mysql_coercibility_system_constant);
        return MYLITE_OK;
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "IF")) {
        return infer_function_arguments_collation_info(database, context, arguments, 1U, false,
                                                       out_info);
    }
    if (name != NULL &&
        (mylite_span_equal_ci(name->span, "IFNULL") || mylite_span_equal_ci(name->span, "NULLIF") ||
         mylite_span_equal_ci(name->span, "COALESCE"))) {
        return infer_function_arguments_collation_info(database, context, arguments, 0U, false,
                                                       out_info);
    }
    if (mylite_function_name_is_quote(name)) {
        return infer_quote_function_collation_info(database, context, arguments, out_info);
    }
    if (mylite_function_name_has_text_result(name) ||
        mylite_function_name_has_slice_string_result(name)) {
        size_t first_argument =
            mylite_function_name_is_make_set(name) || mylite_function_name_is_elt(name) ? 1U : 0U;

        return infer_function_arguments_collation_info(database, context, arguments, first_argument,
                                                       true, out_info);
    }

    return infer_descriptor_collation_info(database, context, expression,
                                           mylite_mysql_coercibility_coercible, out_info);
}

static int infer_char_function_collation_info(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *charset_node = mylite_ast_child_at(expression, 2U);
    char *charset_name = mylite_copy_schema_text_span(charset_node);

    if (charset_node == NULL) {
        *out_info = binary_collation_info(mylite_mysql_coercibility_coercible);
        return MYLITE_OK;
    }
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
        int status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);

        free(charset_name);
        return status;
    }
    *out_info = char_function_collation_info(charset_name);
    free(charset_name);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_quote_function_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, struct mylite_charset_collation_info *out_info)
{
    struct mylite_charset_collation_info source =
        binary_collation_info(mylite_mysql_coercibility_ignorable);
    int status = infer_expression_collation_info(database, context,
                                                 mylite_ast_child_at(arguments, 0U), &source);

    if (status != MYLITE_OK) {
        return status;
    }
    if (source.coercibility == mylite_mysql_coercibility_numeric &&
        mylite_ascii_case_equal(source.character_set, mylite_mysql_binary_charset_name)) {
        *out_info = latin1_swedish_collation_info(source.coercibility);
        return MYLITE_OK;
    }
    if (mylite_ascii_case_equal(source.character_set, mylite_mysql_binary_charset_name)) {
        *out_info = connection_collation_info(database, source.coercibility);
        return MYLITE_OK;
    }
    *out_info = source;
    return MYLITE_OK;
}

static bool
function_name_has_binary_numeric_collation_result(const struct mylite_sql_ast_node *name)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (mylite_function_name_is_coercibility(name) ||
        mylite_function_name_has_length_result(name) || mylite_function_name_is_bit_count(name) ||
        mylite_function_name_is_crc32(name) || mylite_function_name_is_inet_aton(name) ||
        mylite_function_name_is_is_uuid(name) || mylite_function_name_has_integer_result(name) ||
        mylite_function_name_is_strcmp(name)) {
        return true;
    }
    if (infer_code_search_function_descriptor(name, true, &descriptor) ||
        infer_list_index_function_descriptor(name, true, &descriptor)) {
        return true;
    }
    if (name == NULL) {
        return false;
    }
    if (mylite_span_equal_ci(name->span, "PI") || mylite_span_equal_ci(name->span, "MOD") ||
        mylite_function_name_is_exp(name) || mylite_function_name_is_logarithm(name) ||
        mylite_function_name_is_power(name) || mylite_function_name_is_sqrt(name) ||
        mylite_function_name_is_trigonometric(name) ||
        mylite_function_name_is_inverse_trigonometric(name) ||
        mylite_function_name_is_angle_conversion(name) ||
        mylite_span_equal_ci(name->span, "ISNULL") ||
        mylite_span_equal_ci(name->span, "LAST_INSERT_ID") ||
        mylite_span_equal_ci(name->span, "CONNECTION_ID")) {
        return true;
    }
    return mylite_span_equal_ci(name->span, "ROW_COUNT");
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *arguments, size_t first_argument, bool numeric_as_connection,
    struct mylite_charset_collation_info *out_info)
{
    struct mylite_charset_collation_info best =
        binary_collation_info(mylite_mysql_coercibility_ignorable);
    size_t index = 0U;
    bool saw_candidate = false;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL; argument = argument->next_sibling, ++index) {
        struct mylite_charset_collation_info current =
            binary_collation_info(mylite_mysql_coercibility_ignorable);
        int status = MYLITE_OK;

        if (index < first_argument) {
            continue;
        }
        status = infer_expression_collation_info(database, context, argument, &current);
        if (status != MYLITE_OK) {
            return status;
        }
        if (numeric_as_connection && current.coercibility == mylite_mysql_coercibility_numeric &&
            mylite_ascii_case_equal(current.character_set, mylite_mysql_binary_charset_name)) {
            current = connection_collation_info(database, mylite_mysql_coercibility_coercible);
        }
        if (current.coercibility == mylite_mysql_coercibility_ignorable) {
            continue;
        }
        if (!saw_candidate || current.coercibility < best.coercibility ||
            (current.coercibility == best.coercibility &&
             mylite_ascii_case_equal(current.character_set, mylite_mysql_binary_charset_name))) {
            best = current;
            saw_candidate = true;
        }
    }
    if (saw_candidate) {
        *out_info = best;
    } else {
        *out_info = binary_collation_info(mylite_mysql_coercibility_ignorable);
    }
    return MYLITE_OK;
}

static int infer_cast_collation_info(mylite_db *database,
                                     const struct mylite_sql_ast_node *expression,
                                     struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);

    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return MYLITE_UNSUPPORTED;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
        *out_info = binary_collation_info(mylite_mysql_coercibility_implicit);
        return MYLITE_OK;
    }
    if (target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        char *charset_name = NULL;

        if (!target->has_column_character_set) {
            *out_info = connection_collation_info(database, mylite_mysql_coercibility_implicit);
            return MYLITE_OK;
        }
        charset_name = mylite_copy_unquoted_span_text(target->column_character_set);
        if (charset_name == NULL) {
            return MYLITE_NOMEM;
        }
        if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
            int status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);

            free(charset_name);
            return status;
        }
        *out_info = char_function_collation_info(charset_name);
        out_info->coercibility = mylite_mysql_coercibility_implicit;
        free(charset_name);
        return MYLITE_OK;
    }
    return infer_descriptor_collation_info(database, NULL, expression,
                                           mylite_mysql_coercibility_coercible, out_info);
}

static int
infer_descriptor_collation_info(mylite_db *database,
                                const struct mylite_expression_collation_context *context,
                                const struct mylite_sql_ast_node *expression, int text_coercibility,
                                struct mylite_charset_collation_info *out_info)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status = infer_expression_descriptor(database, context == NULL ? NULL : context->plan,
                                             expression, NULL, &descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    *out_info = descriptor_collation_info(&descriptor, text_coercibility);
    return MYLITE_OK;
}

static int infer_table_identifier_descriptor(mylite_db *database,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_field_descriptor *out_descriptor)
{
    size_t column_index = table == NULL ? 0U : table->column_count;
    int status = table == NULL
                     ? MYLITE_UNSUPPORTED
                     : mylite_select_resolve_column_reference(table, expression, &column_index);

    (void)database;
    if (status != MYLITE_OK || table == NULL || column_index >= table->column_count) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status == MYLITE_OK ? MYLITE_UNSUPPORTED : status;
    }
    *out_descriptor = table->columns[column_index].descriptor;
    return MYLITE_OK;
}

static struct mylite_charset_collation_info binary_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_binary_charset_name,
        .collation = mylite_mysql_binary_charset_name,
        .coercibility = coercibility,
    };
}

static struct mylite_charset_collation_info connection_collation_info(const mylite_db *database,
                                                                      int coercibility)
{
    const char *character_set = database == NULL || database->character_set_connection == NULL
                                    ? mylite_charset_default_name()
                                    : database->character_set_connection;
    const char *collation = database == NULL || database->collation_connection == NULL
                                ? mylite_charset_default_collation_name()
                                : database->collation_connection;

    return (struct mylite_charset_collation_info){
        .character_set = character_set,
        .collation = collation,
        .coercibility = coercibility,
    };
}

static struct mylite_charset_collation_info latin1_swedish_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_latin1_charset_name,
        .collation = mylite_mysql_latin1_swedish_ci_collation_name,
        .coercibility = coercibility,
    };
}

static struct mylite_charset_collation_info utf8mb3_general_collation_info(int coercibility)
{
    return (struct mylite_charset_collation_info){
        .character_set = mylite_mysql_utf8mb3_charset_name,
        .collation = mylite_mysql_utf8mb3_general_ci_collation_name,
        .coercibility = coercibility,
    };
}

static struct mylite_charset_collation_info char_function_collation_info(const char *charset_name)
{
    const struct mylite_charset *charset = mylite_charset_lookup(charset_name);
    const struct mylite_collation *collation =
        charset == NULL ? NULL : mylite_collation_lookup(charset->default_collation);

    if (mylite_ascii_case_equal(charset_name, mylite_mysql_binary_charset_name)) {
        return binary_collation_info(mylite_mysql_coercibility_coercible);
    }
    if (mylite_ascii_case_equal(charset_name, "utf8")) {
        return utf8mb3_general_collation_info(mylite_mysql_coercibility_coercible);
    }
    if (mylite_ascii_case_equal(charset_name, mylite_mysql_ascii_charset_name)) {
        return (struct mylite_charset_collation_info){
            .character_set = mylite_mysql_ascii_charset_name,
            .collation = mylite_mysql_ascii_general_ci_collation_name,
            .coercibility = mylite_mysql_coercibility_coercible,
        };
    }
    if (charset != NULL && collation != NULL) {
        return (struct mylite_charset_collation_info){
            .character_set = charset->name,
            .collation = collation->name,
            .coercibility = mylite_mysql_coercibility_coercible,
        };
    }
    return binary_collation_info(mylite_mysql_coercibility_coercible);
}

static struct mylite_charset_collation_info
descriptor_collation_info(const struct mylite_field_descriptor *descriptor, int text_coercibility)
{
    const struct mylite_collation *collation = NULL;

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return binary_collation_info(mylite_mysql_coercibility_ignorable);
    }
    if (mylite_expression_descriptor_has_numeric_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_DATE || descriptor->type == MYLITE_FIELD_TYPE_TIME ||
        descriptor->type == MYLITE_FIELD_TYPE_DATETIME ||
        descriptor->type == MYLITE_FIELD_TYPE_TIMESTAMP ||
        descriptor->type == MYLITE_FIELD_TYPE_YEAR) {
        return binary_collation_info(mylite_mysql_coercibility_numeric);
    }
    if (descriptor->type != MYLITE_FIELD_TYPE_STRING &&
        descriptor->type != MYLITE_FIELD_TYPE_VAR_STRING &&
        descriptor->type != MYLITE_FIELD_TYPE_BLOB) {
        return binary_collation_info(mylite_mysql_coercibility_numeric);
    }
    collation = mylite_expression_descriptor_collation_lookup_id(descriptor->charset_id);
    if (collation == NULL) {
        return binary_collation_info(text_coercibility);
    }
    return (struct mylite_charset_collation_info){
        .character_set = collation->character_set,
        .collation = collation->name,
        .coercibility = text_coercibility,
    };
}

static int execute_table_select_statement(mylite_stmt *stmt)
{
    if (stmt->sqlite_stmt == NULL && mylite_select_plan_table_count(&stmt->select_plan) <= 1U) {
        return MYLITE_MISUSE;
    }
    stmt->executed = true;
    stmt->affected_rows = -1;

    int status = materialize_table_select_result(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        mylite_select_result_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status = mylite_select_eval_set_current_row(
        stmt, &stmt->select_result.rows[stmt->select_result.next_row],
        &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int execute_union_query_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    stmt->executed = true;
    stmt->affected_rows = -1;

    status = materialize_union_query_result(stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        mylite_select_result_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status = mylite_select_eval_set_current_row(
        stmt, &stmt->select_result.rows[stmt->select_result.next_row],
        &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int materialize_union_query_result(mylite_stmt *stmt)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings accumulated_warnings = {0};
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->union_plan.operand_count;
         ++index) {
        enum mylite_sql_ast_set_duplicate_mode duplicate_mode =
            index == 0U ? MYLITE_SQL_AST_SET_DUPLICATES_ALL
                        : stmt->union_plan.operators[index - 1U];

        status = scan_union_operand(stmt, stmt->union_plan.operands[index], duplicate_mode);
        if (append_and_clear_union_database_warnings(stmt->database, &accumulated_warnings) !=
            MYLITE_OK) {
            status = MYLITE_NOMEM;
        }
    }

    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_result.row_count;
             ++index) {
            status = evaluate_union_order_values(stmt, &stmt->select_result.rows[index]);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                    &stmt->select_plan);
        }
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }

    if (append_and_clear_union_database_warnings(stmt->database, &accumulated_warnings) !=
        MYLITE_OK) {
        status = MYLITE_NOMEM;
    }
    stmt->database->warnings = saved_warnings;
    if (append_subquery_warnings(&stmt->database->warnings, &accumulated_warnings) != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&accumulated_warnings);
    return status;
}

static int scan_union_operand(mylite_stmt *stmt, mylite_stmt *operand,
                              enum mylite_sql_ast_set_duplicate_mode duplicate_mode)
{
    bool distinct = duplicate_mode == MYLITE_SQL_AST_SET_DUPLICATES_DISTINCT;

    if (distinct) {
        int status = deduplicate_union_result_rows(stmt);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    for (;;) {
        int status = execute_union_operand_statement(operand);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        status = append_union_operand_current_row(stmt, operand, distinct);
        if (status != MYLITE_OK) {
            return status;
        }
    }
}

static int append_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand, bool distinct)
{
    struct mylite_table_select_row row = {0};
    int status = copy_union_operand_current_row(stmt, operand, &row);

    if (status == MYLITE_OK) {
        if (distinct) {
            status = append_union_distinct_row(stmt, &row);
        } else {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
        }
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int execute_union_operand_statement(mylite_stmt *operand)
{
    if (operand == NULL) {
        return MYLITE_MISUSE;
    }

    switch (operand->kind) {
    case MYLITE_STMT_SQLITE: {
        int rc = sqlite3_step(operand->sqlite_stmt);

        if (rc == SQLITE_ROW) {
            return MYLITE_ROW;
        }
        if (rc == SQLITE_DONE) {
            return MYLITE_DONE;
        }
        return mylite_diagnostics_set_sqlite_error(operand->database);
    }
    case MYLITE_STMT_SCALAR_SELECT:
        return execute_scalar_select_statement(operand);
    case MYLITE_STMT_TABLE_SELECT:
        return execute_table_select_statement(operand);
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_ALTER_TABLE:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_REPLACE_VALUES:
    case MYLITE_STMT_REPLACE_SET:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int copy_union_operand_current_row(mylite_stmt *stmt, mylite_stmt *operand,
                                          struct mylite_table_select_row *out_row)
{
    size_t column_count = stmt->select_plan.output_count;

    *out_row = (struct mylite_table_select_row){0};
    out_row->output_values = calloc(column_count, sizeof(*out_row->output_values));
    if (out_row->output_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->output_value_count = column_count;

    for (size_t index = 0U; index < column_count; ++index) {
        int status =
            copy_subquery_statement_row_value(operand, index, &out_row->output_values[index]);

        if (status != MYLITE_OK) {
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            }
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_union_distinct_row(mylite_stmt *stmt, struct mylite_table_select_row *row)
{
    if (mylite_select_result_distinct_row_exists(&stmt->select_result, &stmt->select_plan,
                                                 &stmt->result_metadata, row)) {
        mylite_select_row_deinit(row);
        return MYLITE_OK;
    }
    return mylite_select_result_append_row(stmt->database, &stmt->select_result, row);
}

static int deduplicate_union_result_rows(mylite_stmt *stmt)
{
    size_t kept = 0U;

    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        bool duplicate = false;

        for (size_t compare = 0U; compare < kept; ++compare) {
            if (mylite_select_output_values_equal(&stmt->select_plan, &stmt->result_metadata,
                                                  &stmt->select_result.rows[compare],
                                                  &stmt->select_result.rows[index])) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            mylite_select_row_deinit(&stmt->select_result.rows[index]);
            continue;
        }
        if (kept != index) {
            stmt->select_result.rows[kept] = stmt->select_result.rows[index];
            stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
        }
        ++kept;
    }
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static int evaluate_union_order_values(mylite_stmt *stmt, struct mylite_table_select_row *row)
{
    row->order_values = calloc(stmt->select_plan.order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = stmt->select_plan.order_key_count;

    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        int status = evaluate_union_order_key(stmt, row, &stmt->select_plan.order_keys[index],
                                              &row->order_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_union_order_key(mylite_stmt *stmt, const struct mylite_table_select_row *row,
                                    const struct mylite_select_order_key *order_key,
                                    struct mylite_expression_value *out_value)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        if (order_key->output_index >= row->output_value_count ||
            mylite_expression_value_copy(&row->output_values[order_key->output_index], out_value) !=
                0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    {
        struct mylite_union_expression_context user_context = {
            .stmt = stmt,
            .row = row,
        };
        struct mylite_expression_eval_context context = {
            .user_data = &user_context,
            .resolve_identifier = resolve_union_expression_identifier,
            .eval_session_function = evaluate_union_session_function,
        };
        int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                         &stmt->database->warnings, out_value);

        if (status == 0) {
            return MYLITE_OK;
        }
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (stmt->database->error_message != NULL) {
            return MYLITE_EXEC_ERROR;
        }
    }
    return set_select_unsupported_order_error(stmt->database);
}

static int resolve_union_expression_identifier(void *user_data,
                                               const struct mylite_sql_ast_node *identifier,
                                               struct mylite_expression_value *out_value)
{
    struct mylite_union_expression_context *context = user_data;
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->stmt == NULL || context->row == NULL) {
        return -1;
    }

    status = mylite_copy_identifier_parts(identifier, parts, &part_count);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return -1;
    }
    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches =
            select_output_label_count(&context->stmt->select_plan, parts[0], &output_index);

        if (output_matches == 1U && output_index < context->row->output_value_count) {
            status = mylite_expression_value_copy(&context->row->output_values[output_index],
                                                  out_value) == 0
                         ? 0
                         : MYLITE_NOMEM;
        } else {
            status = -1;
        }
    } else {
        status = -1;
    }

    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(context->stmt->database, "out of memory");
    }
    return status;
}

static int append_and_clear_union_database_warnings(mylite_db *database,
                                                    struct mylite_expression_warnings *warnings)
{
    struct mylite_expression_warnings current = database->warnings;
    int status = MYLITE_OK;

    database->warnings = (struct mylite_expression_warnings){0};
    status = append_subquery_warnings(warnings, &current);
    mylite_expression_warnings_deinit(&current);
    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int materialize_table_select_result(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (mylite_select_plan_table_count(&stmt->select_plan) > 1U) {
        status = materialize_joined_table_select_result(stmt);
    } else if (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
               stmt->select_plan.has_having) {
        status = materialize_aggregate_table_select_result(stmt);
    } else if (stmt->select_plan.order_key_count != 0U) {
        status = materialize_ordered_table_select_result(stmt);
    } else {
        status = materialize_unordered_table_select_result(stmt);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}

static int materialize_ordered_table_select_result(mylite_stmt *stmt)
{
    int status = mylite_select_eval_constant_predicate(stmt, &table_select_eval_callbacks);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = evaluate_table_select_row_matches(stmt, &row, &matches);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }

        if (mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode)) {
            bool duplicate = false;

            status = check_table_select_distinct_duplicate(stmt, &row, &duplicate);
            if (status != MYLITE_OK) {
                mylite_select_row_deinit(&row);
                return status;
            }
            if (duplicate) {
                mylite_select_row_deinit(&row);
                continue;
            }
        }
        status = mylite_select_eval_order_values(stmt, &row, &table_select_eval_callbacks);
        if (status == MYLITE_OK) {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
        }
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }

    status =
        mylite_select_result_sort_rows(stmt->database, &stmt->select_result, &stmt->select_plan);
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    return status;
}

static int materialize_unordered_table_select_result(mylite_stmt *stmt)
{
    struct mylite_unordered_table_select_append_state append_state = {0};
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = evaluate_table_select_row_matches(stmt, &row, &matches);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }
        status = append_unordered_table_select_matched_row(stmt, &row, &append_state, distinct);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (append_state.stop) {
            break;
        }
    }
    if (rc != SQLITE_DONE &&
        !mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        return mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    if (distinct) {
        return mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }
    return MYLITE_OK;
}

static int
append_unordered_table_select_matched_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state,
                                          bool distinct)
{
    if (distinct) {
        return append_unordered_table_select_distinct_row(stmt, row);
    }
    return append_unordered_table_select_limited_row(stmt, row, state);
}

static int append_unordered_table_select_distinct_row(mylite_stmt *stmt,
                                                      struct mylite_table_select_row *row)
{
    bool duplicate = false;
    int status = check_table_select_distinct_duplicate(stmt, row, &duplicate);

    if (status != MYLITE_OK) {
        return status;
    }
    if (duplicate) {
        mylite_select_row_deinit(row);
        return MYLITE_OK;
    }
    return mylite_select_result_append_row(stmt->database, &stmt->select_result, row);
}

static int
append_unordered_table_select_limited_row(mylite_stmt *stmt, struct mylite_table_select_row *row,
                                          struct mylite_unordered_table_select_append_state *state)
{
    if (mylite_select_limit_row_is_kept(&stmt->select_plan.limit,
                                        (struct mylite_select_limit_position){
                                            .matched_row = state->matched_row,
                                            .kept_count = stmt->select_result.row_count,
                                        })) {
        int status = mylite_select_result_append_row(stmt->database, &stmt->select_result, row);

        if (status != MYLITE_OK) {
            return status;
        }
    } else {
        mylite_select_row_deinit(row);
    }
    if (state->matched_row != UINT64_MAX) {
        ++state->matched_row;
    }
    state->stop =
        mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count);
    return MYLITE_OK;
}

static int materialize_joined_table_select_result(mylite_stmt *stmt)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    struct mylite_table_select_row row = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

    if (mylite_select_plan_has_outer_join(&stmt->select_plan)) {
        return materialize_outer_joined_table_select_result(stmt);
    }

    if (!aggregate_query && stmt->select_plan.order_key_count == 0U &&
        stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }
    if (table_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    state.rowsets = calloc(table_count, sizeof(*state.rowsets));
    if (state.rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    row.value_count = mylite_select_plan_column_count(&stmt->select_plan);
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            mylite_select_rowsets_deinit(state.rowsets, table_count);
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    status = mylite_select_load_join_rowsets(stmt, state.rowsets);
    if (status == MYLITE_OK) {
        status = scan_joined_table_select_rows(stmt, &state, &row);
    }

    if (status == MYLITE_OK && aggregate_query && state.group_count == 0U &&
        !stmt->select_plan.has_group_by) {
        status = append_empty_implicit_table_select_group(stmt, &state.groups, &state.group_count);
    }
    if (status == MYLITE_OK && aggregate_query) {
        status = append_finalized_table_select_groups(stmt, state.groups, state.group_count);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK &&
        (aggregate_query || stmt->select_plan.order_key_count != 0U || distinct)) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    for (size_t index = 0U; index < state.group_count; ++index) {
        table_select_group_deinit(&state.groups[index]);
    }
    free(state.groups);
    mylite_select_row_deinit(&row);
    mylite_select_join_condition_cache_deinit(&state.condition_cache);
    mylite_select_rowsets_deinit(state.rowsets, table_count);
    return status;
}

static int materialize_outer_joined_table_select_result(mylite_stmt *stmt)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_materialize_state state = {0};
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);
    int status = MYLITE_OK;

    if (!aggregate_query && stmt->select_plan.order_key_count == 0U &&
        stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = mylite_select_eval_constant_predicate(stmt, &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }
    if (table_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    state.rowsets = calloc(table_count, sizeof(*state.rowsets));
    if (state.rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_select_load_join_rowsets(stmt, state.rowsets);
    if (status == MYLITE_OK) {
        status = scan_outer_joined_table_select_rows(stmt, &state);
    }

    if (status == MYLITE_OK && aggregate_query && state.group_count == 0U &&
        !stmt->select_plan.has_group_by) {
        status = append_empty_implicit_table_select_group(stmt, &state.groups, &state.group_count);
    }
    if (status == MYLITE_OK && aggregate_query) {
        status = append_finalized_table_select_groups(stmt, state.groups, state.group_count);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK &&
        (aggregate_query || stmt->select_plan.order_key_count != 0U || distinct)) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    for (size_t index = 0U; index < state.group_count; ++index) {
        table_select_group_deinit(&state.groups[index]);
    }
    free(state.groups);
    mylite_select_join_condition_cache_deinit(&state.condition_cache);
    mylite_select_rowsets_deinit(state.rowsets, table_count);
    return status;
}

static int scan_joined_table_select_rows(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         struct mylite_table_select_row *row)
{
    size_t table_count = mylite_select_plan_table_count(&stmt->select_plan);
    struct mylite_table_select_join_scan_state scan = {
        .row = row,
        .table_count = table_count,
    };
    bool finished = false;
    int status = MYLITE_OK;

    if (state->stop) {
        return MYLITE_OK;
    }

    if (table_count == 0U) {
        return process_joined_table_select_full_row(stmt, state, row);
    }

    scan.frames = calloc(table_count, sizeof(*scan.frames));
    if (scan.frames == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    while (!finished && !state->stop) {
        status = advance_joined_table_select_scan(stmt, state, &scan, &finished);
        if (status != MYLITE_OK) {
            break;
        }
    }

    clear_joined_table_select_scan_copies(stmt, &scan);
    free(scan.frames);
    return status;
}

static int
scan_outer_joined_table_select_rows(mylite_stmt *stmt,
                                    struct mylite_table_select_join_materialize_state *state)
{
    size_t range_count = stmt->select_plan.from_range_count;
    struct mylite_table_select_table_rowset *range_rowsets = NULL;
    int status = MYLITE_OK;

    if (range_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    range_rowsets = calloc(range_count, sizeof(*range_rowsets));
    if (range_rowsets == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t range_index = 0U; status == MYLITE_OK && range_index < range_count; ++range_index) {
        struct mylite_select_table_range range = stmt->select_plan.from_ranges[range_index];

        status =
            materialize_select_from_range_rowset(stmt, state, range, &range_rowsets[range_index]);
    }
    if (status == MYLITE_OK) {
        status = process_outer_joined_table_range_rows(stmt, state, range_rowsets, range_count);
    }

    for (size_t index = 0U; index < range_count; ++index) {
        mylite_select_rowset_deinit(&range_rowsets[index]);
    }
    free(range_rowsets);
    return status;
}

static int materialize_select_from_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset)
{
    int status = materialize_select_base_range_rowset(stmt, state, range, out_rowset);

    for (size_t index = 0U; status == MYLITE_OK && index < stmt->select_plan.join_step_count;
         ++index) {
        const struct mylite_select_join_step *step = &stmt->select_plan.join_steps[index];

        if (!mylite_select_join_step_is_in_range(step, range)) {
            continue;
        }
        status = apply_select_join_step_to_rowset(stmt, state, step, out_rowset);
    }
    return status;
}

static int materialize_select_base_range_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_select_table_range range, struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, range.first_table);

    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t row_index = 0U; row_index < state->rowsets[range.first_table].row_count;
         ++row_index) {
        struct mylite_table_select_row *row = NULL;
        int status = append_empty_joined_table_select_row(stmt, out_rowset, &row);

        if (status == MYLITE_OK) {
            status = copy_select_base_table_row_values(
                stmt->database, row, table, range.first_table,
                &state->rowsets[range.first_table].rows[row_index], row_index);
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_select_join_step_to_rowset(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, struct mylite_table_select_table_rowset *rowset)
{
    const struct mylite_table_select_table_rowset *right =
        &state->rowsets[step->right_range.first_table];
    struct mylite_table_select_table_rowset next = {0};
    bool *right_matched = NULL;
    int status = MYLITE_OK;

    if (step->join_type == MYLITE_SQL_AST_JOIN_RIGHT && right->row_count != 0U) {
        right_matched = calloc(right->row_count, sizeof(*right_matched));
        if (right_matched == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    status = append_select_join_step_matches(stmt, state, step, rowset, right_matched, &next);
    if (status == MYLITE_OK && step->join_type == MYLITE_SQL_AST_JOIN_RIGHT) {
        status = append_select_null_extended_right_rows(stmt, step, right, right_matched, &next);
    }

    free(right_matched);
    if (status != MYLITE_OK) {
        mylite_select_rowset_deinit(&next);
        return status;
    }
    mylite_select_rowset_deinit(rowset);
    *rowset = next;
    return MYLITE_OK;
}

static int append_select_join_step_matches(mylite_stmt *stmt,
                                           struct mylite_table_select_join_materialize_state *state,
                                           const struct mylite_select_join_step *step,
                                           const struct mylite_table_select_table_rowset *left,
                                           bool *right_matched,
                                           struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_table_select_table_rowset *right =
        &state->rowsets[step->right_range.first_table];

    for (size_t left_index = 0U; left_index < left->row_count; ++left_index) {
        struct mylite_select_join_match_tracking tracking = {
            .left_matched = false,
            .right_matched = right_matched,
        };
        int status = append_select_join_step_left_matches(
            stmt, state, step, &left->rows[left_index], right, &tracking, out_rowset);

        if (status != MYLITE_OK) {
            return status;
        }
        if (!tracking.left_matched && step->join_type == MYLITE_SQL_AST_JOIN_LEFT) {
            status =
                append_select_null_extended_left_row(stmt, &left->rows[left_index], out_rowset);
            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_join_step_left_matches(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_join_step *step, const struct mylite_table_select_row *left_row,
    const struct mylite_table_select_table_rowset *right,
    struct mylite_select_join_match_tracking *tracking,
    struct mylite_table_select_table_rowset *out_rowset)
{
    tracking->left_matched = false;
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        struct mylite_select_join_row_pair rows = {
            .left = left_row,
            .right = &right->rows[right_index],
            .right_index = right_index,
        };
        bool matches = false;
        int status = append_select_join_step_match(stmt, state, step, &rows, &matches, out_rowset);

        if (status != MYLITE_OK) {
            return status;
        }
        if (matches) {
            tracking->left_matched = true;
            if (tracking->right_matched != NULL) {
                tracking->right_matched[right_index] = true;
            }
        }
    }
    return MYLITE_OK;
}

static int append_select_join_step_match(mylite_stmt *stmt,
                                         struct mylite_table_select_join_materialize_state *state,
                                         const struct mylite_select_join_step *step,
                                         const struct mylite_select_join_row_pair *rows,
                                         bool *out_matches,
                                         struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *right_table =
        mylite_select_plan_table_const(&stmt->select_plan, step->right_range.first_table);
    struct mylite_table_select_row candidate = {0};
    int status = mylite_select_row_copy(rows->left, &candidate);

    *out_matches = false;
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (right_table == NULL) {
        mylite_select_row_deinit(&candidate);
        return MYLITE_UNSUPPORTED;
    }

    status = copy_select_base_table_row_values(stmt->database, &candidate, right_table,
                                               step->right_range.first_table, rows->right,
                                               rows->right_index);
    if (status == MYLITE_OK) {
        status =
            evaluate_table_select_join_step_conditions(stmt, state, &candidate, step, out_matches);
    }
    if (status == MYLITE_OK && *out_matches) {
        status = mylite_select_rowset_append_row(stmt->database, out_rowset, &candidate);
    }
    mylite_select_row_deinit(&candidate);
    return status;
}

static int append_select_null_extended_left_row(mylite_stmt *stmt,
                                                const struct mylite_table_select_row *left_row,
                                                struct mylite_table_select_table_rowset *out_rowset)
{
    return mylite_select_rowset_append_row_copy(stmt->database, out_rowset, left_row);
}

static int append_select_null_extended_right_rows(
    mylite_stmt *stmt, const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right, const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset)
{
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        if (right_matched != NULL && right_matched[right_index]) {
            continue;
        }
        int status = append_select_null_extended_right_row(stmt, step, &right->rows[right_index],
                                                           right_index, out_rowset);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
append_select_null_extended_right_row(mylite_stmt *stmt, const struct mylite_select_join_step *step,
                                      const struct mylite_table_select_row *right_row,
                                      size_t right_row_index,
                                      struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *right_table =
        mylite_select_plan_table_const(&stmt->select_plan, step->right_range.first_table);
    struct mylite_table_select_row *row = NULL;
    int status = append_empty_joined_table_select_row(stmt, out_rowset, &row);

    if (status == MYLITE_OK && right_table == NULL) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = copy_select_base_table_row_values(stmt->database, row, right_table,
                                                   step->right_range.first_table, right_row,
                                                   right_row_index);
    }
    return status;
}

static int append_empty_joined_table_select_row(mylite_stmt *stmt,
                                                struct mylite_table_select_table_rowset *rowset,
                                                struct mylite_table_select_row **out_row)
{
    struct mylite_table_select_row row = {
        .value_count = mylite_select_plan_column_count(&stmt->select_plan),
        .source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan),
    };
    int status = MYLITE_OK;

    *out_row = NULL;
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_row_index_count != 0U) {
        row.source_row_indexes =
            calloc(row.source_row_index_count, sizeof(*row.source_row_indexes));
        if (row.source_row_indexes == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return status;
    }
    for (size_t index = 0U; index < row.source_row_index_count; ++index) {
        row.source_row_indexes[index] = SIZE_MAX;
    }

    status = mylite_select_rowset_append_row(stmt->database, rowset, &row);
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        return status;
    }
    *out_row = &rowset->rows[rowset->row_count - 1U];
    return MYLITE_OK;
}

static int copy_select_base_table_row_values(mylite_db *database,
                                             struct mylite_table_select_row *row,
                                             const struct mylite_select_table *table,
                                             size_t table_index,
                                             const struct mylite_table_select_row *source,
                                             size_t source_row_index)
{
    if (table == NULL || row->source_row_index_count <= table_index) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count) {
            return MYLITE_UNSUPPORTED;
        }
        mylite_expression_value_deinit(&row->values[column_index]);
        if (mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    row->source_row_indexes[table_index] = source_row_index;
    return MYLITE_OK;
}

static int copy_select_row_range_values(struct mylite_table_select_row *target,
                                        const struct mylite_table_select_row *source,
                                        struct mylite_select_table_range range,
                                        const struct mylite_select_plan *plan)
{
    size_t last_table = range.first_table + range.table_count;

    for (size_t table_index = range.first_table; table_index < last_table; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table == NULL || table_index >= target->source_row_index_count ||
            table_index >= source->source_row_index_count) {
            return MYLITE_UNSUPPORTED;
        }
        target->source_row_indexes[table_index] = source->source_row_indexes[table_index];
        for (size_t column = 0U; column < table->column_count; ++column) {
            size_t column_index = table->first_column_index + column;

            if (column_index >= target->value_count || column_index >= source->value_count) {
                return MYLITE_UNSUPPORTED;
            }
            mylite_expression_value_deinit(&target->values[column_index]);
            if (mylite_expression_value_copy(&source->values[column_index],
                                             &target->values[column_index]) != 0) {
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

static int process_outer_joined_table_range_rows(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_table_rowset *range_rowsets, size_t range_count)
{
    size_t *row_indexes = NULL;
    bool finished = false;
    int status = MYLITE_OK;

    for (size_t range_index = 0U; range_index < range_count; ++range_index) {
        if (range_rowsets[range_index].row_count == 0U) {
            return MYLITE_OK;
        }
    }

    row_indexes = calloc(range_count, sizeof(*row_indexes));
    if (row_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    while (!finished && !state->stop) {
        status = process_outer_joined_table_range_row(stmt, state, range_rowsets, row_indexes,
                                                      range_count);
        if (status != MYLITE_OK) {
            break;
        }

        for (size_t index = range_count; index > 0U; --index) {
            size_t range_index = index - 1U;

            ++row_indexes[range_index];
            if (row_indexes[range_index] < range_rowsets[range_index].row_count) {
                break;
            }
            row_indexes[range_index] = 0U;
            if (range_index == 0U) {
                finished = true;
            }
        }
    }

    free(row_indexes);
    return status;
}

static int
process_outer_joined_table_range_row(mylite_stmt *stmt,
                                     struct mylite_table_select_join_materialize_state *state,
                                     const struct mylite_table_select_table_rowset *range_rowsets,
                                     const size_t *row_indexes, size_t range_count)
{
    struct mylite_table_select_row row = {0};
    int status = MYLITE_OK;

    row.value_count = mylite_select_plan_column_count(&stmt->select_plan);
    row.source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan);
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_row_index_count != 0U) {
        row.source_row_indexes =
            calloc(row.source_row_index_count, sizeof(*row.source_row_indexes));
        if (row.source_row_indexes == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return status;
    }
    for (size_t index = 0U; index < row.source_row_index_count; ++index) {
        row.source_row_indexes[index] = SIZE_MAX;
    }

    for (size_t range_index = 0U; status == MYLITE_OK && range_index < range_count; ++range_index) {
        struct mylite_select_table_range range = stmt->select_plan.from_ranges[range_index];

        status = copy_select_row_range_values(
            &row, &range_rowsets[range_index].rows[row_indexes[range_index]], range,
            &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = process_joined_table_select_full_row(stmt, state, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int evaluate_table_select_join_step_conditions(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row, const struct mylite_select_join_step *step,
    bool *out_matches)
{
    size_t available_table_count = step->joined_range.first_table + step->joined_range.table_count;
    struct mylite_select_table_range cache_range = {0};
    struct mylite_table_select_join_condition_cache_lookup lookup = {
        .found = false,
        .matches = false,
    };
    int status = MYLITE_OK;

    if (!mylite_select_join_cache_stage_range(stmt->database, &stmt->select_plan,
                                              available_table_count, &cache_range)) {
        return evaluate_table_select_join_stage_conditions_uncached(
            stmt, row, available_table_count, out_matches);
    }

    status =
        mylite_select_join_cache_lookup_row(&state->condition_cache, row, cache_range, &lookup);
    if (status != MYLITE_OK) {
        return status;
    }
    if (lookup.found) {
        *out_matches = lookup.matches;
        return MYLITE_OK;
    }

    status = evaluate_table_select_join_stage_conditions_uncached(stmt, row, available_table_count,
                                                                  out_matches);
    if (status == MYLITE_OK) {
        status = mylite_select_join_cache_store_row(stmt->database, &state->condition_cache, row,
                                                    cache_range, *out_matches);
    }
    return status;
}

static int advance_joined_table_select_scan(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, bool *out_finished)
{
    const struct mylite_select_table *table =
        mylite_select_plan_table_const(&stmt->select_plan, scan->table_index);
    const struct mylite_table_select_table_rowset *rowset = &state->rowsets[scan->table_index];

    *out_finished = false;
    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (scan->frames[scan->table_index].row_index >= rowset->row_count) {
        return backtrack_joined_table_select_scan(stmt, scan, out_finished);
    }
    return process_joined_table_select_scan_row(stmt, state, scan, table, rowset);
}

static int backtrack_joined_table_select_scan(mylite_stmt *stmt,
                                              struct mylite_table_select_join_scan_state *scan,
                                              bool *out_finished)
{
    const struct mylite_select_table *table = NULL;

    scan->frames[scan->table_index].row_index = 0U;
    if (scan->table_index == 0U) {
        *out_finished = true;
        return MYLITE_OK;
    }

    --scan->table_index;
    table = mylite_select_plan_table_const(&stmt->select_plan, scan->table_index);
    if (table == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    clear_joined_table_select_scan_frame(scan, table, scan->table_index);
    ++scan->frames[scan->table_index].row_index;
    return MYLITE_OK;
}

static int process_joined_table_select_scan_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    struct mylite_table_select_join_scan_state *scan, const struct mylite_select_table *table,
    const struct mylite_table_select_table_rowset *rowset)
{
    bool matches = false;
    int status = copy_joined_table_select_row_values(
        scan->row, table, &rowset->rows[scan->frames[scan->table_index].row_index]);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }

    scan->frames[scan->table_index].copied = true;
    status = evaluate_table_select_join_stage_conditions(stmt, state, scan, &matches);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!matches) {
        clear_joined_table_select_scan_frame(scan, table, scan->table_index);
        ++scan->frames[scan->table_index].row_index;
        return MYLITE_OK;
    }

    if (scan->table_index + 1U == scan->table_count) {
        status = process_joined_table_select_full_row(stmt, state, scan->row);
        clear_joined_table_select_scan_frame(scan, table, scan->table_index);
        ++scan->frames[scan->table_index].row_index;
        return status;
    }

    ++scan->table_index;
    scan->frames[scan->table_index].row_index = 0U;
    scan->frames[scan->table_index].copied = false;
    return MYLITE_OK;
}

static void clear_joined_table_select_scan_frame(struct mylite_table_select_join_scan_state *scan,
                                                 const struct mylite_select_table *table,
                                                 size_t table_index)
{
    if (!scan->frames[table_index].copied) {
        return;
    }
    clear_joined_table_select_row_values(scan->row, table);
    scan->frames[table_index].copied = false;
}

static void
clear_joined_table_select_scan_copies(mylite_stmt *stmt,
                                      const struct mylite_table_select_join_scan_state *scan)
{
    for (size_t index = 0U; index < scan->table_count; ++index) {
        const struct mylite_select_table *table =
            mylite_select_plan_table_const(&stmt->select_plan, index);

        if (table != NULL && scan->frames[index].copied) {
            clear_joined_table_select_row_values(scan->row, table);
        }
    }
}

static int copy_joined_table_select_row_values(struct mylite_table_select_row *row,
                                               const struct mylite_select_table *table,
                                               const struct mylite_table_select_row *source)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count ||
            mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            for (size_t copied = 0U; copied < index; ++copied) {
                mylite_expression_value_deinit(&row->values[table->first_column_index + copied]);
            }
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static void clear_joined_table_select_row_values(struct mylite_table_select_row *row,
                                                 const struct mylite_select_table *table)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index < row->value_count) {
            mylite_expression_value_deinit(&row->values[column_index]);
        }
    }
}

static int evaluate_table_select_join_stage_conditions(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_join_scan_state *scan, bool *out_matches)
{
    struct mylite_select_table_range cache_range = {0};
    struct mylite_table_select_join_condition_cache_lookup lookup = {
        .found = false,
        .matches = false,
    };
    int status = MYLITE_OK;

    if (!mylite_select_join_cache_stage_range(stmt->database, &stmt->select_plan,
                                              scan->table_index + 1U, &cache_range)) {
        return evaluate_table_select_join_stage_conditions_uncached(
            stmt, scan->row, scan->table_index + 1U, out_matches);
    }

    status =
        mylite_select_join_cache_lookup_scan(&state->condition_cache, scan, cache_range, &lookup);
    if (status != MYLITE_OK) {
        return status;
    }
    if (lookup.found) {
        *out_matches = lookup.matches;
        return MYLITE_OK;
    }

    status = evaluate_table_select_join_stage_conditions_uncached(
        stmt, scan->row, scan->table_index + 1U, out_matches);
    if (status == MYLITE_OK) {
        status = mylite_select_join_cache_store_scan(stmt->database, &state->condition_cache, scan,
                                                     cache_range, *out_matches);
    }
    return status;
}

static int evaluate_table_select_join_stage_conditions_uncached(
    mylite_stmt *stmt, const struct mylite_table_select_row *row, size_t available_table_count,
    bool *out_matches)
{
    int status =
        evaluate_table_select_using_stage_conditions(stmt, row, available_table_count, out_matches);

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    return evaluate_table_select_join_stage_predicates(stmt, row, available_table_count,
                                                       out_matches);
}

static int evaluate_table_select_using_stage_conditions(mylite_stmt *stmt,
                                                        const struct mylite_table_select_row *row,
                                                        size_t available_table_count,
                                                        bool *out_matches)
{
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.using_column_count; ++index) {
        const struct mylite_select_join_using_column *column =
            &stmt->select_plan.using_columns[index];

        if (column->first_table + column->table_count != available_table_count) {
            continue;
        }
        int status = evaluate_table_select_using_column(stmt, row, column, out_matches);

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_join_stage_predicates(mylite_stmt *stmt,
                                                       const struct mylite_table_select_row *row,
                                                       size_t available_table_count,
                                                       bool *out_matches)
{
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        const struct mylite_select_join_predicate *predicate =
            &stmt->select_plan.join_predicates[index];

        if (predicate->first_table + predicate->table_count != available_table_count) {
            continue;
        }
        int status = mylite_select_eval_expression_predicate(
            stmt, row, predicate->expression, &table_select_eval_callbacks, out_matches);

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_using_column(mylite_stmt *stmt,
                                              const struct mylite_table_select_row *row,
                                              const struct mylite_select_join_using_column *column,
                                              bool *out_matches)
{
    const struct mylite_expression_value *left = NULL;
    const struct mylite_expression_value *right = NULL;
    int comparison = 0;

    *out_matches = false;
    if (row == NULL || column->left_column_index >= row->value_count ||
        column->right_column_index >= row->value_count) {
        return set_where_predicate_eval_error(stmt);
    }

    left = &row->values[column->left_column_index];
    right = &row->values[column->right_column_index];
    if (left->kind == MYLITE_EXPRESSION_VALUE_NULL || right->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }
    if (mylite_expression_value_compare(left, right, &stmt->database->warnings, &comparison) != 0) {
        return set_where_predicate_eval_error(stmt);
    }
    *out_matches = comparison == 0;
    return MYLITE_OK;
}

static int
process_joined_table_select_full_row(mylite_stmt *stmt,
                                     struct mylite_table_select_join_materialize_state *state,
                                     const struct mylite_table_select_row *row)
{
    bool matches = false;
    bool aggregate_query = (stmt->select_plan.has_group_by || stmt->select_plan.has_aggregate ||
                            stmt->select_plan.has_having) != 0;
    int status = MYLITE_OK;

    if (stmt->select_predicate == NULL || stmt->select_constant_predicate_evaluated) {
        matches = true;
    } else {
        status =
            mylite_select_eval_row_predicate(stmt, row, &table_select_eval_callbacks, &matches);
    }
    if (status != MYLITE_OK || !matches) {
        return status;
    }
    if (!aggregate_query) {
        return process_joined_table_select_nonaggregate_row(stmt, state, row);
    }

    struct mylite_table_select_group *group = NULL;

    status = find_table_select_group(stmt, state->groups, state->group_count, row, &group);
    if (status == MYLITE_OK && group == NULL) {
        status = append_table_select_group(stmt, &state->groups, &state->group_count, row, &group);
    }
    if (status == MYLITE_OK) {
        status = update_table_select_group(stmt, group, row);
    }
    return status;
}

static int process_joined_table_select_nonaggregate_row(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row)
{
    bool distinct = mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode);

    if (stmt->select_plan.order_key_count != 0U || distinct) {
        struct mylite_table_select_row copy = {0};
        int status = mylite_select_row_copy(row, &copy);
        bool duplicate = false;

        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        if (status == MYLITE_OK && distinct) {
            status = check_table_select_distinct_duplicate(stmt, &copy, &duplicate);
        }
        if (status == MYLITE_OK && duplicate) {
            mylite_select_row_deinit(&copy);
            return MYLITE_OK;
        }
        if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
            status = mylite_select_eval_order_values(stmt, &copy, &table_select_eval_callbacks);
        }
        if (status == MYLITE_OK) {
            status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &copy);
        }
        mylite_select_row_deinit(&copy);
        return status;
    }

    if (mylite_select_limit_row_is_kept(&stmt->select_plan.limit,
                                        (struct mylite_select_limit_position){
                                            .matched_row = state->matched_row,
                                            .kept_count = stmt->select_result.row_count,
                                        })) {
        int status =
            mylite_select_result_append_row_copy(stmt->database, &stmt->select_result, row);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (state->matched_row != UINT64_MAX) {
        ++state->matched_row;
    }
    if (mylite_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        state->stop = true;
    }
    return MYLITE_OK;
}

static int materialize_aggregate_table_select_result(mylite_stmt *stmt)
{
    struct mylite_table_select_group *groups = NULL;
    size_t group_count = 0U;
    int status = scan_aggregate_table_select_groups(stmt, &groups, &group_count);

    if (status == MYLITE_OK && group_count == 0U && !stmt->select_plan.has_group_by) {
        status = append_empty_implicit_table_select_group(stmt, &groups, &group_count);
    }
    if (status == MYLITE_OK) {
        status = append_finalized_table_select_groups(stmt, groups, group_count);
    }
    if (status == MYLITE_OK && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_result_sort_rows(stmt->database, &stmt->select_result,
                                                &stmt->select_plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_apply_limit(&stmt->select_result, &stmt->select_plan.limit);
    }

    for (size_t index = 0U; index < group_count; ++index) {
        table_select_group_deinit(&groups[index]);
    }
    free(groups);
    return status;
}

static int scan_aggregate_table_select_groups(mylite_stmt *stmt,
                                              struct mylite_table_select_group **groups,
                                              size_t *group_count)
{
    int status = mylite_select_eval_constant_predicate(stmt, &table_select_eval_callbacks);
    int rc = SQLITE_OK;

    *groups = NULL;
    *group_count = 0U;
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_group *group = NULL;
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = mylite_select_copy_sqlite_row(stmt, &row);
        if (status != MYLITE_OK) {
            return status;
        }
        status = evaluate_table_select_row_matches(stmt, &row, &matches);
        if (status != MYLITE_OK) {
            mylite_select_row_deinit(&row);
            return status;
        }
        if (!matches) {
            mylite_select_row_deinit(&row);
            continue;
        }

        status = find_table_select_group(stmt, *groups, *group_count, &row, &group);
        if (status == MYLITE_OK && group == NULL) {
            status = append_table_select_group(stmt, groups, group_count, &row, &group);
        }
        if (status == MYLITE_OK) {
            status = update_table_select_group(stmt, group, &row);
        }
        mylite_select_row_deinit(&row);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        status = mylite_diagnostics_set_sqlite_error(stmt->database);
    }
    return status;
}

static int append_empty_implicit_table_select_group(mylite_stmt *stmt,
                                                    struct mylite_table_select_group **groups,
                                                    size_t *group_count)
{
    struct mylite_table_select_group *new_groups = calloc(1U, sizeof(*new_groups));

    if (new_groups == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    *groups = new_groups;
    *group_count = 1U;
    (*groups)[0].aggregate_states =
        calloc(stmt->select_plan.aggregate_binding_count, sizeof(*(*groups)[0].aggregate_states));
    if ((*groups)[0].aggregate_states == NULL && stmt->select_plan.aggregate_binding_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    (*groups)[0].aggregate_state_count = stmt->select_plan.aggregate_binding_count;
    return MYLITE_OK;
}

static int append_finalized_table_select_groups(mylite_stmt *stmt,
                                                struct mylite_table_select_group *groups,
                                                size_t group_count)
{
    for (size_t index = 0U; index < group_count; ++index) {
        int status = append_finalized_table_select_group(stmt, &groups[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_finalized_table_select_group(mylite_stmt *stmt,
                                               struct mylite_table_select_group *group)
{
    struct mylite_table_select_row row = {0};
    bool having_matches = true;
    bool duplicate = false;
    int status = finalize_table_select_group(stmt, group, &row);

    if (status == MYLITE_OK) {
        status =
            mylite_select_eval_having(stmt, &row, &table_select_eval_callbacks, &having_matches);
    }
    if (status == MYLITE_OK && having_matches &&
        mylite_select_duplicate_mode_is_distinct(stmt->select_plan.duplicate_mode)) {
        status = check_table_select_distinct_duplicate(stmt, &row, &duplicate);
    }
    if (status == MYLITE_OK && duplicate) {
        mylite_select_row_deinit(&row);
        return MYLITE_OK;
    }
    if (status == MYLITE_OK && having_matches && stmt->select_plan.order_key_count != 0U) {
        status = mylite_select_eval_order_values(stmt, &row, &table_select_eval_callbacks);
    }
    if (status == MYLITE_OK && having_matches) {
        status = mylite_select_result_append_row(stmt->database, &stmt->select_result, &row);
    }
    mylite_select_row_deinit(&row);
    return status;
}

static int evaluate_table_select_row_matches(mylite_stmt *stmt,
                                             const struct mylite_table_select_row *row,
                                             bool *out_matches)
{
    *out_matches = true;
    int status = evaluate_table_select_join_conditions(stmt, row, out_matches);

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    if (stmt->select_predicate == NULL || stmt->select_constant_predicate_evaluated) {
        return MYLITE_OK;
    }
    return mylite_select_eval_row_predicate(stmt, row, &table_select_eval_callbacks, out_matches);
}

static int evaluate_table_select_join_conditions(mylite_stmt *stmt,
                                                 const struct mylite_table_select_row *row,
                                                 bool *out_matches)
{
    int status = evaluate_table_select_using_conditions(stmt, row, out_matches);

    if (status != MYLITE_OK || !*out_matches) {
        return status;
    }
    return evaluate_table_select_join_predicates(stmt, row, out_matches);
}

static int evaluate_table_select_using_conditions(mylite_stmt *stmt,
                                                  const struct mylite_table_select_row *row,
                                                  bool *out_matches)
{
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.using_column_count; ++index) {
        const struct mylite_select_join_using_column *column =
            &stmt->select_plan.using_columns[index];
        const struct mylite_expression_value *left = NULL;
        const struct mylite_expression_value *right = NULL;

        if (row == NULL || column->left_column_index >= row->value_count ||
            column->right_column_index >= row->value_count) {
            return set_where_predicate_eval_error(stmt);
        }
        left = &row->values[column->left_column_index];
        right = &row->values[column->right_column_index];
        if (left->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            mylite_select_compare_values(left, right) != 0) {
            *out_matches = false;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_join_predicates(mylite_stmt *stmt,
                                                 const struct mylite_table_select_row *row,
                                                 bool *out_matches)
{
    *out_matches = true;
    for (size_t index = 0U; index < stmt->select_plan.join_predicate_count; ++index) {
        int status = mylite_select_eval_expression_predicate(
            stmt, row, stmt->select_plan.join_predicates[index].expression,
            &table_select_eval_callbacks, out_matches);

        if (status != MYLITE_OK || !*out_matches) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int check_table_select_distinct_duplicate(mylite_stmt *stmt,
                                                 struct mylite_table_select_row *row,
                                                 bool *out_duplicate)
{
    int status =
        mylite_select_eval_materialize_output_values(stmt, row, &table_select_eval_callbacks);

    *out_duplicate = false;
    if (status != MYLITE_OK) {
        return status;
    }
    *out_duplicate = mylite_select_result_distinct_row_exists(
        &stmt->select_result, &stmt->select_plan, &stmt->result_metadata, row);
    return MYLITE_OK;
}

static int append_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group **groups,
                                     size_t *group_count, const struct mylite_table_select_row *row,
                                     struct mylite_table_select_group **out_group)
{
    struct mylite_table_select_group *new_groups =
        realloc(*groups, (*group_count + 1U) * sizeof(**groups));
    int status = MYLITE_OK;

    *out_group = NULL;
    if (new_groups == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    *groups = new_groups;
    (*groups)[*group_count] = (struct mylite_table_select_group){0};
    status = initialize_table_select_group(stmt, &(*groups)[*group_count], row);
    if (status != MYLITE_OK) {
        table_select_group_deinit(&(*groups)[*group_count]);
        return status;
    }

    *out_group = &(*groups)[*group_count];
    *group_count += 1U;
    return MYLITE_OK;
}

static int find_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *groups,
                                   size_t group_count, const struct mylite_table_select_row *row,
                                   struct mylite_table_select_group **out_group)
{
    struct mylite_expression_value *values = NULL;
    size_t value_count = stmt->select_plan.group_key_count;
    int status = MYLITE_OK;

    *out_group = NULL;
    if (value_count == 0U) {
        if (group_count != 0U) {
            *out_group = &groups[0];
        }
        return MYLITE_OK;
    }

    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        status = mylite_select_eval_group_key(stmt, row, &stmt->select_plan.group_keys[index],
                                              &table_select_eval_callbacks, &values[index]);
        if (status != MYLITE_OK) {
            goto cleanup;
        }
    }

    for (size_t group_index = 0U; group_index < group_count; ++group_index) {
        bool matches = groups[group_index].group_value_count == value_count;

        for (size_t value_index = 0U; matches && value_index < value_count; ++value_index) {
            if (mylite_select_compare_values(&groups[group_index].group_values[value_index],
                                             &values[value_index]) != 0) {
                matches = false;
            }
        }
        if (matches) {
            *out_group = &groups[group_index];
            break;
        }
    }

cleanup:
    for (size_t index = 0U; index < value_count; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    free(values);
    return status;
}

static int initialize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                         const struct mylite_table_select_row *row)
{
    size_t column_count =
        row == NULL ? mylite_select_plan_column_count(&stmt->select_plan) : row->value_count;

    group->representative.value_count = column_count;
    if (column_count != 0U) {
        group->representative.values = calloc(column_count, sizeof(*group->representative.values));
        if (group->representative.values == NULL) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (row != NULL) {
        for (size_t index = 0U; index < column_count; ++index) {
            if (mylite_expression_value_copy(&row->values[index],
                                             &group->representative.values[index]) != 0) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
        group->has_representative = true;
    }

    group->group_value_count = stmt->select_plan.group_key_count;
    group->group_values = calloc(group->group_value_count, sizeof(*group->group_values));
    if (group->group_values == NULL && group->group_value_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < group->group_value_count; ++index) {
        int status =
            mylite_select_eval_group_key(stmt, row, &stmt->select_plan.group_keys[index],
                                         &table_select_eval_callbacks, &group->group_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    group->aggregate_state_count = stmt->select_plan.aggregate_binding_count;
    group->aggregate_states =
        calloc(group->aggregate_state_count, sizeof(*group->aggregate_states));
    if (group->aggregate_states == NULL && group->aggregate_state_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int update_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                     const struct mylite_table_select_row *row)
{
    for (size_t index = 0U; index < stmt->select_plan.aggregate_binding_count; ++index) {
        int status = mylite_select_update_aggregate_state(
            stmt, &group->aggregate_states[index], &stmt->select_plan.aggregate_bindings[index],
            row, &table_select_eval_callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int finalize_table_select_group(mylite_stmt *stmt, struct mylite_table_select_group *group,
                                       struct mylite_table_select_row *out_row)
{
    size_t column_count = group->representative.value_count;

    out_row->values = calloc(column_count, sizeof(*out_row->values));
    if (out_row->values == NULL && column_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = column_count;
    for (size_t index = 0U; index < column_count; ++index) {
        if (mylite_expression_value_copy(&group->representative.values[index],
                                         &out_row->values[index]) != 0) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }

    out_row->aggregate_values =
        calloc(group->aggregate_state_count, sizeof(*out_row->aggregate_values));
    if (out_row->aggregate_values == NULL && group->aggregate_state_count != 0U) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->aggregate_value_count = group->aggregate_state_count;
    for (size_t index = 0U; index < group->aggregate_state_count; ++index) {
        int status = mylite_select_finalize_aggregate_state(
            stmt, &group->aggregate_states[index], &stmt->select_plan.aggregate_bindings[index],
            &out_row->aggregate_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int set_where_predicate_eval_error(mylite_stmt *stmt)
{
    mylite_db *database = stmt == NULL ? NULL : stmt->database;

    if (database == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    if (database->warnings.count != 0U) {
        const struct mylite_expression_warning *warning =
            &database->warnings.items[database->warnings.count - 1U];

        if (warning->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        if (warning->code == MYLITE_MYSQL_ER_WRONG_ARGUMENTS) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return set_select_unsupported_where_error(database);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int copy_scalar_select_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    size_t column_count = 0U;

    for (const struct mylite_sql_ast_node *item = select_list == NULL ? NULL
                                                                      : select_list->first_child;
         item != NULL; item = item->next_sibling) {
        ++column_count;
    }
    if (column_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    stmt->scalar_select_sql_text =
        mylite_copy_span_text(statement->span.text, statement->span.length);
    stmt->scalar_result.values = calloc(column_count, sizeof(*stmt->scalar_result.values));
    stmt->scalar_result.texts = (char **)calloc(column_count, sizeof(*stmt->scalar_result.texts));
    stmt->scalar_result.expressions = (const struct mylite_sql_ast_node **)calloc(
        column_count, sizeof(*stmt->scalar_result.expressions));
    stmt->result_metadata.columns = calloc(column_count, sizeof(*stmt->result_metadata.columns));
    if (stmt->scalar_select_sql_text == NULL || stmt->scalar_result.values == NULL ||
        stmt->scalar_result.texts == NULL || stmt->scalar_result.expressions == NULL ||
        stmt->result_metadata.columns == NULL) {
        return MYLITE_NOMEM;
    }
    stmt->scalar_result.value_count = column_count;
    stmt->result_metadata.column_count = column_count;
    stmt->affected_rows = -1;
    stmt->scalar_result.row_available = true;

    if (limit_clause != NULL) {
        int status = bind_scalar_select_limit_clause(stmt, limit_clause);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling, ++index) {
        int status = copy_scalar_select_item(stmt, item, index, statement->span.text,
                                             statement->span.length);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (order_by_clause != NULL) {
        return validate_scalar_select_order_by_clause(stmt->database, order_by_clause,
                                                      &stmt->result_metadata);
    }
    return MYLITE_OK;
}

static int bind_scalar_select_limit_clause(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *limit_clause)
{
    int status = mylite_select_bind_limit_clause(limit_clause, &stmt->select_plan);

    if (status != MYLITE_OK) {
        return status;
    }
    stmt->scalar_result.row_available = mylite_select_limit_row_is_kept(
        &stmt->select_plan.limit, (struct mylite_select_limit_position){
                                      .matched_row = 0U,
                                      .kept_count = 0U,
                                  });
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int copy_scalar_select_item(mylite_stmt *stmt, const struct mylite_sql_ast_node *item,
                                   size_t index, const char *source_sql, size_t source_sql_length)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);
    const struct mylite_sql_ast_node *alias = mylite_ast_child_at(item, 1U);
    const struct mylite_expression_value *descriptor_value = NULL;
    bool defer_expression = false;
    int status = MYLITE_OK;

    if (stmt->scalar_result.row_available) {
        const bool supported_no_table = mylite_expression_is_supported_no_table(expression);
        const bool cacheable_no_table = mylite_expression_is_cacheable_no_table(expression);

        if (supported_no_table && !cacheable_no_table) {
            defer_expression = true;
        }
    }
    if (stmt->scalar_result.row_available && !defer_expression) {
        status =
            evaluate_scalar_select_expression(stmt, expression, &stmt->scalar_result.values[index]);
        if (status != MYLITE_OK) {
            int warning_status = append_scalar_select_warnings_to_database(stmt);

            return warning_status != MYLITE_OK ? warning_status : status;
        }
        stmt->scalar_result.texts[index] =
            mylite_expression_value_to_text(&stmt->scalar_result.values[index]);
        if (stmt->scalar_result.values[index].kind != MYLITE_EXPRESSION_VALUE_NULL &&
            stmt->scalar_result.texts[index] == NULL) {
            return MYLITE_NOMEM;
        }
        descriptor_value = &stmt->scalar_result.values[index];
    } else if (stmt->scalar_result.row_available) {
        status = copy_scalar_select_item_expression(stmt, expression, index, source_sql,
                                                    source_sql_length);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (alias != NULL) {
        stmt->result_metadata.columns[index].name = copy_select_alias(alias);
    } else {
        stmt->result_metadata.columns[index].name =
            mylite_copy_span_text(expression->span.text, expression->span.length);
    }
    if (stmt->result_metadata.columns[index].name == NULL) {
        return MYLITE_NOMEM;
    }
    return infer_scalar_expression_descriptor(stmt->database, expression, descriptor_value,
                                              &stmt->result_metadata.columns[index].descriptor);
}

static int copy_scalar_select_item_expression(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              size_t index, const char *source_sql,
                                              size_t source_sql_length)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = mylite_statement_clone_sql_ast_subtree(&stmt->scalar_select_ast, expression,
                                                        source_sql, stmt->scalar_select_sql_text,
                                                        source_sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    if (status == MYLITE_OK) {
        stmt->scalar_result.expressions[index] = clone;
    }
    return status;
}

static int validate_scalar_select_order_by_clause(mylite_db *database,
                                                  const struct mylite_sql_ast_node *order_by_clause,
                                                  const struct mylite_result_metadata *metadata)
{
    const struct mylite_sql_ast_node *items = mylite_ast_child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return set_select_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = validate_scalar_select_order_item(database, item, metadata);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_scalar_select_order_item(mylite_db *database,
                                             const struct mylite_sql_ast_node *order_item,
                                             const struct mylite_result_metadata *metadata)
{
    const struct mylite_sql_ast_node *expression = mylite_ast_child_at(order_item, 0U);

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return set_select_unsupported_order_error(database);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > metadata->column_count) {
            char *reference = mylite_copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)mylite_diagnostics_set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = mylite_select_set_unknown_order_column_error(database, reference);
            free(reference);
            return status;
        }
        return MYLITE_OK;
    }

    return validate_scalar_select_order_expression(database, expression, metadata);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_scalar_select_order_expression(mylite_db *database,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_result_metadata *metadata)
{
    if (expression == NULL) {
        return set_select_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return resolve_scalar_select_order_reference(database, metadata, expression);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        if (expression->kind == MYLITE_SQL_AST_CAST_EXPRESSION) {
            int status = mylite_expression_validate_cast_target_charset(database, expression);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = validate_scalar_select_order_expression(database, child, metadata);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return validate_scalar_select_order_function_call(database, expression, metadata);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    default:
        return set_select_unsupported_order_error(database);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_scalar_select_order_function_call(mylite_db *database,
                                                      const struct mylite_sql_ast_node *expression,
                                                      const struct mylite_result_metadata *metadata)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (!mylite_expression_is_supported_function_call(expression)) {
        return set_select_unsupported_order_error(database);
    }
    {
        int status = mylite_expression_validate_char_function_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        int status = validate_scalar_select_order_expression(database, child, metadata);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_scalar_select_order_reference(mylite_db *database,
                                                 const struct mylite_result_metadata *metadata,
                                                 const struct mylite_sql_ast_node *expression)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = mylite_copy_identifier_parts(expression, parts, &part_count);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count != 1U) {
        const char *table_name = part_count == 2U ? parts[0] : parts[1];

        status = mylite_select_set_unknown_table_error(database, table_name);
        goto cleanup;
    }

    {
        size_t output_index = 0U;
        size_t output_matches =
            mylite_result_metadata_label_count(metadata, parts[0], &output_index);

        (void)output_index;
        if (output_matches > 1U) {
            status = set_select_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 0U) {
            status = mylite_select_set_unknown_order_column_error(database, parts[0]);
        }
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static int append_scalar_select_warnings_to_database(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt->scalar_result.warnings.count == 0U) {
        return MYLITE_OK;
    }

    status = append_subquery_warnings(&stmt->database->warnings, &stmt->scalar_result.warnings);
    if (status != MYLITE_OK) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int evaluate_scalar_select_expression(mylite_stmt *stmt,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_eval_context context = {
        .user_data = stmt,
        .eval_subquery = evaluate_scalar_select_subquery_expression,
        .eval_in_subquery = evaluate_scalar_select_in_subquery_expression,
        .eval_quantified_subquery = evaluate_scalar_select_quantified_subquery_expression,
        .eval_row_subquery = evaluate_scalar_select_row_subquery_expression,
        .eval_session_function = evaluate_scalar_select_session_function,
    };
    int status = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_AGGREGATE_CALL) {
        return evaluate_scalar_aggregate_expression(stmt, expression, out_value);
    }

    status = mylite_expression_eval_with_context(expression, &context,
                                                 &stmt->scalar_result.warnings, out_value);
    if (status == 0) {
        return MYLITE_OK;
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (stmt->database->error_message != NULL) {
        return status > 0 ? status : MYLITE_EXEC_ERROR;
    }
    for (size_t index = 0U; index < stmt->scalar_result.warnings.count; ++index) {
        const struct mylite_expression_warning *warning =
            &stmt->scalar_result.warnings.items[index];

        if (warning->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            int error_status =
                mylite_diagnostics_set_error_message(stmt->database, warning->message);

            return error_status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return MYLITE_UNSUPPORTED;
}

static int evaluate_scalar_aggregate_expression(mylite_stmt *stmt,
                                                const struct mylite_sql_ast_node *expression,
                                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value argument = {0};
    struct mylite_expression_eval_context context = {
        .user_data = stmt,
        .eval_subquery = evaluate_scalar_select_subquery_expression,
        .eval_in_subquery = evaluate_scalar_select_in_subquery_expression,
        .eval_quantified_subquery = evaluate_scalar_select_quantified_subquery_expression,
        .eval_row_subquery = evaluate_scalar_select_row_subquery_expression,
        .eval_session_function = evaluate_scalar_select_session_function,
    };
    int status = 0;

    if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT &&
        expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_STAR) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return MYLITE_OK;
    }
    if (expression->aggregate_argument != MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT &&
            expression->aggregate_argument ==
                MYLITE_SQL_AST_AGGREGATE_ARGUMENT_DISTINCT_EXPRESSION_LIST) {
            return evaluate_scalar_count_distinct_expression(
                stmt, mylite_ast_child_at(expression, 1U), &context, out_value);
        }
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_expression_eval_with_context(mylite_ast_child_at(expression, 1U), &context,
                                                 &stmt->scalar_result.warnings, &argument);
    if (status != 0) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (stmt->database->error_message != NULL) {
            return status > 0 ? status : MYLITE_EXEC_ERROR;
        }
        return MYLITE_UNSUPPORTED;
    }

    if (expression->aggregate_kind == MYLITE_SQL_AST_AGGREGATE_COUNT) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = argument.kind == MYLITE_EXPRESSION_VALUE_NULL ? 0 : 1,
        };
        mylite_expression_value_deinit(&argument);
        return MYLITE_OK;
    }
    if (argument.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&argument);
        return MYLITE_OK;
    }

    switch (expression->aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_SUM:
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        status = evaluate_scalar_numeric_aggregate_expression(stmt, expression->aggregate_kind,
                                                              &argument, out_value);
        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&argument);
            return status;
        }
        break;
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        if (mylite_expression_value_copy(&argument, out_value) != 0) {
            mylite_expression_value_deinit(&argument);
            return MYLITE_NOMEM;
        }
        break;
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        mylite_expression_value_deinit(&argument);
        return MYLITE_UNSUPPORTED;
    }

    mylite_expression_value_deinit(&argument);
    return MYLITE_OK;
}

static int evaluate_scalar_count_distinct_expression(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_eval_context *context, struct mylite_expression_value *out_value)
{
    bool has_null = false;

    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_EXPRESSION_LIST ||
        arguments->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *argument_node = arguments->first_child;
         argument_node != NULL; argument_node = argument_node->next_sibling) {
        struct mylite_expression_value argument = {0};
        int status = mylite_expression_eval_with_context(argument_node, context,
                                                         &stmt->scalar_result.warnings, &argument);

        if (status != 0) {
            mylite_expression_value_deinit(&argument);
            if (status == MYLITE_NOMEM) {
                (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
            if (stmt->database->error_message != NULL) {
                return status > 0 ? status : MYLITE_EXEC_ERROR;
            }
            return MYLITE_UNSUPPORTED;
        }
        if (argument.kind == MYLITE_EXPRESSION_VALUE_NULL) {
            has_null = true;
        }
        mylite_expression_value_deinit(&argument);
    }

    int64_t count = 1;

    if (has_null) {
        count = 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = count,
    };
    return MYLITE_OK;
}

static int evaluate_scalar_numeric_aggregate_expression(
    mylite_stmt *stmt, enum mylite_sql_ast_aggregate_kind aggregate_kind,
    const struct mylite_expression_value *argument, struct mylite_expression_value *out_value)
{
    struct mylite_aggregate_numeric_value numeric = {0};
    int status =
        mylite_select_aggregate_value_to_double(&stmt->scalar_result.warnings, argument, &numeric);

    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }
    if (aggregate_kind == MYLITE_SQL_AST_AGGREGATE_SUM && numeric.integral) {
        if (numeric.unsigned_value) {
            *out_value = (struct mylite_expression_value){
                .kind = MYLITE_EXPRESSION_VALUE_UINT64,
                .uint64_value = (uint64_t)numeric.value,
            };
            return MYLITE_OK;
        }
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = (int64_t)numeric.value,
        };
        return MYLITE_OK;
    }
    if (aggregate_kind == MYLITE_SQL_AST_AGGREGATE_AVG && numeric.integral) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = numeric.value,
        };
        return MYLITE_OK;
    }

    status = mylite_select_aggregate_format_double(numeric.value, out_value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int evaluate_scalar_select_subquery_expression(void *user_data,
                                                      const struct mylite_sql_ast_node *subquery,
                                                      struct mylite_expression_warnings *warnings,
                                                      struct mylite_expression_value *out_value)
{
    return evaluate_select_subquery_expression(user_data, subquery, warnings, out_value);
}

static int evaluate_scalar_select_in_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value)
{
    return evaluate_in_subquery_expression(user_data, expression, left, warnings, out_value);
}

static int evaluate_scalar_select_quantified_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value)
{
    return evaluate_quantified_subquery_expression(user_data, expression, left, warnings,
                                                   out_value);
}

static int evaluate_scalar_select_row_subquery_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    return evaluate_row_subquery_expression(user_data, expression, expression_context, warnings,
                                            out_value);
}

static int evaluate_select_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || subquery == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    switch (subquery->kind) {
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        status = evaluate_scalar_subquery_expression(stmt, subquery, out_value);
        break;
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        status = evaluate_exists_subquery_expression(stmt, subquery, out_value);
        break;
    default:
        status = MYLITE_UNSUPPORTED;
        break;
    }

    subquery_warnings = stmt->database->warnings;
    stmt->database->warnings = saved_warnings;
    if (append_subquery_warnings(warnings, &subquery_warnings) != MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&subquery_warnings);
    return status;
}

static int evaluate_in_subquery_expression(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || expression == NULL || left == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    status = evaluate_in_subquery_expression_inner(stmt, expression, left, warnings, out_value);

    subquery_warnings = stmt->database->warnings;
    stmt->database->warnings = saved_warnings;
    if (append_subquery_warnings(warnings, &subquery_warnings) != MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&subquery_warnings);
    return status;
}

static int evaluate_in_subquery_expression_inner(mylite_stmt *stmt,
                                                 const struct mylite_sql_ast_node *expression,
                                                 const struct mylite_expression_value *left,
                                                 struct mylite_expression_warnings *warnings,
                                                 struct mylite_expression_value *out_value)
{
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    struct mylite_in_subquery_scan_state scan = {
        .has_row = false,
        .matched = false,
        .saw_unknown = false,
    };
    int status = MYLITE_OK;

    status = prepare_in_subquery_statement(stmt, expression, &subquery_stmt, &order_key_count,
                                           &restore_order_keys);
    if (status != MYLITE_OK) {
        return status;
    }

    struct mylite_in_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = warnings,
    };

    status = scan_in_subquery_statement(&scan_context, &scan);
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    return finish_in_subquery_expression(expression, left, scan.has_row, scan.matched,
                                         scan.saw_unknown, out_value);
}

static int prepare_in_subquery_statement(mylite_stmt *stmt,
                                         const struct mylite_sql_ast_node *expression,
                                         mylite_stmt **out_subquery_stmt,
                                         size_t *out_order_key_count, bool *out_restore_order_keys)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (!binary_expression_is_in_subquery(expression)) {
        return MYLITE_UNSUPPORTED;
    }

    status = validate_in_subquery_select(stmt->database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }

    status = prepare_select_subquery_statement(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = validate_in_subquery_prepared_columns(stmt->database, subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        return status;
    }
    if (subquery_stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        subquery_stmt->select_plan.order_key_count != 0U) {
        *out_order_key_count = subquery_stmt->select_plan.order_key_count;
        *out_restore_order_keys = true;
        subquery_stmt->select_plan.order_key_count = 0U;
    }
    *out_subquery_stmt = subquery_stmt;
    return MYLITE_OK;
}

static int scan_in_subquery_statement(const struct mylite_in_subquery_scan_context *context,
                                      struct mylite_in_subquery_scan_state *state)
{
    for (;;) {
        int status = mylite_step(context->subquery_stmt);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        state->has_row = true;
        status = scan_in_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->matched) {
            return status;
        }
    }
}

static int scan_in_subquery_statement_row(const struct mylite_in_subquery_scan_context *context,
                                          struct mylite_in_subquery_scan_state *state)
{
    struct mylite_expression_value right = {0};
    int comparison = 0;
    int status = MYLITE_OK;

    if (context->left->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return MYLITE_OK;
    }

    status = copy_subquery_statement_column_value(context->subquery_stmt, &right);
    if (status != MYLITE_OK) {
        mylite_expression_value_deinit(&right);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->outer_stmt->database,
                                                       "out of memory");
        }
        return status;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        state->saw_unknown = true;
        mylite_expression_value_deinit(&right);
        return MYLITE_OK;
    }
    status = mylite_expression_value_compare(context->left, &right, context->warnings, &comparison);
    mylite_expression_value_deinit(&right);
    if (status != 0) {
        return status;
    }
    if (comparison == 0) {
        state->matched = true;
    }
    return MYLITE_OK;
}

static int finish_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                         const struct mylite_expression_value *left, bool has_row,
                                         bool matched, bool saw_unknown,
                                         struct mylite_expression_value *out_value)
{
    if (matched) {
        int64_t result = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 1 : 0;

        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = result};
        return MYLITE_OK;
    }
    if (!has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
        return MYLITE_OK;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_NULL || saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
    return MYLITE_OK;
}

static int
evaluate_row_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || expression == NULL ||
        expression_context == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    status = evaluate_row_subquery_expression_inner(stmt, expression, expression_context, warnings,
                                                    out_value);

    subquery_warnings = stmt->database->warnings;
    stmt->database->warnings = saved_warnings;
    if (append_subquery_warnings(warnings, &subquery_warnings) != MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&subquery_warnings);
    return status;
}

static int evaluate_row_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *left_expression =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    struct mylite_row_expression_values left = {0};
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    int status = MYLITE_OK;

    if (quantified_comparison_has_row_left(expression) &&
        !quantified_comparison_is_row_subquery_alias(expression)) {
        return set_row_quantified_non_alias_error(stmt->database, expression);
    }

    status = evaluate_row_constructor_values(left_expression, expression_context, warnings, &left);
    if (status != MYLITE_OK) {
        row_expression_values_deinit(&left);
        return status;
    }
    status = prepare_row_subquery_statement(stmt, expression, left.count, &subquery_stmt,
                                            &order_key_count, &restore_order_keys);
    if (status != MYLITE_OK) {
        row_expression_values_deinit(&left);
        return status;
    }

    if (row_subquery_expression_is_membership(expression)) {
        status = evaluate_row_in_subquery_statement(stmt, expression, subquery_stmt, &left,
                                                    warnings, out_value);
    } else {
        status = evaluate_row_scalar_subquery_statement(stmt, expression, subquery_stmt, &left,
                                                        out_value);
    }
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    row_expression_values_deinit(&left);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int evaluate_row_in_subquery_statement(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              mylite_stmt *subquery_stmt,
                                              const struct mylite_row_expression_values *left,
                                              struct mylite_expression_warnings *warnings,
                                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings comparison_warnings = {0};
    struct mylite_row_in_subquery_scan_state scan = {
        .has_row = false,
        .matched = false,
        .saw_unknown = false,
    };
    struct mylite_row_in_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = &comparison_warnings,
        .left_has_null = row_expression_values_has_null(left),
    };
    int status = scan_row_in_subquery_statement(&scan_context, &scan);

    if (status != MYLITE_OK) {
        mylite_expression_warnings_deinit(&comparison_warnings);
        return status;
    }
    if (append_subquery_warnings(warnings, &comparison_warnings) != MYLITE_OK) {
        mylite_expression_warnings_deinit(&comparison_warnings);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&comparison_warnings);
    return finish_row_in_subquery_expression(expression, scan.has_row, scan.matched,
                                             scan.saw_unknown, out_value);
}

static int evaluate_row_scalar_subquery_statement(mylite_stmt *stmt,
                                                  const struct mylite_sql_ast_node *expression,
                                                  mylite_stmt *subquery_stmt,
                                                  const struct mylite_row_expression_values *left,
                                                  struct mylite_expression_value *out_value)
{
    struct mylite_row_expression_values right = {0};
    int truth = -1;
    int status = mylite_step(subquery_stmt);

    if (status == MYLITE_DONE) {
        if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return MYLITE_OK;
    }
    if (status != MYLITE_ROW) {
        return status;
    }

    status = copy_subquery_statement_row_values(subquery_stmt, left->count, &right);
    if (status == MYLITE_OK) {
        /* MySQL suppresses tuple element conversion warnings here, unlike row IN. */
        status = compare_row_values(expression->operator_kind, left, &right, NULL, &truth);
    }
    row_expression_values_deinit(&right);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_ROW) {
        return set_scalar_subquery_cardinality_error(stmt->database);
    }
    if (status != MYLITE_DONE) {
        return status;
    }
    if (truth < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = truth};
    }
    return MYLITE_OK;
}

static int prepare_row_subquery_statement(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *expression,
                                          size_t expected_width, mylite_stmt **out_subquery_stmt,
                                          size_t *out_order_key_count, bool *out_restore_order_keys)
{
    const struct mylite_sql_ast_node *select_statement = row_subquery_select_statement(expression);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (!row_subquery_expression_is_supported(expression) || expected_width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    if (row_subquery_expression_is_membership(expression) &&
        mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return set_in_subquery_limit_error(stmt->database);
    }

    status = validate_row_subquery_select_columns(stmt->database, select_statement, expected_width);
    if (status != MYLITE_OK) {
        return status;
    }

    status = prepare_select_subquery_statement(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = validate_row_subquery_prepared_columns(stmt->database, subquery_stmt, expected_width);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        return status;
    }
    if (row_subquery_expression_is_membership(expression) &&
        subquery_stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        subquery_stmt->select_plan.order_key_count != 0U) {
        *out_order_key_count = subquery_stmt->select_plan.order_key_count;
        *out_restore_order_keys = true;
        subquery_stmt->select_plan.order_key_count = 0U;
    }
    *out_subquery_stmt = subquery_stmt;
    return MYLITE_OK;
}

static int scan_row_in_subquery_statement(const struct mylite_row_in_subquery_scan_context *context,
                                          struct mylite_row_in_subquery_scan_state *state)
{
    for (;;) {
        int status = mylite_step(context->subquery_stmt);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        state->has_row = true;
        status = scan_row_in_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->matched ||
            (context->left_has_null && state->saw_unknown)) {
            return status;
        }
    }
}

static int
scan_row_in_subquery_statement_row(const struct mylite_row_in_subquery_scan_context *context,
                                   struct mylite_row_in_subquery_scan_state *state)
{
    struct mylite_row_expression_values right = {0};
    int truth = -1;
    int status =
        copy_subquery_statement_row_values(context->subquery_stmt, context->left->count, &right);

    if (status == MYLITE_OK) {
        status = compare_row_values(MYLITE_SQL_AST_OPERATOR_EQUAL, context->left, &right,
                                    context->warnings, &truth);
    }
    row_expression_values_deinit(&right);
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->outer_stmt->database,
                                                       "out of memory");
        }
        return status;
    }
    if (truth == 1) {
        state->matched = true;
    } else if (truth < 0) {
        state->saw_unknown = true;
    }
    return MYLITE_OK;
}

static bool row_expression_values_has_null(const struct mylite_row_expression_values *values)
{
    if (values == NULL) {
        return false;
    }
    for (size_t index = 0U; index < values->count; ++index) {
        if (values->items[index].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return true;
        }
    }
    return false;
}

static int finish_row_in_subquery_expression(const struct mylite_sql_ast_node *expression,
                                             bool has_row, bool matched, bool saw_unknown,
                                             struct mylite_expression_value *out_value)
{
    bool positive = row_subquery_expression_is_positive_membership(expression);
    int64_t matched_value = 0;
    int64_t unmatched_value = 1;

    if (positive) {
        matched_value = 1;
        unmatched_value = 0;
    }

    if (matched) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = matched_value,
        };
        return MYLITE_OK;
    }
    if (!has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = unmatched_value,
        };
        return MYLITE_OK;
    }
    if (saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = unmatched_value,
    };
    return MYLITE_OK;
}

static int evaluate_quantified_subquery_expression(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_warnings saved_warnings = {0};
    struct mylite_expression_warnings subquery_warnings = {0};
    int status = MYLITE_UNSUPPORTED;

    if (stmt == NULL || stmt->database == NULL || expression == NULL || left == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    saved_warnings = stmt->database->warnings;
    stmt->database->warnings = (struct mylite_expression_warnings){0};

    status =
        evaluate_quantified_subquery_expression_inner(stmt, expression, left, warnings, out_value);

    subquery_warnings = stmt->database->warnings;
    stmt->database->warnings = saved_warnings;
    if (append_subquery_warnings(warnings, &subquery_warnings) != MYLITE_OK) {
        mylite_expression_value_deinit(out_value);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
    }
    mylite_expression_warnings_deinit(&subquery_warnings);
    return status;
}

static int evaluate_quantified_subquery_expression_inner(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *left, struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value)
{
    mylite_stmt *subquery_stmt = NULL;
    size_t order_key_count = 0U;
    bool restore_order_keys = false;
    struct mylite_quantified_subquery_scan_state scan = {
        .has_row = false,
        .decided = false,
        .result = false,
        .saw_unknown = false,
    };
    int status = MYLITE_OK;

    status = prepare_quantified_subquery_statement(stmt, expression, &subquery_stmt,
                                                   &order_key_count, &restore_order_keys);
    if (status != MYLITE_OK) {
        return status;
    }

    struct mylite_quantified_subquery_scan_context scan_context = {
        .outer_stmt = stmt,
        .subquery_stmt = subquery_stmt,
        .left = left,
        .warnings = warnings,
        .operator_kind = expression->operator_kind,
        .quantifier = expression->subquery_quantifier,
    };

    status = scan_quantified_subquery_statement(&scan_context, &scan);
    if (restore_order_keys) {
        subquery_stmt->select_plan.order_key_count = order_key_count;
    }
    mylite_finalize(subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    return finish_quantified_subquery_expression(expression->subquery_quantifier, &scan, out_value);
}

static int prepare_quantified_subquery_statement(mylite_stmt *stmt,
                                                 const struct mylite_sql_ast_node *expression,
                                                 mylite_stmt **out_subquery_stmt,
                                                 size_t *out_order_key_count,
                                                 bool *out_restore_order_keys)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);
    mylite_stmt *subquery_stmt = NULL;
    int status = MYLITE_OK;

    *out_subquery_stmt = NULL;
    *out_order_key_count = 0U;
    *out_restore_order_keys = false;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !quantified_comparison_operator_is_supported(expression->operator_kind) ||
        expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_NONE) {
        return MYLITE_UNSUPPORTED;
    }

    status = validate_in_subquery_select(stmt->database, select_statement);
    if (status != MYLITE_OK) {
        return status;
    }

    status = prepare_select_subquery_statement(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    status = validate_in_subquery_prepared_columns(stmt->database, subquery_stmt);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        return status;
    }
    if (subquery_stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        subquery_stmt->select_plan.order_key_count != 0U) {
        *out_order_key_count = subquery_stmt->select_plan.order_key_count;
        *out_restore_order_keys = true;
        subquery_stmt->select_plan.order_key_count = 0U;
    }
    *out_subquery_stmt = subquery_stmt;
    return MYLITE_OK;
}

static int
scan_quantified_subquery_statement(const struct mylite_quantified_subquery_scan_context *context,
                                   struct mylite_quantified_subquery_scan_state *state)
{
    for (;;) {
        int status = mylite_step(context->subquery_stmt);

        if (status == MYLITE_DONE) {
            return MYLITE_OK;
        }
        if (status != MYLITE_ROW) {
            return status;
        }
        state->has_row = true;
        status = scan_quantified_subquery_statement_row(context, state);
        if (status != MYLITE_OK || state->decided) {
            return status;
        }
    }
}

static int scan_quantified_subquery_statement_row(
    const struct mylite_quantified_subquery_scan_context *context,
    struct mylite_quantified_subquery_scan_state *state)
{
    struct mylite_expression_value right = {0};
    int comparison = 0;
    int status = MYLITE_OK;
    bool comparison_result = false;

    if (context->left->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        state->saw_unknown = true;
        return MYLITE_OK;
    }

    status = copy_subquery_statement_column_value(context->subquery_stmt, &right);
    if (status != MYLITE_OK) {
        mylite_expression_value_deinit(&right);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(context->outer_stmt->database,
                                                       "out of memory");
        }
        return status;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        state->saw_unknown = true;
        mylite_expression_value_deinit(&right);
        return MYLITE_OK;
    }
    status = mylite_expression_value_compare(context->left, &right, context->warnings, &comparison);
    mylite_expression_value_deinit(&right);
    if (status != 0) {
        return status;
    }

    comparison_result = quantified_comparison_result(context, comparison);
    if (context->quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL) {
        if (!comparison_result) {
            state->decided = true;
            state->result = false;
        }
        return MYLITE_OK;
    }
    if (comparison_result) {
        state->decided = true;
        state->result = true;
    }
    return MYLITE_OK;
}

static int
finish_quantified_subquery_expression(enum mylite_sql_ast_subquery_quantifier quantifier,
                                      const struct mylite_quantified_subquery_scan_state *scan,
                                      struct mylite_expression_value *out_value)
{
    if (!scan->has_row) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL ? 1 : 0};
        return MYLITE_OK;
    }
    if (scan->decided) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (int64_t)scan->result};
        return MYLITE_OK;
    }
    if (scan->saw_unknown) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL ? 1 : 0};
    return MYLITE_OK;
}

static bool
quantified_comparison_result(const struct mylite_quantified_subquery_scan_context *context,
                             int comparison)
{
    switch (context->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return comparison == 0;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return comparison != 0;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return comparison < 0;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return comparison <= 0;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return comparison > 0;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return comparison >= 0;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

static bool quantified_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

static int
evaluate_row_constructor_values(const struct mylite_sql_ast_node *row,
                                const struct mylite_expression_eval_context *expression_context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_row_expression_values *out_values)
{
    size_t width = row_constructor_width(row);
    size_t index = 0U;

    *out_values = (struct mylite_row_expression_values){0};
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || width < 2U) {
        return MYLITE_UNSUPPORTED;
    }
    out_values->items = calloc(width, sizeof(*out_values->items));
    if (out_values->items == NULL) {
        return MYLITE_NOMEM;
    }
    out_values->count = width;
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling, ++index) {
        int status = mylite_expression_eval_with_context(child, expression_context, warnings,
                                                         &out_values->items[index]);

        if (status != 0) {
            return status > 0 ? status : MYLITE_UNSUPPORTED;
        }
    }
    return MYLITE_OK;
}

static int copy_subquery_statement_row_values(mylite_stmt *stmt, size_t width,
                                              struct mylite_row_expression_values *out_values)
{
    *out_values = (struct mylite_row_expression_values){0};
    if (stmt == NULL || mylite_column_count(stmt) != (int)width) {
        return MYLITE_UNSUPPORTED;
    }
    if (width == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    out_values->items = calloc(width, sizeof(*out_values->items));
    if (out_values->items == NULL) {
        return MYLITE_NOMEM;
    }
    out_values->count = width;
    for (size_t index = 0U; index < width; ++index) {
        int status = copy_subquery_statement_row_value(stmt, index, &out_values->items[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int copy_subquery_statement_row_value(mylite_stmt *stmt, size_t index,
                                             struct mylite_expression_value *out_value)
{
    const struct mylite_expression_value *value = NULL;

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        if (index >= stmt->scalar_result.value_count) {
            return MYLITE_UNSUPPORTED;
        }
        return mylite_expression_value_copy(&stmt->scalar_result.values[index], out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }

    value = mylite_statement_table_select_current_output_value(stmt, (int)index);
    if (value != NULL) {
        return mylite_expression_value_copy(value, out_value) == 0 ? MYLITE_OK : MYLITE_NOMEM;
    }
    if (stmt->sqlite_stmt != NULL) {
        return mylite_sqlite_copy_column_value(stmt->sqlite_stmt, index, out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int compare_row_values(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_row_expression_values *left,
                              const struct mylite_row_expression_values *right,
                              struct mylite_expression_warnings *warnings, int *out_truth)
{
    int truth = -1;
    int status = MYLITE_OK;

    if (left == NULL || right == NULL || left->count != right->count || out_truth == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return compare_row_values_for_equality(left, right, warnings, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return compare_row_values_for_null_safe_equality(left, right, warnings, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        status = compare_row_values_for_equality(left, right, warnings, &truth);
        if (status != MYLITE_OK || truth < 0) {
            *out_truth = truth;
            return status;
        }
        *out_truth = truth == 0 ? 1 : 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return compare_row_values_for_order(operator_kind, left, right, warnings, out_truth);
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return MYLITE_UNSUPPORTED;
}

static int compare_row_values_for_equality(const struct mylite_row_expression_values *left,
                                           const struct mylite_row_expression_values *right,
                                           struct mylite_expression_warnings *warnings,
                                           int *out_truth)
{
    bool saw_unknown = false;

    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        int comparison = 0;
        int status = MYLITE_OK;

        if (left_value->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right_value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            saw_unknown = true;
            continue;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            *out_truth = 0;
            return MYLITE_OK;
        }
    }
    if (saw_unknown) {
        *out_truth = -1;
    } else {
        *out_truth = 1;
    }
    return MYLITE_OK;
}

static int
compare_row_values_for_null_safe_equality(const struct mylite_row_expression_values *left,
                                          const struct mylite_row_expression_values *right,
                                          struct mylite_expression_warnings *warnings,
                                          int *out_truth)
{
    bool saw_null = false;
    bool last_both_null = false;

    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        bool left_null = left_value->kind == MYLITE_EXPRESSION_VALUE_NULL;
        bool right_null = right_value->kind == MYLITE_EXPRESSION_VALUE_NULL;
        int comparison = 0;
        int status = MYLITE_OK;

        last_both_null = false;
        if (left_null || right_null) {
            saw_null = true;
            if (left_null != right_null) {
                *out_truth = 0;
                return MYLITE_OK;
            }
            last_both_null = true;
            continue;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            *out_truth = 0;
            return MYLITE_OK;
        }
    }
    if (!saw_null || last_both_null) {
        *out_truth = 1;
    } else {
        *out_truth = 0;
    }
    return MYLITE_OK;
}

static int compare_row_values_for_order(enum mylite_sql_ast_operator operator_kind,
                                        const struct mylite_row_expression_values *left,
                                        const struct mylite_row_expression_values *right,
                                        struct mylite_expression_warnings *warnings, int *out_truth)
{
    for (size_t index = 0U; index < left->count; ++index) {
        const struct mylite_expression_value *left_value = &left->items[index];
        const struct mylite_expression_value *right_value = &right->items[index];
        int comparison = 0;
        int status = MYLITE_OK;

        if (left_value->kind == MYLITE_EXPRESSION_VALUE_NULL ||
            right_value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_truth = -1;
            return MYLITE_OK;
        }
        status = mylite_expression_value_compare(left_value, right_value, warnings, &comparison);
        if (status != 0) {
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_UNSUPPORTED;
        }
        if (comparison != 0) {
            struct mylite_row_order_comparison order_comparison = {
                .operator_kind = operator_kind,
                .comparison = comparison,
            };

            return row_order_comparison_truth(order_comparison, out_truth);
        }
    }

    struct mylite_row_order_comparison order_comparison = {
        .operator_kind = operator_kind,
        .comparison = 0,
    };

    return row_order_comparison_truth(order_comparison, out_truth);
}

static int row_order_comparison_truth(struct mylite_row_order_comparison comparison, int *out_truth)
{
    bool matched = false;

    switch (comparison.operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        matched = comparison.comparison < 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        matched = comparison.comparison <= 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        matched = comparison.comparison > 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        matched = comparison.comparison >= 0;
        break;
    default:
        return MYLITE_UNSUPPORTED;
    }
    if (matched) {
        *out_truth = 1;
    } else {
        *out_truth = 0;
    }
    return MYLITE_OK;
}

static void row_expression_values_deinit(struct mylite_row_expression_values *values)
{
    if (values == NULL) {
        return;
    }
    for (size_t index = 0U; index < values->count; ++index) {
        mylite_expression_value_deinit(&values->items[index]);
    }
    free(values->items);
    *values = (struct mylite_row_expression_values){0};
}

static int evaluate_scalar_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(subquery, 0U);
    mylite_stmt *subquery_stmt = NULL;
    int status = validate_scalar_subquery_select_list(stmt->database, select_statement);

    if (status != MYLITE_OK) {
        return status;
    }

    status = prepare_select_subquery_statement(stmt->database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_DONE) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_finalize(subquery_stmt);
        return MYLITE_OK;
    }
    if (status != MYLITE_ROW) {
        mylite_finalize(subquery_stmt);
        return status;
    }

    status = copy_subquery_statement_column_value(subquery_stmt, out_value);
    if (status != MYLITE_OK) {
        mylite_finalize(subquery_stmt);
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        }
        return status;
    }

    status = mylite_step(subquery_stmt);
    if (status == MYLITE_ROW) {
        mylite_expression_value_deinit(out_value);
        status = set_scalar_subquery_cardinality_error(stmt->database);
    } else if (status == MYLITE_DONE) {
        status = MYLITE_OK;
    }
    mylite_finalize(subquery_stmt);
    return status;
}

static int evaluate_exists_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(subquery, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    mylite_stmt *subquery_stmt = NULL;
    bool has_row = false;
    bool negated = subquery->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT;
    int64_t exists_value = 0;
    int status = MYLITE_OK;

    if (select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        has_row = true;
    } else {
        status =
            prepare_select_subquery_statement(stmt->database, select_statement, &subquery_stmt);
        if (status != MYLITE_OK) {
            return status;
        }
        if (subquery_stmt == NULL) {
            return MYLITE_UNSUPPORTED;
        }
        status = subquery_statement_has_row(subquery_stmt, &has_row);
        mylite_finalize(subquery_stmt);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (negated) {
        if (has_row) {
            has_row = false;
        } else {
            has_row = true;
        }
    }
    if (has_row) {
        exists_value = 1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = exists_value};
    return MYLITE_OK;
}

static int subquery_statement_has_row(mylite_stmt *stmt, bool *out_has_row)
{
    int status = MYLITE_OK;

    *out_has_row = false;
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        *out_has_row = true;
        stmt->executed = true;
        stmt->affected_rows = -1;
        return MYLITE_OK;
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        enum mylite_sql_ast_select_duplicate_mode duplicate_mode = stmt->select_plan.duplicate_mode;
        size_t order_key_count = stmt->select_plan.order_key_count;

        stmt->select_plan.duplicate_mode = MYLITE_SQL_AST_SELECT_DUPLICATES_IMPLICIT_ALL;
        stmt->select_plan.order_key_count = 0U;
        status = materialize_table_select_result(stmt);
        stmt->select_plan.duplicate_mode = duplicate_mode;
        stmt->select_plan.order_key_count = order_key_count;
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->executed = true;
        stmt->affected_rows = -1;
        *out_has_row = stmt->select_result.row_count != 0U;
        return MYLITE_OK;
    }

    status = mylite_step(stmt);
    if (status == MYLITE_ROW) {
        *out_has_row = true;
        return MYLITE_OK;
    }
    if (status == MYLITE_DONE) {
        return MYLITE_OK;
    }
    return status;
}

static int copy_subquery_statement_column_value(mylite_stmt *stmt,
                                                struct mylite_expression_value *out_value)
{
    const struct mylite_expression_value *value = NULL;

    if (stmt == NULL || mylite_column_count(stmt) != 1) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return mylite_expression_value_copy(&stmt->scalar_result.values[0], out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    value = mylite_statement_table_select_current_output_value(stmt, 0);
    if (value != NULL) {
        return mylite_expression_value_copy(value, out_value) == 0 ? MYLITE_OK : MYLITE_NOMEM;
    }
    if (stmt->sqlite_stmt != NULL) {
        return mylite_sqlite_copy_column_value(stmt->sqlite_stmt, 0U, out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int validate_scalar_subquery_select_list(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    size_t column_count = 0U;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        ++column_count;
    }
    if (column_count != 1U) {
        return set_subquery_operand_columns_error(database);
    }
    return MYLITE_OK;
}

static int validate_in_subquery_select(mylite_db *database,
                                       const struct mylite_sql_ast_node *statement)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return set_in_subquery_limit_error(database);
    }
    return validate_scalar_subquery_select_list(database, statement);
}

static int validate_in_subquery_prepared_columns(mylite_db *database, const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_column_count(stmt) != 1) {
        return set_subquery_operand_columns_error(database);
    }
    return MYLITE_OK;
}

static int validate_row_subquery_select_columns(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                size_t expected_width)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    size_t column_count = 0U;
    bool has_wildcard = false;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT ||
        select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = mylite_ast_child_at(item, 0U);

        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD) {
            has_wildcard = true;
        }
        ++column_count;
    }
    if (!has_wildcard && column_count != expected_width) {
        return set_subquery_operand_column_count_error(database, expected_width);
    }
    return MYLITE_OK;
}

static int validate_row_subquery_prepared_columns(mylite_db *database, const mylite_stmt *stmt,
                                                  size_t expected_width)
{
    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_column_count(stmt) != (int)expected_width) {
        return set_subquery_operand_column_count_error(database, expected_width);
    }
    return MYLITE_OK;
}

static int append_subquery_warnings(struct mylite_expression_warnings *destination,
                                    const struct mylite_expression_warnings *source)
{
    if (source == NULL) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < source->count; ++index) {
        if (mylite_expression_warnings_append_condition(destination, source->items[index].level,
                                                        source->items[index].code,
                                                        source->items[index].message) != 0) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int set_subquery_operand_columns_error(mylite_db *database)
{
    return set_subquery_operand_column_count_error(database, 1U);
}

static int set_subquery_operand_column_count_error(mylite_db *database, size_t expected_width)
{
    char *message = sqlite3_mprintf("Operand should contain %llu column(s)",
                                    (unsigned long long)expected_width);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message(database, message);
    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_OPERAND_COLUMNS, message);
    }
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_in_subquery_limit_error(mylite_db *database)
{
    static const char message[] =
        "This version of MySQL doesn't yet support 'LIMIT & IN/ALL/ANY/SOME subquery'";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_NOT_SUPPORTED_YET, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_row_quantified_non_alias_error(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 1U);

    if (mylite_ast_find_child_kind(select_statement, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        return set_in_subquery_limit_error(database);
    }
    return set_subquery_operand_columns_error(database);
}

static int set_scalar_subquery_cardinality_error(mylite_db *database)
{
    static const char message[] = "Subquery returns more than 1 row";
    int status = mylite_diagnostics_set_error_message(database, message);

    if (status == MYLITE_OK) {
        status =
            mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_SUBQUERY_NO_1_ROW, message);
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static void table_select_group_deinit(struct mylite_table_select_group *group)
{
    if (group == NULL) {
        return;
    }

    mylite_select_row_deinit(&group->representative);
    for (size_t index = 0U; index < group->group_value_count; ++index) {
        mylite_expression_value_deinit(&group->group_values[index]);
    }
    for (size_t index = 0U; index < group->aggregate_state_count; ++index) {
        mylite_select_aggregate_state_deinit(&group->aggregate_states[index]);
    }
    free(group->group_values);
    free(group->aggregate_states);
    *group = (struct mylite_table_select_group){0};
}

static bool row_subquery_expression_is_supported(const struct mylite_sql_ast_node *expression)
{
    if (binary_expression_is_row_in_subquery(expression)) {
        return true;
    }
    if (binary_expression_is_row_scalar_subquery(expression)) {
        return true;
    }
    return quantified_comparison_is_row_subquery_alias(expression);
}

static bool row_subquery_expression_is_membership(const struct mylite_sql_ast_node *expression)
{
    if (binary_expression_is_row_in_subquery(expression)) {
        return true;
    }
    return quantified_comparison_is_row_subquery_alias(expression);
}

static bool
row_subquery_expression_is_positive_membership(const struct mylite_sql_ast_node *expression)
{
    if (binary_expression_is_row_in_subquery(expression)) {
        return expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN;
    }
    if (quantified_comparison_is_row_subquery_alias(expression)) {
        if (expression->operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL) {
            return false;
        }
        if (expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY) {
            return true;
        }
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
    }
    return false;
}

static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *expression)
{
    if (binary_expression_is_row_in_subquery(expression)) {
        return true;
    }
    return binary_expression_is_row_scalar_subquery(expression);
}

static bool binary_expression_is_row_in_subquery(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *right =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_IN) {
        return true;
    }
    return expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN;
}

static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
    const struct mylite_sql_ast_node *right =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION) {
        return false;
    }
    return row_subquery_comparison_operator_is_supported(expression->operator_kind);
}

static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

static const struct mylite_sql_ast_node *
row_subquery_select_statement(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *right =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 1U));

    if (quantified_comparison_is_row_subquery_alias(expression)) {
        return mylite_ast_child_at(expression, 1U);
    }
    if (binary_expression_is_row_in_subquery(expression)) {
        return right;
    }
    if (binary_expression_is_row_scalar_subquery(expression)) {
        return mylite_ast_child_at(right, 0U);
    }
    return NULL;
}

static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *left =
        unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        left == NULL) {
        return false;
    }
    return left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR;
}

static bool
quantified_comparison_is_row_subquery_alias(const struct mylite_sql_ast_node *expression)
{
    if (!quantified_comparison_has_row_left(expression)) {
        return false;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL) {
        if (expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY) {
            return true;
        }
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
    }
    if (expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        return expression->subquery_quantifier == MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL;
    }
    return false;
}

static size_t row_constructor_width(const struct mylite_sql_ast_node *row)
{
    size_t width = 0U;

    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return 0U;
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        ++width;
    }
    return width;
}

static bool binary_expression_is_in_subquery(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *right = mylite_ast_child_at(expression, 1U);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    if (expression->operator_kind != MYLITE_SQL_AST_OPERATOR_IN &&
        expression->operator_kind != MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        return false;
    }
    if (right == NULL) {
        return false;
    }
    switch (right->kind) {
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return true;
    default:
        return false;
    }
}
